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

using testing::_;

namespace content {

namespace {

const content::GlobalRenderFrameHostId kPipOwnerId(1, 1);
const content::GlobalRenderFrameHostId kOtherId(2, 2);
constexpr content::DesktopMediaID::Id kPipWindowId = 123;
constexpr content::DesktopMediaID::Id kDesktopId = 234;
constexpr content::DesktopMediaID::Id kDiffDesktopId = 567;

class MockObserver : public PipScreenCaptureCoordinatorImpl::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(
      void,
      OnStateChanged,
      (std::optional<DesktopMediaID::Id>,
       const GlobalRenderFrameHostId&,
       const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo>&),
      (override));
};

class MockProxyObserver : public PipScreenCaptureCoordinatorProxy::Observer {
 public:
  MockProxyObserver() = default;
  ~MockProxyObserver() override = default;

  MOCK_METHOD(
      void,
      OnStateChanged,
      ((const std::optional<DesktopMediaID::Id>&),
       (const GlobalRenderFrameHostId&),
       (const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo>&)),
      (override));
};

void CallOnPipShownAndWaitUntilDone(
    content::BrowserTaskEnvironment& task_environment,
    PipScreenCaptureCoordinatorImpl* coordinator,
    DesktopMediaID::Id window_id,
    const GlobalRenderFrameHostId& owner_id) {
  base::RunLoop run_loop;
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PipScreenCaptureCoordinatorImpl* coordinator,
                        DesktopMediaID::Id window_id,
                        const GlobalRenderFrameHostId& owner_id,
                        base::OnceClosure quit_closure) {
                       coordinator->OnPipShown(window_id, owner_id);
                       std::move(quit_closure).Run();
                     },
                     base::Unretained(coordinator), window_id, owner_id,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

void CallOnPipClosedAndWaitForObserver(
    content::BrowserTaskEnvironment& task_environment,
    PipScreenCaptureCoordinatorImpl* coordinator,
    MockProxyObserver& observer) {
  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnStateChanged(std::optional<DesktopMediaID::Id>(), _, _))
      .WillOnce([&run_loop](const auto&, const auto&, const auto&) {
        run_loop.Quit();
      });
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
    const std::optional<DesktopMediaID::Id>& new_pip_window_id,
    const GlobalRenderFrameHostId& new_pip_owner_id) {
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnStateChanged(new_pip_window_id, new_pip_owner_id, _))
      .WillOnce([&run_loop](const auto&, const auto&, const auto&) {
        run_loop.Quit();
      });
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](PipScreenCaptureCoordinatorImpl* coordinator,
             DesktopMediaID::Id window_id,
             const GlobalRenderFrameHostId& owner_id) {
            coordinator->OnPipShown(window_id, owner_id);
          },
          base::Unretained(coordinator), *new_pip_window_id, new_pip_owner_id));
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

TEST_F(PipScreenCaptureCoordinatorImplTest, PipStateAccessors) {
  EXPECT_EQ(coordinator_->PipWindowId(), std::nullopt);
  EXPECT_EQ(coordinator_->GetPipOwnerRenderFrameHostId(),
            GlobalRenderFrameHostId());

  const DesktopMediaID::Id pip_window_id = 123;
  const GlobalRenderFrameHostId pip_owner_id(1, 1);
  coordinator_->OnPipShown(pip_window_id, pip_owner_id);
  EXPECT_EQ(coordinator_->PipWindowId(), pip_window_id);
  EXPECT_EQ(coordinator_->GetPipOwnerRenderFrameHostId(), pip_owner_id);

  coordinator_->OnPipClosed();
  EXPECT_EQ(coordinator_->PipWindowId(), std::nullopt);
  EXPECT_EQ(coordinator_->GetPipOwnerRenderFrameHostId(),
            GlobalRenderFrameHostId());
}

