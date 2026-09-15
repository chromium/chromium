// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_TRANSPORT_SECURITY_STATE_SOURCE_H_
#define NET_HTTP_TRANSPORT_SECURITY_STATE_SOURCE_H_

// Note that this include list also includes all the headers for types used
// in the generated output of transport_security_state_static.template.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"

namespace net {

struct TransportSecurityStateSource {
  // RAW_PTR_EXCLUSION: huffman_tree always points to static data.
  RAW_PTR_EXCLUSION const base::span<const uint8_t> huffman_tree;
  // RAW_PTR_EXCLUSION: preloaded_data always points to static data.
  RAW_PTR_EXCLUSION const base::span<const uint8_t> preloaded_data;
  size_t preloaded_bits;
  size_t root_position;
};

}  // namespace net

#endif  // NET_HTTP_TRANSPORT_SECURITY_STATE_SOURCE_H_
