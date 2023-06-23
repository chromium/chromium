// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_font_feature_values_map.h"

#include "third_party/blink/renderer/core/css/css_font_feature_values_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"

namespace blink {

class FontFeatureValuesMapIterationSource final
    : public PairSyncIterable<CSSFontFeatureValuesMap>::IterationSource {
 public:
  FontFeatureValuesMapIterationSource(const CSSFontFeatureValuesMap& map,
                                      const FontFeatureAliases* aliases)
      : map_(map), aliases_(aliases), iterator_(aliases->begin()) {}

  bool FetchNextItem(ScriptState* script_state,
                     String& map_key,
                     Vector<uint32_t>& map_value,
                     ExceptionState&) override {
    if (!aliases_) {
      return false;
    }
    if (iterator_ == aliases_->end()) {
      return false;
    }
    map_key = iterator_->key;
    map_value = iterator_->value.indices;
    ++iterator_;
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(map_);
    PairSyncIterable<CSSFontFeatureValuesMap>::IterationSource::Trace(visitor);
  }

 private:
  // Needs to be kept alive while we're iterating over it.
  const Member<const CSSFontFeatureValuesMap> map_;
  const FontFeatureAliases* aliases_;
  FontFeatureAliases::const_iterator iterator_;
};

uint32_t CSSFontFeatureValuesMap::size() const {
  return aliases_ ? aliases_->size() : 0u;
}

PairSyncIterable<CSSFontFeatureValuesMap>::IterationSource*
CSSFontFeatureValuesMap::CreateIterationSource(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<FontFeatureValuesMapIterationSource>(*this,
                                                                   aliases_);
}

bool CSSFontFeatureValuesMap::GetMapEntry(ScriptState*,
                                          const String& key,
                                          Vector<uint32_t>& value,
                                          ExceptionState&) {
  auto it = aliases_->find(AtomicString(key));
  if (it == aliases_->end()) {
    return false;
  }
  value = it->value.indices;
  return true;
}

CSSFontFeatureValuesMap* CSSFontFeatureValuesMap::set(
    const String& key,
    V8UnionUnsignedLongOrUnsignedLongSequence* value) {
  CSSStyleSheet::RuleMutationScope mutation_scope(parent_rule_);

  AtomicString key_atomic(key);
  switch (value->GetContentType()) {
    case V8UnionUnsignedLongOrUnsignedLongSequence::ContentType::
        kUnsignedLong: {
      aliases_->Set(key_atomic, FeatureIndicesWithPriority{Vector<uint32_t>(
                                    {value->GetAsUnsignedLong()})});
      break;
    }
    case V8UnionUnsignedLongOrUnsignedLongSequence::ContentType::
        kUnsignedLongSequence: {
      aliases_->Set(key_atomic, FeatureIndicesWithPriority{
                                    value->GetAsUnsignedLongSequence()});
      break;
    }
  }

  return this;
}

void CSSFontFeatureValuesMap::clearForBinding(ScriptState*, ExceptionState&) {
  CSSStyleSheet::RuleMutationScope mutation_scope(parent_rule_);
  aliases_->clear();
}

bool CSSFontFeatureValuesMap::deleteForBinding(ScriptState*,
                                               const String& key,
                                               ExceptionState&) {
  CSSStyleSheet::RuleMutationScope mutation_scope(parent_rule_);
  auto it = aliases_->find(AtomicString(key));
  if (it == aliases_->end()) {
    return false;
  }
  aliases_->erase(it);
  return true;
}

}  // namespace blink
