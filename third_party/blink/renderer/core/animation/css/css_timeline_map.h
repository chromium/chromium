// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TIMELINE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TIMELINE_MAP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/style_timeline_scope.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class DeferredTimeline;
class Document;
class ScrollTimeline;
class ViewTimeline;

template <typename TimelineType>
using CSSTimelineMap = HeapHashMap<AtomicString, Member<TimelineType>>;
using CSSViewTimelineMap = CSSTimelineMap<ViewTimeline>;
using CSSScrollTimelineMap = CSSTimelineMap<ScrollTimeline>;
using TimelineAttachmentMap =
    HeapHashMap<Member<ScrollTimeline>, Member<DeferredTimeline>>;

// CSSDeferredTimelineMap, logically, contains one DeferredTimeline for every
// possible key, except that only the names matching the specified filter
// are retrievable.
//
// The 'timeline-scope' property, when specified one some element, determines
// the filter used for that element's CSSDeferredTimelineMap:
//
// - For 'timeline-scope:none', an empty filter.
// - For 'timeline-scope:--a,--b', a filter containing those names.
// - For 'timeline-scope:all', a filter (logically) containing all possible
//   names (StyleTimelineScope::Type::kAll).
//
class CORE_EXPORT CSSDeferredTimelineMap {
  DISALLOW_NEW();

 public:
  // The default constructor a map with a "none" filter.
  CSSDeferredTimelineMap() = default;
  explicit CSSDeferredTimelineMap(StyleTimelineScope filter)
      : filter_(filter) {}
  CSSDeferredTimelineMap(const CSSDeferredTimelineMap& other,
                         StyleTimelineScope filter)
      : filter_(std::move(filter)), map_(other.map_) {}

  void Trace(blink::Visitor* visitor) const;

  // Find a DeferredTimeline with a name matching `filter_`.
  //
  // As long as the name matches `filter_`, a non-nullptr value will always
  // be returned. However, it does not necessarily return the same instance
  // (for the same key) over time.
  DeferredTimeline* Find(Document&, const AtomicString& name) const;

  bool IsEmpty() const { return filter_.IsNone(); }

  const StyleTimelineScope& GetFilter() const { return filter_; }

 private:
  StyleTimelineScope filter_;
  using InnerMap = HeapHashMap<AtomicString, WeakMember<DeferredTimeline>>;
  mutable InnerMap map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TIMELINE_MAP_H_
