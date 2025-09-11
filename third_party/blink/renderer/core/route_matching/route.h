// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_H_

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class URLPattern;

class Route : public EventTarget {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Route(Document& document) : document_(&document) {}
  void Trace(Visitor* v) const final;

  URLPattern* pattern() const;
  bool matches() const { return matches_; }

  void AddPattern(URLPattern*);

  // Check if this route matches or not, and store the current state. Fire
  // "activate" or "deactivate" events if the match status changes. Return true
  // if match status changed.
  bool UpdateMatchStatus();

 private:
  // EventTarget:
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  Member<Document> document_;
  HeapVector<Member<URLPattern>> patterns_;
  bool matches_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_H_
