// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANCHOR_SCROLL_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANCHOR_SCROLL_VALUE_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ScopedCSSName;

// Represents the computed value of `anchor-scroll` property:
// - `none` is represented by a nullptr AnchorScrollValue
// - `implicit` is represented by an AnchorScrollValue with nullptr name
// - Named values are represented by an AnchorScrollValue with that name
class AnchorScrollValue : public GarbageCollected<AnchorScrollValue> {
 public:
  // Creates a named value.
  explicit AnchorScrollValue(const ScopedCSSName&);

  // Gets or creates the implicit value.
  static AnchorScrollValue* Implicit();

  // For creating the implicit value only.
  explicit AnchorScrollValue(base::PassKey<AnchorScrollValue>);

  bool IsImplicit() const { return !name_; }
  bool IsNamed() const { return name_; }
  const ScopedCSSName& GetName() const {
    DCHECK(name_);
    return *name_;
  }

  bool operator==(const AnchorScrollValue&) const;
  bool operator!=(const AnchorScrollValue& other) const {
    return !operator==(other);
  }

  void Trace(Visitor*) const;

 private:
  Member<const ScopedCSSName> name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANCHOR_SCROLL_VALUE_H_
