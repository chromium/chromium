/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class ComputedStyle;

class CORE_EXPORT PseudoElement : public Element {
 public:
  static PseudoElement* Create(Element* parent, PseudoId);

  PseudoElement(Element*, PseudoId);

  scoped_refptr<ComputedStyle> CustomStyleForLayoutObject() override;
  void AttachLayoutTree(AttachContext&) override;
  bool LayoutObjectIsNeeded(const ComputedStyle&) const override;

  bool CanStartSelection() const override { return false; }
  bool CanContainRangeEndPoint() const override { return false; }
  PseudoId GetPseudoId() const override { return pseudo_id_; }
  scoped_refptr<ComputedStyle> LayoutStyleForDisplayContents(
      const ComputedStyle&);

  static String PseudoElementNameForEvents(PseudoId);

  // Pseudo element are not allowed to be the inner node for hit testing. Find
  // the closest ancestor which is a real dom node.
  virtual Node* InnerNodeForHitTesting() const;

  virtual void Dispose();

 private:
  class AttachLayoutTreeScope {
    STACK_ALLOCATED();

   public:
    AttachLayoutTreeScope(PseudoElement*);
    ~AttachLayoutTreeScope();

   private:
    Member<PseudoElement> element_;
    scoped_refptr<const ComputedStyle> original_style_;
  };

  PseudoId pseudo_id_;
};

const QualifiedName& PseudoElementTagName();

bool PseudoElementLayoutObjectIsNeeded(const ComputedStyle*);

template <>
struct DowncastTraits<PseudoElement> {
  static bool AllowFrom(const Node& node) { return node.IsPseudoElement(); }
};

}  // namespace blink

#endif
