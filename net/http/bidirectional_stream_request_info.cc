// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/bidirectional_stream_request_info.h"

namespace net {

BidirectionalStreamRequestInfo::BidirectionalStreamRequestInfo()
    : allow_early_data_override(false),
      priority(LOW),
      end_stream_on_headers(false),
      detect_broken_connection(false) {}

BidirectionalStreamRequestInfo::~BidirectionalStreamRequestInfo() = default;

}  // namespace net
