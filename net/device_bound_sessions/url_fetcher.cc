// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/url_fetcher.h"

#include "net/base/io_buffer.h"
#include "net/device_bound_sessions/session_binding_utils.h"
#include "net/url_request/url_request_context.h"

namespace net::device_bound_sessions {

namespace {

constexpr net::NetworkTrafficAnnotationTag kRegistrationTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("dbsc_registration", R"(
        semantics {
          sender: "Device Bound Session Credentials API"
          description:
            "Device Bound Session Credentials (DBSC) let a server create a "
            "session with the local device. For more info see "
            "https://github.com/WICG/dbsc."
          trigger:
            "Server sending a response with a Sec-Session-Registration header."
          data: "A signed JWT with the new key created for this session."
          destination: WEBSITE
          last_reviewed: "2024-04-10"
          user_data {
            type: ACCESS_TOKEN
          }
          internal {
            contacts {
              email: "kristianm@chromium.org"
            }
            contacts {
              email: "chrome-counter-abuse-alerts@google.com"
            }
          }
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "There is no separate setting for this feature, but it will "
            "follow the cookie settings."
          policy_exception_justification: "Not implemented."
        })");

constexpr int kBufferSize = 4096;

}  // namespace

URLFetcher::URLFetcher(const URLRequestContext* context,
                       GURL url,
                       std::optional<net::NetLogSource> net_log_source)
    : request_(context->CreateRequest(url,
                                      IDLE,
                                      this,
                                      kRegistrationTrafficAnnotation,
                                      /*is_for_websockets=*/false,
                                      net_log_source)),
      buf_(base::MakeRefCounted<IOBufferWithSize>(kBufferSize)) {}

URLFetcher::~URLFetcher() = default;

void URLFetcher::Start(base::OnceClosure complete_callback) {
  callback_ = std::move(complete_callback);
  request_->Start();
}

void URLFetcher::OnResponseStarted(URLRequest* request, int net_error) {
  net_error_ = net_error;
  if (net_error != OK) {
    std::move(callback_).Run();
    // `this` may be deleted.
    return;
  }

  HttpResponseHeaders* headers = request->response_headers();
  const int response_code = headers ? headers->response_code() : 0;

  if (response_code < 200 || response_code >= 300) {
    std::move(callback_).Run();
    // `this` may be deleted.
    return;
  }

  // Initiate the first read.
  int bytes_read_or_error = request->Read(buf_.get(), kBufferSize);
  if (bytes_read_or_error >= 0) {
    OnReadCompleted(request, bytes_read_or_error);
    // `this` may be deleted.
    return;
  } else if (bytes_read_or_error != ERR_IO_PENDING) {
    net_error_ = bytes_read_or_error;
    std::move(callback_).Run();
    // `this` may be deleted.
    return;
  }
}

void URLFetcher::OnReadCompleted(URLRequest* request, int bytes_read_or_error) {
  data_received_.append(buf_->data(), bytes_read_or_error);

  while (bytes_read_or_error > 0) {
    bytes_read_or_error = request->Read(buf_.get(), kBufferSize);
    if (bytes_read_or_error > 0) {
      data_received_.append(buf_->data(), bytes_read_or_error);
    }
  }

  if (bytes_read_or_error < 0 && bytes_read_or_error != ERR_IO_PENDING) {
    net_error_ = bytes_read_or_error;
  }

  if (bytes_read_or_error != ERR_IO_PENDING) {
    std::move(callback_).Run();
    // `this` may be deleted.
    return;
  }
}

}  // namespace net::device_bound_sessions
