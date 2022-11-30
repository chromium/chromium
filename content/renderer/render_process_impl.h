// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_PROCESS_IMPL_H_
#define CONTENT_RENDERER_RENDER_PROCESS_IMPL_H_

#include <memory>
#include <vector>

#include "content/renderer/render_process.h"

namespace content {

// Implementation of the RenderProcess interface for the regular browser.
// See also MockRenderProcess which implements the active "RenderProcess" when
// running under certain unit tests.
class RenderProcessImpl : public RenderProcess {
 public:
  RenderProcessImpl(const RenderProcessImpl&) = delete;
  RenderProcessImpl& operator=(const RenderProcessImpl&) = delete;

  ~RenderProcessImpl() override;

  // Creates and returns a RenderProcessImpl instance.
  //
  // RenderProcessImpl is created via a static method instead of a simple
  // constructor because non-trivial calls must be made to get the arguments
  // required by constructor of the base class.
  static std::unique_ptr<RenderProcess> Create();

  // Do not use these functions.
  // The browser process is the only one responsible for knowing when to
  // shutdown its renderer processes. Reference counting to keep this process
  // alive is not used. To keep this process alive longer, see
  // mojo::KeepAliveHandle and content::RenderProcessHostImpl.
  void AddRefProcess() override;
  void ReleaseProcess() override;

 private:
  RenderProcessImpl();
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_PROCESS_IMPL_H_
