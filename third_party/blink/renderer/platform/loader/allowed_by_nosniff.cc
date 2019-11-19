// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/allowed_by_nosniff.h"

#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"

namespace blink {

namespace {

using WebFeature = mojom::WebFeature;

// In addition to makeing an allowed/not-allowed decision,
// AllowedByNosniff::MimeTypeAsScript reports common usage patterns to support
// future decisions about which types can be safely be disallowed. Below
// is a number of constants about which use counters to report.

const WebFeature kApplicationFeatures[2] = {
    WebFeature::kCrossOriginApplicationScript,
    WebFeature::kSameOriginApplicationScript};

const WebFeature kTextFeatures[2] = {WebFeature::kCrossOriginTextScript,
                                     WebFeature::kSameOriginTextScript};

const WebFeature kApplicationOctetStreamFeatures[2] = {
    WebFeature::kCrossOriginApplicationOctetStream,
    WebFeature::kSameOriginApplicationOctetStream,
};

const WebFeature kApplicationXmlFeatures[2] = {
    WebFeature::kCrossOriginApplicationXml,
    WebFeature::kSameOriginApplicationXml,
};

const WebFeature kTextHtmlFeatures[2] = {
    WebFeature::kCrossOriginTextHtml,
    WebFeature::kSameOriginTextHtml,
};

const WebFeature kTextPlainFeatures[2] = {
    WebFeature::kCrossOriginTextPlain,
    WebFeature::kSameOriginTextPlain,
};

const WebFeature kTextXmlFeatures[2] = {
    WebFeature::kCrossOriginTextXml,
    WebFeature::kSameOriginTextXml,
};

// Helper function to decide what to do with with a given mime type. This takes
// - a mime type
// - inputs that affect the decision (is_same_origin, mime_type_check_mode).
//
// The return value determines whether this mime should be allowed or blocked.
// Additionally, warn returns whether we should log a console warning about
// expected future blocking of this resource. 'counter' determines which
// Use counter should be used to count this. 'is_worker_global_scope' is used
// for choosing 'counter' value.
bool AllowMimeTypeAsScript(const String& mime_type,
                           bool same_origin,
                           AllowedByNosniff::MimeTypeCheck mime_type_check_mode,
                           WebFeature& counter) {
  using MimeTypeCheck = AllowedByNosniff::MimeTypeCheck;

  // The common case: A proper JavaScript MIME type
  if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type))
    return true;

  // Check for certain non-executable MIME types.
  // See:
  // https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-mime-type?
  if (mime_type.StartsWithIgnoringASCIICase("image/")) {
    counter = WebFeature::kBlockedSniffingImageToScript;
    return false;
  }
  if (mime_type.StartsWithIgnoringASCIICase("audio/")) {
    counter = WebFeature::kBlockedSniffingAudioToScript;
    return false;
  }
  if (mime_type.StartsWithIgnoringASCIICase("video/")) {
    counter = WebFeature::kBlockedSniffingVideoToScript;
    return false;
  }
  if (mime_type.StartsWithIgnoringASCIICase("text/csv")) {
    counter = WebFeature::kBlockedSniffingCSVToScript;
    return false;
  }

  if (mime_type_check_mode == MimeTypeCheck::kStrict) {
    return false;
  }
  DCHECK(mime_type_check_mode == MimeTypeCheck::kLaxForWorker ||
         mime_type_check_mode == MimeTypeCheck::kLaxForElement);

  // Beyond this point we handle legacy MIME types, where it depends whether
  // we still wish to accept them (or log them using UseCounter, or add a
  // deprecation warning to the console).

  if (mime_type.StartsWithIgnoringASCIICase("text/") &&
      MIMETypeRegistry::IsLegacySupportedJavaScriptLanguage(
          mime_type.Substring(5))) {
    return true;
  }

  if (mime_type.StartsWithIgnoringASCIICase("application/octet-stream")) {
    counter = kApplicationOctetStreamFeatures[same_origin];
  } else if (mime_type.StartsWithIgnoringASCIICase("application/xml")) {
    counter = kApplicationXmlFeatures[same_origin];
  } else if (mime_type.StartsWithIgnoringASCIICase("text/html")) {
    counter = kTextHtmlFeatures[same_origin];
  } else if (mime_type.StartsWithIgnoringASCIICase("text/plain")) {
    counter = kTextPlainFeatures[same_origin];
  } else if (mime_type.StartsWithIgnoringCase("text/xml")) {
    counter = kTextXmlFeatures[same_origin];
  }

  return true;
}

}  // namespace

bool AllowedByNosniff::MimeTypeAsScript(UseCounter& use_counter,
                                        ConsoleLogger* console_logger,
                                        const ResourceResponse& response,
                                        MimeTypeCheck mime_type_check_mode) {
  // The content type is really only meaningful for `http:`-family schemes.
  if (!response.CurrentRequestUrl().ProtocolIsInHTTPFamily() &&
      (response.CurrentRequestUrl().LastPathComponent().EndsWith(".js") ||
       response.CurrentRequestUrl().LastPathComponent().EndsWith(".mjs"))) {
    return true;
  }

  // Exclude `data:`, `blob:` and `filesystem:` URLs from MIME checks.
  if (response.CurrentRequestUrl().ProtocolIsData() ||
      response.CurrentRequestUrl().ProtocolIs(url::kBlobScheme) ||
      response.CurrentRequestUrl().ProtocolIs(url::kFileSystemScheme)) {
    return true;
  }

  String mime_type = response.HttpContentType();

  // Allowed by nosniff?
  if (!(ParseContentTypeOptionsHeader(response.HttpHeaderField(
            http_names::kXContentTypeOptions)) != kContentTypeOptionsNosniff ||
        MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type))) {
    console_logger->AddConsoleMessage(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Refused to execute script from '" +
            response.CurrentRequestUrl().ElidedString() +
            "' because its MIME type ('" + mime_type +
            "') is not executable, and strict MIME type checking is enabled.");
    return false;
  }

  // Check for certain non-executable MIME types.
  // See:
  // https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-mime-type?
  const bool same_origin =
      response.GetType() == network::mojom::FetchResponseType::kBasic;

  // For any MIME type, we can do three things: accept/reject it, print a
  // warning into the console, and count it using a use counter.
  const WebFeature kWebFeatureNone = WebFeature::kNumberOfFeatures;
  WebFeature counter = kWebFeatureNone;
  bool allow = AllowMimeTypeAsScript(mime_type, same_origin,
                                     mime_type_check_mode, counter);

  // These record usages for two MIME types (without subtypes), per same/cross
  // origin.
  if (mime_type.StartsWithIgnoringASCIICase("application/")) {
    use_counter.CountUse(kApplicationFeatures[same_origin]);
  } else if (mime_type.StartsWithIgnoringASCIICase("text/")) {
    use_counter.CountUse(kTextFeatures[same_origin]);
  }

  // The code above has made a decision and handed down the result in accept
  // and counter.
  if (counter != kWebFeatureNone) {
    use_counter.CountUse(counter);
  }
  if (!allow) {
    console_logger->AddConsoleMessage(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Refused to execute script from '" +
            response.CurrentRequestUrl().ElidedString() +
            "' because its MIME type ('" + mime_type + "') is not executable.");
  } else if (mime_type_check_mode == MimeTypeCheck::kLaxForWorker) {
    bool strict_allow = AllowMimeTypeAsScript(mime_type, same_origin,
                                              MimeTypeCheck::kStrict, counter);
    if (!strict_allow)
      use_counter.CountUse(WebFeature::kStrictMimeTypeChecksWouldBlockWorker);
  }
  return allow;
}

}  // namespace blink
