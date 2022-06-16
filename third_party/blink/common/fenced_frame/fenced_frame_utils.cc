// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"

#include <cstring>

#include "base/guid.h"
#include "base/strings/string_util.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace blink {

bool IsValidFencedFrameURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  return (url.SchemeIs(url::kHttpsScheme) || url.IsAboutBlank() ||
          net::IsLocalhost(url)) &&
         !url.parsed_for_possibly_invalid_spec().potentially_dangling_markup;
}

const char kURNUUIDprefix[] = "urn:uuid:";

bool IsValidUrnUuidURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  const std::string& spec = url.spec();
  return base::StartsWith(spec, kURNUUIDprefix,
                          base::CompareCase::INSENSITIVE_ASCII) &&
         base::GUID::ParseCaseInsensitive(
             base::StringPiece(spec).substr(std::strlen(kURNUUIDprefix)))
             .is_valid();
}

}  // namespace blink
