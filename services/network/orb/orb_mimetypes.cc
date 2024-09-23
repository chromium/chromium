// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/orb/orb_mimetypes.h"

#include <stddef.h>

#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network::orb {

namespace {

// MIME types
const char kTextHtml[] = "text/html";
const char kTextXml[] = "text/xml";
const char kAppXml[] = "application/xml";
const char kAppJson[] = "application/json";
const char kImageSvg[] = "image/svg+xml";
const char kDashVideo[] = "application/dash+xml";  // https://crbug.com/947498
const char kTextJson[] = "text/json";
const char kTextPlain[] = "text/plain";

// Javascript MIME type suffixes for use in CORB protection logging. See also
// https://mimesniff.spec.whatwg.org/#javascript-mime-type.
const char* kJavaScriptSuffixes[] = {"ecmascript",
                                     "javascript",
                                     "x-ecmascript",
                                     "x-javascript",
                                     "javascript1.0",
                                     "javascript1.1",
                                     "javascript1.2",
                                     "javascript1.3",
                                     "javascript1.4",
                                     "javascript1.5",
                                     "jscript",
                                     "livescript",
                                     "js",
                                     "x-js"};

// TODO(lukasza): Remove kJsonProtobuf once this MIME type is not used in
// practice.  See also https://crbug.com/826756#c3
const char kJsonProtobuf[] = "application/json+protobuf";

// MIME type suffixes
const char kJsonSuffix[] = "+json";
const char kXmlSuffix[] = "+xml";

// The function below returns a set of MIME types below may be blocked by CORB
// without any confirmation sniffing (in contrast to HTML/JSON/XML which require
// confirmation sniffing because images, scripts, etc. are frequently
// mislabelled by http servers as HTML/JSON/XML).
//
// CORB cannot block images, scripts, stylesheets and other resources that the
// web standards allows to be fetched in `no-cors` mode.  CORB cannot block
// these resources even if they are not explicitly labeled with their type - in
// practice http servers may serve images as application/octet-stream or even as
// text/html.  OTOH, CORB *can* block all Content-Types that are very unlikely
// to represent images, scripts, stylesheets, etc. - such Content-Types are
// returned by GetNeverSniffedMimeTypes.
//
// Some of the Content-Types returned below might seem like a layering violation
// (e.g. why would //services/network care about application/zip or
// application/pdf or application/msword), but note that the decision to list a
// Content-Type below is not driven by whether the type is handled above or
// below //services/network layer.  Instead the decision to list a Content-Type
// below is driven by whether the Content-Type is unlikely to be attached to an
// image, script, stylesheet or other subresource type that web standards
// require to be fetched in `no-cors` mode.  In particular, CORB would still
// want to prevent cross-site disclosure of "application/msword" even if Chrome
// did not support this type (AFAIK today this support is only present on
// ChromeOS) in one of Chrome's many layers.  Similarly, CORB wants to prevent
// disclosure of "application/zip" even though Chrome doesn't have built-in
// support for this resource type.  And CORB also wants to protect
// "application/pdf" even though Chrome happens to support this resource type.
const auto& GetNeverSniffedMimeTypes() {
  static constexpr auto kNeverSniffedMimeTypes = base::MakeFixedFlatSet<
      std::string_view>({
      // clang-format off
      // The types below (zip, protobuf, etc.) are based on most commonly used
      // content types according to HTTP Archive - see:
      // https://github.com/whatwg/fetch/issues/860#issuecomment-457330454
      "application/gzip",
      "application/x-gzip",
      "application/x-protobuf",
      "application/zip",
      "text/event-stream",
      // The types listed below were initially taken from the list of types
      // handled by MimeHandlerView (although we would want to protect them even
      // if Chrome didn't support rendering these content types and/or if there
      // was no such thing as MimeHandlerView).
      "application/msexcel",
      "application/mspowerpoint",
      "application/msword",
      "application/msword-template",
      "application/pdf",
      "application/vnd.ces-quickpoint",
      "application/vnd.ces-quicksheet",
      "application/vnd.ces-quickword",
      "application/vnd.ms-excel",
      "application/vnd.ms-excel.sheet.macroenabled.12",
      "application/vnd.ms-powerpoint",
      "application/vnd.ms-powerpoint.presentation.macroenabled.12",
      "application/vnd.ms-word",
      "application/vnd.ms-word.document.12",
      "application/vnd.ms-word.document.macroenabled.12",
      "application/vnd.msword",
      "application/"
          "vnd.openxmlformats-officedocument.presentationml.presentation",
      "application/"
          "vnd.openxmlformats-officedocument.presentationml.template",
      "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
      "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
      "application/"
          "vnd.openxmlformats-officedocument.wordprocessingml.document",
      "application/"
          "vnd.openxmlformats-officedocument.wordprocessingml.template",
      "application/vnd.presentation-openxml",
      "application/vnd.presentation-openxmlm",
      "application/vnd.spreadsheet-openxml",
      "application/vnd.wordprocessing-openxml",
      "text/csv",
      // Block signed documents to protect (potentially sensitive) unencrypted
      // body of the signed document.  There should be no need to block
      // encrypted documents (e.g. `multipart/encrypted` nor
      // `application/pgp-encrypted`) and no need to block the signatures (e.g.
      // `application/pgp-signature`).
      "multipart/signed",
      // Block multipart responses because a protected type (e.g. JSON) can
      // become multipart if returned in a range request with multiple parts.
      // This is compatible with the web because the renderer can only see into
      // the result of a fetch for a multipart file when the request is made
      // with CORS. Media tags only make single-range requests which will not
      // have the multipart type.
      "multipart/byteranges",
      // TODO(lukasza): https://crbug.com/802836#c11: Add
      // application/signed-exchange.
      // clang-format on
  });

  // All items need to be lower-case, to support case-insensitive comparisons
  // later.
  DCHECK(base::ranges::all_of(kNeverSniffedMimeTypes, [](const auto& s) {
    return s == base::ToLowerASCII(s);
  }));

  return kNeverSniffedMimeTypes;
}

}  // namespace

