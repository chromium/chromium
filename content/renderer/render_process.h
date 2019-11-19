// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_PROCESS_H_
#define CONTENT_RENDERER_RENDER_PROCESS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "content/child/child_process.h"

namespace content {

// A abstract interface representing the renderer end of the browser<->renderer
// connection. The opposite end is the RenderProcessHost. This is a singleton
// object for each renderer.
//
// RenderProcessImpl implements this interface for the regular browser.
// MockRenderProcess implements this interface for certain tests, especially
// ones derived from RenderViewTest.
class RenderProcess : public ChildProcess {
 public:
  RenderProcess() = default;
  RenderProcess(const std::string& thread_pool_name,
                std::unique_ptr<base::ThreadPoolInstance::InitParams>
                    thread_pool_init_params);
  ~RenderProcess() override {}

  // Keep track of the cumulative set of enabled bindings for this process,
  // across any view.
  virtual void AddBindings(int bindings) = 0;

  // The cumulative set of enabled bindings for this process.
  virtual int GetEnabledBindings() const = 0;

  // Returns a pointer to the RenderProcess singleton instance. Assuming that
  // we're actually a renderer or a renderer test, this static cast will
  // be correct.
  static RenderProcess* current() {
    return static_cast<RenderProcess*>(ChildProcess::current());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderProcess);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_PROCESS_H_
