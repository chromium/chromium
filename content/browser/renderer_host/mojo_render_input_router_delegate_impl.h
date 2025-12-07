// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MOJO_RENDER_INPUT_ROUTER_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MOJO_RENDER_INPUT_ROUTER_DELEGATE_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/input/render_input_router.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

namespace content {

class RenderWidgetHostImpl;

// This class is owned by RenderWidgetHostImpl and its lifetime is directly tied
// to the RenderWidgetHostImpl instance that creates it. This class encapsulates
// the mojo-specific communication logic for syncing input handling related
// state, acting as an intermediary between the RenderWidgetHostImpl (on the
// CrBrowserMain thread) and the InputManager (on the VizCompositorThread). See
// mojo interfaces `RenderInputRouterDelegate` and
// `RenderInputRouterDelegateClient` for relevant state syncing methods.
class MojoRenderInputRouterDelegateImpl
    : public input::mojom::RenderInputRouterDelegateClient {
 public:
  explicit MojoRenderInputRouterDelegateImpl(RenderWidgetHostImpl* host);

  ~MojoRenderInputRouterDelegateImpl() override;

  // Sets up RenderInputRouterDelegate mojo connections with InputManager on
  // the VizCompositorThread for input handling with InputVizard.
  void SetupRenderInputRouterDelegateConnection();

  // Get remote for making calls to RenderInputRouterDelegate interface. Returns
  // nullptr if the remote is not bound yet. The mojo connection is setup when
  // layer tree frame sinks from renderer are being requested (see
  // RenderWidgetHostImpl::CreateFrameSink). This connection is setup before
  // input handling starts on VizCompositorThread for its corresponding
  // `frame_sink_id`.
  input::mojom::RenderInputRouterDelegate* GetRenderInputRouterDelegateRemote();

  // input::mojom::RenderInputRouterDelegateClient overrides.
  void NotifyObserversOfInputEvent(
      std::unique_ptr<blink::WebCoalescedInputEvent> event,
      bool dispatched_to_renderer) override;
  void NotifyObserversOfInputEventAcks(
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result,
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override;
  void OnInvalidInputEventSource() override;
  void StateOnOverscrollTransfer(
      blink::mojom::DidOverscrollParamsPtr params) override;
  void RendererInputResponsivenessChanged(
      bool is_responsive,
      std::optional<base::TimeTicks> ack_timeout_ts) override;

  void SetRenderInputRouterDelegateRemoteForTesting(
      mojo::PendingAssociatedRemote<input::mojom::RenderInputRouterDelegate>
          remote) {
    rir_delegate_remote_.Bind(std::move(remote));
  }

 private:
  mojo::AssociatedReceiver<input::mojom::RenderInputRouterDelegateClient>
      rir_delegate_client_receiver_{this};
  mojo::AssociatedRemote<input::mojom::RenderInputRouterDelegate>
      rir_delegate_remote_;

  // It is safe to use `raw_ref` here since RenderWidgetHostImpl owns this class
  // and is bound to outlive |this|.
  raw_ref<RenderWidgetHostImpl> host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MOJO_RENDER_INPUT_ROUTER_DELEGATE_IMPL_H_
