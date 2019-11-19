// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_IPC_UTILS_H_
#define CONTENT_BROWSER_FRAME_HOST_IPC_UTILS_H_

#include "base/memory/ref_counted.h"
#include "content/common/frame.mojom.h"
#include "content/common/navigation_params.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

struct FrameHostMsg_DownloadUrl_Params;
struct FrameHostMsg_OpenURL_Params;

namespace content {

class SiteInstance;

// Verifies that |params| are valid and can be accessed by the renderer process
// associated with |site_instance|.
//
// Returns true if the |params| are valid.  As a side-effect of the verification
// |out_blob_url_token_remote| will be populated.
//
// Terminates the renderer with the given |process_id| and returns false if the
// |params| are invalid.
//
// This function has to be called on the UI thread.
bool VerifyDownloadUrlParams(
    SiteInstance* site_instance,
    const FrameHostMsg_DownloadUrl_Params& params,
    mojo::PendingRemote<blink::mojom::BlobURLToken>* out_blob_url_token_remote);

// Verifies that |params| are valid and can be accessed by the renderer process
// associated with |site_instance|.
//
// Returns true if the |params| are valid.  As a side-effect of the verification
// |out_validated_url| and |out_blob_url_loader_factory| will be populated.
//
// Terminates the renderer the process associated with |site_instance| and
// returns false if the |params| are invalid.
//
// This function has to be called on the UI thread.
bool VerifyOpenURLParams(SiteInstance* site_instance,
                         const FrameHostMsg_OpenURL_Params& params,
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
    SiteInstance* site_instance,
    mojom::CommonNavigationParams* common_params);

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_IPC_UTILS_H_
