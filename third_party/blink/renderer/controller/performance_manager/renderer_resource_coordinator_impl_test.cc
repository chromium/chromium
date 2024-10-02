// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/renderer_resource_coordinator_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

using performance_manager::mojom::blink::IframeAttributionData;
using performance_manager::mojom::blink::IframeAttributionDataPtr;
using performance_manager::mojom::blink::ProcessCoordinationUnit;
using performance_manager::mojom::blink::V8ContextDescription;
using performance_manager::mojom::blink::V8ContextDescriptionPtr;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Pointee;

class MockProcessCoordinationUnit : public ProcessCoordinationUnit {
 public:
  explicit MockProcessCoordinationUnit(
      mojo::PendingReceiver<ProcessCoordinationUnit> receiver)
      : receiver_(this, std::move(receiver)) {}

  // Don't mock uninteresting property signals.
  void SetMainThreadTaskLoadIsLow(bool main_thread_task_load_is_low) final {}

  MOCK_METHOD(void,
              OnV8ContextCreated,
              (V8ContextDescriptionPtr description,
               IframeAttributionDataPtr attribution),
              (override));
  MOCK_METHOD(void,
              OnV8ContextDetached,
              (const blink::V8ContextToken& token),
              (override));
  MOCK_METHOD(void,
              OnV8ContextDestroyed,
              (const blink::V8ContextToken& token),
              (override));
  MOCK_METHOD(void,
              OnRemoteIframeAttached,
              (const blink::LocalFrameToken& parent_frame_token,
               const blink::RemoteFrameToken& remote_frame_token,
               IframeAttributionDataPtr attribution),
              (override));
  MOCK_METHOD(void,
              OnRemoteIframeDetached,
              (const blink::LocalFrameToken& parent_frame_token,
               const blink::RemoteFrameToken& remote_frame_token),
              (override));

  void VerifyExpectations() {
    // Ensure that any pending Mojo messages are processed.
    receiver_.FlushForTesting();
    Mock::VerifyAndClearExpectations(this);
  }

 private:
  mojo::Receiver<ProcessCoordinationUnit> receiver_;
};

MATCHER_P(MatchV8ContextDescription,
          execution_context_token,
          "V8ContextDescription::execution_context_token matches") {
  return arg->execution_context_token ==
         blink::ExecutionContextToken(execution_context_token);
}

MATCHER_P2(MatchAndSaveV8ContextDescription,
           execution_context_token,
           output_token,
           "V8ContextDescription::execution_context_token matches") {
  DCHECK(output_token);
  *output_token = arg->token;
  return arg->execution_context_token ==
         blink::ExecutionContextToken(execution_context_token);
}

}  // namespace

class RendererResourceCoordinatorImplTest : public ::testing::Test {
 protected:
  void TearDown() override {
    // Uninstall any RendererResourceCoordinator that was set by
    // InitializeMockProcessCoordinationUnit.
    RendererResourceCoordinator::Set(nullptr);
  }

