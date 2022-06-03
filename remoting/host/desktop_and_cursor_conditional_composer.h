// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_AND_CURSOR_CONDITIONAL_COMPOSER_H_
#define REMOTING_HOST_DESKTOP_AND_CURSOR_CONDITIONAL_COMPOSER_H_

#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_and_cursor_composer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

namespace remoting {

// A wrapper for DesktopAndCursorComposer that allows compositing of the cursor
// to be enabled and disabled, and which exposes a WeakPtr to simplify memory
// management.
class DesktopAndCursorConditionalComposer : public webrtc::DesktopCapturer {
 public:
  explicit DesktopAndCursorConditionalComposer(
      std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer);
  ~DesktopAndCursorConditionalComposer() override;

  base::WeakPtr<DesktopAndCursorConditionalComposer> GetWeakPtr();

  void SetComposeEnabled(bool enabled);

  void SetMouseCursor(webrtc::MouseCursor* mouse_cursor);
  void SetMouseCursorPosition(const webrtc::DesktopVector& position);

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

 private:
  DesktopAndCursorConditionalComposer(
      const DesktopAndCursorConditionalComposer&) = delete;
  DesktopAndCursorConditionalComposer& operator=(
      const DesktopAndCursorConditionalComposer&) = delete;

  std::unique_ptr<webrtc::MouseCursor> mouse_cursor_;
  bool compose_enabled_ = false;
  std::unique_ptr<webrtc::DesktopAndCursorComposer> capturer_;
  base::WeakPtrFactory<DesktopAndCursorConditionalComposer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_AND_CURSOR_CONDITIONAL_COMPOSER_H_
