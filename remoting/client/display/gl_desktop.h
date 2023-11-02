// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_GL_DESKTOP_H_
#define REMOTING_CLIENT_DISPLAY_GL_DESKTOP_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/display/drawable.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

class Canvas;

// This class draws the desktop on the canvas.
class GlDesktop : public Drawable {
 public:
  GlDesktop();

  GlDesktop(const GlDesktop&) = delete;
  GlDesktop& operator=(const GlDesktop&) = delete;

  ~GlDesktop() override;

  // |frame| can be either a full frame or updated regions only frame.
  void SetVideoFrame(const webrtc::DesktopFrame& frame);

  // Drawable implementation.
  void SetCanvas(base::WeakPtr<Canvas> canvas) override;
  bool Draw() override;
  int GetZIndex() override;
  base::WeakPtr<Drawable> GetWeakPtr() override;

 private:
  struct GlDesktopTextureContainer;

  void ReallocateTextures(const webrtc::DesktopSize& size);

  std::vector<std::unique_ptr<GlDesktopTextureContainer>> textures_;

  webrtc::DesktopSize last_desktop_size_;
  int max_texture_size_ = 0;
  base::WeakPtr<Canvas> canvas_ = nullptr;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<Drawable> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_DISPLAY_GL_DESKTOP_H_
