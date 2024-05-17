// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/x_callback_url.h"

#include <string_view>

#include "base/check.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/base/url_util.h"
#include "url/url_features.h"

namespace {

const char kXCallbackURLHost[] = "x-callback-url";
const char kSuccessURLParameterName[] = "x-success";
const char kErrorURLParameterName[] = "x-error";
const char kCancelURLParameterName[] = "x-cancel";

}  // namespace

bool IsXCallbackURL(const GURL& url) {
  if (!url.is_valid())
    return false;

  if (url::IsUsingStandardCompliantNonSpecialSchemeURLParsing()) {
    return url.host_piece() == kXCallbackURLHost;
  }

  // The following is a workaround when non-special URLs are not properly
  // supported. We have to parse `url` manually for non-special URL.
  if (url.IsStandard()) {
    return url.host_piece() == kXCallbackURLHost;
  }
  std::string_view path_piece = url.path_piece();
  if (base::StartsWith(path_piece, "//"))
    path_piece = path_piece.substr(2, std::string_view::npos);

  size_t pos = path_piece.find('/', 0);
  if (pos != std::string_view::npos) {
    path_piece = path_piece.substr(0, pos);
  }

  return path_piece == kXCallbackURLHost;
}

GURL CreateXCallbackURL(std::string_view scheme, std::string_view action) {
  return CreateXCallbackURLWithParameters(scheme, action, GURL(), GURL(),
                                          GURL(),
                                          std::map<std::string, std::string>());
}

GURL CreateXCallbackURLWithParameters(
    std::string_view scheme,
    std::string_view action,
    const GURL& success_url,
    const GURL& error_url,
    const GURL& cancel_url,
    const std::map<std::string, std::string>& parameters) {
  DCHECK(!scheme.empty());
  GURL url(base::StrCat({scheme, "://", kXCallbackURLHost, "/", action}));

  if (success_url.is_valid()) {
    url = net::AppendQueryParameter(url, kSuccessURLParameterName,
                                    success_url.spec());
  }

  if (error_url.is_valid()) {
    url = net::AppendQueryParameter(url, kErrorURLParameterName,
                                    error_url.spec());
  }

  if (cancel_url.is_valid()) {
    url = net::AppendQueryParameter(url, kCancelURLParameterName,
                                    cancel_url.spec());
  }

  if (!parameters.empty()) {
    for (const auto& pair : parameters) {
      url = net::AppendQueryParameter(url, pair.first, pair.second);
    }
  }

  DCHECK(IsXCallbackURL(url));
  return url;
}

std::map<std::string, std::string> ExtractQueryParametersFromXCallbackURL(
    const GURL& x_callback_url) {
  DCHECK(IsXCallbackURL(x_callback_url));
  std::map<std::string, std::string> parameters;

  net::QueryIterator query_iterator(x_callback_url);
  while (!query_iterator.IsAtEnd()) {
    parameters.insert(std::make_pair(query_iterator.GetKey(),
                                     query_iterator.GetUnescapedValue()));
    query_iterator.Advance();
  }

  return parameters;
}
