// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/history_util.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/url_util.h"

namespace blink {

bool CanChangeToUrlForHistoryApi(const KURL& url,
                                 const SecurityOrigin* document_origin,
                                 const KURL& document_url) {
  if (!url.IsValid()) {
    return false;
  }

  // "If targetURL and documentURL differ in their scheme, username, password,
  // host, or port components, then return false."
  if (url.Protocol() != document_url.Protocol() ||
      url.User() != document_url.User() || url.Pass() != document_url.Pass() ||
      url.Host() != document_url.Host() || url.Port() != document_url.Port()) {
    return false;
  }

  // "If targetURL's scheme is an HTTP(S) scheme, then return true. (Differences
  // in path, query, and fragment are allowed for http: and https: URLs.)"
  if (url.ProtocolIsInHTTPFamily()) {
    return true;
  }

  const bool differ_in_path = url.GetPath() != document_url.GetPath();
  const bool differ_in_query = url.Query() != document_url.Query();

  // "If targetURL's scheme is "file", and targetURL and documentURL differ in
  // their path component, then return false. (Differences in query and fragment
  // are allowed for file: URLs.)"
  if (url.ProtocolIs(url::kFileScheme)) {
    if (differ_in_path) {
      return false;
    }
  }

  // Non-standard: we allow sandboxed documents, `data:`/`file:` URLs, etc. to
  // rewrite their URL fragment *and* query: see https://crbug.com/528681 for
  // the compatibility concerns. We should consider removing this special
  // allowance.
  if (document_origin->IsOpaque()) {
    // For opaque/sandboxed contexts, we *always* return whether the URLs only
    // `differ_in_path`, so that we allow the URLs to vary in query/fragment
    // without falling through to the later conditions in this function, which
    // otherwise prevent query/fragment variations.
    return !differ_in_path;
  }

  // Non-standard: we allow "standard" URLs (including those have been manually
  // registered as such) to change in both query and path (and of course
  // fragment), provided they are BOTH the same scheme. The host still cannot
  // change (i.e., "chrome://bookmarks" => "chrome://history" is not allowed).
  // This is a relaxed version of the final condition in this function, which is
  // why it must come before it.
  // The set of "standard" URLs includes the following schemes:
  //   1. https/http
  //   2. file
  //   3. filesystem
  //   4. ftp
  //   5. wss/ws
  //   6. Any scheme registered with the browser via
  //      `ContentClient::AddAdditionalSchemes()`, or `url::AddStandardScheme()`
  //      more generally.
  //
  // (1) & (2) are handled earlier in this algorithm, and (4) & (5) cannot be
  // used for document creation. That leaves (3), `filesystem:` URLs and (6),
  // custom-registered "standard" URLs. These are allowed to vary in path
  // whereas other URLs (like `blob:` URLs for example) are not allowed to.
  bool is_standard = false;
  // Schemes are always ASCII strings:
  // https://url.spec.whatwg.org/#concept-url-scheme.
  CHECK(url.Protocol().Is8Bit());
  std::string protocol = url.Protocol().Ascii();
  is_standard = url::IsStandard(
      protocol.data(), url::Component(0, static_cast<int>(protocol.size())));
  if (is_standard) {
    return true;
  }

  // "If targetURL and documentURL differ in their path component or query
  // components, then return false. (Only differences in fragment are allowed
  // for other types of URLs.)"
  if (differ_in_path || differ_in_query) {
    return false;
  }

  return true;
}

}  // namespace blink
