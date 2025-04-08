// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/load_timing_internal_info_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
const base::TimeDelta&
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    create_stream_delay(const net::LoadTimingInternalInfo& info) {
  return info.create_stream_delay;
}

// static
const base::TimeDelta&
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    connected_callback_delay(const net::LoadTimingInternalInfo& info) {
  return info.connected_callback_delay;
}

// static
const base::TimeDelta&
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    initialize_stream_delay(const net::LoadTimingInternalInfo& info) {
  return info.initialize_stream_delay;
}

// static
bool StructTraits<network::mojom::LoadTimingInternalInfoDataView,
                  net::LoadTimingInternalInfo>::
    Read(network::mojom::LoadTimingInternalInfoDataView data,
         net::LoadTimingInternalInfo* info) {
  if (!data.ReadInitializeStreamDelay(&info->initialize_stream_delay)) {
    return false;
  }
  return true;
}

}  // namespace mojo
