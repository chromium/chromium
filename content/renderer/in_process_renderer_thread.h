// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_IN_PROCESS_RENDERER_THREAD_H_
#define CONTENT_RENDERER_IN_PROCESS_RENDERER_THREAD_H_

#include <memory>

#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/common/in_process_child_thread_params.h"

namespace blink::scheduler {
class WebThreadScheduler;
}  // namespace blink::scheduler
namespace content {
class RenderProcess;

// This class creates the IO thread for the renderer when running in
// single-process mode.  It's not used in multi-process mode.
class InProcessRendererThread : public base::Thread {
 public:
  class ThreadDelegate : public base::Thread::Delegate {
   public:
    explicit ThreadDelegate(InProcessRendererThread* outer);
    ~ThreadDelegate() override = default;
    scoped_refptr<base::SingleThreadTaskRunner> GetDefaultTaskRunner() override;
    void BindToCurrentThread() override;

   private:
    raw_ptr<blink::scheduler::WebThreadScheduler> web_thread_scheduler_;
    raw_ptr<InProcessRendererThread> outer_;
  };

  InProcessRendererThread(const InProcessChildThreadParams& params,
                          int32_t renderer_client_id);

  InProcessRendererThread(const InProcessRendererThread&) = delete;
  InProcessRendererThread& operator=(const InProcessRendererThread&) = delete;

  ~InProcessRendererThread() override;

 protected:
  void CleanUp() override;

 private:
  const InProcessChildThreadParams params_;
  const int32_t renderer_client_id_;
  std::unique_ptr<RenderProcess> render_process_;
};

CONTENT_EXPORT base::Thread* CreateInProcessRendererThread(
    const InProcessChildThreadParams& params,
    int32_t renderer_client_id);

}  // namespace content

#endif  // CONTENT_RENDERER_IN_PROCESS_RENDERER_THREAD_H_
