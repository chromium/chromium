// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_3a_controller.h"

#include <functional>
#include <utility>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/request_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media {

namespace {

class MockCaptureMetadataDispatcher : public CaptureMetadataDispatcher {
 public:
  MockCaptureMetadataDispatcher() {}
  ~MockCaptureMetadataDispatcher() override {}
  MOCK_METHOD1(
      AddResultMetadataObserver,
      void(CaptureMetadataDispatcher::ResultMetadataObserver* observer));
  MOCK_METHOD1(
      RemoveResultMetadataObserver,
      void(CaptureMetadataDispatcher::ResultMetadataObserver* observer));
  MOCK_METHOD4(SetCaptureMetadata,
               void(cros::mojom::CameraMetadataTag tag,
                    cros::mojom::EntryType type,
                    size_t count,
                    std::vector<uint8_t> value));
  MOCK_METHOD4(SetRepeatingCaptureMetadata,
               void(cros::mojom::CameraMetadataTag tag,
                    cros::mojom::EntryType type,
                    size_t count,
                    std::vector<uint8_t> value));
  MOCK_METHOD1(UnsetRepeatingCaptureMetadata,
               void(cros::mojom::CameraMetadataTag tag));
};

}  // namespace

class Camera3AControllerTest : public ::testing::Test {
 public:
  Camera3AControllerTest() : thread_("Camera3AControllerThread") {}

  void SetUp() override {
    thread_.Start();
    mock_capture_metadata_dispatcher_ =
        std::make_unique<MockCaptureMetadataDispatcher>();
  }

