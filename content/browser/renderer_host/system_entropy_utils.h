// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SYSTEM_ENTROPY_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_SYSTEM_ENTROPY_UTILS_H_

#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/mojom/navigation/system_entropy.mojom.h"

namespace content {

class SystemEntropyUtils {
 public:
  // Determines the current system entropy value for `frame_tree_node`.
  //
  // Returns `SystemEntropy::kHigh` for all top level navigations that
  // occur between the start of the browser process up until the first
  // visible page completes loading.
  //
  // Returns `SystemEntropy::kNormal` for all top level navigations
  // that occur after the first visible page has completed loading.
  //
  // Returns `SystemEntropy::kEmpty` for all framed navigations, or if the
  // `suggested_system_entropy` is `SystemEntropy::kEmpty`.
  static blink::mojom::SystemEntropy ComputeSystemEntropyForFrameTreeNode(
      FrameTreeNode* frame_tree_node,
      blink::mojom::SystemEntropy suggested_system_entropy) {
    // If the suggested system entropy is `SystemEntropy::kEmpty`, or this is
    // a framed navigation, return `SystemEntropy::kEmpty`.
    if (suggested_system_entropy == blink::mojom::SystemEntropy::kEmpty ||
        !frame_tree_node->IsOutermostMainFrame()) {
      return blink::mojom::SystemEntropy::kEmpty;
    }

    // During browser startup, return `SystemEntropy::kHigh`.
    if (!GetContentClient()->browser()->IsBrowserStartupComplete()) {
      return blink::mojom::SystemEntropy::kHigh;
    }

    CHECK_NE(suggested_system_entropy, blink::mojom::SystemEntropy::kEmpty);
    return suggested_system_entropy;
  }
};
}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SYSTEM_ENTROPY_UTILS_H_
