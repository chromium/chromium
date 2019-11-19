// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_AX_TREE_CONVERTER_H_
#define FUCHSIA_ENGINE_BROWSER_AX_TREE_CONVERTER_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>

#include "content/public/browser/ax_event_notification_details.h"
#include "fuchsia/engine/web_engine_export.h"

// Converts an AXNodeData to a Fuchsia Semantic Node.
// Both data types represent a single node, and no additional state is needed.
// AXNodeData is used to convey partial updates, so not all fields may be
// present. Those that are will be converted. The Fuchsia SemanticsManager
// accepts partial updates, so |node| does not require all fields to be set.
WEB_ENGINE_EXPORT fuchsia::accessibility::semantics::Node
AXNodeDataToSemanticNode(const ui::AXNodeData& node);

#endif  // FUCHSIA_ENGINE_BROWSER_AX_TREE_CONVERTER_H_
