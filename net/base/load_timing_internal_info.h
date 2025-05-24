// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_TIMING_INTERNAL_INFO_H_
#define NET_BASE_LOAD_TIMING_INTERNAL_INFO_H_

#include <stdint.h>

#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

// Structure containing internal load timing information. This is similar to
// LoadTimingInfo, but contains extra information which shouldn't be exposed to
// the web. We use this structure for internal measurements.
struct NET_EXPORT LoadTimingInternalInfo {
  LoadTimingInternalInfo();
  LoadTimingInternalInfo(const LoadTimingInternalInfo& other);
  bool operator==(const LoadTimingInternalInfo& other) const;
  ~LoadTimingInternalInfo();

  // The time taken for HTTP stream creating to finish.
  base::TimeDelta create_stream_delay;

  // The time taken for HTTP transaction connected callback.
  base::TimeDelta connected_callback_delay;

  // The time taken for HTTP stream initialization to finish if the
  // initialization was blocked.
  base::TimeDelta initialize_stream_delay;
};

}  // namespace net

#endif  // NET_BASE_LOAD_TIMING_INTERNAL_INFO_H_
