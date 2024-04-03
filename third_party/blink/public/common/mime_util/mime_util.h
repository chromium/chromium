// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MIME_UTIL_MIME_UTIL_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MIME_UTIL_MIME_UTIL_H_

#include <string>
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Check to see if a particular MIME type is in the list of
// supported/recognized MIME types.
bool BLINK_COMMON_EXPORT IsSupportedImageMimeType(const std::string& mime_type);
bool BLINK_COMMON_EXPORT
IsSupportedNonImageMimeType(const std::string& mime_type);
bool BLINK_COMMON_EXPORT
IsUnsupportedTextMimeType(const std::string& mime_type);
bool BLINK_COMMON_EXPORT
IsSupportedJavascriptMimeType(const std::string& mime_type);
bool BLINK_COMMON_EXPORT IsJSONMimeType(const std::string& mime_type);

// Convenience function.
bool BLINK_COMMON_EXPORT IsSupportedMimeType(const std::string& mime_type);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MIME_UTIL_MIME_UTIL_H_
