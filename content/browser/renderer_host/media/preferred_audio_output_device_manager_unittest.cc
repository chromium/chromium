// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/preferred_audio_output_device_manager.h"

#include <string>

#include "base/test/bind.h"
#include "content/browser/media/audio_output_stream_broker.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "media/audio/audio_device_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::StrictMock;
using ::testing::Test;

namespace content {

namespace {

constexpr char kRawDeviceId[] = "Speaker (High Definition Audio)";

class MockAudioOutputDeviceSwitcher : public AudioOutputDeviceSwitcher {
 public:
  MockAudioOutputDeviceSwitcher(
      int render_process_id,
      int render_frame_id,
      const GlobalRenderFrameHostToken& main_frame_token,
      PreferredAudioOutputDeviceManagerImpl* preferred_device_manager)
      : render_process_id_(render_process_id),
        render_frame_id_(render_frame_id),
        main_frame_token_(main_frame_token),
        preferred_device_manager_(preferred_device_manager) {}
  ~MockAudioOutputDeviceSwitcher() override {
    preferred_device_manager_->RemoveSwitcher(main_frame_token_, this);
  }

  MOCK_METHOD(void,
              SwitchAudioOutputDeviceId,
              (const std::string&),
              (override));

  GlobalRenderFrameHostId GetGlobalRenderFrameHostId() const {
    return GlobalRenderFrameHostId(render_process_id_, render_frame_id_);
  }

 private:
  int render_process_id_;
  int render_frame_id_;
  const GlobalRenderFrameHostToken main_frame_token_;
  raw_ptr<PreferredAudioOutputDeviceManagerImpl> preferred_device_manager_;
  base::WeakPtrFactory<MockAudioOutputDeviceSwitcher> weak_ptr_factory_{this};
};

}  // namespace

class PreferredAudioOutputDeviceManagerImplTest
    : public RenderViewHostTestHarness {
 public:
  PreferredAudioOutputDeviceManagerImplTest() = default;
  ~PreferredAudioOutputDeviceManagerImplTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { RenderViewHostTestHarness::TearDown(); }

 protected:
  std::unique_ptr<MockAudioOutputDeviceSwitcher> CreateMockSwitcherForMainframe(
      PreferredAudioOutputDeviceManagerImpl& preferred_device_manager) {
    NavigateAndCommit(GURL("https://main-frame.com"));

    return std::make_unique<StrictMock<MockAudioOutputDeviceSwitcher>>(
        main_rfh()->GetGlobalId().child_id,
        main_rfh()->GetGlobalId().frame_routing_id,
        main_rfh()->GetGlobalFrameToken(), &preferred_device_manager);
  }

  std::unique_ptr<MockAudioOutputDeviceSwitcher> CreateMockSwitcherForSubframe(
      PreferredAudioOutputDeviceManagerImpl& preferred_device_manager,
      bool main_page_navigation = true) {
    if (main_page_navigation) {
      NavigateAndCommit(GURL("https://main-frame.com"));
    }

    RenderFrameHost* subframe =
        NavigationSimulator::NavigateAndCommitFromDocument(
            GURL("https://sub-frame.com"),
            RenderFrameHostTester::For(main_rfh())->AppendChild("subframe"));

    return std::make_unique<StrictMock<MockAudioOutputDeviceSwitcher>>(
        subframe->GetGlobalId().child_id,
        subframe->GetGlobalId().frame_routing_id,
        main_rfh()->GetGlobalFrameToken(), &preferred_device_manager);
  }

  void AddSwitcher(
      PreferredAudioOutputDeviceManagerImpl& preferred_device_manager,
      MockAudioOutputDeviceSwitcher* frame_switcher) {
    preferred_device_manager.AddSwitcher(main_rfh()->GetGlobalFrameToken(),
                                         frame_switcher);
  }

  void RemoveSwitcher(
      PreferredAudioOutputDeviceManagerImpl& preferred_device_manager,
      MockAudioOutputDeviceSwitcher* frame_switcher) {
    preferred_device_manager.RemoveSwitcher(main_rfh()->GetGlobalFrameToken(),
                                            frame_switcher);
  }

  void RunAndValidateSetPreferredSinkId(
      PreferredAudioOutputDeviceManagerImpl& preferred_device_manager,
      const std::string& raw_device_id,
      media::OutputDeviceStatus expected_status) {
    preferred_device_manager.SetPreferredSinkId(
        main_rfh()->GetGlobalFrameToken(), raw_device_id,
        base::BindLambdaForTesting(
            [expected_status](media::OutputDeviceStatus status) {
              EXPECT_EQ(status, expected_status);
            }));
  }
};

TEST_F(PreferredAudioOutputDeviceManagerImplTest, SwitchDeviceSwitcherExists) {
  // Calls switcher for active stream.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> main_frame_switcher =
      CreateMockSwitcherForMainframe(preferred_device_manager);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(1);

  AddSwitcher(preferred_device_manager, main_frame_switcher.get());
  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
}

TEST_F(PreferredAudioOutputDeviceManagerImplTest,
       SwitchDeviceOnSubframeSwitcherExists) {
  // Calls sub frame switcher for active stream.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> sub_frame_switcher =
      CreateMockSwitcherForSubframe(preferred_device_manager);

  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);

  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(1);

