// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_permission_dispatcher.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_frame_impl.h"
#include "media/base/media_permission.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {
namespace {

class FakePermissionService : public blink::mojom::PermissionService {
 public:
  FakePermissionService() = default;
  FakePermissionService(const FakePermissionService&) = delete;
  FakePermissionService& operator=(const FakePermissionService&) = delete;
  ~FakePermissionService() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this, mojo::PendingReceiver<blink::mojom::PermissionService>(
                             std::move(handle)));
  }

  void WaitForRequests(size_t count = 1) {
    while (pending_callbacks_.size() < count) {
      base::test::TestFuture<void> future;
      on_request_ = future.GetCallback();
      EXPECT_TRUE(future.Wait());
    }
  }

  void ReplyNext(blink::mojom::PermissionStatus status) {
    ASSERT_FALSE(pending_callbacks_.empty());
    auto callback = std::move(pending_callbacks_.front());
    pending_callbacks_.erase(pending_callbacks_.begin());
    std::move(callback).Run(
        blink::mojom::PermissionStatusWithDetails::New(status, nullptr));
  }

  void DisconnectAll() {
    receivers_.Clear();
    pending_callbacks_.clear();
  }

  // blink::mojom::PermissionService:
  void HasPermission(blink::mojom::PermissionDescriptorPtr permission,
                     HasPermissionCallback callback) override {
    pending_callbacks_.push_back(std::move(callback));
    if (on_request_) {
      std::move(on_request_).Run();
    }
  }

  void RequestPermission(blink::mojom::PermissionDescriptorPtr permission,
                         RequestPermissionCallback callback) override {
    pending_callbacks_.push_back(std::move(callback));
    if (on_request_) {
      std::move(on_request_).Run();
    }
  }

  void RegisterPageEmbeddedPermissionControl(
      std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
      blink::mojom::EmbeddedPermissionRequestDescriptorPtr descriptor,
      mojo::PendingRemote<blink::mojom::EmbeddedPermissionControlClient> client)
      override {
    NOTREACHED();
  }

  void RequestPageEmbeddedPermission(
      std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
      blink::mojom::EmbeddedPermissionRequestDescriptorPtr descriptor,
      RequestPageEmbeddedPermissionCallback callback) override {
    NOTREACHED();
  }

  void RequestPermissions(
      std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
      RequestPermissionsCallback callback) override {
    NOTREACHED();
  }

  void RevokePermission(blink::mojom::PermissionDescriptorPtr permission,
                        RevokePermissionCallback callback) override {
    NOTREACHED();
  }

  void AddPermissionObserver(
      blink::mojom::PermissionDescriptorPtr permission,
      blink::mojom::PermissionStatusWithDetailsPtr last_known_status,
      mojo::PendingRemote<blink::mojom::PermissionObserver> observer) override {
    NOTREACHED();
  }

  void AddPageEmbeddedPermissionObserver(
      blink::mojom::PermissionDescriptorPtr permission,
      blink::mojom::PermissionStatus last_known_status,
      mojo::PendingRemote<blink::mojom::PermissionObserver> observer) override {
    NOTREACHED();
  }

  void NotifyEventListener(blink::mojom::PermissionDescriptorPtr permission,
                           const std::string& event_type,
                           bool is_added) override {
    NOTREACHED();
  }

 private:
  base::OnceClosure on_request_;
  std::vector<RequestPermissionCallback> pending_callbacks_;
  // Declared after `pending_callbacks_` so receivers close before callbacks are
  // destroyed during teardown.
  mojo::ReceiverSet<blink::mojom::PermissionService> receivers_;
};

class ScopedPermissionServiceBinder {
 public:
  ScopedPermissionServiceBinder(RenderFrame* frame,
                                FakePermissionService* service)
      : broker_(&frame->GetBrowserInterfaceBroker()) {
    broker_->SetBinderForTesting(
        blink::mojom::PermissionService::Name_,
        base::BindRepeating(&FakePermissionService::Bind,
                            base::Unretained(service)));
  }

