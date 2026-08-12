// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webid/native_idp_fetcher.h"

namespace content {

NativeIdpFetcher::RequestParams::RequestParams() = default;
NativeIdpFetcher::RequestParams::~RequestParams() = default;
NativeIdpFetcher::RequestParams::RequestParams(const RequestParams&) = default;
NativeIdpFetcher::RequestParams& NativeIdpFetcher::RequestParams::operator=(
    const RequestParams&) = default;
NativeIdpFetcher::RequestParams::RequestParams(RequestParams&&) = default;
NativeIdpFetcher::RequestParams& NativeIdpFetcher::RequestParams::operator=(
    RequestParams&&) = default;

}  // namespace content
