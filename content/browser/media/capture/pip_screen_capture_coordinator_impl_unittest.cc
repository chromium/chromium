// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/pip_screen_capture_coordinator_impl.h"

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "media/capture/capture_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using NativeWindowId = content::NativeWindowId;
using testing::_;

namespace content {

namespace {

class MockObserver : public PipScreenCaptureCoordinatorImpl::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnPipWindowIdChanged,
              (std::optional<NativeWindowId>),
              (override));
  MOCK_METHOD(
      void,
      OnCapturesChanged,
      (const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo>&),
      (override));
};

class MockProxyObserver : public PipScreenCaptureCoordinatorProxy::Observer {
 public:
  MockProxyObserver() = default;
  ~MockProxyObserver() override = default;

  MOCK_METHOD(void,
              OnPipWindowIdChanged,
              (const std::optional<NativeWindowId>&),
              (override));
  MOCK_METHOD(
      void,
      OnCapturesChanged,
      (const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo>&),
      (override));
};

void CallOnPipShownAndWaitUntilDone(
    content::BrowserTaskEnvironment& task_environment,
    PipScreenCaptureCoordinatorImpl* coordinator,
    NativeWindowId window_id) {
  base::RunLoop run_loop;
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](PipScreenCaptureCoordinatorImpl* coordinator,
             NativeWindowId window_id, base::OnceClosure quit_closure) {
            coordinator->OnPipShown(window_id);
            std::move(quit_closure).Run();
          },
          base::Unretained(coordinator), window_id, run_loop.QuitClosure()));
  run_loop.Run();
}

void CallOnPipClosedAndWaitForObserver(
    content::BrowserTaskEnvironment& task_environment,
    PipScreenCaptureCoordinatorImpl* coordinator,
    MockProxyObserver& observer) {
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnPipWindowIdChanged(std::optional<NativeWindowId>()))
      .WillOnce([&run_loop](const auto&) { run_loop.Quit(); });
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PipScreenCaptureCoordinatorImpl* coordinator) {
                       coordinator->OnPipClosed();
                     },
                     base::Unretained(coordinator)));
  run_loop.Run();
}

void CallOnPipShownAndWaitForObserver(
    content::BrowserTaskEnvironment& task_environment,
    PipScreenCaptureCoordinatorImpl* coordinator,
    MockProxyObserver& observer,
    const std::optional<NativeWindowId>& new_pip_window_id) {
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnPipWindowIdChanged(new_pip_window_id))
      .WillOnce([&run_loop](const auto&) { run_loop.Quit(); });
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](PipScreenCaptureCoordinatorImpl* coordinator,
             NativeWindowId window_id) { coordinator->OnPipShown(window_id); },
          base::Unretained(coordinator), *new_pip_window_id));
  run_loop.Run();
}

}  // namespace

class PipScreenCaptureCoordinatorImplTest : public testing::Test {
 public:
  PipScreenCaptureCoordinatorImplTest() {
    feature_list_.InitAndEnableFeature(features::kExcludePipFromScreenCapture);
    coordinator_ = PipScreenCaptureCoordinatorImpl::GetInstance();
  }

