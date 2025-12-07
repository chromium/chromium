// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SHARED_DICTIONARY_SHARED_DICTIONARY_TRANSACTION_OUTCOME_H_
#define NET_SHARED_DICTIONARY_SHARED_DICTIONARY_TRANSACTION_OUTCOME_H_

namespace net {

// Used for UMA. Logged to "Net.SharedDictionary.Transaction.Outcome".
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SharedDictionaryTransactionOutcome)
enum class SharedDictionaryTransactionOutcome {
  kDictionaryUsedBrotli = 0,
  kDictionaryUsedZstandard = 1,
  kDictionaryNotUsed = 2,
  kMaxValue = kDictionaryNotUsed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:SharedDictionaryTransactionOutcome)

}  // namespace net

#endif  // NET_SHARED_DICTIONARY_SHARED_DICTIONARY_TRANSACTION_OUTCOME_H_
