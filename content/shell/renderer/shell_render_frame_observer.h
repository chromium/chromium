// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_SHELL_RENDER_FRAME_OBSERVER_H_
#define CONTENT_SHELL_RENDERER_SHELL_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame_observer.h"

namespace content {

class ShellRenderFrameObserver : public RenderFrameObserver {
 public:
  explicit ShellRenderFrameObserver(RenderFrame* frame);
  ~ShellRenderFrameObserver() override;

  ShellRenderFrameObserver(const ShellRenderFrameObserver&) = delete;
  ShellRenderFrameObserver& operator=(const ShellRenderFrameObserver&) = delete;

 private:
  // RenderFrameObserver implementation.
  void DidClearWindowObject() override;
  void OnDestruct() override;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_SHELL_RENDER_FRAME_OBSERVER_H_
