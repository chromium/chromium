// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/patching/patch_supplement.h"

#include <optional>

#include "third_party/blink/renderer/core/patching/dom_patch_status.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

// static
const char PatchSupplement::kSupplementName[] = "Patch";

// static
PatchSupplement* PatchSupplement::FromIfExists(const Document& document) {
  return Supplement<Document>::From<PatchSupplement>(document);
}

// static
PatchSupplement* PatchSupplement::From(Document& document) {
  auto* supplement = Supplement<Document>::From<PatchSupplement>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<PatchSupplement>(document);
    Supplement<Document>::ProvideTo(document, supplement);
  }
  return supplement;
}

DOMPatchStatus* PatchSupplement::CurrentPatchFor(const Node& target) {
  if (auto index = IndexOfPatch(target)) {
    return patches_.at(*index);
  } else {
    return nullptr;
  }
}

std::optional<size_t> PatchSupplement::IndexOfPatch(const Node& target) {
  for (size_t i = 0; i < patches_.size(); ++i) {
    if (patches_[i]->GetTarget() == target) {
      return i;
    }
  }
  return std::nullopt;
}

void PatchSupplement::DidStart(Node& target, DOMPatchStatus* status) {
  patches_.push_back(status);
  if (Element* element = DynamicTo<Element>(target)) {
    element->PatchStateChanged();
  }
}

void PatchSupplement::DidComplete(Node& target) {
  if (auto index = IndexOfPatch(target)) {
    patches_.EraseAt(*index);
  }
  if (Element* element = DynamicTo<Element>(target)) {
    element->PatchStateChanged();
  }
}

void PatchSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(patches_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
