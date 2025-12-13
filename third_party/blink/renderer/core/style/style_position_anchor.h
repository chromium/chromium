// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_POSITION_ANCHOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_POSITION_ANCHOR_H_

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Represents the computed value of 'position-anchor'.
// https://drafts.csswg.org/css-anchor-position/#position-anchor
class CORE_EXPORT StylePositionAnchor {
  DISALLOW_NEW();

 public:
  enum class Type { kNone, kAuto, kName };

  explicit StylePositionAnchor(Type type) : type_(type) {}
  explicit StylePositionAnchor(const ScopedCSSName* name)
      : type_(Type::kName), name_(name) {}

  static StylePositionAnchor Initial() {
    return RuntimeEnabledFeatures::CSSPositionAnchorNoneEnabled()
               ? StylePositionAnchor(StylePositionAnchor::Type::kNone)
               : StylePositionAnchor(StylePositionAnchor::Type::kAuto);
  }

  bool operator==(const StylePositionAnchor& o) const {
    return type_ == o.type_ && base::ValuesEquivalent(name_, o.name_);
  }

  Type GetType() const { return type_; }
  bool IsAuto() const { return type_ == Type::kAuto; }
  bool IsName() const { return type_ == Type::kName; }

  const ScopedCSSName& GetName() const {
    DCHECK_EQ(type_, Type::kName);
    DCHECK(name_);
    return *name_;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(name_); }

 private:
  Type type_ = Type::kNone;
  Member<const ScopedCSSName> name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_POSITION_ANCHOR_H_
