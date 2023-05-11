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

// The default value (1 year) of expiration time in "use-as-dictionary"
// HTTP header.
constexpr base::TimeDelta kDefaultExpiration = base::Seconds(31536000);

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

// The dictionary option name of "expires".
COMPONENT_EXPORT(NETWORK_SERVICE) extern const char kOptionNameExpires[];

// The dictionary option name of "algorithms".
COMPONENT_EXPORT(NETWORK_SERVICE) extern const char kOptionNameAlgorithms[];

}  // namespace network::shared_dictionary

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_CONSTANTS_H_
