// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_NATIVE_SCREEN_CAPTURE_PICKER_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_NATIVE_SCREEN_CAPTURE_PICKER_MAC_H_

#include <memory>

#include "build/build_config.h"
#include "content/common/content_export.h"

static_assert(BUILDFLAG(IS_MAC));

#if defined(__OBJC__)
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "content/browser/media/capture/native_screen_capture_picker.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

@class PickerObserver;
#endif  // defined(__OBJC__)

namespace media {
class VideoCaptureDevice;
}  // namespace media

namespace content {

class NativeScreenCapturePicker;

CONTENT_EXPORT std::unique_ptr<NativeScreenCapturePicker>
CreateNativeScreenCapturePickerMac();

#if defined(__OBJC__)

class API_AVAILABLE(macos(14.0)) CONTENT_EXPORT NativeScreenCapturePickerMac
    : public NativeScreenCapturePicker {
 public:
  using PickerUpdateCallback =
      base::RepeatingCallback<void(SCContentFilter*, SCStream*)>;
  using PickerCancelCallback = base::RepeatingCallback<void(SCStream*)>;
  using GetWindowOwnerPidCallback =
      base::RepeatingCallback<pid_t(DesktopMediaID::Id)>;

  NativeScreenCapturePickerMac();
  ~NativeScreenCapturePickerMac() override;

  void Open(
      DesktopMediaID::Type type,
      base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
      base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
      base::OnceClosure cancel_callback,
      base::OnceClosure error_callback,
      base::OnceCallback<void(DesktopMediaID::Id)> stop_audio_callback)
      override;
  void Close(DesktopMediaID device_id) override;
  void GetApplicationAudioCaptureId(
      DesktopMediaID::Id session_id,
      GetApplicationAudioCaptureIdCallback callback) override;
  std::unique_ptr<media::VideoCaptureDevice> CreateDevice(
      const DesktopMediaID& source) override;

  base::WeakPtr<NativeScreenCapturePicker> GetWeakPtr() override;

  static void SetGetWindowOwnerPidForTesting(
      GetWindowOwnerPidCallback callback);

 private:
  struct CaptureSession {
    CaptureSession();
    ~CaptureSession();

    // The filter defining what is being captured.
    SCContentFilter* __strong filter;
    // Tracks if the initial OS response (update/cancel/error) has occurred.
    bool received_first_response = false;
    // Timer to cleanup the session state after the device is closed.
    base::OneShotTimer cleanup_timer;

    std::optional<desktop_capture::ApplicationAudioCaptureId>
        primary_audio_capture_id;
    base::OnceCallback<void(DesktopMediaID::Id)> stop_audio_callback;
  };

  // Callbacks called by PickerObserver when it receives an event from the OS.
  void OnPickerObserverUpdated(SCContentFilter* filter, SCStream* stream);
  void OnPickerObserverCancelled(SCStream* stream);
  void OnPickerObserverEncounteredError(NSError* error);

  void UpdateAudioStatusForSession(CaptureSession& session,
                                   DesktopMediaID::Id session_id,
                                   SCContentFilter* filter);

  void ScheduleCleanup(DesktopMediaID::Id id);
  void CleanupContentFilter(DesktopMediaID::Id id);

  // Callback called by `ScreenCaptureKitDeviceMac` on creating a new stream.
  void UpdateStreamMap(DesktopMediaID::Id id, SCStream* stream);

  // Get the capture session or create it if it doesn't exist.
  CaptureSession& GetOrCreateCaptureSession(DesktopMediaID::Id id);

  // There is only one picker observer which has callbacks initialized only
  // once.
  PickerObserver* __strong picker_observer_;
  base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback_;
  base::OnceClosure cancel_callback_;
  base::OnceClosure error_callback_;

  // On every new getDisplayMedia request, we increment
  // `active_picker_source_id_` and assign `active_picker_type_` the type of the
  // request. Since while making a selection of the capture surface the rest of
  // the UI becomes non-interactive, it is ensured that the callbacks are called
  // for the right selection.
  DesktopMediaID::Id active_picker_source_id_ = 0;
  DesktopMediaID::Type active_picker_type_;

  // Map of all sessions, indexed by their unique DesktopMediaID::Id.
  absl::flat_hash_map<DesktopMediaID::Id, std::unique_ptr<CaptureSession>>
      sessions_;
  // `active_source_ids_` keeps a track of the number of active capture
  // sessions.
  absl::flat_hash_set<DesktopMediaID::Id> active_source_ids_;
  // Reverse lookup to find session ID when the OS provides an SCStream*.
  absl::flat_hash_map<SCStream*, DesktopMediaID::Id> stream_to_id_map_;

  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
  base::WeakPtrFactory<NativeScreenCapturePickerMac> weak_ptr_factory_{this};
};

#endif  // defined(__OBJC__)

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_NATIVE_SCREEN_CAPTURE_PICKER_MAC_H_
