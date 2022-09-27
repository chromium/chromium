// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_CONFIG_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_CONFIG_UTIL_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_sanitizer_config.h"
#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer_config_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

// Helper functions for the Sanitizer config.
//
// The API representation of the Sanitizer configuration is defined by the
// sanitizer_config.idl file. The internal representation - which uses more
// suitable data structures for fast querying - is defined in
// sanitizer_config_impl.h.
//
// This file provides a number of low-level routines to:
// - Convert between API and internal config representation.
// - Matching element or attributes against a config list/set/dictionary.

namespace blink {

// Create internal sanitizer config representation from API representation.
SanitizerConfigImpl FromAPI(const SanitizerConfig*);
SanitizerConfigImpl::ElementList FromAPI(const Vector<String>& elements);
SanitizerConfigImpl::AttributeList FromAPI(
    const Vector<std::pair<String, Vector<String>>>& attributes);

// For names, we're more particular about what kind of name we want.
String ElementFromAPI(const String&);
String AttributeFromAPI(const String&);
String AttributeOrWildcardFromAPI(const String&);

// Create the API sanitizer config from the internal representation.
SanitizerConfig* ToAPI(const SanitizerConfigImpl&);
Vector<String> ToAPI(const SanitizerConfigImpl::ElementList&);
Vector<std::pair<String, Vector<String>>> ToAPI(
    const SanitizerConfigImpl::AttributeList&);
String ToAPI(const String&, bool is_element);

// Access to a few useful constants.
String Wildcard();
String Invalid();
SanitizerConfigImpl::ElementList WildcardList();
bool IsWildcard(const String&);
bool IsInvalid(const String&);
bool IsWildcardList(const SanitizerConfigImpl::ElementList&);

// Match against a config item.
bool Match(const String& element_name, const SanitizerConfigImpl::ElementList&);
bool Match(const String& attribute_name,
           const String& element_name,
           const SanitizerConfigImpl::AttributeList&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_CONFIG_UTIL_H_
