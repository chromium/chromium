// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_V8_COMPILE_HINTS_HISTOGRAMS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_V8_COMPILE_HINTS_HISTOGRAMS_H_

namespace blink::v8_compile_hints {

inline constexpr const char* kStatusHistogram =
    "WebCore.Scripts.V8CompileHintsStatus";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Status {
  kConsumeLocalCompileHintsClassicNonStreaming = 0,
  kConsumeLocalCompileHintsModuleNonStreaming = 1,
  kConsumeCrowdsourcedCompileHintsClassicNonStreaming = 2,
  kConsumeCrowdsourcedCompileHintsModuleNonStreaming = 3,
  kProduceCompileHintsClassicNonStreaming = 4,
  kProduceCompileHintsModuleNonStreaming = 5,
  kConsumeCodeCacheClassicNonStreaming = 6,
  kConsumeCodeCacheModuleNonStreaming = 7,
  kConsumeLocalCompileHintsStreaming = 8,
  kConsumeCrowdsourcedCompileHintsStreaming = 9,
  kProduceCompileHintsStreaming = 10,
  kMaxValue = kProduceCompileHintsStreaming
};

}  // namespace blink::v8_compile_hints

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_V8_COMPILE_HINTS_HISTOGRAMS_H_
