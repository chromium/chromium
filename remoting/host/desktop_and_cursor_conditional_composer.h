// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_AND_CURSOR_CONDITIONAL_COMPOSER_H_
#define REMOTING_HOST_DESKTOP_AND_CURSOR_CONDITIONAL_COMPOSER_H_

#include <memory>

#include "base/functional/callback.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_and_cursor_composer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_metadata.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

namespace remoting {

// A wrapper for DesktopAndCursorComposer that allows compositing of the cursor
// to be enabled and disabled.
class DesktopAndCursorConditionalComposer : public DesktopCapturer {
 public:
  explicit DesktopAndCursorConditionalComposer(
      std::unique_ptr<DesktopCapturer> desktop_capturer);
  ~DesktopAndCursorConditionalComposer() override;

  // DesktopCapturer interface.
  void Start(webrtc::DesktopCapturer::Callback* callback) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void CaptureFrame() override;
  void SetExcludedWindow(webrtc::WindowId window) override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool FocusOnSelectedSource() override;
  bool IsOccluded(const webrtc::DesktopVector& pos) override;
  void SetComposeEnabled(bool enabled) override;
  void SetMouseCursor(
      std::unique_ptr<webrtc::MouseCursor> mouse_cursor) override;
  void SetMouseCursorPosition(const webrtc::DesktopVector& position) override;
  bool SupportsFrameCallbacks() override;
  void SetMaxFrameRate(uint32_t max_frame_rate) override;
#if defined(WEBRTC_USE_GIO)
  void GetMetadataAsync(base::OnceCallback<void(webrtc::DesktopCaptureMetadata)>
                            callback) override;
#endif

 private:
  DesktopAndCursorConditionalComposer(
      const DesktopAndCursorConditionalComposer&) = delete;
  DesktopAndCursorConditionalComposer& operator=(
      const DesktopAndCursorConditionalComposer&) = delete;

  std::unique_ptr<webrtc::MouseCursor> mouse_cursor_;
  bool compose_enabled_ = false;
#if defined(WEBRTC_USE_GIO)
  // Following pointer is not owned by |this| class.
  raw_ptr<DesktopCapturer> desktop_capturer_ = nullptr;
#endif
  std::unique_ptr<webrtc::DesktopAndCursorComposer> capturer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_AND_CURSOR_CONDITIONAL_COMPOSER_H_
