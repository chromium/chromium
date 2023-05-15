// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_AURA_DESKTOP_CAPTURER_H_
#define REMOTING_HOST_CHROMEOS_AURA_DESKTOP_CAPTURER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/chromeos/ash_proxy.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {

// A webrtc::DesktopCapturer that captures pixels from the primary display.
// The resulting screen capture will use the display's native resolution.
// This is implemented through the abstractions provided by |AshProxy|,
// allowing us to mock display interactions during unittests.
// Start() and CaptureFrame() must be called on the Browser UI thread.
class AuraDesktopCapturer : public webrtc::DesktopCapturer {
 public:
  AuraDesktopCapturer();
  explicit AuraDesktopCapturer(AshProxy& ash_proxy);

  AuraDesktopCapturer(const AuraDesktopCapturer&) = delete;
  AuraDesktopCapturer& operator=(const AuraDesktopCapturer&) = delete;

  ~AuraDesktopCapturer() override;

  // webrtc::DesktopCapturer implementation.
  void Start(webrtc::DesktopCapturer::Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  // Called when a copy of the layer is captured.
  void OnFrameCaptured(std::unique_ptr<webrtc::DesktopFrame> frame);

  const display::Display* GetSourceDisplay() const;

  const raw_ref<AshProxy, ExperimentalAsh> ash_;

  // Points to the callback passed to webrtc::DesktopCapturer::Start().
  raw_ptr<webrtc::DesktopCapturer::Callback, ExperimentalAsh> callback_ =
      nullptr;

  // The id of the display we're currently capturing.
  DisplayId source_display_id_;

  base::WeakPtrFactory<AuraDesktopCapturer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_AURA_DESKTOP_CAPTURER_H_
