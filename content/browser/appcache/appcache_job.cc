// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_job.h"

#include "base/command_line.h"
#include "content/browser/appcache/appcache_request.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_url_loader_job.h"
#include "content/public/common/content_features.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace content {

AppCacheJob::~AppCacheJob() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

AppCacheURLLoaderJob* AppCacheJob::AsURLLoaderJob() {
  return nullptr;
}

AppCacheJob::AppCacheJob()
    : cache_entry_not_found_(false),
      delivery_type_(DeliveryType::kAwaitingDeliverCall) {}

void AppCacheJob::InitializeRangeRequestInfo(
    const net::HttpRequestHeaders& headers) {
  std::string value;
  std::vector<net::HttpByteRange> ranges;
  if (!headers.GetHeader(net::HttpRequestHeaders::kRange, &value) ||
      !net::HttpUtil::ParseRangeHeader(value, &ranges)) {
    return;
  }

  // If multiple ranges are requested, we play dumb and
  // return the entire response with 200 OK.
  if (ranges.size() == 1U)
    range_requested_ = ranges[0];
}

void AppCacheJob::SetupRangeResponse() {
  DCHECK(is_range_request() && reader_.get() && IsDeliveringAppCacheResponse());
  int resource_size = static_cast<int>(info_->response_data_size());
  if (resource_size < 0 || !range_requested_.ComputeBounds(resource_size)) {
    range_requested_ = net::HttpByteRange();
    return;
  }

  DCHECK(range_requested_.IsValid());
  int offset = static_cast<int>(range_requested_.first_byte_position());
  int length = static_cast<int>(range_requested_.last_byte_position() -
                                range_requested_.first_byte_position() + 1);

  // Tell the reader about the range to read.
  reader_->SetReadRange(offset, length);

  // Make a copy of the full response headers and fix them up
  // for the range we'll be returning.
  range_response_info_ =
      std::make_unique<net::HttpResponseInfo>(info_->http_response_info());
  net::HttpResponseHeaders* headers = range_response_info_->headers.get();
  headers->UpdateWithNewRange(range_requested_, resource_size,
                              true /* replace status line */);
}

}  // namespace content
