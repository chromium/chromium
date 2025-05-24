// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/fetch_retry_options.h"

namespace network {

FetchRetryOptions::FetchRetryOptions() = default;
FetchRetryOptions::~FetchRetryOptions() = default;

FetchRetryOptions::FetchRetryOptions(FetchRetryOptions&&) = default;
FetchRetryOptions& FetchRetryOptions::operator=(FetchRetryOptions&&) = default;

FetchRetryOptions::FetchRetryOptions(const FetchRetryOptions&) = default;
FetchRetryOptions& FetchRetryOptions::operator=(const FetchRetryOptions&) =
    default;
bool FetchRetryOptions::operator==(const FetchRetryOptions&) const = default;

}  // namespace network
