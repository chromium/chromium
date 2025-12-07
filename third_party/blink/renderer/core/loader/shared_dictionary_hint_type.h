// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SHARED_DICTIONARY_HINT_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SHARED_DICTIONARY_HINT_TYPE_H_

namespace blink {

// Used for UMA. Logged to "Blink.SharedDictionary.Hint.Discovery".
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SharedDictionaryHintType)
enum class SharedDictionaryHintType {
  kHtmlLinkTag = 0,
  kHttpHeader = 1,
  kMaxValue = kHttpHeader,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:SharedDictionaryHintType)
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SHARED_DICTIONARY_HINT_TYPE_H_
