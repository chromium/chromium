// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/sanitizer/streaming_sanitizer.h"

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"

namespace blink {

StreamingSanitizer* StreamingSanitizer::CreateSafe(
    const Sanitizer* sanitizer,
    const ContainerNode* context) {
  Sanitizer* safe = MakeGarbageCollected<Sanitizer>();
  safe->setFrom(*sanitizer);
  safe->removeUnsafe();
  return MakeGarbageCollected<StreamingSanitizer>(safe, context, true);
}

StreamingSanitizer* StreamingSanitizer::CreateUnsafe(
    const Sanitizer* sanitizer,
    const ContainerNode* context) {
  Sanitizer* sanitizer_clone = MakeGarbageCollected<Sanitizer>();
  sanitizer_clone->setFrom(*sanitizer);
  return MakeGarbageCollected<StreamingSanitizer>(sanitizer_clone, context,
                                                  false);
}

bool StreamingSanitizer::ShouldReplaceWithChildren(Node* target) {
  return target != context_ &&
         sanitizer_->ShouldReplaceNodeWithChildren(target);
}

bool StreamingSanitizer::Sanitize(Node* target) {
  return sanitizer_->SanitizeSingleNode(
      target, safe_ ? Sanitizer::Mode::kSafe : Sanitizer::Mode::kUnsafe);
}

void StreamingSanitizer::Trace(Visitor* visitor) const {
  visitor->Trace(sanitizer_);
  visitor->Trace(context_);
}

}  // namespace blink
