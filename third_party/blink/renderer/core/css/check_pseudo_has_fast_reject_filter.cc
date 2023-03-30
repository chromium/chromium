// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/check_pseudo_has_fast_reject_filter.h"

#include "third_party/blink/renderer/core/css/css_selector.h"

namespace blink {

namespace {

// Salt to separate otherwise identical string hashes so a class-selector like
// .article won't match <article> elements.
enum { kTagNameSalt = 13, kIdSalt = 17, kClassSalt = 19, kAttributeSalt = 23 };

inline bool IsExcludedAttribute(const AtomicString& name) {
  return name == html_names::kClassAttr.LocalName() ||
         name == html_names::kIdAttr.LocalName() ||
         name == html_names::kStyleAttr.LocalName();
}

inline unsigned GetTagHash(const AtomicString& tag_name) {
  return tag_name.Hash() * kTagNameSalt;
}

inline unsigned GetClassHash(const AtomicString& class_name) {
  return class_name.Hash() * kClassSalt;
}

inline unsigned GetIdHash(const AtomicString& id) {
  return id.Hash() * kIdSalt;
}

inline unsigned GetAttributeHash(const AtomicString& attribute_name) {
  return attribute_name.Hash() * kAttributeSalt;
}

}  // namespace

void CheckPseudoHasFastRejectFilter::AddElementIdentifierHashes(
    const Element& element) {
  DCHECK(filter_.get());
  filter_->Add(GetTagHash(element.LocalNameForSelectorMatching()));
  if (element.HasID()) {
    filter_->Add(GetIdHash(element.IdForStyleResolution()));
  }
  if (element.HasClass()) {
    const SpaceSplitString& class_names = element.ClassNames();
    wtf_size_t count = class_names.size();
    for (wtf_size_t i = 0; i < count; ++i) {
      filter_->Add(GetClassHash(class_names[i]));
    }
  }
  AttributeCollection attributes = element.AttributesWithoutUpdate();
  for (const auto& attribute_item : attributes) {
    auto attribute_name = attribute_item.LocalName();
    if (IsExcludedAttribute(attribute_name)) {
      continue;
    }
    auto lower = attribute_name.IsLowerASCII() ? attribute_name
                                               : attribute_name.LowerASCII();
    filter_->Add(GetAttributeHash(lower));
  }
}

bool CheckPseudoHasFastRejectFilter::FastReject(
    const Vector<unsigned>& pseudo_has_argument_hashes) const {
  DCHECK(filter_.get());
  if (pseudo_has_argument_hashes.empty()) {
    return false;
  }
  for (unsigned hash : pseudo_has_argument_hashes) {
    if (!filter_->MayContain(hash)) {
      return true;
    }
  }
  return false;
}

// static
void CheckPseudoHasFastRejectFilter::CollectPseudoHasArgumentHashes(
    Vector<unsigned>& pseudo_has_argument_hashes,
    const CSSSelector* simple_selector) {
  DCHECK(simple_selector);
  switch (simple_selector->Match()) {
    case CSSSelector::kId:
      if (simple_selector->Value().empty()) {
        break;
      }
      pseudo_has_argument_hashes.push_back(GetIdHash(simple_selector->Value()));
      break;
    case CSSSelector::kClass:
      if (simple_selector->Value().empty()) {
        break;
      }
      pseudo_has_argument_hashes.push_back(
          GetClassHash(simple_selector->Value()));
      break;
    case CSSSelector::kTag:
      if (simple_selector->TagQName().LocalName() !=
          CSSSelector::UniversalSelectorAtom()) {
        pseudo_has_argument_hashes.push_back(
            GetTagHash(simple_selector->TagQName().LocalName()));
      }
      break;
    case CSSSelector::kAttributeExact:
    case CSSSelector::kAttributeSet:
    case CSSSelector::kAttributeList:
    case CSSSelector::kAttributeContain:
    case CSSSelector::kAttributeBegin:
    case CSSSelector::kAttributeEnd:
    case CSSSelector::kAttributeHyphen: {
      auto attribute_name = simple_selector->Attribute().LocalName();
      if (IsExcludedAttribute(attribute_name)) {
        break;
      }
      auto lower_name = attribute_name.IsLowerASCII()
                            ? attribute_name
                            : attribute_name.LowerASCII();
      pseudo_has_argument_hashes.push_back(GetAttributeHash(lower_name));
    } break;
    default:
      break;
  }
}

void CheckPseudoHasFastRejectFilter::AllocateBloomFilter() {
  if (filter_) {
    return;
  }
  filter_ = std::make_unique<FastRejectFilter>();
}

}  // namespace blink