  void TearDown() override {
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&Camera3AControllerTest::Clear3AControllerOnThread,
                       base::Unretained(this)));
    thread_.Stop();
    mock_capture_metadata_dispatcher_.reset();
  }

  void RunOnThreadSync(const base::Location& location,
                       base::OnceClosure closure) {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_.task_runner()->PostTask(
        location, base::BindOnce(&Camera3AControllerTest::RunOnThread,
                                 base::Unretained(this), std::cref(location),
                                 std::move(closure), base::Unretained(&done)));
    done.Wait();
  }

  void Reset3AController(
      const cros::mojom::CameraMetadataPtr& static_metadata) {
    RunOnThreadSync(
        FROM_HERE,
        base::BindOnce(&Camera3AControllerTest::Reset3AControllerOnThread,
                       base::Unretained(this), std::cref(static_metadata)));
  }

  template <typename Value>
  void Set3AMode(cros::mojom::CameraMetadataPtr* metadata,
                 cros::mojom::CameraMetadataTag control,
                 Value value,
                 bool append = false) {
    auto* e = GetMetadataEntry(*metadata, control);
    if (e) {
      if (append) {
        (*e)->count++;
        (*e)->data.push_back(base::checked_cast<uint8_t>(value));
      } else {
        (*e)->count = 1;
        (*e)->data = {base::checked_cast<uint8_t>(value)};
      }
    } else {
      cros::mojom::CameraMetadataEntryPtr entry =
          cros::mojom::CameraMetadataEntry::New();
      entry->index = (*metadata)->entries.value().size();
      entry->tag = control;
      entry->type = cros::mojom::EntryType::TYPE_BYTE;
      entry->count = 1;
      entry->data = {base::checked_cast<uint8_t>(value)};

      (*metadata)->entries.value().push_back(std::move(entry));
      (*metadata)->entry_count++;
      (*metadata)->entry_capacity++;
    }
    SortCameraMetadata(metadata);
  }

  cros::mojom::CameraMetadataPtr CreateDefaultFakeStaticMetadata() {
    auto metadata = cros::mojom::CameraMetadata::New();
    metadata->entries = std::vector<cros::mojom::CameraMetadataEntryPtr>();
    metadata->entry_count = 0;
    metadata->entry_capacity = 0;

    // Set the available AF modes.
    Set3AMode(
        &metadata,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES,
        cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF);
    Set3AMode(
        &metadata,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES,
        cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_AUTO,
        /* append */ true);
    Set3AMode(
        &metadata,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES,
        cros::mojom::AndroidControlAfMode::
            ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
        /* append */ true);
    Set3AMode(
        &metadata,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES,
        cros::mojom::AndroidControlAfMode::
            ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO,
        /* append */ true);

    // Set the available AE modes.
    Set3AMode(
        &metadata,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_AVAILABLE_MODES,
        cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_OFF);
    Set3AMode(
        &metadata,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_AVAILABLE_MODES,
        cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_ON,
        /* append */ true);

    // Set the available AWB modes.
    Set3AMode(
        &metadata,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_AVAILABLE_MODES,
        cros::mojom::AndroidControlAwbMode::ANDROID_CONTROL_AWB_MODE_OFF);
    Set3AMode(
        &metadata,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_AVAILABLE_MODES,
        cros::mojom::AndroidControlAwbMode::ANDROID_CONTROL_AWB_MODE_AUTO,
        /* append */ true);

    return metadata;
  }

  void On3AStabilizedCallback(base::WaitableEvent* done) { done->Signal(); }

 protected:
  base::Thread thread_;
  std::unique_ptr<MockCaptureMetadataDispatcher>
      mock_capture_metadata_dispatcher_;
  std::unique_ptr<Camera3AController> camera_3a_controller_;

 private:
  void RunOnThread(const base::Location& location,
                   base::OnceClosure closure,
                   base::WaitableEvent* done) {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());

    std::move(closure).Run();
    done->Signal();
  }

  void Clear3AControllerOnThread() {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());

    if (camera_3a_controller_) {
      EXPECT_CALL(*mock_capture_metadata_dispatcher_,
                  RemoveResultMetadataObserver(camera_3a_controller_.get()))
          .Times(1);
    }
    camera_3a_controller_.reset();
  }

  void Reset3AControllerOnThread(
      const cros::mojom::CameraMetadataPtr& static_metadata) {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());

    Clear3AControllerOnThread();
    EXPECT_CALL(*mock_capture_metadata_dispatcher_,
                AddResultMetadataObserver(_))
        .Times(1);
    EXPECT_CALL(*mock_capture_metadata_dispatcher_,
                SetRepeatingCaptureMetadata(
                    cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
                    cros::mojom::EntryType::TYPE_BYTE, 1, _))
        .Times(1);
    EXPECT_CALL(*mock_capture_metadata_dispatcher_,
                SetRepeatingCaptureMetadata(
                    cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_MODE,
                    cros::mojom::EntryType::TYPE_BYTE, 1, _))
        .Times(1);
    EXPECT_CALL(*mock_capture_metadata_dispatcher_,
                SetRepeatingCaptureMetadata(
                    cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_MODE,
                    cros::mojom::EntryType::TYPE_BYTE, 1, _))
        .Times(1);
    camera_3a_controller_ = std::make_unique<Camera3AController>(
        static_metadata, mock_capture_metadata_dispatcher_.get(),
        thread_.task_runner());
  }
};

