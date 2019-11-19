/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#include "third_party/blink/renderer/core/css/select_rule_feature_set.h"

#include <bitset>
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

void SelectRuleFeatureSet::CollectFeaturesFromSelectorList(
    const CSSSelectorList& list) {
  for (const CSSSelector* selector = list.First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    for (const CSSSelector* component = selector; component;
         component = component->TagHistory()) {
      if (InvalidationSetForSimpleSelector(
              *component, InvalidationType::kInvalidateDescendants, kSubject)) {
        continue;
      }

      if (component->SelectorList())
        CollectFeaturesFromSelectorList(*component->SelectorList());
    }
  }
}

bool SelectRuleFeatureSet::CheckSelectorsForClassChange(
    const SpaceSplitString& changed_classes) const {
  unsigned changed_size = changed_classes.size();
  for (unsigned i = 0; i < changed_size; ++i) {
    if (HasSelectorForClass(changed_classes[i]))
      return true;
  }
  return false;
}

bool SelectRuleFeatureSet::CheckSelectorsForClassChange(
    const SpaceSplitString& old_classes,
    const SpaceSplitString& new_classes) const {
  if (!old_classes.size())
    return CheckSelectorsForClassChange(new_classes);

  // Class vectors tend to be very short. This is faster than using a hash
  // table.
  WTF::Vector<bool> remaining_class_bits(old_classes.size());

  for (unsigned i = 0; i < new_classes.size(); ++i) {
    bool found = false;
    for (unsigned j = 0; j < old_classes.size(); ++j) {
      if (new_classes[i] == old_classes[j]) {
        // Mark each class that is still in the newClasses so we can skip doing
        // an n^2 search below when looking for removals. We can't break from
        // this loop early since a class can appear more than once.
        remaining_class_bits[j] = true;
        found = true;
      }
    }
    // Class was added.
    if (!found) {
      if (HasSelectorForClass(new_classes[i]))
        return true;
    }
  }

  for (unsigned i = 0; i < old_classes.size(); ++i) {
    if (remaining_class_bits[i])
      continue;

    // Class was removed.
    if (HasSelectorForClass(old_classes[i]))
      return true;
  }
  return false;
}

}  // namespace blink
