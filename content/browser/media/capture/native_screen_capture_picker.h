// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_NATIVE_SCREEN_CAPTURE_PICKER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_NATIVE_SCREEN_CAPTURE_PICKER_H_

#include "base/functional/callback.h"
#include "content/public/browser/desktop_media_id.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace media {
class VideoCaptureDevice;
}  // namespace media

namespace content {

class NativeScreenCapturePicker {
 public:
  virtual ~NativeScreenCapturePicker() = default;

  // Opens the picker dialog.
  // `type` is the type of the content capture (window/screen).
  // `created_callback` is called when the picker is opened/created.
  // `picker_callback` is called when the user picks a source.
  // `cancel_callback` is called when the user closes the picker without
  // picking.
  // `error_callback` is called when the picker fails to open.
  // Exactly one of these three callbacks is called depending on the picker
  // selection and success.
  virtual void Open(
      DesktopMediaID::Type type,
      base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
      base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
      base::OnceCallback<void()> cancel_callback,
      base::OnceCallback<void()> error_callback) = 0;

  // Closes the picker.
  virtual void Close(DesktopMediaID device_id) = 0;

  // Creates a video capture device for a surface selected during a previous
  // call to Open.
  virtual std::unique_ptr<media::VideoCaptureDevice> CreateDevice(
      const DesktopMediaID& source) = 0;

  virtual base::WeakPtr<NativeScreenCapturePicker> GetWeakPtr() = 0;
};

std::unique_ptr<NativeScreenCapturePicker>
MaybeCreateNativeScreenCapturePicker();

}  // namespace content
#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_NATIVE_SCREEN_CAPTURE_PICKER_H_
