// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SHARED_DICTIONARY_SHARED_DICTIONARY_CONSTANTS_H_
#define NET_SHARED_DICTIONARY_SHARED_DICTIONARY_CONSTANTS_H_

namespace net::shared_dictionary {

// The content encoding name of Dictionary-Compressed Brotli.
static constexpr char kSharedBrotliContentEncodingName[] = "dcb";

// The content encoding name of Dictionary-Compressed Zstandard.
static constexpr char kSharedZstdContentEncodingName[] = "dcz";

// The header name of "available-dictionary".
static constexpr char kAvailableDictionaryHeaderName[] = "available-dictionary";

}  // namespace net::shared_dictionary

#endif  // NET_SHARED_DICTIONARY_SHARED_DICTIONARY_CONSTANTS_H_
