// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_DEBUG_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_DEBUG_UTILS_H_

#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class String;

// Friendly output of a subtree, useful for debugging.
// Pass in a `marked_node` if you want to show an * before that node in the
// tree.
MODULES_EXPORT String
ContentNodeTreeToString(const mojom::blink::AIPageContentNode* node);
MODULES_EXPORT String ContentNodeTreeToStringWithMarkedNodeHelper(
    const mojom::blink::AIPageContentNode* node,
    const mojom::blink::AIPageContentNode* marked_node);

// Get a string that shows a friendly output of the parent-child chain that
// reaches from the `root` to the `target`.
MODULES_EXPORT String
ContentNodeParentChainToString(const mojom::blink::AIPageContentNode* root,
                               const mojom::blink::AIPageContentNode* target);

MODULES_EXPORT String
ContentNodeToString(const mojom::blink::AIPageContentNode* target,
                    bool format_on_single_line = true);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_DEBUG_UTILS_H_
