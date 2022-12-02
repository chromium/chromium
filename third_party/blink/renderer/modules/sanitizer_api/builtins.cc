// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sanitizer_api/builtins.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/modules/sanitizer_api/builtins/sanitizer_builtins.h"
#include "third_party/blink/renderer/modules/sanitizer_api/config_util.h"
#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer_config_impl.h"

namespace blink {

namespace {

// A String->String map for mixed-case names.
//
// The DEFINE_STATIC_LOCAL macro - where we will use this - cannot use type
// names with a comma because the C++ pre-processor will consider those separate
// arguments. Thus we have this typedef as a work-around.
typedef HashMap<String, String> StringMap;

SanitizerConfigImpl::ElementList ElementsFromAPI(const char* const* elements) {
  SanitizerConfigImpl::ElementList element_list;
  for (const char* const* elem = elements; *elem; ++elem) {
    element_list.insert(*elem);
  }
  return element_list;
}

SanitizerConfigImpl::AttributeList AttributesFromAPI(
    const char* const* attributes) {
  SanitizerConfigImpl::ElementList wildcard_list;
  wildcard_list.insert(Wildcard());
  SanitizerConfigImpl::AttributeList attributes_list;
  for (const char* const* attr = attributes; *attr; ++attr) {
    attributes_list.insert(*attr, wildcard_list);
  }
  return attributes_list;
}

SanitizerConfigImpl BuildDefaultConfigImpl(const char* const* elements,
                                           const char* const* attributes) {
  SanitizerConfigImpl config;
  config.allow_elements_ = ElementsFromAPI(elements);
  config.allow_attributes_ = AttributesFromAPI(attributes);
  config.allow_custom_elements_ = false;
  config.allow_unknown_markup_ = false;
  config.allow_comments_ = false;
  config.had_allow_elements_ = true;
  config.had_allow_attributes_ = true;
  config.had_allow_custom_elements_ = true;
  config.had_allow_unknown_markup_ = true;
  return config;
}

}  // anonymous namespace

// To support two sets of baseline/default constants, we'll stick them into two
// c++ namespaces, so that the code mirrors each other, but we can still
// unambiguously refer to them.

namespace default_config_names {

const SanitizerConfigImpl& GetDefaultConfig() {
  DEFINE_STATIC_LOCAL(
      SanitizerConfigImpl, config_,
      (BuildDefaultConfigImpl(kDefaultElements, kDefaultAttributes)));
  return config_;
}

const SanitizerConfigImpl::ElementList& GetBaselineAllowElements() {
  DEFINE_STATIC_LOCAL(SanitizerConfigImpl::ElementList, elements_,
                      (ElementsFromAPI(kBaselineElements)));
  return elements_;
}

const SanitizerConfigImpl::AttributeList& GetBaselineAllowAttributes() {
  DEFINE_STATIC_LOCAL(SanitizerConfigImpl::AttributeList, attributes_,
                      (AttributesFromAPI(kBaselineAttributes)));
  return attributes_;
}

const SanitizerConfigImpl::AttributeList& GetKnownAttributes() {
  DEFINE_STATIC_LOCAL(SanitizerConfigImpl::AttributeList, attributes_,
                      (AttributesFromAPI(kKnownAttributes)));
  return attributes_;
}

}  // namespace default_config_names

// Now we'll implement the API functions, by "bouncing" to the corresponding
// C++ namespaced version.

const SanitizerConfigImpl& GetDefaultConfig() {
  return default_config_names::GetDefaultConfig();
}

const SanitizerConfigImpl::ElementList& GetBaselineAllowElements() {
  return default_config_names::GetBaselineAllowElements();
}

const SanitizerConfigImpl::AttributeList& GetBaselineAllowAttributes() {
  return default_config_names::GetBaselineAllowAttributes();
}

const SanitizerConfigImpl::AttributeList& GetKnownAttributes() {
  return default_config_names::GetKnownAttributes();
}

}  // namespace blink
