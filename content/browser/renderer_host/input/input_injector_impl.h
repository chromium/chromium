// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_INJECTOR_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_INJECTOR_IMPL_H_

#include <memory>

#include "content/common/input/input_injector.mojom.h"
#include "content/common/input/synthetic_gesture.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class RenderFrameHostImpl;

// An implementation of InputInjector.
class InputInjectorImpl : public mojom::InputInjector {
 public:
  explicit InputInjectorImpl(base::WeakPtr<RenderFrameHostImpl> frame_host);

  InputInjectorImpl(const InputInjectorImpl&) = delete;
  InputInjectorImpl& operator=(const InputInjectorImpl&) = delete;

  ~InputInjectorImpl() override;

  static void Create(base::WeakPtr<RenderFrameHostImpl> frame_host,
                     mojo::PendingReceiver<mojom::InputInjector> receiver);

  // mojom::InputInjector overrides.
  void QueueSyntheticSmoothDrag(
      const SyntheticSmoothDragGestureParams& drag,
      QueueSyntheticSmoothDragCallback callback) override;
  void QueueSyntheticSmoothScroll(
      const SyntheticSmoothScrollGestureParams& scroll,
      QueueSyntheticSmoothScrollCallback callback) override;
  void QueueSyntheticPinch(const SyntheticPinchGestureParams& pinch,
                           QueueSyntheticPinchCallback callback) override;
  void QueueSyntheticTap(const SyntheticTapGestureParams& tap,
                         QueueSyntheticTapCallback callback) override;
  void QueueSyntheticPointerAction(
      const SyntheticPointerActionListParams& pointer_action,
      QueueSyntheticPointerActionCallback callback) override;

 private:
  void QueueSyntheticGesture(
      std::unique_ptr<SyntheticGesture> synthetic_gesture,
      base::OnceCallback<void(SyntheticGesture::Result)> callback);

  base::WeakPtr<RenderFrameHostImpl> frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_INJECTOR_IMPL_H_
