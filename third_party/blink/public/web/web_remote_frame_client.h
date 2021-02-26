// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_

#include "base/optional.h"
#include "cc/paint/paint_canvas.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-forward.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_impression.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_touch_action.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_remote_frame.h"

namespace blink {

class WebURLRequest;

class WebRemoteFrameClient {
 public:
  // Specifies the reason for the detachment.
  enum class DetachType { kRemove, kSwap };

  // Notify the embedder that it should remove this frame from the frame tree
  // and release any resources associated with it.
  virtual void FrameDetached(DetachType) {}

  // A remote frame was asked to start a navigation.
  virtual void Navigate(
      const WebURLRequest& request,
      bool should_replace_current_entry,
      bool is_opener_navigation,
      bool initiator_frame_has_download_sandbox_flag,
      bool blocking_downloads_in_sandbox_enabled,
      bool initiator_frame_is_ad,
      CrossVariantMojoRemote<mojom::BlobURLTokenInterfaceBase> blob_url_token,
      const base::Optional<WebImpression>& impression,
      const LocalFrameToken* initiator_frame_token,
      CrossVariantMojoRemote<
          blink::mojom::PolicyContainerHostKeepAliveHandleInterfaceBase>
          initiator_policy_container_keep_alive_handle) {}

  // Returns an AssociatedInterfaceProvider the frame can use to request
  // associated interfaces from the browser.
  virtual AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() = 0;

  // Returns token to be used as a frame id in the devtools protocol.
  // It is derived from the content's devtools_frame_token, is
  // defined by the browser and passed into Blink upon frame creation.
  virtual base::UnguessableToken GetDevToolsFrameToken() {
    return base::UnguessableToken::Create();
  }

 protected:
  virtual ~WebRemoteFrameClient() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_