TEST_F(PipScreenCaptureCoordinatorImplTest, OnPipShownNotifiesObservers) {
  MockObserver observer;
  coordinator_->AddObserver(&observer);

  const DesktopMediaID::Id pip_window_id = 123;
  const GlobalRenderFrameHostId pip_owner_id(1, 1);
  EXPECT_CALL(observer, OnStateChanged(std::make_optional(pip_window_id),
                                       pip_owner_id, _));
  coordinator_->OnPipShown(pip_window_id, pip_owner_id);

  EXPECT_EQ(coordinator_->PipWindowId(), pip_window_id);

  // Calling again with the same ID should not notify.
  EXPECT_CALL(observer, OnStateChanged(_, _, _)).Times(0);
  coordinator_->OnPipShown(pip_window_id, pip_owner_id);

  coordinator_->RemoveObserver(&observer);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, OnPipClosedNotifiesObservers) {
  MockObserver observer;
  coordinator_->AddObserver(&observer);

  const DesktopMediaID::Id pip_window_id = 123;
  const GlobalRenderFrameHostId pip_owner_id(1, 1);
  EXPECT_CALL(observer, OnStateChanged(std::make_optional(pip_window_id),
                                       pip_owner_id, _));
  coordinator_->OnPipShown(pip_window_id, pip_owner_id);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnStateChanged(testing::Eq(std::nullopt),
                                       GlobalRenderFrameHostId(), _));
  coordinator_->OnPipClosed();

  coordinator_->RemoveObserver(&observer);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, AddAndRemoveObserver) {
  MockObserver observer1;
  MockObserver observer2;

  coordinator_->AddObserver(&observer1);
  coordinator_->AddObserver(&observer2);

  const DesktopMediaID::Id pip_window_id = 123;
  const GlobalRenderFrameHostId pip_owner_id(1, 1);
  EXPECT_CALL(observer1, OnStateChanged(std::make_optional(pip_window_id),
                                        pip_owner_id, _));
  EXPECT_CALL(observer2, OnStateChanged(std::make_optional(pip_window_id),
                                        pip_owner_id, _));
  coordinator_->OnPipShown(pip_window_id, pip_owner_id);
  testing::Mock::VerifyAndClearExpectations(&observer1);
  testing::Mock::VerifyAndClearExpectations(&observer2);

  coordinator_->RemoveObserver(&observer1);

  const DesktopMediaID::Id new_pip_window_id = 456;
  const GlobalRenderFrameHostId new_pip_owner_id(2, 2);
  EXPECT_CALL(observer1, OnStateChanged(_, _, _)).Times(0);
  EXPECT_CALL(observer2, OnStateChanged(std::make_optional(new_pip_window_id),
                                        new_pip_owner_id, _));
  coordinator_->OnPipShown(new_pip_window_id, new_pip_owner_id);

  coordinator_->RemoveObserver(&observer2);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, CreateProxy) {
  // The proxy should start with the current ID.
  const DesktopMediaID::Id pip_window_id = 123;
  const GlobalRenderFrameHostId pip_owner_id(1, 1);
  CallOnPipShownAndWaitUntilDone(task_environment_, coordinator_, pip_window_id,
                                 pip_owner_id);
  auto proxy = coordinator_->CreateProxy();
  ASSERT_TRUE(proxy);

  // The proxy's ID should be the initial pip_window_id.
  MockProxyObserver observer;
  proxy->AddObserver(&observer);
  EXPECT_EQ(proxy->PipWindowId(), pip_window_id);
  EXPECT_EQ(proxy->GetPipOwnerRenderFrameHostId(), pip_owner_id);

  // The proxy should be updated when the ID changes.
  const std::optional<DesktopMediaID::Id> new_pip_window_id = 456;
  const GlobalRenderFrameHostId new_pip_owner_id(2, 2);
  CallOnPipShownAndWaitForObserver(task_environment_, coordinator_, observer,
                                   new_pip_window_id, new_pip_owner_id);
  EXPECT_EQ(proxy->PipWindowId(), new_pip_window_id);
  EXPECT_EQ(proxy->GetPipOwnerRenderFrameHostId(), new_pip_owner_id);

  // The proxy should be updated when the pip window is closed.
  CallOnPipClosedAndWaitForObserver(task_environment_, coordinator_, observer);
  EXPECT_EQ(proxy->PipWindowId(), std::nullopt);
  EXPECT_EQ(proxy->GetPipOwnerRenderFrameHostId(), GlobalRenderFrameHostId());

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

  EXPECT_CALL(observer, OnStateChanged(_, _,
                                       testing::ElementsAre(testing::Eq(
                                           std::ref(expected_capture_info)))));
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

  EXPECT_CALL(observer, OnStateChanged(_, _, _)).Times(1);
  coordinator_->AddCapture(expected_capture_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnStateChanged(_, _, testing::IsEmpty()));
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

  EXPECT_CALL(observer, OnStateChanged(_, _,
                                       testing::ElementsAre(testing::Eq(
                                           std::ref(expected_capture_info1)))));
  coordinator_->AddCapture(expected_capture_info1);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(
      observer,
      OnStateChanged(_, _,
                     testing::UnorderedElementsAre(
                         testing::Eq(std::ref(expected_capture_info1)),
                         testing::Eq(std::ref(expected_capture_info2)))));
  coordinator_->AddCapture(expected_capture_info2);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnStateChanged(_, _,
                                       testing::ElementsAre(testing::Eq(
                                           std::ref(expected_capture_info2)))));
  coordinator_->RemoveCapture(session_id1);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnStateChanged(_, _, testing::IsEmpty()));
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
    EXPECT_CALL(observer,
                OnStateChanged(_, _,
                               testing::ElementsAre(testing::Eq(
                                   std::ref(expected_capture_info1)))))
        .WillOnce([&run_loop](const auto&, const auto&, const auto&) {
          run_loop.Quit();
        });
    PipScreenCaptureCoordinatorImpl::AddCapture(expected_capture_info1);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(
        observer,
        OnStateChanged(_, _,
                       testing::UnorderedElementsAre(
                           testing::Eq(std::ref(expected_capture_info1)),
                           testing::Eq(std::ref(expected_capture_info2)))))
        .WillOnce([&run_loop](const auto&, const auto&, const auto&) {
          run_loop.Quit();
        });
    PipScreenCaptureCoordinatorImpl::AddCapture(expected_capture_info2);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer,
                OnStateChanged(_, _,
                               testing::ElementsAre(testing::Eq(
                                   std::ref(expected_capture_info2)))))
        .WillOnce([&run_loop](const auto&, const auto&, const auto&) {
          run_loop.Quit();
        });
    PipScreenCaptureCoordinatorImpl::RemoveCapture(session_id1);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnStateChanged(_, _, testing::IsEmpty()))
        .WillOnce([&run_loop](const auto&, const auto&, const auto&) {
          run_loop.Quit();
        });
    PipScreenCaptureCoordinatorImpl::RemoveCapture(session_id2);
    run_loop.Run();
  }

  proxy->RemoveObserver(&observer);
}

