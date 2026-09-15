// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_TRANSPORT_SECURITY_STATE_PINS_SOURCE_H_
#define NET_HTTP_TRANSPORT_SECURITY_STATE_PINS_SOURCE_H_

// Note that this include list also includes all the headers for types used
// in the generated output of transport_security_state_static_pins.template.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"
#include "net/base/hash_value.h"

namespace net {

struct TransportSecurityStatePinsSource {
  struct Pinset {
    // RAW_PTR_EXCLUSION: accepted_pins always points to static data.
    RAW_PTR_EXCLUSION const base::span<const SHA256HashValue* const>
        accepted_pins;
    // RAW_PTR_EXCLUSION: rejected_pins always points to static data.
    RAW_PTR_EXCLUSION const base::span<const SHA256HashValue* const>
        rejected_pins;
  };

  struct HostPin {
    // RAW_PTR_EXCLUSION: pinset always points to static data.
    RAW_PTR_EXCLUSION const Pinset* pinset;
    bool include_subdomains;
  };

  // RAW_PTR_EXCLUSION: find_host_pin always points to static data.
  RAW_PTR_EXCLUSION const HostPin* (*const find_host_pin)(
      std::string_view hostname);

  // TODO(crbug.com/497882860): Should the timestamp go in here too?
};

}  // namespace net

#endif  // NET_HTTP_TRANSPORT_SECURITY_STATE_PINS_SOURCE_H_
