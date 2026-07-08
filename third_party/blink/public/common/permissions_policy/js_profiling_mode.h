// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_JS_PROFILING_MODE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_JS_PROFILING_MODE_H_

namespace blink {

// Profiling mode for the js-profiling-mode Document Policy feature.
// Values must match the token mappings in document_policy_enum_values.h.
enum class JSProfilingMode {
  kNone = 0,
  kEager = 1,
  kLazy = 2,
  kMax = kLazy,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_JS_PROFILING_MODE_H_
