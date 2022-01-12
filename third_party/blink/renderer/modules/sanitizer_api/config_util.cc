// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sanitizer_api/config_util.h"

#include "third_party/blink/renderer/modules/sanitizer_api/builtins.h"
#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer_config_impl.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

SanitizerConfigImpl FromAPI(const SanitizerConfig* config) {
  if (!config) {
    return GetDefaultConfig();
  }

  SanitizerConfigImpl impl;

  impl.allow_custom_elements_ =
      config->hasAllowCustomElements() && config->allowCustomElements();
  impl.allow_comments_ = config->hasAllowComments() && config->allowComments();

  // Format dropElements to lower case.
  if (config->hasDropElements()) {
    impl.drop_elements_ = FromAPI(config->dropElements());
  }

  // Format blockElements to lower case.
  if (config->hasBlockElements()) {
    impl.block_elements_ = FromAPI(config->blockElements());
  }

  // Format allowElements to lower case.
  if (config->hasAllowElements()) {
    impl.allow_elements_ = FromAPI(config->allowElements());
  } else {
    impl.allow_elements_ = GetDefaultConfig().allow_elements_;
  }

  // Format dropAttributes to lowercase.
  if (config->hasDropAttributes()) {
    impl.drop_attributes_ = FromAPI(config->dropAttributes());
  }

  // Format allowAttributes to lowercase.
  if (config->hasAllowAttributes()) {
    impl.allow_attributes_ = FromAPI(config->allowAttributes());
  } else {
    impl.allow_attributes_ = GetDefaultConfig().allow_attributes_;
  }

  impl.had_allow_elements_ = config->hasAllowElements();
  impl.had_allow_attributes_ = config->hasAllowAttributes();
  impl.had_allow_custom_elements_ = config->hasAllowCustomElements();

  return impl;
}

SanitizerConfigImpl::ElementList FromAPI(const Vector<String>& elements) {
  SanitizerConfigImpl::ElementList result;
  for (const String& element : elements) {
    const auto name = ElementFromAPI(element);
    if (!IsInvalid(name))
      result.insert(name);
  }
  return result;
}

SanitizerConfigImpl::AttributeList FromAPI(
    const Vector<std::pair<String, Vector<String>>>& attrs) {
  SanitizerConfigImpl::AttributeList result;
  for (const std::pair<String, Vector<String>>& pair : attrs) {
    const auto attr = AttributeOrWildcardFromAPI(pair.first);
    if (!IsInvalid(attr)) {
      result.insert(attr, pair.second.Contains("*") ? WildcardList()
                                                    : FromAPI(pair.second));
    }
  }
  return result;
}

String ElementFromAPI(const String& name) {
  // Check well-formed-ness.
  // TODO(vogelheim): Sync well-formedness with Sanitizer spec
  //     The HTML spec is a bit obtuse, but it seems names can be any allowed
  //     XML names. The HTML parser however allows only ascii. Here, we settle
  //     for the simplest variant and limit ourselves to an ascii check.
  //     We should likely _also_ check for other characters (e.g. control,
  //     whitespace, etc.)
  if (name == "*" || name.IsEmpty() || !name.ContainsOnlyASCIIOrEmpty())
    return Invalid();

  // Normalize mixed-case names:
  return name.LowerASCII();
}

String AttributeFromAPI(const String& name) {
  // See ElementFromAPI for the name check.
  if (name.IsEmpty() || !name.ContainsOnlyASCIIOrEmpty())
    return Invalid();
  return name.LowerASCII();
}

String AttributeOrWildcardFromAPI(const String& name) {
  return (name == "*") ? Wildcard() : AttributeFromAPI(name);
}

SanitizerConfig* ToAPI(const SanitizerConfigImpl& impl) {
  SanitizerConfig* config = SanitizerConfig::Create();

  if (impl.had_allow_elements_) {
    config->setAllowElements(ToAPI(impl.allow_elements_));
  }

  if (!impl.drop_elements_.IsEmpty()) {
    config->setDropElements(ToAPI(impl.drop_elements_));
  }

  if (!impl.block_elements_.IsEmpty()) {
    config->setBlockElements(ToAPI(impl.block_elements_));
  }

  if (impl.had_allow_attributes_) {
    config->setAllowAttributes(ToAPI(impl.allow_attributes_));
  }

  if (!impl.drop_attributes_.IsEmpty()) {
    config->setDropAttributes(ToAPI(impl.drop_attributes_));
  }

  if (impl.had_allow_custom_elements_)
    config->setAllowCustomElements(impl.allow_custom_elements_);
  return config;
}

String ToAPI(const String& name) {
  return name;
}

Vector<String> ToAPI(const SanitizerConfigImpl::ElementList& set) {
  Vector<String> result;
  for (const auto& element : set)
    result.push_back(ToAPI(element));
  return result;
}

Vector<std::pair<String, Vector<String>>> ToAPI(
    const SanitizerConfigImpl::AttributeList& attr_list) {
  Vector<std::pair<String, Vector<String>>> result;
  for (const auto& item : attr_list) {
    result.push_back(std::make_pair(ToAPI(item.key), ToAPI(item.value)));
  }
  return result;
}

String Wildcard() {
  return String("*");
}

String Invalid() {
  return String();
}

SanitizerConfigImpl::ElementList WildcardList() {
  return {Wildcard()};
}

bool IsWildcard(const String& name) {
  return name == "*";
}

bool IsInvalid(const String& name) {
  return name.IsNull();
}

bool IsWildcardList(const SanitizerConfigImpl::ElementList& list) {
  // ElementList construction should ensure that a wildcard list contains only
  // the wildcard, and we don't have mixed lists with proper element names and
  // wildcards in them.
  DCHECK(!list.Contains(Wildcard()) || list.size() == 1);
  return list.size() == 1 && IsWildcard(*list.begin());
}

bool Match(const String& element_name,
           const SanitizerConfigImpl::ElementList& elements) {
  // We only match against actual element names, not against "*" or empty
  // strings.
  DCHECK(!IsWildcard(element_name));
  DCHECK(!IsInvalid(element_name));
  return elements.Contains(element_name) || IsWildcardList(elements);
}

bool Match(const String& attribute_name,
           const String& element_name,
           const SanitizerConfigImpl::AttributeList& attributes) {
  const auto iter = attributes.find(attribute_name);
  return iter != attributes.end() && Match(element_name, iter->value);
}

}  // namespace blink
