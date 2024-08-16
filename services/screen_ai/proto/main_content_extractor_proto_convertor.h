// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SCREEN_AI_PROTO_MAIN_CONTENT_EXTRACTOR_PROTO_CONVERTOR_H_
#define SERVICES_SCREEN_AI_PROTO_MAIN_CONTENT_EXTRACTOR_PROTO_CONVERTOR_H_

#include <map>
#include <optional>
#include <string>

#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_tree_update_forward.h"
#include "ui/gfx/geometry/size_f.h"

namespace ui {
class AXTree;
}

namespace screen_ai {

struct ViewHierarchyAndTreeSize {
  std::string serialized_proto;
  gfx::SizeF tree_dimensions;
};

// Converts an AXTree with an unserialized snapshot to a serialized
// ViewHierarchy proto.
std::optional<ViewHierarchyAndTreeSize> SnapshotToViewHierarchy(
    const ui::AXTree& tree);

// Returns a map of MainContentExtractor role strings to Chrome roles.
std::map<std::string, ax::mojom::Role>
GetMainContentExtractorToChromeRoleConversionMapForTesting();

}  // namespace screen_ai

#endif  // SERVICES_SCREEN_AI_PROTO_MAIN_CONTENT_EXTRACTOR_PROTO_CONVERTOR_H_
