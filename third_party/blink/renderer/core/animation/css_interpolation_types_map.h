// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_INTERPOLATION_TYPES_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_INTERPOLATION_TYPES_MAP_H_

#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolation_types_map.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CSSSyntaxDefinition;
class PropertyRegistry;

class CORE_EXPORT CSSInterpolationTypesMap : public InterpolationTypesMap {
 public:
  CSSInterpolationTypesMap(const PropertyRegistry* registry,
                           const Document& document);

  const InterpolationTypes& Get(const PropertyHandle&) const final;
  size_t Version() const final;

  static InterpolationTypes CreateInterpolationTypesForCSSSyntax(
      const AtomicString& property_name,
      const CSSSyntaxDefinition&,
      const PropertyRegistration&);

 private:
  Member<const PropertyRegistry> registry_;
  bool allow_all_animations_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_INTERPOLATION_TYPES_MAP_H_