TEST_F(PipScreenCaptureCoordinatorImplTest,
       GetPipWindowToExcludeFromScreenCapture_NoPip) {
  // If no PiP window is shown, nothing should be excluded.
  EXPECT_EQ(coordinator_->GetPipWindowToExcludeFromScreenCapture(kDesktopId),
            std::nullopt);
}

TEST_F(PipScreenCaptureCoordinatorImplTest,
       GetPipWindowToExcludeFromScreenCapture_PipShownNoCaptures) {
  coordinator_->OnPipShown(kPipWindowId, kPipOwnerId);

  // If PiP is shown but no one is capturing yet (e.g. the user is about to
  // select a screen), it can be excluded from captures from the RenderFrameHost
  // owning the PiP-window.
  EXPECT_EQ(coordinator_->GetPipWindowToExcludeFromScreenCapture(kDesktopId),
            std::make_optional(kPipWindowId));
}

TEST_F(PipScreenCaptureCoordinatorImplTest,
       GetPipWindowToExcludeFromScreenCapture_CaptureByOwner) {
  coordinator_->OnPipShown(kPipWindowId, kPipOwnerId);

  // Register a capture by the PiP owner.
  const auto capture_info = PipScreenCaptureCoordinatorProxy::CaptureInfo{
      .session_id = base::UnguessableToken::Create(),
      .render_frame_host_id = kPipOwnerId,
      .desktop_media_id = content::DesktopMediaID(
          content::DesktopMediaID::TYPE_SCREEN, kDesktopId)};
  coordinator_->AddCapture(capture_info);

  // The owner capturing the screen containing the PiP should trigger exclusion.
  EXPECT_EQ(coordinator_->GetPipWindowToExcludeFromScreenCapture(kDesktopId),
            std::make_optional(kPipWindowId));
}

