// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_REGISTRY_H_

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

using HighlightRegistrySetIterable = SetlikeIterable<Member<Highlight>>;

class CORE_EXPORT HighlightRegistry : public ScriptWrappable,
                                      public Supplement<LocalDOMWindow>,
                                      public HighlightRegistrySetIterable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static HighlightRegistry* From(LocalDOMWindow&);

  explicit HighlightRegistry(LocalDOMWindow&);
  ~HighlightRegistry() override;

  void Trace(blink::Visitor*) const override;

  HighlightRegistry* addForBinding(ScriptState*, Highlight*, ExceptionState&);
  void clearForBinding(ScriptState*, ExceptionState&);
  bool deleteForBinding(ScriptState*, Highlight*, ExceptionState&);
  bool hasForBinding(ScriptState*, Highlight*, ExceptionState&) const;
  wtf_size_t size() const { return highlights_.size(); }

 public:
  class IterationSource final
      : public HighlightRegistrySetIterable::IterationSource {
   public:
    explicit IterationSource(
        const Member<HighlightRegistry>& highlight_registry) {}

    bool Next(ScriptState*,
              Member<Highlight>&,
              Member<Highlight>&,
              ExceptionState&) override;

    void Trace(blink::Visitor*) const override;
  };

 private:
  // Keeps all the names of Highlights added to the registry (in highlights_) so
  // it can be determined in O(1) if a name is already present.
  HashSet<String, StringHash> registered_highlight_names_;
  HeapLinkedHashSet<Member<Highlight>> highlights_;

  HighlightRegistrySetIterable::IterationSource* StartIteration(
      ScriptState*,
      ExceptionState&) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_REGISTRY_H_
