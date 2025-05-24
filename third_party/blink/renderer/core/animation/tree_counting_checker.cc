// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/tree_counting_checker.h"

#include "third_party/blink/renderer/core/css/css_length_resolver.h"
#include "third_party/blink/renderer/core/dom/nth_index_cache.h"

namespace blink {

TreeCountingChecker* TreeCountingChecker::Create(
    const CSSLengthResolver& length_resolver) {
  const Element* element = length_resolver.GetElement();
  CHECK(element);
  unsigned nth_child_index =
      NthIndexCache::NthChildIndex(const_cast<Element&>(*element),
                                   /*filter=*/nullptr,
                                   /*selector_checker=*/nullptr,
                                   /*context=*/nullptr);
  unsigned nth_last_child_index =
      NthIndexCache::NthLastChildIndex(const_cast<Element&>(*element),
                                       /*filter=*/nullptr,
                                       /*selector_checker=*/nullptr,
                                       /*context=*/nullptr);
  return MakeGarbageCollected<TreeCountingChecker>(nth_child_index,
                                                   nth_last_child_index);
}

bool TreeCountingChecker::IsValid(const StyleResolverState& state,
                                  const InterpolationValue& underlying) const {
  const Element* element = state.CssToLengthConversionData().GetElement();
  CHECK(element);
  return nth_child_index_ ==
             NthIndexCache::NthChildIndex(const_cast<Element&>(*element),
                                          /*filter=*/nullptr,
                                          /*selector_checker=*/nullptr,
                                          /*context=*/nullptr) &&
         nth_last_child_index_ ==
             NthIndexCache::NthLastChildIndex(const_cast<Element&>(*element),
                                              /*filter=*/nullptr,
                                              /*selector_checker=*/nullptr,
                                              /*context=*/nullptr);
}

}  // namespace blink
