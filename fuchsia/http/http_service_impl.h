// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_HTTP_HTTP_SERVICE_IMPL_H_
#define FUCHSIA_HTTP_HTTP_SERVICE_IMPL_H_

#include <fuchsia/net/oldhttp/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>

#include "net/url_request/url_request_context.h"

// Implements the Fuchsia HttpService API, using the //net library.
class HttpServiceImpl : public ::fuchsia::net::oldhttp::HttpService {
 public:
  HttpServiceImpl();

  HttpServiceImpl(const HttpServiceImpl&) = delete;
  HttpServiceImpl& operator=(const HttpServiceImpl&) = delete;

  ~HttpServiceImpl() override;

  // HttpService methods:
  void CreateURLLoader(
      fidl::InterfaceRequest<::fuchsia::net::oldhttp::URLLoader> request)
      override;
};

#endif  // FUCHSIA_HTTP_HTTP_SERVICE_IMPL_H_
