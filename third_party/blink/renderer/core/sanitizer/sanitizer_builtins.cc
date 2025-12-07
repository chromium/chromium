// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/core/sanitizer/sanitizer_builtins.h"

#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

const Sanitizer* SanitizerBuiltins::GetDefaultUnsafe() {
  DEFINE_STATIC_LOCAL(Persistent<Sanitizer>, default_unsafe_,
                      (Sanitizer::CreateEmpty()));
  return default_unsafe_.Get();
}

const Sanitizer* SanitizerBuiltins::GetDefaultSafe() {
  DEFINE_STATIC_LOCAL(
      Persistent<Sanitizer>, default_safe_,
      (blink::sanitizer_generated_builtins::BuildDefaultConfig()));
  return default_safe_.Get();
}

const Sanitizer* SanitizerBuiltins::GetBaseline() {
  DEFINE_STATIC_LOCAL(
      Persistent<Sanitizer>, baseline_,
      (blink::sanitizer_generated_builtins::BuildBaselineConfig()));
  return baseline_.Get();
}

}  // namespace blink