bool IsJavascriptMimeType(std::string_view mime_type) {
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  for (const std::string& suffix : kJavaScriptSuffixes) {
    if (base::EndsWith(mime_type, suffix, kCaseInsensitive)) {
      return true;
    }
  }

  return false;
}

MimeType GetCanonicalMimeType(std::string_view mime_type) {
  // Checking for image/svg+xml and application/dash+xml early ensures that they
  // won't get classified as MimeType::kXml by the presence of the "+xml"
  // suffix.
  if (base::EqualsCaseInsensitiveASCII(mime_type, kImageSvg) ||
      base::EqualsCaseInsensitiveASCII(mime_type, kDashVideo)) {
    return MimeType::kOthers;
  }

  // See also https://mimesniff.spec.whatwg.org/#html-mime-type
  if (base::EqualsCaseInsensitiveASCII(mime_type, kTextHtml)) {
    return MimeType::kHtml;
  }

  // See also https://mimesniff.spec.whatwg.org/#json-mime-type
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  if (base::EqualsCaseInsensitiveASCII(mime_type, kAppJson) ||
      base::EqualsCaseInsensitiveASCII(mime_type, kTextJson) ||
      base::EqualsCaseInsensitiveASCII(mime_type, kJsonProtobuf) ||
      base::EndsWith(mime_type, kJsonSuffix, kCaseInsensitive)) {
    return MimeType::kJson;
  }

  // See also https://mimesniff.spec.whatwg.org/#xml-mime-type
  if (base::EqualsCaseInsensitiveASCII(mime_type, kAppXml) ||
      base::EqualsCaseInsensitiveASCII(mime_type, kTextXml) ||
      base::EndsWith(mime_type, kXmlSuffix, kCaseInsensitive)) {
    return MimeType::kXml;
  }

  if (base::EqualsCaseInsensitiveASCII(mime_type, kTextPlain)) {
    return MimeType::kPlain;
  }

  if (base::Contains(GetNeverSniffedMimeTypes(),
                     base::ToLowerASCII(mime_type))) {
    return MimeType::kNeverSniffed;
  }

  return MimeType::kOthers;
}

}  // namespace network::orb
