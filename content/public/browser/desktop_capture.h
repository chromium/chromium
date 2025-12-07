// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_H_
#define CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_H_

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace content::desktop_capture {

// Creates a DesktopCaptureOptions with required settings.
CONTENT_EXPORT webrtc::DesktopCaptureOptions CreateDesktopCaptureOptions();

// Creates specific DesktopCapturer with required settings.
CONTENT_EXPORT std::unique_ptr<webrtc::DesktopCapturer> CreateScreenCapturer(
    webrtc::DesktopCaptureOptions options,
    bool for_snapshot);
CONTENT_EXPORT std::unique_ptr<webrtc::DesktopCapturer> CreateWindowCapturer(
    webrtc::DesktopCaptureOptions options);

// Returns whether we can use PipeWire capturer based on:
// 1) We run Linux Wayland session
// 2) WebRTC is built with PipeWire enabled
CONTENT_EXPORT bool CanUsePipeWire();

// Whether the capturer should find windows owned by the current process.
CONTENT_EXPORT bool ShouldEnumerateCurrentProcessWindows();

// Opens the native screen capture picker dialog.
// `type` is the type of the source being selected (screen, window, tab).
// `created_callback` is called after the picker is created to pass the created
// source_id back to the DelegatedSourceListCapturer.
// `picker_callback` is called when a source has been selected.
// `cancel_callback` is called when the picker is closed without selecting a
// source.
// `error_callback` is called if an error occurs when opening the picker.
// Exactly one of `picker_callback`, `cancel_callback` and `error_callback` will
// be called.
CONTENT_EXPORT void OpenNativeScreenCapturePicker(
    content::DesktopMediaID::Type type,
    base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
    base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
    base::OnceCallback<void()> cancel_callback,
    base::OnceCallback<void()> error_callback);

// Makes the native screen capture picker dialog stop observing `source_id` and
// closes the picker dialog if it is not observing anything else.
CONTENT_EXPORT void CloseNativeScreenCapturePicker(DesktopMediaID source_id);

}  // namespace content::desktop_capture

#endif  // CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_H_