  ~ScopedPermissionServiceBinder() {
    if (broker_) {
      broker_->SetBinderForTesting(blink::mojom::PermissionService::Name_, {});
    }
  }

  void Release() { broker_ = nullptr; }

 private:
  raw_ptr<const blink::BrowserInterfaceBrokerProxy> broker_;
};

}  // namespace

using MediaPermissionDispatcherTest = RenderViewTest;

// Regression test for crbug.com/582925571: navigating a child frame on its
// initial empty document synchronously to about:blank must not run pending
// permission callbacks synchronously on the DidCommitNavigation stack, where
// page script could detach and destroy the RenderFrameImpl mid-commit.
TEST_F(MediaPermissionDispatcherTest,
       OnNavigationDeliversPendingCallbacksAsynchronouslyAndSurvivesDetach) {
  LoadHTML("<iframe id='child' src='javascript:void 0'></iframe>");

  blink::WebFrame* child_web_frame = GetMainFrame()->FirstChild();
  ASSERT_TRUE(child_web_frame);
  RenderFrameImpl* child_frame =
      RenderFrameImpl::FromWebFrame(child_web_frame->ToWebLocalFrame());
  ASSERT_TRUE(child_frame);

  FakePermissionService permission_service;
  ScopedPermissionServiceBinder binder(child_frame, &permission_service);

  bool in_synchronous_commit = false;
  bool callback_ran = false;
  base::test::TestFuture<bool> result_future;

  child_frame->GetMediaPermission()->RequestPermission(
      media::MediaPermission::Type::kProtectedMediaIdentifier,
      base::BindLambdaForTesting([&](bool granted) {
        callback_ran = true;
        EXPECT_FALSE(in_synchronous_commit);
        // Simulate page script detaching the iframe when the callback settles
        // a web-visible promise. Release `binder` first since removing the
        // iframe destroys `child_frame`.
        binder.Release();
        ExecuteJavaScriptForTests("document.getElementById('child').remove();");
        result_future.SetValue(granted);
      }));
  permission_service.WaitForRequests();

  in_synchronous_commit = true;
  ExecuteJavaScriptForTests(
      "document.getElementById('child').src = 'about:blank';");
  in_synchronous_commit = false;

  EXPECT_FALSE(callback_ran);
  EXPECT_FALSE(result_future.Get());
  EXPECT_EQ(nullptr, GetMainFrame()->FirstChild());
}

TEST_F(MediaPermissionDispatcherTest,
       DestructorDeliversPendingCallbacksAsynchronously) {
  LoadHTML("<iframe id='child' src='javascript:void 0'></iframe>");

  blink::WebFrame* child_web_frame = GetMainFrame()->FirstChild();
  ASSERT_TRUE(child_web_frame);
  RenderFrameImpl* child_frame =
      RenderFrameImpl::FromWebFrame(child_web_frame->ToWebLocalFrame());
  ASSERT_TRUE(child_frame);

  FakePermissionService permission_service;
  ScopedPermissionServiceBinder binder(child_frame, &permission_service);

  bool in_frame_detach = false;
  bool callback_ran = false;
  base::test::TestFuture<bool> result_future;

  child_frame->GetMediaPermission()->HasPermission(
      media::MediaPermission::Type::kAudioCapture,
      base::BindLambdaForTesting([&](bool granted) {
        callback_ran = true;
        EXPECT_FALSE(in_frame_detach);
        result_future.SetValue(granted);
      }));
  permission_service.WaitForRequests();

  binder.Release();
  in_frame_detach = true;
  ExecuteJavaScriptForTests("document.getElementById('child').remove();");
  in_frame_detach = false;

  EXPECT_FALSE(callback_ran);
  EXPECT_FALSE(result_future.Get());
}

