// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mime_util/mime_util.h"

#include <stddef.h>
#include <unordered_set>

#include "base/lazy_instance.h"
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

const char* const kSupportedImageTypes[] = {
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
};

//  Support every script type mentioned in the spec, as it notes that "User
//  agents must recognize all JavaScript MIME types." See
//  https://html.spec.whatwg.org/#javascript-mime-type.
const char* const kSupportedJavascriptTypes[] = {
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
};

// These types are excluded from the logic that allows all text/ types because
// while they are technically text, it's very unlikely that a user expects to
// see them rendered in text form.
static const char* const kUnsupportedTextTypes[] = {
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
};

// Note:
// - does not include javascript types list (see supported_javascript_types)
// - does not include types starting with "text/" (see
//   IsSupportedNonImageMimeType())
static const char* const kSupportedNonImageTypes[] = {
    "image/svg+xml",  // SVG is text-based XML, even though it has an image/
                      // type
    "application/xml", "application/atom+xml", "application/rss+xml",
    "application/xhtml+xml", "application/json",
    "message/rfc822",     // For MHTML support.
    "multipart/related",  // For MHTML support.
    "multipart/x-mixed-replace"
    // Note: ADDING a new type here will probably render it AS HTML. This can
    // result in cross site scripting.
};

// Singleton utility class for mime types
class MimeUtil {
 public:
  MimeUtil(const MimeUtil&) = delete;
  MimeUtil& operator=(const MimeUtil&) = delete;

  bool IsSupportedImageMimeType(const std::string& mime_type) const;
  bool IsSupportedNonImageMimeType(const std::string& mime_type) const;
  bool IsUnsupportedTextMimeType(const std::string& mime_type) const;
  bool IsSupportedJavascriptMimeType(const std::string& mime_type) const;
  bool IsJSONMimeType(const std::string&) const;

  bool IsSupportedMimeType(const std::string& mime_type) const;

 private:
  friend struct base::LazyInstanceTraitsBase<MimeUtil>;

  using MimeTypes = std::unordered_set<std::string>;

  MimeUtil();

  MimeTypes image_types_;
  MimeTypes non_image_types_;
  MimeTypes unsupported_text_types_;
  MimeTypes javascript_types_;
};

MimeUtil::MimeUtil() {
  for (const char* type : kSupportedNonImageTypes)
    non_image_types_.insert(type);
  for (const char* type : kSupportedImageTypes)
    image_types_.insert(type);
  for (const char* type : kUnsupportedTextTypes)
    unsupported_text_types_.insert(type);
  for (const char* type : kSupportedJavascriptTypes) {
    javascript_types_.insert(type);
    non_image_types_.insert(type);
  }
}

bool MimeUtil::IsSupportedImageMimeType(const std::string& mime_type) const {
  return image_types_.find(base::ToLowerASCII(mime_type)) != image_types_.end();
}

bool MimeUtil::IsSupportedNonImageMimeType(const std::string& mime_type) const {
  return non_image_types_.find(base::ToLowerASCII(mime_type)) !=
             non_image_types_.end() ||
#if !BUILDFLAG(IS_IOS)
         media::IsSupportedMediaMimeType(mime_type) ||
#endif
         (base::StartsWith(mime_type, "text/",
                           base::CompareCase::INSENSITIVE_ASCII) &&
          !IsUnsupportedTextMimeType(mime_type)) ||
         (base::StartsWith(mime_type, "application/",
                           base::CompareCase::INSENSITIVE_ASCII) &&
          net::MatchesMimeType("application/*+json", mime_type));
}

bool MimeUtil::IsUnsupportedTextMimeType(const std::string& mime_type) const {
  return unsupported_text_types_.find(base::ToLowerASCII(mime_type)) !=
         unsupported_text_types_.end();
}

bool MimeUtil::IsSupportedJavascriptMimeType(
    const std::string& mime_type) const {
  return javascript_types_.find(mime_type) != javascript_types_.end();
}

// TODO(sasebree): Allow non-application `*/*+json` MIME types.
// https://mimesniff.spec.whatwg.org/#json-mime-type
bool MimeUtil::IsJSONMimeType(const std::string& mime_type) const {
  return net::MatchesMimeType("application/json", mime_type) ||
         net::MatchesMimeType("text/json", mime_type) ||
         net::MatchesMimeType("application/*+json", mime_type);
}

bool MimeUtil::IsSupportedMimeType(const std::string& mime_type) const {
  return (base::StartsWith(mime_type, "image/",
                           base::CompareCase::INSENSITIVE_ASCII) &&
          IsSupportedImageMimeType(mime_type)) ||
         IsSupportedNonImageMimeType(mime_type);
}

// This variable is Leaky because it is accessed from WorkerPool threads.
static base::LazyInstance<MimeUtil>::Leaky g_mime_util =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool IsSupportedImageMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedImageMimeType(mime_type);
}

bool IsSupportedNonImageMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedNonImageMimeType(mime_type);
}

bool IsUnsupportedTextMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsUnsupportedTextMimeType(mime_type);
}

bool IsSupportedJavascriptMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedJavascriptMimeType(mime_type);
}

bool IsJSONMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsJSONMimeType(mime_type);
}

bool IsSupportedMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedMimeType(mime_type);
}

}  // namespace blink