  void TearDown() override { coordinator_->ResetForTesting(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<PipScreenCaptureCoordinatorImpl> coordinator_;
};

TEST_F(PipScreenCaptureCoordinatorImplTest, PipWindowId) {
  EXPECT_EQ(coordinator_->PipWindowId(), std::nullopt);

  const NativeWindowId pip_window_id = 123;
  coordinator_->OnPipShown(pip_window_id);
  EXPECT_EQ(coordinator_->PipWindowId(), pip_window_id);

  coordinator_->OnPipClosed();
  EXPECT_EQ(coordinator_->PipWindowId(), std::nullopt);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, OnPipShownNotifiesObservers) {
  MockObserver observer;
  coordinator_->AddObserver(&observer);

  const NativeWindowId pip_window_id = 123;
  EXPECT_CALL(observer,
              OnPipWindowIdChanged(std::make_optional(pip_window_id)));
  coordinator_->OnPipShown(pip_window_id);

  EXPECT_EQ(coordinator_->PipWindowId(), pip_window_id);

  // Calling again with the same ID should not notify.
  EXPECT_CALL(observer, OnPipWindowIdChanged(_)).Times(0);
  coordinator_->OnPipShown(pip_window_id);

  coordinator_->RemoveObserver(&observer);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, OnPipClosedNotifiesObservers) {
  MockObserver observer;
  coordinator_->AddObserver(&observer);

  const NativeWindowId pip_window_id = 123;
  EXPECT_CALL(observer,
              OnPipWindowIdChanged(std::make_optional(pip_window_id)));
  coordinator_->OnPipShown(pip_window_id);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnPipWindowIdChanged(testing::Eq(std::nullopt)));
  coordinator_->OnPipClosed();

  coordinator_->RemoveObserver(&observer);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, AddAndRemoveObserver) {
  MockObserver observer1;
  MockObserver observer2;

  coordinator_->AddObserver(&observer1);
  coordinator_->AddObserver(&observer2);

  const NativeWindowId pip_window_id = 123;
  EXPECT_CALL(observer1,
              OnPipWindowIdChanged(std::make_optional(pip_window_id)));
  EXPECT_CALL(observer2,
              OnPipWindowIdChanged(std::make_optional(pip_window_id)));
  coordinator_->OnPipShown(pip_window_id);
  testing::Mock::VerifyAndClearExpectations(&observer1);
  testing::Mock::VerifyAndClearExpectations(&observer2);

  coordinator_->RemoveObserver(&observer1);

  const NativeWindowId new_pip_window_id = 456;
  EXPECT_CALL(observer1, OnPipWindowIdChanged(_)).Times(0);
  EXPECT_CALL(observer2,
              OnPipWindowIdChanged(std::make_optional(new_pip_window_id)));
  coordinator_->OnPipShown(new_pip_window_id);

  coordinator_->RemoveObserver(&observer2);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, CreateProxy) {
  // The proxy should start with the current ID.
  const NativeWindowId pip_window_id = 123;
  CallOnPipShownAndWaitUntilDone(task_environment_, coordinator_,
                                 pip_window_id);
  auto proxy = coordinator_->CreateProxy();
  ASSERT_TRUE(proxy);

  // The proxy's ID should be the initial pip_window_id.
  MockProxyObserver observer;
  proxy->AddObserver(&observer);
  EXPECT_EQ(proxy->PipWindowId(), pip_window_id);

  // The proxy should be updated when the ID changes.
  const std::optional<NativeWindowId> new_pip_window_id = 456;
  CallOnPipShownAndWaitForObserver(task_environment_, coordinator_, observer,
                                   new_pip_window_id);
  EXPECT_EQ(proxy->PipWindowId(), new_pip_window_id);

  // The proxy should be updated when the pip window is closed.
  CallOnPipClosedAndWaitForObserver(task_environment_, coordinator_, observer);
  EXPECT_EQ(proxy->PipWindowId(), std::nullopt);

  proxy->RemoveObserver(&observer);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, AddCaptureNotifiesObservers) {
  MockObserver observer;
  coordinator_->AddObserver(&observer);

  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  const content::GlobalRenderFrameHostId render_frame_host_id(1, 1);
  const content::DesktopMediaID desktop_media_id(
      content::DesktopMediaID::TYPE_SCREEN, 123);
  const auto& expected_capture_info =
      PipScreenCaptureCoordinatorProxy::CaptureInfo{
          .session_id = session_id,
          .render_frame_host_id = render_frame_host_id,
          .desktop_media_id = desktop_media_id};

  EXPECT_CALL(observer, OnCapturesChanged(testing::ElementsAre(
                            testing::Eq(std::ref(expected_capture_info)))));
  coordinator_->AddCapture(expected_capture_info);

  coordinator_->RemoveObserver(&observer);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, RemoveCaptureNotifiesObservers) {
  MockObserver observer;
  coordinator_->AddObserver(&observer);

  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  const content::GlobalRenderFrameHostId render_frame_host_id(1, 1);
  const content::DesktopMediaID desktop_media_id(
      content::DesktopMediaID::TYPE_SCREEN, 123);
  const auto& expected_capture_info =
      PipScreenCaptureCoordinatorProxy::CaptureInfo{
          .session_id = session_id,
          .render_frame_host_id = render_frame_host_id,
          .desktop_media_id = desktop_media_id};

  EXPECT_CALL(observer, OnCapturesChanged(_)).Times(1);
  coordinator_->AddCapture(expected_capture_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnCapturesChanged(testing::IsEmpty()));
  coordinator_->RemoveCapture(session_id);

  coordinator_->RemoveObserver(&observer);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, AddAndRemoveCapture) {
  MockObserver observer;
  coordinator_->AddObserver(&observer);

  const base::UnguessableToken session_id1 = base::UnguessableToken::Create();
  const content::GlobalRenderFrameHostId render_frame_host_id1(1, 1);
  const content::DesktopMediaID desktop_media_id1(
      content::DesktopMediaID::TYPE_SCREEN, 123);
  const auto& expected_capture_info1 =
      PipScreenCaptureCoordinatorProxy::CaptureInfo{
          .session_id = session_id1,
          .render_frame_host_id = render_frame_host_id1,
          .desktop_media_id = desktop_media_id1};

  const base::UnguessableToken session_id2 = base::UnguessableToken::Create();
  const content::GlobalRenderFrameHostId render_frame_host_id2(2, 2);
  const content::DesktopMediaID desktop_media_id2(
      content::DesktopMediaID::TYPE_WINDOW, 456);
  const auto& expected_capture_info2 =
      PipScreenCaptureCoordinatorProxy::CaptureInfo{
          .session_id = session_id2,
          .render_frame_host_id = render_frame_host_id2,
          .desktop_media_id = desktop_media_id2};

  EXPECT_CALL(observer, OnCapturesChanged(testing::ElementsAre(
                            testing::Eq(std::ref(expected_capture_info1)))));
  coordinator_->AddCapture(expected_capture_info1);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnCapturesChanged(testing::UnorderedElementsAre(
                            testing::Eq(std::ref(expected_capture_info1)),
                            testing::Eq(std::ref(expected_capture_info2)))));
  coordinator_->AddCapture(expected_capture_info2);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnCapturesChanged(testing::ElementsAre(
                            testing::Eq(std::ref(expected_capture_info2)))));
  coordinator_->RemoveCapture(session_id1);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnCapturesChanged(testing::IsEmpty()));
  coordinator_->RemoveCapture(session_id2);

  coordinator_->RemoveObserver(&observer);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, AddAndRemoveCaptureNotifiesProxy) {
  auto proxy = coordinator_->CreateProxy();
  ASSERT_TRUE(proxy);

  MockProxyObserver observer;
  proxy->AddObserver(&observer);

  const base::UnguessableToken session_id1 = base::UnguessableToken::Create();
  const content::GlobalRenderFrameHostId render_frame_host_id1(1, 1);
  const content::DesktopMediaID desktop_media_id1(
      content::DesktopMediaID::TYPE_SCREEN, 123);
  const auto& expected_capture_info1 =
      PipScreenCaptureCoordinatorProxy::CaptureInfo{
          .session_id = session_id1,
          .render_frame_host_id = render_frame_host_id1,
          .desktop_media_id = desktop_media_id1};

  const base::UnguessableToken session_id2 = base::UnguessableToken::Create();
  const content::GlobalRenderFrameHostId render_frame_host_id2(2, 2);
  const content::DesktopMediaID desktop_media_id2(
      content::DesktopMediaID::TYPE_WINDOW, 456);
  const auto& expected_capture_info2 =
      PipScreenCaptureCoordinatorProxy::CaptureInfo{
          .session_id = session_id2,
          .render_frame_host_id = render_frame_host_id2,
          .desktop_media_id = desktop_media_id2};

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnCapturesChanged(testing::ElementsAre(
                              testing::Eq(std::ref(expected_capture_info1)))))
        .WillOnce([&run_loop](const auto&) { run_loop.Quit(); });
    PipScreenCaptureCoordinatorImpl::AddCapture(expected_capture_info1);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnCapturesChanged(testing::UnorderedElementsAre(
                              testing::Eq(std::ref(expected_capture_info1)),
                              testing::Eq(std::ref(expected_capture_info2)))))
        .WillOnce([&run_loop](const auto&) { run_loop.Quit(); });
    PipScreenCaptureCoordinatorImpl::AddCapture(expected_capture_info2);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnCapturesChanged(testing::ElementsAre(
                              testing::Eq(std::ref(expected_capture_info2)))))
        .WillOnce([&run_loop](const auto&) { run_loop.Quit(); });
    PipScreenCaptureCoordinatorImpl::RemoveCapture(session_id1);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnCapturesChanged(testing::IsEmpty()))
        .WillOnce([&run_loop](const auto&) { run_loop.Quit(); });
    PipScreenCaptureCoordinatorImpl::RemoveCapture(session_id2);
    run_loop.Run();
  }

  proxy->RemoveObserver(&observer);
}

}  // namespace content
