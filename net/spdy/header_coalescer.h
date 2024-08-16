// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HEADER_COALESCER_H_
#define NET_SPDY_HEADER_COALESCER_H_

#include <string_view>

#include "net/base/net_export.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_headers_handler_interface.h"

namespace net {

class NET_EXPORT_PRIVATE HeaderCoalescer
    : public spdy::SpdyHeadersHandlerInterface {
 public:
  HeaderCoalescer(uint32_t max_header_list_size,
                  const NetLogWithSource& net_log);

  void OnHeaderBlockStart() override {}

  void OnHeader(std::string_view key, std::string_view value) override;

  void OnHeaderBlockEnd(size_t uncompressed_header_bytes,
                        size_t compressed_header_bytes) override {}

  quiche::HttpHeaderBlock release_headers();
  bool error_seen() const { return error_seen_; }

 private:
  // Helper to add a header. Return true on success.
  bool AddHeader(std::string_view key, std::string_view value);

  quiche::HttpHeaderBlock headers_;
  bool headers_valid_ = true;
  size_t header_list_size_ = 0;
  bool error_seen_ = false;
  bool regular_header_seen_ = false;
  const uint32_t max_header_list_size_;
  NetLogWithSource net_log_;
};

}  // namespace net

#endif  // NET_SPDY_HEADER_COALESCER_H_
