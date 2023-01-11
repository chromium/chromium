// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/display/renderer_proxy.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/base/queued_task_poster.h"
#include "remoting/client/display/gl_renderer.h"
#include "remoting/client/ui/view_matrix.h"

namespace remoting {

RendererProxy::RendererProxy(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      ui_task_poster_(new remoting::QueuedTaskPoster(task_runner_)) {}

RendererProxy::~RendererProxy() = default;

void RendererProxy::Initialize(base::WeakPtr<GlRenderer> renderer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_ = renderer;
}

void RendererProxy::SetTransformation(const ViewMatrix& transformation) {
  // Viewport and cursor movements need to be synchronized into the same frame.
  RunTaskOnProperThread(
      base::BindOnce(&GlRenderer::OnPixelTransformationChanged, renderer_,
                     transformation.ToMatrixArray()),
      true);
}

void RendererProxy::SetCursorPosition(float x, float y) {
  RunTaskOnProperThread(
      base::BindOnce(&GlRenderer::OnCursorMoved, renderer_, x, y), true);
}

void RendererProxy::SetCursorVisibility(bool visible) {
  // Cursor visibility and position should be synchronized.
  RunTaskOnProperThread(base::BindOnce(&GlRenderer::OnCursorVisibilityChanged,
                                       renderer_, visible),
                        true);
}

void RendererProxy::StartInputFeedback(float x, float y, float diameter) {
  RunTaskOnProperThread(base::BindOnce(&GlRenderer::OnCursorInputFeedback,
                                       renderer_, x, y, diameter),
                        false);
}

void RendererProxy::RunTaskOnProperThread(base::OnceClosure task,
                                          bool needs_synchronization) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (task_runner_->BelongsToCurrentThread()) {
    std::move(task).Run();
    return;
  }

  if (needs_synchronization) {
    ui_task_poster_->AddTask(std::move(task));
    return;
  }

  task_runner_->PostTask(FROM_HERE, std::move(task));
}

}  // namespace remoting
