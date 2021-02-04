// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_IMPL_H_

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"

namespace blink {
class WebRemoteFrameImpl;

class RemoteFrameClientImpl final : public RemoteFrameClient {
 public:
  explicit RemoteFrameClientImpl(WebRemoteFrameImpl*);

  void Trace(Visitor*) const override;

  // FrameClient overrides:
  bool InShadowTree() const override;
  void Detached(FrameDetachType) override;
  base::UnguessableToken GetDevToolsFrameToken() const override;

  // RemoteFrameClient overrides:
  void Navigate(const ResourceRequest&,
                blink::WebLocalFrame* initiator_frame,
                bool should_replace_current_entry,
                bool is_opener_navigation,
                bool prevent_sandboxed_download,
                bool initiator_frame_is_ad,
                mojo::PendingRemote<mojom::blink::BlobURLToken>,
                const base::Optional<WebImpression>& impression) override;
  unsigned BackForwardLength() override;
  void WillSynchronizeVisualProperties(
      bool capture_sequence_number_changed,
      const viz::SurfaceId& surface_id,
      const gfx::Size& compositor_viewport_size) override;
  bool RemoteProcessGone() const override;
  void DidSetFrameSinkId() override;
  AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;

  WebRemoteFrameImpl* GetWebFrame() const { return web_frame_; }

 private:
  Member<WebRemoteFrameImpl> web_frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_IMPL_H_
