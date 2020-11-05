// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/mojom/video_capture.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/renderer/platform/video_capture/gpu_memory_buffer_test_support.h"
#include "third_party/blink/renderer/platform/video_capture/video_capture_impl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

using base::test::RunOnceClosure;
using media::BindToCurrentLoop;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::SaveArg;

namespace blink {

namespace {

// Callback interface to be implemented by VideoCaptureImplManagerTest.
// MockVideoCaptureImpl intercepts IPC messages and calls these methods to
// simulate what the VideoCaptureHost would do.
class PauseResumeCallback {
 public:
  PauseResumeCallback() {}
  virtual ~PauseResumeCallback() {}

  virtual void OnPaused(const media::VideoCaptureSessionId& session_id) = 0;
  virtual void OnResumed(const media::VideoCaptureSessionId& session_id) = 0;
};

class MockVideoCaptureImpl : public VideoCaptureImpl,
                             public media::mojom::blink::VideoCaptureHost {
 public:
  MockVideoCaptureImpl(const media::VideoCaptureSessionId& session_id,
                       PauseResumeCallback* pause_callback,
                       base::OnceClosure destruct_callback)
      : VideoCaptureImpl(session_id),
        pause_callback_(pause_callback),
        destruct_callback_(std::move(destruct_callback)) {}

  ~MockVideoCaptureImpl() override { std::move(destruct_callback_).Run(); }

 private:
  void Start(const base::UnguessableToken& device_id,
             const base::UnguessableToken& session_id,
             const media::VideoCaptureParams& params,
             mojo::PendingRemote<media::mojom::blink::VideoCaptureObserver>
                 observer) override {
    // For every Start(), expect a corresponding Stop() call.
    EXPECT_CALL(*this, Stop(_));
    // Simulate device started.
    OnStateChanged(media::mojom::VideoCaptureState::STARTED);
  }

  MOCK_METHOD1(Stop, void(const base::UnguessableToken&));

  void Pause(const base::UnguessableToken& device_id) override {
    pause_callback_->OnPaused(session_id());
  }

  void Resume(const base::UnguessableToken& device_id,
              const base::UnguessableToken& session_id,
              const media::VideoCaptureParams& params) override {
    pause_callback_->OnResumed(session_id);
  }

  MOCK_METHOD1(RequestRefreshFrame, void(const base::UnguessableToken&));
  MOCK_METHOD3(ReleaseBuffer,
               void(const base::UnguessableToken&, int32_t, double));

  void GetDeviceSupportedFormats(const base::UnguessableToken&,
                                 const base::UnguessableToken&,
                                 GetDeviceSupportedFormatsCallback) override {
    NOTREACHED();
  }

  void GetDeviceFormatsInUse(const base::UnguessableToken&,
                             const base::UnguessableToken&,
                             GetDeviceFormatsInUseCallback) override {
    NOTREACHED();
  }

  MOCK_METHOD2(OnFrameDropped,
               void(const base::UnguessableToken&,
                    media::VideoCaptureFrameDropReason));
  MOCK_METHOD2(OnLog, void(const base::UnguessableToken&, const String&));

  PauseResumeCallback* const pause_callback_;
  base::OnceClosure destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureImpl);
};

class MockVideoCaptureImplManager : public WebVideoCaptureImplManager {
 public:
  MockVideoCaptureImplManager(PauseResumeCallback* pause_callback,
                              base::RepeatingClosure stop_capture_callback)
      : pause_callback_(pause_callback),
        stop_capture_callback_(stop_capture_callback) {}
  ~MockVideoCaptureImplManager() override {}

 private:
  std::unique_ptr<VideoCaptureImpl> CreateVideoCaptureImplForTesting(
      const media::VideoCaptureSessionId& session_id) const override {
    auto video_capture_impl = std::make_unique<MockVideoCaptureImpl>(
        session_id, pause_callback_, stop_capture_callback_);
    video_capture_impl->SetVideoCaptureHostForTesting(video_capture_impl.get());
    return std::move(video_capture_impl);
  }

