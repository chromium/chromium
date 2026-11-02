// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_transaction_factory_override.h"

#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"

namespace net {

// --- HttpTransactionFactoryHandle ---

HttpTransactionFactoryHandle::HttpTransactionFactoryHandle(
    HttpTransactionFactory* factory,
    base::OnceClosure destruction_callback,
    bool disables_network_quality_estimation)
    : factory_(factory),
      destruction_callback_(std::move(destruction_callback)),
      disables_network_quality_estimation_(
          disables_network_quality_estimation) {}

HttpTransactionFactoryHandle::~HttpTransactionFactoryHandle() {
  // Clear `factory_` before running the callback: the callback typically
  // releases the last reference to the factory's owner, so leaving the
  // raw_ptr set would leave it dangling for the rest of this destructor
  // (which MiraclePtr's dangling-pointer detector treats as fatal).
  factory_ = nullptr;
  if (destruction_callback_) {
    std::move(destruction_callback_).Run();
  }
}

// --- HttpTransactionFactoryOverride ---

// static
const void* const HttpTransactionFactoryOverride::kKey =
    &HttpTransactionFactoryOverride::kKey;

HttpTransactionFactoryOverride::HttpTransactionFactoryOverride(
    scoped_refptr<HttpTransactionFactoryHandle> handle)
    : handle_(std::move(handle)) {}

HttpTransactionFactoryOverride::~HttpTransactionFactoryOverride() = default;

// static
void HttpTransactionFactoryOverride::Set(
    URLRequest* request,
    scoped_refptr<HttpTransactionFactoryHandle> handle) {
  request->SetUserData(kKey, std::make_unique<HttpTransactionFactoryOverride>(
                                 std::move(handle)));
}

// static
HttpTransactionFactory* HttpTransactionFactoryOverride::Get(
    const URLRequest* request) {
  auto* override =
      static_cast<HttpTransactionFactoryOverride*>(request->GetUserData(kKey));
  return override ? override->factory() : nullptr;
}

// static
bool HttpTransactionFactoryOverride::DisablesNetworkQualityEstimation(
    const URLRequest* request) {
  auto* override =
      static_cast<HttpTransactionFactoryOverride*>(request->GetUserData(kKey));
  return override && override->handle_ &&
         override->handle_->disables_network_quality_estimation();
}

}  // namespace net