  template <typename MockType>
  void InitializeMockProcessCoordinationUnit() {
    DCHECK(!mock_process_coordination_unit_);
    DCHECK(!resource_coordinator_);

    mojo::PendingRemote<ProcessCoordinationUnit> pending_remote;
    mock_process_coordination_unit_ = std::make_unique<MockType>(
        pending_remote.InitWithNewPipeAndPassReceiver());

    // Create a RendererResourceCoordinator bound to the other end of the
    // MockProcessCoordinationUnit's remote.
    // Can't use make_unique with a private constructor.
    resource_coordinator_ = base::WrapUnique(
        new RendererResourceCoordinatorImpl(std::move(pending_remote)));
    RendererResourceCoordinator::Set(resource_coordinator_.get());
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockProcessCoordinationUnit> mock_process_coordination_unit_;
  std::unique_ptr<RendererResourceCoordinatorImpl> resource_coordinator_;
};

TEST_F(RendererResourceCoordinatorImplTest, IframeNotifications) {
  InitializeMockProcessCoordinationUnit<
      ::testing::StrictMock<MockProcessCoordinationUnit>>();

  frame_test_helpers::WebViewHelper helper;
  helper.InitializeAndLoad("about:blank");

  // The <iframe> tag will have a fixed id attribute and no src attribute.
  auto iframe_attribution_matcher =
      Pointee(AllOf(Field(&IframeAttributionData::id, "iframe-id"),
                    Field(&IframeAttributionData::src, WTF::String())));

  // Create an empty frame. This will send a notification as the main frame's
  // context is created.
  WebLocalFrameImpl* main_frame = helper.GetWebView()->MainFrameImpl();
  EXPECT_CALL(
      *mock_process_coordination_unit_,
      OnV8ContextCreated(
          MatchV8ContextDescription(main_frame->GetLocalFrameToken()), _));
  // This load must include some non-empty script to force context creation.
  frame_test_helpers::LoadHTMLString(
      main_frame,
      "<!DOCTYPE html>"
      "<iframe id='iframe-id'></iframe><script>0;</script>",
      url_test_helpers::ToKURL("https://example.com/subframe.html"));
  mock_process_coordination_unit_->VerifyExpectations();

  // Swap for a remote frame.
  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  EXPECT_CALL(*mock_process_coordination_unit_,
              OnRemoteIframeAttached(main_frame->GetLocalFrameToken(),
                                     remote_frame->GetRemoteFrameToken(),
                                     iframe_attribution_matcher));
  frame_test_helpers::SwapRemoteFrame(main_frame->FirstChild(), remote_frame);
  mock_process_coordination_unit_->VerifyExpectations();

  // Create another remote frame, this time with a remote parent. No
  // notification should be received.
  frame_test_helpers::CreateRemoteChild(*remote_frame);
  mock_process_coordination_unit_->VerifyExpectations();

  // Test frame swaps. Each one should send a detach notification for the
  // current frame and an attach notification for the new frame.

  // Save the V8ContextToken reported in OnV8ContextCreated so it can be
  // compared with the token in the matching OnV8ContextDetached.
  blink::V8ContextToken current_v8_context_token;

  // Remote -> Remote
  WebRemoteFrameImpl* new_remote_frame = frame_test_helpers::CreateRemote();
  {
    InSequence seq;
    EXPECT_CALL(*mock_process_coordination_unit_,
                OnRemoteIframeDetached(main_frame->GetLocalFrameToken(),
                                       remote_frame->GetRemoteFrameToken()));
    EXPECT_CALL(*mock_process_coordination_unit_,
                OnRemoteIframeAttached(main_frame->GetLocalFrameToken(),
                                       new_remote_frame->GetRemoteFrameToken(),
                                       iframe_attribution_matcher));
  }
  frame_test_helpers::SwapRemoteFrame(main_frame->FirstChild(),
                                      new_remote_frame);
  mock_process_coordination_unit_->VerifyExpectations();

  // Remote -> Local
  WebLocalFrameImpl* local_frame = helper.CreateProvisional(*new_remote_frame);
  {
    InSequence seq;
    EXPECT_CALL(
        *mock_process_coordination_unit_,
        OnRemoteIframeDetached(main_frame->GetLocalFrameToken(),
                               new_remote_frame->GetRemoteFrameToken()));
    EXPECT_CALL(*mock_process_coordination_unit_,
                OnV8ContextCreated(MatchAndSaveV8ContextDescription(
                                       local_frame->GetLocalFrameToken(),
                                       &current_v8_context_token),
                                   iframe_attribution_matcher));
  }
  // Committing a navigation in the provisional frame swaps it in.
  frame_test_helpers::LoadFrame(local_frame, "data:text/html,");
  mock_process_coordination_unit_->VerifyExpectations();

  // Local -> Local
  WebLocalFrameImpl* new_local_frame = helper.CreateProvisional(*local_frame);
  {
    InSequence seq;
    EXPECT_CALL(*mock_process_coordination_unit_,
                OnV8ContextDetached(current_v8_context_token));
    EXPECT_CALL(*mock_process_coordination_unit_,
                OnV8ContextCreated(MatchAndSaveV8ContextDescription(
                                       new_local_frame->GetLocalFrameToken(),
                                       &current_v8_context_token),
                                   iframe_attribution_matcher));
  }
  // Committing a navigation in the provisional frame swaps it in.
  frame_test_helpers::LoadFrame(new_local_frame, "data:text/html,");
  mock_process_coordination_unit_->VerifyExpectations();

  // Local -> Remote
  remote_frame = frame_test_helpers::CreateRemote();
  {
    InSequence seq;
    EXPECT_CALL(*mock_process_coordination_unit_,
                OnV8ContextDetached(current_v8_context_token));
    EXPECT_CALL(*mock_process_coordination_unit_,
                OnRemoteIframeAttached(main_frame->GetLocalFrameToken(),
                                       remote_frame->GetRemoteFrameToken(),
                                       iframe_attribution_matcher));
  }
  frame_test_helpers::SwapRemoteFrame(main_frame->FirstChild(), remote_frame);
  mock_process_coordination_unit_->VerifyExpectations();
}

TEST_F(RendererResourceCoordinatorImplTest, NonIframeNotifications) {
  // Don't care about mocked methods except for OnRemoteIframeAttached.
  InitializeMockProcessCoordinationUnit<
      ::testing::NiceMock<MockProcessCoordinationUnit>>();

  frame_test_helpers::WebViewHelper helper;
  helper.InitializeAndLoad("about:blank");

  // Create an empty frame.
  WebLocalFrameImpl* main_frame = helper.GetWebView()->MainFrameImpl();
  frame_test_helpers::LoadHTMLString(
      main_frame,
      "<!DOCTYPE html>"
      "<object type=\"text/html\"></object>",
      url_test_helpers::ToKURL("https://example.com/subframe.html"));

  // Swap for a remote frame. Since this is not an iframe, there should be no
  // notification.
  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  EXPECT_CALL(*mock_process_coordination_unit_, OnRemoteIframeAttached(_, _, _))
      .Times(0);
  frame_test_helpers::SwapRemoteFrame(main_frame->FirstChild(), remote_frame);
  mock_process_coordination_unit_->VerifyExpectations();
}

}  // namespace blink