TEST_F(PipScreenCaptureCoordinatorImplTest,
       GetPipWindowToExcludeFromScreenCapture_CaptureByOther) {
  coordinator_->OnPipShown(kPipWindowId, kPipOwnerId);

  // Register a capture by a different frame (Other).
  const auto capture_info = PipScreenCaptureCoordinatorProxy::CaptureInfo{
      .session_id = base::UnguessableToken::Create(),
      .render_frame_host_id = kOtherId,
      .desktop_media_id = content::DesktopMediaID(
          content::DesktopMediaID::TYPE_SCREEN, kDesktopId)};
  coordinator_->AddCapture(capture_info);

  // Since someone other than the owner is capturing, we cannot exclude the PiP.
  EXPECT_EQ(coordinator_->GetPipWindowToExcludeFromScreenCapture(kDesktopId),
            std::nullopt);
}

TEST_F(
    PipScreenCaptureCoordinatorImplTest,
    GetPipWindowToExcludeFromScreenCapture_CaptureByOtherOnDifferentDesktop) {
  coordinator_->OnPipShown(kPipWindowId, kPipOwnerId);

  // Register a capture by 'Other' on a DIFFERENT desktop.
  const auto capture_info = PipScreenCaptureCoordinatorProxy::CaptureInfo{
      .session_id = base::UnguessableToken::Create(),
      .render_frame_host_id = kOtherId,
      .desktop_media_id = content::DesktopMediaID(
          content::DesktopMediaID::TYPE_SCREEN, kDiffDesktopId)};
  coordinator_->AddCapture(capture_info);

  // The other capture is on a different screen, so it shouldn't interfere with
  // the exclusion decision for kDesktopId.
  EXPECT_EQ(coordinator_->GetPipWindowToExcludeFromScreenCapture(kDesktopId),
            std::make_optional(kPipWindowId));
}

TEST_F(PipScreenCaptureCoordinatorImplTest,
       GetPipWindowToExcludeFromScreenCapture_MultipleCaptures) {
  coordinator_->OnPipShown(kPipWindowId, kPipOwnerId);

  // 1. Owner starts capturing -> Exclude.
  const base::UnguessableToken session_owner = base::UnguessableToken::Create();
  coordinator_->AddCapture(
      {.session_id = session_owner,
       .render_frame_host_id = kPipOwnerId,
       .desktop_media_id = content::DesktopMediaID(
           content::DesktopMediaID::TYPE_SCREEN, kDesktopId)});
  EXPECT_EQ(coordinator_->GetPipWindowToExcludeFromScreenCapture(kDesktopId),
            std::make_optional(kPipWindowId));

  // 2. Other starts capturing same screen -> Stop Excluding (Conflict).
  const base::UnguessableToken session_other = base::UnguessableToken::Create();
  coordinator_->AddCapture(
      {.session_id = session_other,
       .render_frame_host_id = kOtherId,
       .desktop_media_id = content::DesktopMediaID(
           content::DesktopMediaID::TYPE_SCREEN, kDesktopId)});
  EXPECT_EQ(coordinator_->GetPipWindowToExcludeFromScreenCapture(kDesktopId),
            std::nullopt);

  // 3. Other stops capturing -> Resume Excluding.
  coordinator_->RemoveCapture(session_other);
  EXPECT_EQ(coordinator_->GetPipWindowToExcludeFromScreenCapture(kDesktopId),
            std::make_optional(kPipWindowId));
}

}  // namespace content