TEST_F(Camera3AControllerTest, Stabilize3AForStillCaptureTest) {
  Reset3AController(CreateDefaultFakeStaticMetadata());

  // Set AF mode.
  std::vector<uint8_t> af_trigger_start, af_trigger_cancel, af_mode, ae_trigger;
  af_trigger_start = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAfTrigger::ANDROID_CONTROL_AF_TRIGGER_START)};
  af_trigger_cancel = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAfTrigger::ANDROID_CONTROL_AF_TRIGGER_CANCEL)};
  af_mode = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAfMode::
          ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE)};
  ae_trigger = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAePrecaptureTrigger::
          ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START)};

  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_trigger_cancel))
      .Times(1);
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetRepeatingCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_mode))
      .Times(1);
  RunOnThreadSync(
      FROM_HERE,
      base::BindOnce(&Camera3AController::SetAutoFocusModeForStillCapture,
                     base::Unretained(camera_3a_controller_.get())));

  // |camera_3a_controller_| should wait until the AF mode is set
  // before setting the AF and AE precapture triggers.
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_trigger_start))
      .Times(0);
  EXPECT_CALL(
      *mock_capture_metadata_dispatcher_,
      SetCaptureMetadata(
          cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
          cros::mojom::EntryType::TYPE_BYTE, 1, ae_trigger))
      .Times(0);
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  RunOnThreadSync(
      FROM_HERE,
      base::BindOnce(
          &Camera3AController::Stabilize3AForStillCapture,
          base::Unretained(camera_3a_controller_.get()),
          base::BindOnce(&Camera3AControllerTest::On3AStabilizedCallback,
                         base::Unretained(this), &done)));
  testing::Mock::VerifyAndClearExpectations(camera_3a_controller_.get());

  // |camera_3a_controller_| should set the AF and AE precapture triggers once
  // the 3A modes are set.
  auto result_metadata = CreateDefaultFakeStaticMetadata();
  Set3AMode(&result_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
            cros::mojom::AndroidControlAfMode::
                ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE);
  Set3AMode(
      &result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_STATE,
      cros::mojom::AndroidControlAfState::ANDROID_CONTROL_AF_STATE_INACTIVE);
  Set3AMode(&result_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_MODE,
            cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_ON);
  Set3AMode(
      &result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_STATE,
      cros::mojom::AndroidControlAeState::ANDROID_CONTROL_AE_STATE_INACTIVE);
  Set3AMode(&result_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_MODE,
            cros::mojom::AndroidControlAwbMode::ANDROID_CONTROL_AWB_MODE_AUTO);
  Set3AMode(
      &result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_STATE,
      cros::mojom::AndroidControlAwbState::ANDROID_CONTROL_AWB_STATE_INACTIVE);
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_trigger_start))
      .Times(1);
  EXPECT_CALL(
      *mock_capture_metadata_dispatcher_,
      SetCaptureMetadata(
          cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
          cros::mojom::EntryType::TYPE_BYTE, 1, ae_trigger))
      .Times(1);
  RunOnThreadSync(FROM_HERE,
                  base::BindOnce(&Camera3AController::OnResultMetadataAvailable,
                                 base::Unretained(camera_3a_controller_.get()),
                                 0, std::cref(result_metadata)));

  // |camera_3a_controller_| should call the registered callback once 3A are
  // stabilized.
  Set3AMode(&result_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_STATE,
            cros::mojom::AndroidControlAfState::
                ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED);
  Set3AMode(
      &result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_STATE,
      cros::mojom::AndroidControlAeState::ANDROID_CONTROL_AE_STATE_CONVERGED);
  Set3AMode(
      &result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_STATE,
      cros::mojom::AndroidControlAwbState::ANDROID_CONTROL_AWB_STATE_CONVERGED);
  RunOnThreadSync(FROM_HERE,
                  base::BindOnce(&Camera3AController::OnResultMetadataAvailable,
                                 base::Unretained(camera_3a_controller_.get()),
                                 0, std::cref(result_metadata)));
  done.Wait();
}

