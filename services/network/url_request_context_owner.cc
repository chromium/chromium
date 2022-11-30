// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_request_context_owner.h"

#include "components/prefs/pref_service.h"
#include "net/url_request/url_request_context.h"

namespace network {

URLRequestContextOwner::URLRequestContextOwner() = default;

URLRequestContextOwner::URLRequestContextOwner(
    std::unique_ptr<PrefService> pref_service_in,
    std::unique_ptr<net::URLRequestContext> url_request_context_in)
    : pref_service(std::move(pref_service_in)),
      url_request_context(std::move(url_request_context_in)) {}

URLRequestContextOwner::~URLRequestContextOwner() {
}

URLRequestContextOwner::URLRequestContextOwner(URLRequestContextOwner&& other)
    : pref_service(std::move(other.pref_service)),
      url_request_context(std::move(other.url_request_context)) {}

URLRequestContextOwner& URLRequestContextOwner::operator=(
    URLRequestContextOwner&& other) {
  pref_service = std::move(other.pref_service);
  url_request_context = std::move(other.url_request_context);
  return *this;
}

}  // namespace network
