// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_BUILTINS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_BUILTINS_H_

#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer_config_impl.h"

// This file provides access to the Sanitizer built-ins.
//
// These are direct equivalents of the generated constants in
// builtins/sanitizer_builtins.h, but converted to the internal config
// representation.

namespace blink {

const SanitizerConfigImpl& GetDefaultConfig();
const SanitizerConfigImpl::ElementList& GetBaselineAllowElements();
const SanitizerConfigImpl::AttributeList& GetBaselineAllowAttributes();
const SanitizerConfigImpl::AttributeList& GetKnownAttributes();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_BUILTINS_H_
