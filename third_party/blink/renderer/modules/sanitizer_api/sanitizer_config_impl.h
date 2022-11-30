// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_CONFIG_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_CONFIG_IMPL_H_

#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Sanitizer configuration, in a form suitable for fast querying.
//
// The SanitizerConfig (defined in santizer_config.idl) defines the
// API-visible configuration of a Sanitizer instance, as required by the spec.
// This defines an equivalent class, which is meant to contain the same
// information but retain it in a fashion more suitable for processing,
// e.g. in HashSet<String> rather then Vector<String>.
//
// Names are represented as Strings with fixed namespace prefixes, as in the
// Sanitizer spec. (E.g. "svg:svg", but prefix-less for HTML, "span".)

struct SanitizerConfigImpl {
  typedef HashSet<String> ElementList;
  typedef HashMap<String, ElementList> AttributeList;

  SanitizerConfigImpl() = default;
  ~SanitizerConfigImpl() = default;

  // These members store the information from the original SanitizerConfig.
  ElementList allow_elements_;
  ElementList block_elements_;
  ElementList drop_elements_;
  AttributeList allow_attributes_;
  AttributeList drop_attributes_;
  bool allow_custom_elements_;
  bool allow_unknown_markup_;
  bool allow_comments_;

  // These members store whether the original SanitizerConfig had the
  // corresponding members set or not. This only serves to reconstruct the
  // SanitizerConfig* in the ToAPI method.
  bool had_allow_elements_;
  bool had_allow_attributes_;
  bool had_allow_custom_elements_;
  bool had_allow_unknown_markup_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_CONFIG_IMPL_H_
