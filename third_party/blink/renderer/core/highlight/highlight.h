// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_H_

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/abstract_range.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

using HighlightSetIterable = SetlikeIterable<Member<AbstractRange>>;

class CORE_EXPORT Highlight : public ScriptWrappable,
                              public HighlightSetIterable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Highlight* Create(const String&, HeapVector<Member<AbstractRange>>&);

  Highlight(const String&, HeapVector<Member<AbstractRange>>&);
  ~Highlight() override;

  void Trace(blink::Visitor*) const override;

  Highlight* addForBinding(ScriptState*, AbstractRange*, ExceptionState&);
  void clearForBinding(ScriptState*, ExceptionState&);
  bool deleteForBinding(ScriptState*, AbstractRange*, ExceptionState&);
  bool hasForBinding(ScriptState*, AbstractRange*, ExceptionState&) const;
  wtf_size_t size() const;

  const String& name() const { return name_; }
  const int32_t& priority() const { return priority_; }
  void setPriority(const int32_t& priority) { priority_ = priority; }

  class IterationSource final : public HighlightSetIterable::IterationSource {
   public:
    explicit IterationSource(const Highlight& highlight);

    bool Next(ScriptState*,
              Member<AbstractRange>&,
              Member<AbstractRange>&,
              ExceptionState&) override;

    void Trace(blink::Visitor*) const override;

   private:
    wtf_size_t index_;
    HeapVector<Member<AbstractRange>> highlight_ranges_snapshot_;
  };

  HighlightSetIterable::IterationSource* StartIteration(
      ScriptState*,
      ExceptionState&) override;

 private:
  HeapLinkedHashSet<Member<AbstractRange>> highlight_ranges_;
  int32_t priority_ = 0;
  AtomicString name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_H_