// Test that SetAutoFocusModeForStillCapture sets the right auto-focus mode on
// cameras with different capabilities.
TEST_F(Camera3AControllerTest, SetAutoFocusModeForStillCaptureTest) {
  auto static_metadata = CreateDefaultFakeStaticMetadata();
  std::vector<uint8_t> af_mode;
  std::vector<uint8_t> af_trigger = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAfTrigger::ANDROID_CONTROL_AF_TRIGGER_CANCEL)};

  // For camera that supports continuous auto-focus for picture mode.
  Reset3AController(static_metadata);
  af_mode = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAfMode::
          ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE)};
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_trigger))
      .Times(1);
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetRepeatingCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_mode))
      .Times(1);
  RunOnThreadSync(
      FROM_HERE,
      base::BindOnce(&Camera3AController::SetAutoFocusModeForStillCapture,
                     base::Unretained(camera_3a_controller_.get())));
  testing::Mock::VerifyAndClearExpectations(camera_3a_controller_.get());

  // For camera that only supports basic auto focus.
  Set3AMode(&static_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES,
            cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF);
  Set3AMode(&static_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES,
            cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_AUTO,
            /* append */ true);
  Reset3AController(static_metadata);
  af_mode.clear();
  af_mode = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_AUTO)};
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_trigger))
      .Times(1);
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetRepeatingCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_mode))
      .Times(1);
  RunOnThreadSync(
      FROM_HERE,
      base::BindOnce(&Camera3AController::SetAutoFocusModeForStillCapture,
                     base::Unretained(camera_3a_controller_.get())));
  testing::Mock::VerifyAndClearExpectations(camera_3a_controller_.get());

  // For camera that is fixed-focus.
  Set3AMode(&static_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES,
            cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF);
  Reset3AController(static_metadata);
  af_mode.clear();
  af_mode = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF)};
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_trigger))
      .Times(1);
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetRepeatingCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_mode))
      .Times(1);
  RunOnThreadSync(
      FROM_HERE,
      base::BindOnce(&Camera3AController::SetAutoFocusModeForStillCapture,
                     base::Unretained(camera_3a_controller_.get())));
  testing::Mock::VerifyAndClearExpectations(camera_3a_controller_.get());
}

// Test that SetAutoFocusModeForVideoRecording sets the right auto-focus mode on
// cameras with different capabilities.
TEST_F(Camera3AControllerTest, SetAutoFocusModeForVideoRecordingTest) {
  auto static_metadata = CreateDefaultFakeStaticMetadata();
  std::vector<uint8_t> af_mode;
  std::vector<uint8_t> af_trigger = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAfTrigger::ANDROID_CONTROL_AF_TRIGGER_CANCEL)};

  // For camera that supports continuous auto-focus for picture mode.
  Reset3AController(static_metadata);
  af_mode = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAfMode::
          ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO)};
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_trigger))
      .Times(1);
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetRepeatingCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_mode))
      .Times(1);
  RunOnThreadSync(
      FROM_HERE,
      base::BindOnce(&Camera3AController::SetAutoFocusModeForVideoRecording,
                     base::Unretained(camera_3a_controller_.get())));
  testing::Mock::VerifyAndClearExpectations(camera_3a_controller_.get());

  // For camera that only supports basic auto focus.
  Set3AMode(&static_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES,
            cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF);
  Set3AMode(&static_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES,
            cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_AUTO,
            /* append */ true);
  Reset3AController(static_metadata);
  af_mode.clear();
  af_mode = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_AUTO)};
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_trigger))
      .Times(1);
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetRepeatingCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_mode))
      .Times(1);
  RunOnThreadSync(
      FROM_HERE,
      base::BindOnce(&Camera3AController::SetAutoFocusModeForVideoRecording,
                     base::Unretained(camera_3a_controller_.get())));
  testing::Mock::VerifyAndClearExpectations(camera_3a_controller_.get());

  // For camera that is fixed-focus.
  Set3AMode(&static_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES,
            cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF);
  Reset3AController(static_metadata);
  af_mode.clear();
  af_mode = {base::checked_cast<uint8_t>(
      cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF)};
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_trigger))
      .Times(1);
  EXPECT_CALL(*mock_capture_metadata_dispatcher_,
              SetRepeatingCaptureMetadata(
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
                  cros::mojom::EntryType::TYPE_BYTE, 1, af_mode))
      .Times(1);
  RunOnThreadSync(
      FROM_HERE,
      base::BindOnce(&Camera3AController::SetAutoFocusModeForVideoRecording,
                     base::Unretained(camera_3a_controller_.get())));
  testing::Mock::VerifyAndClearExpectations(camera_3a_controller_.get());
}

// TODO(shik): Add tests for SetPointOfInterest().
// TODO(shik): Add fake timestamps for result metadata.

}  // namespace media
