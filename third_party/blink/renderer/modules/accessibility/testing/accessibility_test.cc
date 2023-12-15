// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {

AccessibilityTest::AccessibilityTest(LocalFrameClient* local_frame_client)
    : RenderingTest(local_frame_client) {}

void AccessibilityTest::SetUp() {
  RenderingTest::SetUp();
  ax_context_ = std::make_unique<AXContext>(GetDocument(), ui::kAXModeComplete);
}

AXObjectCacheImpl& AccessibilityTest::GetAXObjectCache() const {
  DCHECK(GetDocument().View());
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(GetDocument().ExistingAXObjectCache());
  DCHECK(ax_object_cache);
  return *ax_object_cache;
}

AXObject* AccessibilityTest::GetAXObject(LayoutObject* layout_object) const {
  return GetAXObjectCache().Get(layout_object);
}

AXObject* AccessibilityTest::GetAXObject(const Node& node) const {
  return GetAXObjectCache().Get(&node);
}

AXObject* AccessibilityTest::GetAXRootObject() const {
  GetAXObjectCache().UpdateAXForAllDocuments();
  return GetAXObjectCache().Root();
}

AXObject* AccessibilityTest::GetAXBodyObject() const {
  return GetAXObjectCache().Get(GetDocument().body());
}

AXObject* AccessibilityTest::GetAXFocusedObject() const {
  return GetAXObjectCache().FocusedObject();
}

AXObject* AccessibilityTest::GetAXObjectByElementId(const char* id) const {
  const auto* element = GetElementById(id);
  return GetAXObjectCache().Get(element);
}

std::string AccessibilityTest::PrintAXTree() const {
  std::ostringstream stream;
  PrintAXTreeHelper(stream, GetAXRootObject(), 0);
  return stream.str();
}

std::ostringstream& AccessibilityTest::PrintAXTreeHelper(
    std::ostringstream& stream,
    const AXObject* root,
    size_t level) const {
  if (!root)
    return stream;

  stream << std::string(level * 2, '+');
  stream << *root << std::endl;
  for (const AXObject* child : root->ChildrenIncludingIgnored()) {
    DCHECK(child);
    PrintAXTreeHelper(stream, child, level + 1);
  }
  return stream;
}

}  // namespace blink