TEST_F(MediaPermissionDispatcherTest,
       ConnectionErrorReconnectsAndDeliversReplies) {
  RenderFrameImpl* main_frame = RenderFrameImpl::FromWebFrame(GetMainFrame());
  ASSERT_TRUE(main_frame);

  FakePermissionService permission_service;
  ScopedPermissionServiceBinder binder(main_frame, &permission_service);

  base::test::TestFuture<bool> first_result;
  main_frame->GetMediaPermission()->HasPermission(
      media::MediaPermission::Type::kVideoCapture, first_result.GetCallback());
  permission_service.WaitForRequests();

  permission_service.DisconnectAll();
  EXPECT_FALSE(first_result.Get());

  base::test::TestFuture<bool> second_result;
  main_frame->GetMediaPermission()->RequestPermission(
      media::MediaPermission::Type::kProtectedMediaIdentifier,
      second_result.GetCallback());
  permission_service.WaitForRequests();

  permission_service.ReplyNext(blink::mojom::PermissionStatus::GRANTED);
  EXPECT_TRUE(second_result.Get());
}

TEST_F(MediaPermissionDispatcherTest,
       ConnectionErrorWithMultipleCallbacksSurvivesFrameDetach) {
  LoadHTML("<iframe id='child' src='javascript:void 0'></iframe>");

  blink::WebFrame* child_web_frame = GetMainFrame()->FirstChild();
  ASSERT_TRUE(child_web_frame);
  RenderFrameImpl* child_frame =
      RenderFrameImpl::FromWebFrame(child_web_frame->ToWebLocalFrame());
  ASSERT_TRUE(child_frame);

  FakePermissionService permission_service;
  ScopedPermissionServiceBinder binder(child_frame, &permission_service);

  bool first_callback_on_stack = false;
  base::test::TestFuture<bool> first_result;
  base::test::TestFuture<bool> second_result;

  child_frame->GetMediaPermission()->RequestPermission(
      media::MediaPermission::Type::kProtectedMediaIdentifier,
      base::BindLambdaForTesting([&](bool granted) {
        first_callback_on_stack = true;
        binder.Release();
        ExecuteJavaScriptForTests("document.getElementById('child').remove();");
        first_result.SetValue(granted);
        first_callback_on_stack = false;
      }));
  child_frame->GetMediaPermission()->HasPermission(
      media::MediaPermission::Type::kAudioCapture,
      base::BindLambdaForTesting([&](bool granted) {
        EXPECT_FALSE(first_callback_on_stack);
        second_result.SetValue(granted);
      }));
  permission_service.WaitForRequests(2);
  permission_service.DisconnectAll();

  EXPECT_FALSE(first_result.Get());
  EXPECT_FALSE(second_result.Get());
  EXPECT_EQ(nullptr, GetMainFrame()->FirstChild());
}

TEST_F(MediaPermissionDispatcherTest,
       OffSequenceRequestDeliversCallbackOnCallingSequence) {
  RenderFrameImpl* main_frame = RenderFrameImpl::FromWebFrame(GetMainFrame());
  ASSERT_TRUE(main_frame);

  FakePermissionService permission_service;
  ScopedPermissionServiceBinder binder(main_frame, &permission_service);

  scoped_refptr<base::SingleThreadTaskRunner> worker_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner({});
  media::MediaPermission* media_permission = main_frame->GetMediaPermission();

  base::test::TestFuture<bool> result_future;
  auto reply_callback = result_future.GetSequenceBoundCallback();

  worker_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        media_permission->HasPermission(
            media::MediaPermission::Type::kAudioCapture,
            base::BindOnce(
                [](scoped_refptr<base::SingleThreadTaskRunner> expected_runner,
                   base::OnceCallback<void(bool)> done, bool granted) {
                  EXPECT_TRUE(expected_runner->RunsTasksInCurrentSequence());
                  std::move(done).Run(granted);
                },
                worker_runner, std::move(reply_callback)));
      }));
  permission_service.WaitForRequests();

  permission_service.DisconnectAll();

  EXPECT_FALSE(result_future.Get());
}

}  // namespace content
