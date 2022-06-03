// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_

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

class WebRemoteFrameClient {
 public:
  // Specifies the reason for the detachment.
  enum class DetachType { kRemove, kSwap };

  // Notify the embedder that it should remove this frame from the frame tree
  // and release any resources associated with it.
  virtual void FrameDetached(DetachType) {}

  // Returns an AssociatedInterfaceProvider the frame can use to request
  // associated interfaces from the browser.
  virtual AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() = 0;

 protected:
  virtual ~WebRemoteFrameClient() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_
