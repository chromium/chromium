// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_CONTEXT_H_

#include <cstdint>

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class Node;
class CORE_EXPORT SoftNavigationContext
    : public GarbageCollected<SoftNavigationContext> {
  static uint64_t last_context_id_;

 public:
  explicit SoftNavigationContext(bool advanced_paint_attribution_enabled);

  bool IsMostRecentlyCreatedContext() const {
    return context_id_ == last_context_id_;
  }

  base::TimeTicks UserInteractionTimestamp() const {
    return user_interaction_timestamp_;
  }
  void SetUserInteractionTimestamp(base::TimeTicks value) {
    user_interaction_timestamp_ = value;
  }

  const String& Url() const { return url_; }
  void SetUrl(const String& url) { url_ = url; }

  bool WasEmitted() const { return was_emitted_; }
  void SetWasEmitted() { was_emitted_ = true; }

  void AddModifiedNode(Node* node);
  // Returns true if this paint updated the attributed area, and so we should
  // check for sufficient paints to emit a soft-nav entry.
  bool HasDomModification() const { return num_modified_dom_nodes_ > 0; }

  // Reports a new contentful paint area to this context, and the Node painted.
  // Returns true if we update the total attributed area (meaning this context
  // was involved in modifying this dom node, and we grew the painted region).
  // Return value is used to check if it is worthwhile to check for "sufficient
  // paints" (to emit a new soft-nav entry).
  bool AddPaintedArea(Node* node,
                      const gfx::RectF& rect,
                      bool is_newest_context);
  bool OnPaintFinished();

  bool SatisfiesSoftNavNonPaintCriteria() const;
  bool SatisfiesSoftNavPaintCriteria(uint64_t required_paint_area) const;

  void WriteIntoTrace(perfetto::TracedValue context) const;

  void Trace(Visitor* visitor) const;

 private:
  // Pre-Increment `last_context_id_` such that the newest context uses the
  // largest value and can be used to identify the most recent context.
  const uint64_t context_id_ = ++last_context_id_;

  bool advanced_paint_attribution_enabled_;

  base::TimeTicks user_interaction_timestamp_;
  String url_;
  bool was_emitted_ = false;

  blink::HeapHashSet<WeakMember<Node>> modified_nodes_;
  blink::HeapHashSet<WeakMember<Node>> already_painted_modified_nodes_;

  // Elements of `modified_nodes_` can get GC-ed, so we need to keep a count of
  // the total nodes modified.
  size_t num_modified_dom_nodes_ = 0;
  uint64_t painted_area_ = 0;
  uint64_t repainted_area_ = 0;
  uint64_t unattributed_area_ = 0;

  size_t num_modified_dom_nodes_last_animation_frame_ = 0;
  size_t num_live_nodes_last_animation_frame_ = 0;
  uint64_t painted_area_last_animation_frame_ = 0;
  uint64_t repainted_area_last_animation_frame_ = 0;

  WeakMember<Node> known_not_related_parent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_CONTEXT_H_