  AddSwitcher(preferred_device_manager, sub_frame_switcher.get());
  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
}

TEST_F(PreferredAudioOutputDeviceManagerImplTest,
       SwitchDeviceOnMultipleSubframeEachHasSwitcher) {
  // Calls all registered switchers.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> main_frame_switcher =
      CreateMockSwitcherForMainframe(preferred_device_manager);
  std::unique_ptr<MockAudioOutputDeviceSwitcher>
      sub_frame_audio_output_device_switcher_1 = CreateMockSwitcherForSubframe(
          preferred_device_manager, /*main_page_navigation=*/false);
  std::unique_ptr<MockAudioOutputDeviceSwitcher>
      sub_frame_audio_output_device_switcher_2 = CreateMockSwitcherForSubframe(
          preferred_device_manager, /*main_page_navigation=*/false);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*sub_frame_audio_output_device_switcher_1.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*sub_frame_audio_output_device_switcher_2.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(1);
  EXPECT_CALL(*sub_frame_audio_output_device_switcher_1.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(1);
  EXPECT_CALL(*sub_frame_audio_output_device_switcher_2.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(1);

  AddSwitcher(preferred_device_manager, main_frame_switcher.get());
  AddSwitcher(preferred_device_manager,
              sub_frame_audio_output_device_switcher_1.get());
  AddSwitcher(preferred_device_manager,
              sub_frame_audio_output_device_switcher_2.get());

  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
}

TEST_F(PreferredAudioOutputDeviceManagerImplTest,
       RemoveSwitcherDoesNotCallSwitchDevice) {
  // Removed swicher should not be called.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> sub_frame_switcher =
      CreateMockSwitcherForSubframe(preferred_device_manager);

  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(0);

  AddSwitcher(preferred_device_manager, sub_frame_switcher.get());

  RemoveSwitcher(preferred_device_manager, sub_frame_switcher.get());

  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
}

TEST_F(PreferredAudioOutputDeviceManagerImplTest,
       RemoveSwitcherMainFrameDoesNotCallSwitchDevice) {
  // Does not call removed switcher, but call not removed one.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> main_frame_switcher =
      CreateMockSwitcherForMainframe(preferred_device_manager);

  std::unique_ptr<MockAudioOutputDeviceSwitcher> sub_frame_switcher =
      CreateMockSwitcherForSubframe(preferred_device_manager,
                                    /*main_page_navigation=*/false);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId));

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(0);
  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId));

  AddSwitcher(preferred_device_manager, main_frame_switcher.get());
  AddSwitcher(preferred_device_manager, sub_frame_switcher.get());

  // Remove main frame only.
  RemoveSwitcher(preferred_device_manager, main_frame_switcher.get());

  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
}

TEST_F(PreferredAudioOutputDeviceManagerImplTest,
       RemoveMainRenderFrameHostDoesNotCallSwitchDevice) {
  // Removed RenderFrameHost will disable switcher call.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> main_frame_switcher =
      CreateMockSwitcherForMainframe(preferred_device_manager);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(0);

  AddSwitcher(preferred_device_manager, main_frame_switcher.get());

  preferred_device_manager.UnregisterMainFrameOnUIThread(main_rfh());
  base::RunLoop().RunUntilIdle();

  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
}

TEST_F(PreferredAudioOutputDeviceManagerImplTest,
       AddAndRemovalWithUnregisteredMainFrame) {
  // Re-add switcher after unregistered main frame.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> main_frame_switcher =
      CreateMockSwitcherForMainframe(preferred_device_manager);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(0);

  AddSwitcher(preferred_device_manager, main_frame_switcher.get());

  preferred_device_manager.UnregisterMainFrameOnUIThread(main_rfh());
  base::RunLoop().RunUntilIdle();

  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);

  std::unique_ptr<MockAudioOutputDeviceSwitcher> sub_frame_switcher =
      CreateMockSwitcherForSubframe(preferred_device_manager,
                                    /*main_page_navigation=*/false);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(1);
  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(1);
  AddSwitcher(preferred_device_manager, main_frame_switcher.get());
  AddSwitcher(preferred_device_manager, sub_frame_switcher.get());
}