  PauseResumeCallback* const pause_callback_;
  const base::RepeatingClosure stop_capture_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureImplManager);
};

}  // namespace

class VideoCaptureImplManagerTest : public ::testing::Test,
                                    public PauseResumeCallback {
 public:
  VideoCaptureImplManagerTest()
      : manager_(new MockVideoCaptureImplManager(
            this,
            BindToCurrentLoop(cleanup_run_loop_.QuitClosure()))) {
    for (size_t i = 0; i < kNumClients; ++i) {
      session_ids_[i] = base::UnguessableToken::Create();
    }
  }

 protected:
  static constexpr size_t kNumClients = 3;
  std::array<base::UnguessableToken, kNumClients> session_ids_;

  std::array<base::OnceClosure, kNumClients> StartCaptureForAllClients(
      bool same_session_id) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure =
        BindToCurrentLoop(run_loop.QuitClosure());

    InSequence s;
    if (!same_session_id) {
      // |OnStarted| will only be received once from each device if there are
      // multiple request to the same device.
      EXPECT_CALL(*this, OnStarted(_))
          .Times(kNumClients - 1)
          .RetiresOnSaturation();
    }
    EXPECT_CALL(*this, OnStarted(_))
        .WillOnce(RunOnceClosure(std::move(quit_closure)))
        .RetiresOnSaturation();
    std::array<base::OnceClosure, kNumClients> stop_callbacks;
    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(176, 144), 30, media::PIXEL_FORMAT_I420);
    for (size_t i = 0; i < kNumClients; ++i) {
      stop_callbacks[i] = StartCapture(
          same_session_id ? session_ids_[0] : session_ids_[i], params);
    }
    run_loop.Run();
    return stop_callbacks;
  }

  void StopCaptureForAllClients(
      std::array<base::OnceClosure, kNumClients>* stop_callbacks) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure =
        BindToCurrentLoop(run_loop.QuitClosure());
    EXPECT_CALL(*this, OnStopped(_))
        .Times(kNumClients - 1)
        .RetiresOnSaturation();
    EXPECT_CALL(*this, OnStopped(_))
        .WillOnce(RunOnceClosure(std::move(quit_closure)))
        .RetiresOnSaturation();
    for (auto& stop_callback : *stop_callbacks)
      std::move(stop_callback).Run();
    run_loop.Run();
  }

  MOCK_METHOD2(OnFrameReady,
               void(scoped_refptr<media::VideoFrame>,
                    base::TimeTicks estimated_capture_time));
  MOCK_METHOD1(OnStarted, void(const media::VideoCaptureSessionId& id));
  MOCK_METHOD1(OnStopped, void(const media::VideoCaptureSessionId& id));
  MOCK_METHOD1(OnPaused, void(const media::VideoCaptureSessionId& id));
  MOCK_METHOD1(OnResumed, void(const media::VideoCaptureSessionId& id));

  void OnStateUpdate(const media::VideoCaptureSessionId& id,
                     VideoCaptureState state) {
    if (state == VIDEO_CAPTURE_STATE_STARTED)
      OnStarted(id);
    else if (state == VIDEO_CAPTURE_STATE_STOPPED)
      OnStopped(id);
    else
      NOTREACHED();
  }

  base::OnceClosure StartCapture(const media::VideoCaptureSessionId& id,
                                 const media::VideoCaptureParams& params) {
    return manager_->StartCapture(
        id, params,
        ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
            &VideoCaptureImplManagerTest::OnStateUpdate,
            CrossThreadUnretained(this), id)),
        ConvertToBaseRepeatingCallback(
            CrossThreadBindRepeating(&VideoCaptureImplManagerTest::OnFrameReady,
                                     CrossThreadUnretained(this))));
  }

  base::test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupportForGpuMemoryBuffer>
      platform_;
  base::RunLoop cleanup_run_loop_;
  std::unique_ptr<MockVideoCaptureImplManager> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImplManagerTest);
};

