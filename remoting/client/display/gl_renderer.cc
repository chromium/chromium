// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/display/gl_renderer.h"

#include <algorithm>

#include "base/bind.h"
#include "base/check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/client/display/drawable.h"
#include "remoting/client/display/gl_canvas.h"
#include "remoting/client/display/gl_math.h"
#include "remoting/client/display/gl_renderer_delegate.h"
#include "remoting/client/display/sys_opengl.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

namespace {

bool CompareDrawableZOrder(base::WeakPtr<Drawable> a,
                           base::WeakPtr<Drawable> b) {
  return a->GetZIndex() < b->GetZIndex();
}

}  // namespace

GlRenderer::GlRenderer() {
  weak_ptr_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

GlRenderer::~GlRenderer() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void GlRenderer::SetDelegate(base::WeakPtr<GlRendererDelegate> delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

void GlRenderer::RequestCanvasSize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delegate_) {
    delegate_->OnSizeChanged(canvas_width_, canvas_height_);
  }
}

void GlRenderer::OnPixelTransformationChanged(
    const std::array<float, 9>& matrix) {
  DCHECK(thread_checker_.CalledOnValidThread());
  transformation_matrix_ = matrix;
  if (!canvas_) {
    return;
  }
  canvas_->SetTransformationMatrix(matrix);
  RequestRender();
}

void GlRenderer::OnCursorMoved(float x, float y) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cursor_.SetCursorPosition(x, y);
  RequestRender();
}

void GlRenderer::OnCursorInputFeedback(float x, float y, float diameter) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cursor_feedback_.StartAnimation(x, y, diameter);
  RequestRender();
}

void GlRenderer::OnCursorVisibilityChanged(bool visible) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cursor_.SetCursorVisible(visible);
  RequestRender();
}

void GlRenderer::OnFrameReceived(std::unique_ptr<webrtc::DesktopFrame> frame,
                                 base::OnceClosure done) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(frame->size().width() > 0 && frame->size().height() > 0);
  if (canvas_width_ != frame->size().width() ||
      canvas_height_ != frame->size().height()) {
    if (delegate_) {
      delegate_->OnSizeChanged(frame->size().width(), frame->size().height());
    }
    canvas_width_ = frame->size().width();
    canvas_height_ = frame->size().height();
  }

  desktop_.SetVideoFrame(*frame);
  pending_done_callbacks_.push(std::move(done));
  RequestRender();
}

void GlRenderer::OnCursorShapeChanged(const protocol::CursorShapeInfo& shape) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cursor_.SetCursorShape(shape);
  RequestRender();
}

void GlRenderer::OnSurfaceCreated(std::unique_ptr<Canvas> canvas) {
  DCHECK(thread_checker_.CalledOnValidThread());
  canvas_ = std::move(canvas);
  if (view_width_ > 0 && view_height_ > 0) {
    canvas_->SetViewSize(view_width_, view_height_);
  }
  if (transformation_matrix_) {
    canvas_->SetTransformationMatrix(*transformation_matrix_);
  }
  for (auto& drawable : drawables_) {
    drawable->SetCanvas(canvas_->GetWeakPtr());
  }
}

void GlRenderer::OnSurfaceChanged(int view_width, int view_height) {
  DCHECK(thread_checker_.CalledOnValidThread());
  view_width_ = view_width;
  view_height_ = view_height;

  if (!canvas_) {
    return;
  }

  canvas_->SetViewSize(view_width, view_height);
  RequestRender();
}

void GlRenderer::OnSurfaceDestroyed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  cursor_feedback_.SetCanvas(nullptr);
  cursor_.SetCanvas(nullptr);
  desktop_.SetCanvas(nullptr);
  canvas_.reset();
}

base::WeakPtr<GlRenderer> GlRenderer::GetWeakPtr() {
  return weak_ptr_;
}

void GlRenderer::RequestRender() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (render_scheduled_) {
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&GlRenderer::OnRender, weak_ptr_));
  render_scheduled_ = true;
}

void GlRenderer::AddDrawable(base::WeakPtr<Drawable> drawable) {
  drawable->SetCanvas(canvas_ ? canvas_->GetWeakPtr() : nullptr);
  drawables_.push_back(drawable);
  std::sort(drawables_.begin(), drawables_.end(), CompareDrawableZOrder);
}

void GlRenderer::OnRender() {
  DCHECK(thread_checker_.CalledOnValidThread());
  render_scheduled_ = false;
  if (!delegate_ || !delegate_->CanRenderFrame()) {
    return;
  }

  if (canvas_) {
    canvas_->Clear();
    // Draw each drawable in order.
    for (auto& drawable : drawables_) {
      if (drawable->Draw()) {
        RequestRender();
      }
    }
  }

  delegate_->OnFrameRendered();

  while (!pending_done_callbacks_.empty()) {
    std::move(pending_done_callbacks_.front()).Run();
    pending_done_callbacks_.pop();
  }
}

std::unique_ptr<GlRenderer> GlRenderer::CreateGlRendererWithDesktop() {
  std::unique_ptr<GlRenderer> renderer(new GlRenderer());
  renderer->AddDrawable(renderer->desktop_.GetWeakPtr());
  renderer->AddDrawable(renderer->cursor_.GetWeakPtr());
  renderer->AddDrawable(renderer->cursor_feedback_.GetWeakPtr());
  return renderer;
}

}  // namespace remoting
