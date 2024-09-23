// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_CONSTANTS_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_CONSTANTS_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"

namespace network::shared_dictionary {

// The max expiration time (30 days) for Origin Trial. This is used when
// CompressionDictionaryTransport feature is disabled in the network service.
// TODO(crbug.com/40255884): Remove this after the Origin Trial experiment.
constexpr base::TimeDelta kMaxExpirationForOriginTrial = base::Days(30);

// The total dictionary count limit per NetworkContext.
constexpr uint64_t kDictionaryMaxCountPerNetworkContext = 1000u;

// The size limit of a shared dictionary.
size_t GetDictionarySizeLimit();

// Changes the size limit of a shared dictionary, and returns a
// ScopedClosureRunner which will reset the size limit in the destructor.
COMPONENT_EXPORT(NETWORK_SERVICE)
base::ScopedClosureRunner SetDictionarySizeLimitForTesting(
    size_t dictionary_size_limit);

// The header name of "use-as-dictionary".
COMPONENT_EXPORT(NETWORK_SERVICE)
extern const char kUseAsDictionaryHeaderName[];

// The dictionary option name of "match".
COMPONENT_EXPORT(NETWORK_SERVICE) extern const char kOptionNameMatch[];

// The dictionary option name of "match-dest".
COMPONENT_EXPORT(NETWORK_SERVICE) extern const char kOptionNameMatchDest[];

// The dictionary option name of "type".
COMPONENT_EXPORT(NETWORK_SERVICE) extern const char kOptionNameType[];

// The dictionary option name of "id".
COMPONENT_EXPORT(NETWORK_SERVICE) extern const char kOptionNameId[];
// The max length of dictionary id.
inline constexpr uint64_t kDictionaryIdMaxLength = 1024;

}  // namespace network::shared_dictionary

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_CONSTANTS_H_
