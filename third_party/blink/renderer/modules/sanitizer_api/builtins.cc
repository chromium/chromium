// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sanitizer_api/builtins.h"

#include "third_party/blink/renderer/modules/sanitizer_api/builtins/sanitizer_builtins.h"
#include "third_party/blink/renderer/modules/sanitizer_api/config_util.h"
#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer_config_impl.h"

namespace blink {

namespace {

SanitizerConfigImpl::ElementList ElementsFromAPI(const char* const* elements) {
  SanitizerConfigImpl::ElementList element_list;
  for (const char* const* elem = elements; *elem; ++elem) {
    element_list.insert(ElementFromAPI(String(*elem)));
  }
  return element_list;
}

SanitizerConfigImpl::AttributeList AttributesFromAPI(
    const char* const* attributes) {
  SanitizerConfigImpl::ElementList wildcard_list;
  wildcard_list.insert(Wildcard());
  SanitizerConfigImpl::AttributeList attributes_list;
  for (const char* const* attr = attributes; *attr; ++attr) {
    attributes_list.insert(String(*attr), wildcard_list);
  }
  return attributes_list;
}

SanitizerConfigImpl BuildDefaultConfigImpl() {
  SanitizerConfigImpl config;
  config.allow_elements_ = ElementsFromAPI(kDefaultElements);
  config.allow_attributes_ = AttributesFromAPI(kDefaultAttributes);
  config.allow_custom_elements_ = false;
  config.allow_comments_ = false;
  config.had_allow_elements_ = true;
  config.had_allow_attributes_ = true;
  config.had_allow_custom_elements_ = true;
  return config;
}

}  // anonymous namespace

const SanitizerConfigImpl& GetDefaultConfig() {
  DEFINE_STATIC_LOCAL(SanitizerConfigImpl, config_, (BuildDefaultConfigImpl()));
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

}  // namespace blink
