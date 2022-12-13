// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/scoped_blink_ax_event_intent.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"

namespace blink {

ScopedBlinkAXEventIntent::ScopedBlinkAXEventIntent(
    const BlinkAXEventIntent& intent,
    Document* document)
    : document_(document) {
  DCHECK(document_);
  DCHECK(document_->IsActive());

  if (!intent.is_initialized())
    return;
  intents_.push_back(intent);

  if (AXObjectCache* cache = document_->ExistingAXObjectCache()) {
    AXObjectCache::BlinkAXEventIntentsSet& active_intents =
        cache->ActiveEventIntents();
    active_intents.insert(intent);
  }
}

ScopedBlinkAXEventIntent::ScopedBlinkAXEventIntent(
    const Vector<BlinkAXEventIntent>& intents,
    Document* document)
    : intents_(intents), document_(document) {
  DCHECK(document_);
  DCHECK(document_->IsActive());
  if (AXObjectCache* cache = document_->ExistingAXObjectCache()) {
    AXObjectCache::BlinkAXEventIntentsSet& active_intents =
        cache->ActiveEventIntents();

    for (const auto& intent : intents) {
      if (intent.is_initialized())
        active_intents.insert(intent);
    }
  }
}

ScopedBlinkAXEventIntent::~ScopedBlinkAXEventIntent() {
  if (!document_->IsActive())
    return;

  if (AXObjectCache* cache = document_->ExistingAXObjectCache()) {
    AXObjectCache::BlinkAXEventIntentsSet& active_intents =
        cache->ActiveEventIntents();

    for (const auto& intent : intents_) {
      DCHECK(active_intents.Contains(intent));
      active_intents.erase(intent);
    }
  }
}

}  // namespace blink
