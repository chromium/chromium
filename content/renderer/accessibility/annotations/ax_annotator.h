// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_ANNOTATOR_H_
#define CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_ANNOTATOR_H_

#include <vector>

#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace blink {
class WebDocument;
}  // namespace blink

namespace content {

class CONTENT_EXPORT AXAnnotator {
 public:
  virtual ~AXAnnotator() = default;

  // Annotate the document with the given updates. |load_complete| is a boolean
  // denoting whether this annotate call was the result of a load complete.
  virtual void Annotate(const blink::WebDocument& document,
                        ui::AXTreeUpdate* update,
                        bool load_complete) = 0;

  // Enables the annotations feature. In many cases, sets up the mojo pipe
  // to the service which will perform the annotations.
  virtual void EnableAnnotations() = 0;

  // Cancel any in-progress annotations.
  virtual void CancelAnnotations() = 0;

  // Returns the AXMode which enables this AXAnnotator.
  virtual uint32_t GetAXModeToEnableAnnotations() = 0;

  // Returns whether this AXAnnotator has an action which enables it.
  virtual bool HasAXActionToEnableAnnotations() = 0;

  // Returns the action which enables this AXAnnotator.
  virtual ax::mojom::Action GetAXActionToEnableAnnotations() = 0;

  // Add any additional debugging attributes.
  virtual void AddDebuggingAttributes(
      const std::vector<ui::AXTreeUpdate>& updates) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_ANNOTATOR_H_
