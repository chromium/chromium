// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/load_timing_internal_info_mojom_traits.h"

#include "base/notreached.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "net/base/load_timing_internal_info.h"

namespace mojo {

network::mojom::SessionSource
EnumTraits<network::mojom::SessionSource, net::SessionSource>::ToMojom(
    net::SessionSource session_source) {
  switch (session_source) {
    case net::SessionSource::kNew:
      return network::mojom::SessionSource::kNew;
    case net::SessionSource::kExisting:
      return network::mojom::SessionSource::kExisting;
  }
  NOTREACHED();
}

bool EnumTraits<network::mojom::SessionSource, net::SessionSource>::FromMojom(
    network::mojom::SessionSource in,
    net::SessionSource* out) {
  switch (in) {
    case network::mojom::SessionSource::kNew:
      *out = net::SessionSource::kNew;
      return true;
    case network::mojom::SessionSource::kExisting:
      *out = net::SessionSource::kExisting;
      return true;
  }
  return false;
}

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
  if (!data.ReadCreateStreamDelay(&info->create_stream_delay)) {
    return false;
  }
  if (!data.ReadConnectedCallbackDelay(&info->connected_callback_delay)) {
    return false;
  }
  if (!data.ReadInitializeStreamDelay(&info->initialize_stream_delay)) {
    return false;
  }
  if (!data.ReadSessionSource(&info->session_source)) {
    return false;
  }
  return true;
}

// static
std::optional<net::SessionSource>
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    session_source(const net::LoadTimingInternalInfo& info) {
  return info.session_source;
}

}  // namespace mojo
