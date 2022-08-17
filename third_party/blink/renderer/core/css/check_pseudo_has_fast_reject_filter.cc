// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/check_pseudo_has_fast_reject_filter.h"

#include "third_party/blink/renderer/core/css/css_selector.h"

namespace blink {

namespace {

// Salt to separate otherwise identical string hashes so a class-selector like
// .article won't match <article> elements.
enum {
  // Primitive identifier hashes.
  kTagNameSalt = 13,
  kIdSalt = 17,
  kClassSalt = 19,
  kAttributeSalt = 23,

  // Salts for pseudo class compounding conditions.
  // For pseudo classes whose state can be changed frequently, such as :hover,
  // use additional hashes of compounding conditions to make the fast reject
  // filter more accurate.
  // We can get the compounding condition hashes by multiplying the each
  // primitive identifier hash by the salt value for the pseudo class.
  // Please note that, adding additional compounding condition salt has a risk
  // of reducing the accuracy of the filter by increasing the number of hashes
  // added to the bloom filter.
  kHoverCompoundingSalt = 101
};

inline bool IsExcludedAttribute(const AtomicString& name) {
  return name == html_names::kClassAttr.LocalName() ||
         name == html_names::kIdAttr.LocalName() ||
         name == html_names::kStyleAttr.LocalName();
}

inline unsigned GetTagHash(const AtomicString& tag_name) {
  return tag_name.Impl()->ExistingHash() * kTagNameSalt;
}

inline unsigned GetClassHash(const AtomicString& class_name) {
  return class_name.Impl()->ExistingHash() * kClassSalt;
}

inline unsigned GetIdHash(const AtomicString& id) {
  return id.Impl()->ExistingHash() * kIdSalt;
}

inline unsigned GetAttributeHash(const AtomicString& attribute_name) {
  return attribute_name.Impl()->ExistingHash() * kAttributeSalt;
}

inline unsigned GetHoverCompoundedHash(unsigned hash) {
  return hash * kHoverCompoundingSalt;
}

void AddHoverCompoundedPseudoHasArgumentHash(
    Vector<unsigned>& pseudo_has_argument_hashes,
    unsigned hash) {
  pseudo_has_argument_hashes.push_back(GetHoverCompoundedHash(hash));
}

void AddPseudoHasArgumentHash(Vector<unsigned>& pseudo_has_argument_hashes,
                              unsigned hash) {
  pseudo_has_argument_hashes.push_back(hash);
}

void AddHashToFilter(CheckPseudoHasFastRejectFilter::FastRejectFilter& filter,
                     unsigned hash) {
  filter.Add(hash);
}

void AddHashAndHoverCompoundingHashToFilter(
    CheckPseudoHasFastRejectFilter::FastRejectFilter& filter,
    unsigned hash) {
  filter.Add(hash);
  filter.Add(GetHoverCompoundedHash(hash));
}

}  // namespace

void CheckPseudoHasFastRejectFilter::AddElementIdentifierHashes(
    const Element& element) {
  DCHECK(filter_.get());

  void (*add_hash)(FastRejectFilter&, unsigned) =
      element.IsHovered() ? AddHashAndHoverCompoundingHashToFilter
                          : AddHashToFilter;

  add_hash(*filter_, GetTagHash(element.LocalNameForSelectorMatching()));

  if (element.HasID())
    add_hash(*filter_, GetIdHash(element.IdForStyleResolution()));

  if (element.HasClass()) {
    const SpaceSplitString& class_names = element.ClassNames();
    wtf_size_t count = class_names.size();
    for (wtf_size_t i = 0; i < count; ++i)
      add_hash(*filter_, GetClassHash(class_names[i]));
  }
  AttributeCollection attributes = element.AttributesWithoutUpdate();
  for (const auto& attribute_item : attributes) {
    auto attribute_name = attribute_item.LocalName();
    if (IsExcludedAttribute(attribute_name))
      continue;
    auto lower = attribute_name.IsLowerASCII() ? attribute_name
                                               : attribute_name.LowerASCII();
    add_hash(*filter_, GetAttributeHash(lower));
  }
}

bool CheckPseudoHasFastRejectFilter::FastReject(
    const Vector<unsigned>& pseudo_has_argument_hashes) const {
  DCHECK(filter_.get());
  if (pseudo_has_argument_hashes.IsEmpty())
    return false;
  for (unsigned hash : pseudo_has_argument_hashes) {
    if (!filter_->MayContain(hash))
      return true;
  }
  return false;
}

namespace {

inline unsigned GetPseudoHasArgumentHash(const CSSSelector* simple_selector) {
  DCHECK(simple_selector);
  switch (simple_selector->Match()) {
    case CSSSelector::kId:
      if (simple_selector->Value().IsEmpty())
        break;
      return GetIdHash(simple_selector->Value());
    case CSSSelector::kClass:
      if (simple_selector->Value().IsEmpty())
        break;
      return GetClassHash(simple_selector->Value());
    case CSSSelector::kTag:
      if (simple_selector->TagQName().LocalName() !=
          CSSSelector::UniversalSelectorAtom()) {
        return GetTagHash(simple_selector->TagQName().LocalName());
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
      if (IsExcludedAttribute(attribute_name))
        break;
      auto lower_name = attribute_name.IsLowerASCII()
                            ? attribute_name
                            : attribute_name.LowerASCII();
      return GetAttributeHash(lower_name);
    }
    default:
      break;
  }
  return 0;
}

}  // namespace

// static
void CheckPseudoHasFastRejectFilter::CollectPseudoHasArgumentHashesFromCompound(
    Vector<unsigned>& pseudo_has_argument_hashes,
    const CSSSelector* compound_selector,
    CompoundContext& compound_context) {
  void (*add_hash)(Vector<unsigned>&, unsigned hash) =
      compound_context.contains_hover ? AddHoverCompoundedPseudoHasArgumentHash
                                      : AddPseudoHasArgumentHash;
  for (const CSSSelector* simple_selector = compound_selector; simple_selector;
       simple_selector = simple_selector->TagHistory()) {
    if (unsigned hash = GetPseudoHasArgumentHash(simple_selector))
      add_hash(pseudo_has_argument_hashes, hash);

    if (simple_selector->Relation() != CSSSelector::kSubSelector)
      break;
  }
}

void CheckPseudoHasFastRejectFilter::AllocateBloomFilter() {
  if (filter_)
    return;
  filter_ = std::make_unique<FastRejectFilter>();
}

}  // namespace blink
