// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_range.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"

namespace blink {

AXRange::AXRange(const AXPosition& start, const AXPosition& end)
    : start_(), end_() {
  if (!start.IsValid() || !end.IsValid() || start > end)
    return;

  const Document* document = start.ContainerObject()->GetDocument();
  DCHECK(document);
  DCHECK(document->IsActive());
  DCHECK(!document->NeedsLayoutTreeUpdate());
  // We don't support ranges that span across documents.
  if (end.ContainerObject()->GetDocument() != document)
    return;

  start_ = start;
  end_ = end;

#if DCHECK_IS_ON()
  dom_tree_version_ = document->DomTreeVersion();
  style_version_ = document->StyleVersion();
#endif  // DCHECK_IS_ON()
}

AXObject* AXRange::CommonAncestorContainer() const {
  if (!IsValid())
    return nullptr;
  int start_index, end_index;
  return const_cast<AXObject*>(AXObject::LowestCommonAncestor(
      *start_.ContainerObject(), *end_.ContainerObject(), &start_index,
      &end_index));
}

bool AXRange::IsCollapsed() const {
  return IsValid() && start_ == end_;
}

bool AXRange::IsValid() const {
  if (!start_.IsValid() || !end_.IsValid())
    return false;

  // We don't support ranges that span across documents.
  if (start_.ContainerObject()->GetDocument() !=
      end_.ContainerObject()->GetDocument()) {
    return false;
  }

  DCHECK(!start_.ContainerObject()->GetDocument()->NeedsLayoutTreeUpdate());
#if DCHECK_IS_ON()
  DCHECK_EQ(start_.ContainerObject()->GetDocument()->DomTreeVersion(),
            dom_tree_version_);
  DCHECK_EQ(start_.ContainerObject()->GetDocument()->StyleVersion(),
            style_version_);
#endif  // DCHECK_IS_ON()
  return true;
}

// static
AXRange AXRange::RangeOfContents(const AXObject& container) {
  return AXRange(AXPosition::CreateFirstPositionInObject(container),
                 AXPosition::CreateLastPositionInObject(container));
}

String AXRange::ToString() const {
  if (!IsValid())
    return "Invalid AXRange";
  return "AXRange from " + Start().ToString() + " to " + End().ToString();
}

bool operator==(const AXRange& a, const AXRange& b) {
  DCHECK(a.IsValid() && b.IsValid());
  return a.Start() == b.Start() && a.End() == b.End();
}

bool operator!=(const AXRange& a, const AXRange& b) {
  return !(a == b);
}

std::ostream& operator<<(std::ostream& ostream, const AXRange& range) {
  return ostream << range.ToString().Utf8();
}

}  // namespace blink
