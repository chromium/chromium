// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mime_util/mime_util.h"

#include <stddef.h>

#include <unordered_set>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/buildflags.h"

#if !BUILDFLAG(IS_IOS)
// iOS doesn't use and must not depend on //media
#include "media/base/mime_util.h"
#endif

namespace blink {

namespace {

// From WebKit's WebCore/platform/MIMETypeRegistry.cpp:

constexpr auto kSupportedImageTypes = base::MakeFixedFlatSet<std::string_view>({
    "image/jpeg",
    "image/pjpeg",
    "image/jpg",
    "image/webp",
    "image/png",
    "image/apng",
    "image/gif",
    "image/bmp",
    "image/vnd.microsoft.icon",  // ico
    "image/x-icon",              // ico
    "image/x-xbitmap",           // xbm
    "image/x-png",
#if BUILDFLAG(ENABLE_AV1_DECODER)
    "image/avif",
#endif
});

//  Support every script type mentioned in the spec, as it notes that "User
//  agents must recognize all JavaScript MIME types." See
//  https://html.spec.whatwg.org/#javascript-mime-type.
constexpr auto kSupportedJavascriptTypes =
    base::MakeFixedFlatSet<std::string_view>({
        "application/ecmascript",
        "application/javascript",
        "application/x-ecmascript",
        "application/x-javascript",
        "text/ecmascript",
        "text/javascript",
        "text/javascript1.0",
        "text/javascript1.1",
        "text/javascript1.2",
        "text/javascript1.3",
        "text/javascript1.4",
        "text/javascript1.5",
        "text/jscript",
        "text/livescript",
        "text/x-ecmascript",
        "text/x-javascript",
    });

// These types are excluded from the logic that allows all text/ types because
// while they are technically text, it's very unlikely that a user expects to
// see them rendered in text form.
constexpr auto kUnsupportedTextTypes =
    base::MakeFixedFlatSet<std::string_view>({
        "text/calendar",
        "text/x-calendar",
        "text/x-vcalendar",
        "text/vcalendar",
        "text/vcard",
        "text/x-vcard",
        "text/directory",
        "text/ldif",
        "text/qif",
        "text/x-qif",
        "text/x-csv",
        "text/x-vcf",
        "text/rtf",
        "text/comma-separated-values",
        "text/csv",
        "text/tab-separated-values",
        "text/tsv",
        "text/ofx",                          // https://crbug.com/162238
        "text/vnd.sun.j2me.app-descriptor",  // https://crbug.com/176450
        "text/x-ms-iqy",                     // https://crbug.com/1054863
        "text/x-ms-odc",                     // https://crbug.com/1054863
        "text/x-ms-rqy",                     // https://crbug.com/1054863
        "text/x-ms-contact"                  // https://crbug.com/1054863
    });

// Note:
// - does not include javascript types list (see supported_javascript_types)
// - does not include types starting with "text/" (see
//   IsSupportedNonImageMimeType())
constexpr auto kSupportedNonImageTypes =
    base::MakeFixedFlatSet<std::string_view>({
        "image/svg+xml",  // SVG is text-based XML, even though it has an image/
                          // type
        "application/xml", "application/atom+xml", "application/rss+xml",
        "application/xhtml+xml", "application/json",
        "message/rfc822",     // For MHTML support.
        "multipart/related",  // For MHTML support.
        "multipart/x-mixed-replace"
        // Note: ADDING a new type here will probably render it AS HTML. This
        // can result in cross site scripting.
    });

}  // namespace

bool IsSupportedImageMimeType(std::string_view mime_type) {
  return kSupportedImageTypes.contains(base::ToLowerASCII(mime_type));
}

bool IsSupportedNonImageMimeType(std::string_view mime_type) {
  std::string mime_lower = base::ToLowerASCII(mime_type);
  return kSupportedNonImageTypes.contains(mime_lower) ||
         kSupportedJavascriptTypes.contains(mime_lower) ||
#if !BUILDFLAG(IS_IOS)
         media::IsSupportedMediaMimeType(mime_lower) ||
#endif
         (base::StartsWith(mime_lower, "text/") &&
          !kUnsupportedTextTypes.contains(mime_lower)) ||
         (base::StartsWith(mime_lower, "application/") &&
          net::MatchesMimeType("application/*+json", mime_lower));
}

bool IsUnsupportedTextMimeType(std::string_view mime_type) {
  return kUnsupportedTextTypes.contains(base::ToLowerASCII(mime_type));
}

bool IsSupportedJavascriptMimeType(std::string_view mime_type) {
  return kSupportedJavascriptTypes.contains(mime_type);
}

// TODO(crbug.com/362282752): Allow non-application `*/*+json` MIME types.
// https://mimesniff.spec.whatwg.org/#json-mime-type
bool IsJSONMimeType(std::string_view mime_type) {
  return net::MatchesMimeType("application/json", mime_type) ||
         net::MatchesMimeType("text/json", mime_type) ||
         net::MatchesMimeType("application/*+json", mime_type);
}

// TODO(crbug.com/362282752): Allow other `*/*+xml` MIME types.
// https://mimesniff.spec.whatwg.org/#xml-mime-type
bool IsXMLMimeType(std::string_view mime_type) {
  return net::MatchesMimeType("text/xml", mime_type) ||
         net::MatchesMimeType("application/xml", mime_type) ||
         net::MatchesMimeType("application/*+xml", mime_type);
}

// From step 3 of
// https://mimesniff.spec.whatwg.org/#minimize-a-supported-mime-type.
bool IsSVGMimeType(std::string_view mime_type) {
  return net::MatchesMimeType("image/svg+xml", mime_type);
}

bool IsSupportedMimeType(std::string_view mime_type) {
  return (base::StartsWith(mime_type, "image/",
                           base::CompareCase::INSENSITIVE_ASCII) &&
          IsSupportedImageMimeType(mime_type)) ||
         IsSupportedNonImageMimeType(mime_type);
}

}  // namespace blink
