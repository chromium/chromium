// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_SCREEN_CAPTURE_KIT_FULLSCREEN_MODULE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_SCREEN_CAPTURE_KIT_FULLSCREEN_MODULE_H_

#include <CoreGraphics/CoreGraphics.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "base/memory/raw_ref.h"
#import "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"

namespace content {

class API_AVAILABLE(macos(12.3))
    CONTENT_EXPORT ScreenCaptureKitResetStreamInterface {
 public:
  // This function is called if the ScreenCaptureKitFullScreenModule detects a
  // fullscreen presentation that corresponds to the originally captured window.
  // The parameter is the fullscreen window. It is also called with the original
  // window as parameter if the fullscreen window is no longer on screen.
  virtual void ResetStreamTo(SCWindow* window) = 0;
};

class API_AVAILABLE(macos(12.3))
    CONTENT_EXPORT ScreenCaptureKitFullscreenModule {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Mode {
    kUnsupported = 0,
    kPowerPoint = 1,
    kOpenOffice = 2,
    kKeynote = 3,
    kLibreOffice = 4,
    kMaxValue = kLibreOffice,
  };

  using ContentHandler = base::OnceCallback<void(SCShareableContent*)>;
  using GetShareableContentCallback =
      base::RepeatingCallback<void(ContentHandler)>;

  ScreenCaptureKitFullscreenModule(
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
      ScreenCaptureKitResetStreamInterface& reset_stream_interface,
      CGWindowID original_window_id,
      pid_t original_window_pid,
      Mode mode);
  ~ScreenCaptureKitFullscreenModule();

  void Start();
  void Reset();

  bool is_fullscreen_window_active() const { return fullscreen_mode_active_; }
  Mode get_mode() const { return mode_; }

  // Sets a callback to be used whenever the ScreenCaptureKit OS API
  // GetShareableContent is called. Used in tests to enable mocking of this API.
  void set_get_sharable_content_for_test(
      GetShareableContentCallback get_shareable_content) {
    get_shareable_content_for_test_ = get_shareable_content;
  }

 private:
  void CheckForFullscreenPresentation();
  void OnFullscreenShareableContentCreated(SCShareableContent* content);
  void OnExitFullscreenShareableContentCreated(SCShareableContent* content);
  SCWindow* GetFullscreenWindow(SCShareableContent* content,
                                SCWindow* editor_window,
                                int number_of_impress_editor_windows) const;

  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  // Interface to the owner of the SCK stream.
  const raw_ref<ScreenCaptureKitResetStreamInterface> reset_stream_interface_;

  // Identifier of the original window that is captured.
  const CGWindowID original_window_id_;
  const pid_t original_window_pid_;

  // The mode, corresponding to what slideshow application we're tracking.
  const Mode mode_;

  // True, if we've detected a fullscreen presentation and requested the stream
  // to be reset to the new window.
  bool fullscreen_mode_active_ = false;

  // Identified of the fullscreen window.
  CGWindowID fullscreen_window_id_ = 0;

  // Callback to mock function that is used in tests to mock the SCK OS API
  // GetShareableContent.
  GetShareableContentCallback get_shareable_content_for_test_;

  base::RepeatingTimer timer_;

  base::WeakPtrFactory<ScreenCaptureKitFullscreenModule> weak_factory_{this};
};

// Creates and returns a ScreenCaptureKitFullscreenModule if `original_window`
// corresponds to a supported slideshow application. `reset_stream_interface` is
// a reference to the object that owns the SCK stream. Upon detection of a
// fullscreen slideshow, the reset function will be called.
// `reset_stream_interface` must outlive the returned fullscreen module.
// `device_task_runner` specifies the task runner where all operations are run.
std::unique_ptr<ScreenCaptureKitFullscreenModule> CONTENT_EXPORT
MaybeCreateScreenCaptureKitFullscreenModule(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
    ScreenCaptureKitResetStreamInterface& reset_stream_interface,
    SCWindow* original_window) API_AVAILABLE(macos(12.3));

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_SCREEN_CAPTURE_KIT_FULLSCREEN_MODULE_H_
