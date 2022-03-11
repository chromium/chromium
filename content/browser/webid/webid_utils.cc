// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/webid_utils.h"

#include "content/public/browser/render_frame_host.h"

namespace content {

bool IsSameOriginWithAncestors(RenderFrameHost* host,
                               const url::Origin& origin) {
  RenderFrameHost* parent = host->GetParentOrOuterDocument();
  while (parent) {
    if (!parent->GetLastCommittedOrigin().IsSameOriginWith(origin)) {
      return false;
    }
    parent = parent->GetParent();
  }
  return true;
}

}  // namespace content
