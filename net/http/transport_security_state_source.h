// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_TRANSPORT_SECURITY_STATE_SOURCE_H_
#define NET_HTTP_TRANSPORT_SECURITY_STATE_SOURCE_H_

#include <stddef.h>
#include <stdint.h>

#include "net/base/net_export.h"

namespace net {

// kNoReportURI is a placeholder for when a pinset does not have a report URI.
NET_EXPORT_PRIVATE extern const char kNoReportURI[];

struct TransportSecurityStateSource {
  struct Pinset {
    const char* const* const accepted_pins;
    const char* const* const rejected_pins;
    const char* const report_uri;
  };

  const uint8_t* huffman_tree;
  size_t huffman_tree_size;
  const uint8_t* preloaded_data;
  size_t preloaded_bits;
  size_t root_position;
  const Pinset* pinsets;
  size_t pinsets_count;
};

}  // namespace net

#endif  // NET_HTTP_TRANSPORT_SECURITY_STATE_SOURCE_H_
