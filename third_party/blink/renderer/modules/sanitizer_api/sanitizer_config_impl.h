// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_CONFIG_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_CONFIG_IMPL_H_

#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class SanitizerConfig;

/**
 * Helper structure for our Sanitizer implementation.
 *
 * The SanitizerConfig (defined in santizer_config.idl) defined the
 * configuration of a Sanitizer instance, as required by the spec and
 * JavaScript. This defines an equivalent class, which is meant to contain the
 * same information but retain it in a fashion more suitable for processing,
 * e.g. in HashSet<String> rather then Vector<String>.
 */
struct SanitizerConfigImpl {
  // These members store the information from the original SanitizerConfig.
  HashSet<String> allow_elements_;
  HashSet<String> block_elements_;
  HashSet<String> drop_elements_;
  HashMap<String, Vector<String>> allow_attributes_;
  HashMap<String, Vector<String>> drop_attributes_;
  bool allow_custom_elements_;
  bool allow_comments_;

  // These members store whether the original SanitizerConfig had the
  // corresponding members set or not. This only serves to reconstruct the
  // SanitizerConfig* in the ToAPI method.
  bool had_allow_elements_;
  bool had_allow_attributes_;
  bool had_allow_custom_elements_;

  // Create a SantizerConfigImpl from a SanitizerConfig.
  // Will use the default config if it received nullptr.
  static SanitizerConfigImpl From(const SanitizerConfig*);

  // Create an IDL SanitizerConfig from this impl, for use in the config()
  // accessor.
  static SanitizerConfig* ToAPI(const SanitizerConfigImpl&);

  static SanitizerConfig* DefaultConfig();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_CONFIG_IMPL_H_
