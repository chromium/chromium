// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_CONSTANTS_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_CONSTANTS_H_

#include <stdint.h>

#include "base/component_export.h"

namespace network::shared_dictionary {

// The default value (1 year) of expiration time in "use-as-dictionary"
// HTTP header.
constexpr int64_t kDefaultExpiration = 31536000;

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
