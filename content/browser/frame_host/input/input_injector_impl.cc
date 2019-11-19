// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/input/input_injector_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/input/input_injector.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

void SyntheticGestureCallback(base::OnceClosure callback,
                              SyntheticGesture::Result result) {
  std::move(callback).Run();
}

}  // namespace

InputInjectorImpl::InputInjectorImpl(
    base::WeakPtr<RenderFrameHostImpl> frame_host)
    : frame_host_(std::move(frame_host)) {}

InputInjectorImpl::~InputInjectorImpl() {}

void InputInjectorImpl::Create(
    base::WeakPtr<RenderFrameHostImpl> frame_host,
    mojo::PendingReceiver<mojom::InputInjector> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<InputInjectorImpl>(frame_host),
                              std::move(receiver));
}

void InputInjectorImpl::QueueSyntheticSmoothDrag(
    const SyntheticSmoothDragGestureParams& drag,
    QueueSyntheticSmoothDragCallback callback) {
  QueueSyntheticGesture(
      SyntheticGesture::Create(drag),
      base::BindOnce(SyntheticGestureCallback, std::move(callback)));
}

void InputInjectorImpl::QueueSyntheticSmoothScroll(
    const SyntheticSmoothScrollGestureParams& scroll,
    QueueSyntheticSmoothScrollCallback callback) {
  QueueSyntheticGesture(
      SyntheticGesture::Create(scroll),
      base::BindOnce(SyntheticGestureCallback, std::move(callback)));
}

void InputInjectorImpl::QueueSyntheticPinch(
    const SyntheticPinchGestureParams& pinch,
    QueueSyntheticPinchCallback callback) {
  QueueSyntheticGesture(
      SyntheticGesture::Create(pinch),
      base::BindOnce(SyntheticGestureCallback, std::move(callback)));
}

void InputInjectorImpl::QueueSyntheticTap(const SyntheticTapGestureParams& tap,
                                          QueueSyntheticTapCallback callback) {
  QueueSyntheticGesture(
      SyntheticGesture::Create(tap),
      base::BindOnce(SyntheticGestureCallback, std::move(callback)));
}

void InputInjectorImpl::QueueSyntheticPointerAction(
    const SyntheticPointerActionListParams& pointer_action,
    QueueSyntheticPointerActionCallback callback) {
  QueueSyntheticGesture(
      SyntheticGesture::Create(pointer_action),
      base::BindOnce(SyntheticGestureCallback, std::move(callback)));
}

void InputInjectorImpl::QueueSyntheticGesture(
    std::unique_ptr<SyntheticGesture> synthetic_gesture,
    base::OnceCallback<void(SyntheticGesture::Result)> callback) {
  if (!frame_host_)
    return;

  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(frame_host_->GetView());
  if (!view)
    return;

  // Note that we do not transform coordinates in the case of a synthetic
  // gesture that is initiated from an OOPIF. The coordinates are already
  // expressed in terms of the root's visual viewport.
  RenderWidgetHostViewBase* root_view = view->GetRootView();
  if (!root_view)
    return;

  root_view->host()->QueueSyntheticGesture(std::move(synthetic_gesture),
                                           std::move(callback));
}

}  // namespace content
