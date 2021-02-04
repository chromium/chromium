// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_H_

#include "base/optional.h"
#include "cc/paint/paint_canvas.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_impression.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"

namespace viz {
class SurfaceId;
}

namespace blink {
class AssociatedInterfaceProvider;
class ResourceRequest;
class WebLocalFrame;

class RemoteFrameClient : public FrameClient {
 public:
  ~RemoteFrameClient() override = default;

  virtual void Navigate(const ResourceRequest&,
                        blink::WebLocalFrame* initiator_frame,
                        bool should_replace_current_entry,
                        bool is_opener_navigation,
                        bool initiator_frame_has_download_sandbox_flag,
                        bool initiator_frame_is_ad,
                        mojo::PendingRemote<mojom::blink::BlobURLToken>,
                        const base::Optional<WebImpression>& impression) = 0;
  unsigned BackForwardLength() override = 0;

  virtual void WillSynchronizeVisualProperties(
      bool capture_sequence_number_changed,
      const viz::SurfaceId& surface_id,
      const gfx::Size& compositor_viewport_size) = 0;

  virtual bool RemoteProcessGone() const = 0;

  // This is a temporary workaround for https://crbug.com/1166729.
  // TODO(https://crbug.com/1166722): Remove this once the migration is done.
  virtual void DidSetFrameSinkId() = 0;

  virtual AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_H_
