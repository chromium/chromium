// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_DOCUMENT_REQUEST_METADATA_RESULT_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_DOCUMENT_REQUEST_METADATA_RESULT_H_

namespace network {

// Used for UMA. Logged to
// "Network.SharedDictionary.DocumentRequestMetadataResult". These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
//
// LINT.IfChange(SharedDictionaryDocumentRequestMetadataResult)
enum class SharedDictionaryDocumentRequestMetadataResult {
  kMetadataReady = 0,
  kMetadataPending = 1,
  kMaxValue = kMetadataPending,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/network/enums.xml:SharedDictionaryDocumentRequestMetadataResult)

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_DOCUMENT_REQUEST_METADATA_RESULT_H_
