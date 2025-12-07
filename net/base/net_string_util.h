// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_STRING_UTIL_H__
#define NET_BASE_NET_STRING_UTIL_H__

#include <string>
#include <string_view>

#include "net/base/net_export.h"

// String conversion functions.  By default, they're implemented with ICU, but
// when building with USE_ICU_ALTERNATIVES, they use platform functions instead.

namespace net {

extern const char* const kCharsetLatin1;

// Converts |text| using |charset| to UTF-8, and writes it to |output|.
// On failure, returns false and |output| is cleared.
bool ConvertToUtf8(std::string_view text,
                   const char* charset,
                   std::string* output);

// Converts |text| using |charset| to UTF-8, normalizes the result, and writes
// it to |output|.  On failure, returns false and |output| is cleared.
bool ConvertToUtf8AndNormalize(std::string_view text,
                               const char* charset,
                               std::string* output);

// Converts |text| using |charset| to UTF-16, and writes it to |output|.
// On failure, returns false and |output| is cleared.
bool ConvertToUTF16(std::string_view text,
                    const char* charset,
                    std::u16string* output);

// Converts |text| using |charset| to UTF-16, and writes it to |output|.
// Any characters that can not be converted are replaced with U+FFFD.
bool ConvertToUTF16WithSubstitutions(std::string_view text,
                                     const char* charset,
                                     std::u16string* output);

// Converts |str| to uppercase using the default locale, and writes it to
// |output|. On failure returns false and |output| is cleared.
NET_EXPORT_PRIVATE bool ToUpperUsingLocale(std::u16string_view str,
                                           std::u16string* output);

}  // namespace net

#endif  // NET_BASE_NET_STRING_UTIL_H__
