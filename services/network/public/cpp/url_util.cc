// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_util.h"

#include "url/gurl.h"

namespace network {

bool IsURLHandledByNetworkService(const GURL& url) {
  return (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS());
}

GURL SerializeResponseUrlForReporting(const GURL& url) {
  // https://fetch.spec.whatwg.org/#serialize-a-response-url-for-reporting
  // To serialize a response URL for reporting, given a URL url:
  // 1. "Set the username given url and the empty string."
  // 2. "Set the password given url and the empty string."
  // 3. "Return the serialization of url with exclude fragment set to true."
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

}  // namespace network
