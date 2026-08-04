// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_fetch_delegate.h"

namespace content {

BackgroundFetchDelegate::Client::GetUploadDataResponse::
    GetUploadDataResponse() = default;

BackgroundFetchDelegate::Client::GetUploadDataResponse::
    ~GetUploadDataResponse() = default;

BackgroundFetchDelegate::Client::GetUploadDataResponse::GetUploadDataResponse(
    GetUploadDataResponse&& other) = default;

BackgroundFetchDelegate::Client::GetUploadDataResponse&
BackgroundFetchDelegate::Client::GetUploadDataResponse::operator=(
    GetUploadDataResponse&& other) = default;

BackgroundFetchDelegate::BackgroundFetchDelegate() = default;

BackgroundFetchDelegate::~BackgroundFetchDelegate() = default;

}  // namespace content
