// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ISOLATED_WORLD_IDS_H_
#define CONTENT_PUBLIC_COMMON_ISOLATED_WORLD_IDS_H_

namespace content {

enum IsolatedWorldIDs : int32_t {
  // Chrome cannot use ID 0 for an isolated world because 0 represents the main
  // world.
  ISOLATED_WORLD_ID_GLOBAL = 0,
  // Custom isolated world ids used by other embedders should start from here.
  ISOLATED_WORLD_ID_CONTENT_END,
  // If any embedder has more than 10 custom isolated worlds that will be run
  // via RenderFrameImpl::OnJavaScriptExecuteRequestInIsolatedWorld update this.
  ISOLATED_WORLD_ID_MAX = ISOLATED_WORLD_ID_CONTENT_END + 10,
};

}  // namespace content

#endif  // COTENT_PUBLIC_COMMON_ISOLATED_WORLD_IDS_H_
