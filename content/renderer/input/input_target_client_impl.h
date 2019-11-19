// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_INPUT_TARGET_CLIENT_IMPL_H_
#define CONTENT_RENDERER_INPUT_INPUT_TARGET_CLIENT_IMPL_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/hit_test/input_target_client.mojom.h"

namespace content {

class RenderFrameImpl;

// This class provides an implementation of InputTargetClient mojo interface.
class InputTargetClientImpl : public viz::mojom::InputTargetClient {
 public:
  explicit InputTargetClientImpl(RenderFrameImpl* render_frame);
  ~InputTargetClientImpl() override;

  void BindToReceiver(
      mojo::PendingReceiver<viz::mojom::InputTargetClient> receiver);

  // viz::mojom::InputTargetClient:
  void FrameSinkIdAt(const gfx::PointF& point,
                     const uint64_t trace_id,
                     FrameSinkIdAtCallback callback) override;

 private:
  RenderFrameImpl* const render_frame_;

  mojo::Receiver<viz::mojom::InputTargetClient> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(InputTargetClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_TARGET_CLIENT_IMPL_H_
