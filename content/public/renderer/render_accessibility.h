// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_ACCESSIBILITY_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_ACCESSIBILITY_H_

#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_source.h"

namespace ui {

class AXTreeID;

}  // namespace ui

namespace content {

class PluginAXTreeActionTargetAdapter;

// This interface exposes the accessibility tree for one RenderFrame.
class CONTENT_EXPORT RenderAccessibility {
 public:
  virtual bool HasActiveDocument() const = 0;
  virtual ui::AXMode GetAXMode() const = 0;
  virtual void RecordInaccessiblePdfUkm() = 0;
  virtual void SetPluginAXTreeActionTargetAdapter(
      PluginAXTreeActionTargetAdapter* adapter) = 0;

 protected:
  ~RenderAccessibility() {}

 private:
  // This interface should only be implemented inside content.
  friend class RenderAccessibilityImpl;
  RenderAccessibility() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_ACCESSIBILITY_H_
