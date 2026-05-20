// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/pwn_open_url_helper.h"

#include <utility>

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/frame.mojom.h"
#include "services/network/public/mojom/source_location.mojom.h"
#include "third_party/blink/public/mojom/frame/triggering_event_info.mojom-shared.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "url/gurl.h"

namespace content {

void PwnOpenURLWithDisposition(RenderFrameHost* render_frame_host,
                               const GURL& url,
                               WindowOpenDisposition disposition,
                               bool user_gesture) {
  auto params = blink::mojom::OpenURLParams::New();
  params->url = url;
  // A compromised renderer sets initiator_origin to its own committed origin
  // (the only one VerifyInitiatorOrigin() will accept for its process).
  params->initiator_origin = render_frame_host->GetLastCommittedOrigin();
  params->referrer = blink::mojom::Referrer::New();
  params->source_location = network::mojom::SourceLocation::New();
  params->disposition = disposition;
  params->should_replace_current_entry = false;
  params->user_gesture = user_gesture;
  params->triggering_event_info =
      blink::mojom::TriggeringEventInfo::kNotFromEvent;
  // Direct C++ dispatch into the FrameHost mojom impl - identical entry point
  // to the renderer-sent IPC (RenderFrameHostImpl::OpenURL).
  static_cast<mojom::FrameHost*>(
      static_cast<RenderFrameHostImpl*>(render_frame_host))
      ->OpenURL(std::move(params));
}

}  // namespace content
