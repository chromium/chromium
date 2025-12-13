// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"

#include "base/strings/strcat.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
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
  ax_context_ =
      std::make_unique<AXContext>(GetDocument(), ui::kAXModeDefaultForTests);
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

AXObject* AccessibilityTest::GetAXObjectByElementId(const char* id,
                                                    PseudoId pseudo_id) const {
  const auto* element = GetElementById(id);
  if (element && pseudo_id != kPseudoIdNone) {
    return GetAXObjectCache().Get(element->GetPseudoElement(pseudo_id));
  }
  return GetAXObjectCache().Get(element);
}

// static
std::string AccessibilityTest::PrintAXTree(Document& document) {
  auto ax_context =
      std::make_unique<AXContext>(document, ui::kAXModeDefaultForTests);

  DCHECK(document.View());
  document.View()->UpdateAllLifecyclePhasesForTest();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document.ExistingAXObjectCache());
  DCHECK(ax_object_cache);

  ax_object_cache->UpdateAXForAllDocuments();
  AXObject* root = ax_object_cache->Root();

  std::string out;
  PrintAXTreeHelper(out, root, 0);
  return out;
}

// static
void AccessibilityTest::PrintAXTreeHelper(std::string& out,
                                          const AXObject* root,
                                          size_t level) {
  if (!root)
    return;

  base::StrAppend(&out,
                  {std::string(level * 2, '+'), root->ToString().Utf8(), "\n"});
  for (const AXObject* child : root->ChildrenIncludingIgnored()) {
    DCHECK(child);
    PrintAXTreeHelper(out, child, level + 1);
  }
}

}  // namespace blink