TEST_F(PreferredAudioOutputDeviceManagerImplTest, CleanupWithDeletedSwitchers) {
  // Deleted switchers should not be called.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> main_frame_switcher =
      CreateMockSwitcherForMainframe(preferred_device_manager);

  std::unique_ptr<MockAudioOutputDeviceSwitcher> sub_frame_switcher =
      CreateMockSwitcherForSubframe(preferred_device_manager,
                                    /*main_page_navigation=*/false);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(0);
  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(0);

  AddSwitcher(preferred_device_manager, main_frame_switcher.get());
  AddSwitcher(preferred_device_manager, sub_frame_switcher.get());

  // Delete sub frame switcher.
  sub_frame_switcher.reset();

  // It should delete switch device on the main as well as sub frames.
  RemoveSwitcher(preferred_device_manager, main_frame_switcher.get());

  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
}

TEST_F(PreferredAudioOutputDeviceManagerImplTest, MultipleAdditionRemoval) {
  // Repeated additions and deletions.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> main_frame_switcher =
      CreateMockSwitcherForMainframe(preferred_device_manager);

  std::unique_ptr<MockAudioOutputDeviceSwitcher> sub_frame_switcher =
      CreateMockSwitcherForSubframe(preferred_device_manager,
                                    /*main_page_navigation=*/false);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(0);
  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(0);

  AddSwitcher(preferred_device_manager, main_frame_switcher.get());
  AddSwitcher(preferred_device_manager, sub_frame_switcher.get());

  // Delete sub frame switcher.
  sub_frame_switcher.reset();

  // It should delete switch device on the main as well as sub frames.
  RemoveSwitcher(preferred_device_manager, main_frame_switcher.get());

  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(0);
  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(1);
  AddSwitcher(preferred_device_manager, main_frame_switcher.get());

  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
}

TEST_F(PreferredAudioOutputDeviceManagerImplTest, DefaultIdAfterNonDefaultId) {
  // Repeated SetPreferredSinkId calls continue to work.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> main_frame_switcher =
      CreateMockSwitcherForMainframe(preferred_device_manager);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId));
  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId));

  AddSwitcher(preferred_device_manager, main_frame_switcher.get());

  // Call with `kRawDeviceId` sink id
  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);

  // Calls with `default` sink id again.
  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId));
  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, media::AudioDeviceDescription::kDefaultDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);

  // Call with `kRawDeviceId` sink id again.
  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId));
  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
}

TEST_F(PreferredAudioOutputDeviceManagerImplTest, AddAfterRemovalAfterSetId) {
  // It adds switcher after removal that does not lose the registered
  // preferred sink id.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> main_frame_switcher =
      CreateMockSwitcherForMainframe(preferred_device_manager);

  std::unique_ptr<MockAudioOutputDeviceSwitcher> sub_frame_switcher =
      CreateMockSwitcherForSubframe(preferred_device_manager,
                                    /*main_page_navigation=*/false);

  RunAndValidateSetPreferredSinkId(
      preferred_device_manager, kRawDeviceId,
      media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(1);
  AddSwitcher(preferred_device_manager, main_frame_switcher.get());

  // It should delete switch device on the main as well as sub frames.
  RemoveSwitcher(preferred_device_manager, main_frame_switcher.get());

  EXPECT_CALL(*sub_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(1);
  AddSwitcher(preferred_device_manager, sub_frame_switcher.get());
}

TEST_F(PreferredAudioOutputDeviceManagerImplTest,
       RemoveSwitcherAfterUnregisterMainFrameDoNotCrash) {
  // RemoveSwitcher call after unregistering main frame should not crash.
  PreferredAudioOutputDeviceManagerImpl preferred_device_manager;

  std::unique_ptr<MockAudioOutputDeviceSwitcher> main_frame_switcher =
      CreateMockSwitcherForMainframe(preferred_device_manager);

  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*main_frame_switcher.get(),
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(0);

  AddSwitcher(preferred_device_manager, main_frame_switcher.get());

  preferred_device_manager.UnregisterMainFrameOnUIThread(main_rfh());
  base::RunLoop().RunUntilIdle();

  // It does nothing(no crash) as main frame is unregistered.
  RemoveSwitcher(preferred_device_manager, main_frame_switcher.get());
}

}  // namespace content
