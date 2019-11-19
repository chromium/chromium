// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher.h"

#include "net/url_request/url_fetcher_factory.h"
#include "net/url_request/url_fetcher_impl.h"

namespace net {

URLFetcher::~URLFetcher() = default;

#if (!defined(OS_WIN) && !defined(OS_LINUX)) || defined(OS_CHROMEOS)
// static
std::unique_ptr<URLFetcher> URLFetcher::Create(
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcherDelegate* d) {
  return URLFetcher::Create(0, url, request_type, d);
}

// static
std::unique_ptr<URLFetcher> URLFetcher::Create(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcherDelegate* d) {
  return Create(id, url, request_type, d, MISSING_TRAFFIC_ANNOTATION);
}
#endif

// static
std::unique_ptr<URLFetcher> URLFetcher::Create(
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcherDelegate* d,
    NetworkTrafficAnnotationTag traffic_annotation) {
  return URLFetcher::Create(0, url, request_type, d, traffic_annotation);
}

// static
std::unique_ptr<URLFetcher> URLFetcher::Create(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcherDelegate* d,
    NetworkTrafficAnnotationTag traffic_annotation) {
  URLFetcherFactory* factory = URLFetcherImpl::factory();
  return factory ? factory->CreateURLFetcher(id, url, request_type, d,
                                             traffic_annotation)
                 : std::unique_ptr<URLFetcher>(new URLFetcherImpl(
                       url, request_type, d, traffic_annotation));
}

// static
void URLFetcher::CancelAll() {
  URLFetcherImpl::CancelAll();
}

// static
void URLFetcher::SetIgnoreCertificateRequests(bool ignored) {
  URLFetcherImpl::SetIgnoreCertificateRequests(ignored);
}

}  // namespace net