// Multiple clients with the same session id. There is only one
// media::VideoCapture object.
TEST_F(VideoCaptureImplManagerTest, MultipleClients) {
  std::array<base::OnceClosure, kNumClients> release_callbacks;
  for (size_t i = 0; i < kNumClients; ++i)
    release_callbacks[i] = manager_->UseDevice(session_ids_[0]);
  std::array<base::OnceClosure, kNumClients> stop_callbacks =
      StartCaptureForAllClients(true);
  StopCaptureForAllClients(&stop_callbacks);
  for (auto& release_callback : release_callbacks)
    std::move(release_callback).Run();
  cleanup_run_loop_.Run();
}

TEST_F(VideoCaptureImplManagerTest, NoLeak) {
  manager_->UseDevice(session_ids_[0]).Reset();
  manager_.reset();
  cleanup_run_loop_.Run();
}

TEST_F(VideoCaptureImplManagerTest, SuspendAndResumeSessions) {
  std::array<base::OnceClosure, kNumClients> release_callbacks;
  MediaStreamDevices video_devices;
  for (size_t i = 0; i < kNumClients; ++i) {
    release_callbacks[i] = manager_->UseDevice(session_ids_[i]);
    MediaStreamDevice video_device;
    video_device.set_session_id(session_ids_[i]);
    video_devices.push_back(video_device);
  }
  std::array<base::OnceClosure, kNumClients> stop_callbacks =
      StartCaptureForAllClients(false);

  // Call SuspendDevices(true) to suspend all clients, and expect all to be
  // paused.
  {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure =
        BindToCurrentLoop(run_loop.QuitClosure());
    EXPECT_CALL(*this, OnPaused(session_ids_[0]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*this, OnPaused(session_ids_[1]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*this, OnPaused(session_ids_[2]))
        .WillOnce(RunOnceClosure(std::move(quit_closure)))
        .RetiresOnSaturation();
    manager_->SuspendDevices(video_devices, true);
    run_loop.Run();
  }

  // Call SuspendDevices(false) and expect all to be resumed.
  {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure =
        BindToCurrentLoop(run_loop.QuitClosure());
    EXPECT_CALL(*this, OnResumed(session_ids_[0]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*this, OnResumed(session_ids_[1]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*this, OnResumed(session_ids_[2]))
        .WillOnce(RunOnceClosure(std::move(quit_closure)))
        .RetiresOnSaturation();
    manager_->SuspendDevices(video_devices, false);
    run_loop.Run();
  }

  // Suspend just the first client and expect just the first client to be
  // paused.
  {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure =
        BindToCurrentLoop(run_loop.QuitClosure());
    EXPECT_CALL(*this, OnPaused(session_ids_[0]))
        .WillOnce(RunOnceClosure(std::move(quit_closure)))
        .RetiresOnSaturation();
    manager_->Suspend(session_ids_[0]);
    run_loop.Run();
  }

  // Now call SuspendDevices(true) again, and expect just the second and third
  // clients to be paused.
  {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure =
        BindToCurrentLoop(run_loop.QuitClosure());
    EXPECT_CALL(*this, OnPaused(session_ids_[1]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*this, OnPaused(session_ids_[2]))
        .WillOnce(RunOnceClosure(std::move(quit_closure)))
        .RetiresOnSaturation();
    manager_->SuspendDevices(video_devices, true);
    run_loop.Run();
  }

  // Resume just the first client, but it should not resume because all devices
  // are supposed to be suspended.
  {
    manager_->Resume(session_ids_[0]);
    base::RunLoop().RunUntilIdle();
  }

  // Now, call SuspendDevices(false) and expect all to be resumed.
  {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure =
        BindToCurrentLoop(run_loop.QuitClosure());
    EXPECT_CALL(*this, OnResumed(session_ids_[0]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*this, OnResumed(session_ids_[1]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*this, OnResumed(session_ids_[2]))
        .WillOnce(RunOnceClosure(std::move(quit_closure)))
        .RetiresOnSaturation();
    manager_->SuspendDevices(video_devices, false);
    run_loop.Run();
  }

  StopCaptureForAllClients(&stop_callbacks);
  for (auto& release_callback : release_callbacks)
    std::move(release_callback).Run();
  cleanup_run_loop_.Run();
}

}  // namespace blink
