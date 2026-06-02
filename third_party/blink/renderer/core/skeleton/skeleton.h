// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class Document;

// Represents a skeleton being rendered
class Skeleton : public GarbageCollected<Skeleton> {
 public:
  class Observer : public GarbageCollectedMixin {
   public:
    // Invoked when a Render resulted in a Document.
    // Used to signal the hosting document that it can start rendering the
    // skeleton.
    virtual void DocumentReady(Skeleton& skeleton) = 0;
  };

  explicit Skeleton(Observer& observer) : observer_(&observer) {}

  // Render the skeleton for a given url
  void Render(KURL url, Document& owner_document);

  Document& GetDocument() {
    CHECK(skeleton_document_);
    return *skeleton_document_;
  }

  void Trace(Visitor* visitor) const;

 private:
  void GenerateSkeleton(KURL url);

  Member<Observer> observer_;
  Member<Document> skeleton_document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_H_
