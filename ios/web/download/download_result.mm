// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_result.h"

#import "net/base/net_errors.h"

namespace web {

DownloadResult::DownloadResult() = default;

DownloadResult::DownloadResult(int error_code, bool can_retry)
    : error_code_(error_code), can_retry_(can_retry) {}

DownloadResult::~DownloadResult() = default;

bool DownloadResult::can_retry() const {
  return can_retry_;
}

int DownloadResult::error_code() const {
  return error_code_;
}

bool DownloadResult::is_successful() const {
  return error_code_ == net::OK;
}

}  // namespace web
