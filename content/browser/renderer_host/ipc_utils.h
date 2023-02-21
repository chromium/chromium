// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_IPC_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_IPC_UTILS_H_

#include "base/memory/ref_counted.h"
#include "content/common/frame.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "url/gurl.h"

namespace content {

class SiteInstance;
class RenderFrameHostImpl;

// Verifies that |params| are valid and can be accessed by the renderer process
// associated with |site_instance|.
//
// If the |params| are valid, returns true.
//
// Otherwise, terminates the renderer associated with |site_instance| and
// returns false.
//
// This function has to be called on the UI thread.
bool VerifyDownloadUrlParams(SiteInstance* site_instance,
                             const blink::mojom::DownloadURLParams& params);

// Verifies that |params| are valid and can be accessed by the renderer process
// associated with |site_instance|. |current_rfh| represents the current frame
// on which OpenURL is being called
//
// Returns true if the |params| are valid.  As a side-effect of the verification
// |out_validated_url| and |out_blob_url_loader_factory| will be populated.
//
// Terminates the renderer the process associated with |site_instance| and
// returns false if the |params| are invalid.
//
// This function has to be called on the UI thread.
bool VerifyOpenURLParams(RenderFrameHostImpl* current_rfh,
                         SiteInstance* site_instance,
                         const blink::mojom::OpenURLParamsPtr& params,
                         GURL* out_validated_url,
                         scoped_refptr<network::SharedURLLoaderFactory>*
                             out_blob_url_loader_factory);

// Verifies that CommonNavigationParams are valid and can be accessed by the
// renderer process associated with |site_instance|.
//
// Returns true if the CommonNavigationParams are valid.  As a side-effect of
// the verification parts of |common_params| will be rewritten (e.g. some
// URLs will be filtered).
//
// Terminates the renderer the process associated with |site_instance| and
// returns false if the CommonNavigationParams are invalid.
//
// This function has to be called on the UI thread.
bool VerifyBeginNavigationCommonParams(
    const RenderFrameHostImpl& current_rfh,
    SiteInstance* site_instance,
    blink::LocalFrameToken* initiator_frame_token,
    blink::mojom::CommonNavigationParams* common_params);

// Verify that the initiator frame identified by `initiator_frame_token` and
// `initiator_process_id` can navigate `current_rfh`.
//
// This is best effort: the frame with the corresponding frame token may have
// been deleted before the navigation begins.
//
// Returns true if the navigation is valid, or if the an initiator frame is not
// found. Terminates the renderer the process associated with
// |initiator_process_id| and returns false if the navigation is not allowed.
//
// This function has to be called on the UI thread.
bool VerifyNavigationInitiator(
    RenderFrameHostImpl* current_rfh,
    const absl::optional<blink::LocalFrameToken>& initiator_frame_token,
    int initiator_process_id);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_IPC_UTILS_H_
