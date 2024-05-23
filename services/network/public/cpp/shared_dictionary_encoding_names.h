// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SHARED_DICTIONARY_ENCODING_NAMES_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SHARED_DICTIONARY_ENCODING_NAMES_H_

#include "base/component_export.h"

namespace network {

// Returns the content encoding name of Dictionary-Compressed Brotli: "dcb"
COMPONENT_EXPORT(NETWORK_CPP) const char* GetSharedBrotliContentEncodingName();

// Returns the content encoding name of Dictionary-Compressed Zstandard: "dcz".
COMPONENT_EXPORT(NETWORK_CPP) const char* GetSharedZstdContentEncodingName();

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SHARED_DICTIONARY_ENCODING_NAMES_H_
