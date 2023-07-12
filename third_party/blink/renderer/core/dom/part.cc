// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/core/dom/child_node_part.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/part_root.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

Part::Part(PartRoot& root, const Vector<String> metadata)
    : root_(root), metadata_(metadata) {
  root.AddPart(*this);
}

void Part::Trace(Visitor* visitor) const {
  visitor->Trace(root_);
  ScriptWrappable::Trace(visitor);
}

void Part::disconnect() {
  CHECK(root_) << "disconnect should be overridden";
  root_->RemovePart(*this);
  root_ = nullptr;
}

}  // namespace blink
