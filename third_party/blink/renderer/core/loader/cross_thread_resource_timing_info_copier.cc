// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/cross_thread_resource_timing_info_copier.h"

#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"

namespace WTF {

namespace {

Vector<blink::mojom::blink::ServerTimingInfoPtr> CloneServerTimingInfoArray(
    const Vector<blink::mojom::blink::ServerTimingInfoPtr>& server_timing) {
  Vector<blink::mojom::blink::ServerTimingInfoPtr> result;
  for (const auto& entry : server_timing) {
    result.emplace_back(
        CrossThreadCopier<blink::mojom::blink::ServerTimingInfoPtr>::Copy(
            entry));
  }
  return result;
}

}  // namespace

CrossThreadCopier<blink::mojom::blink::ResourceTimingInfoPtr>::Type
CrossThreadCopier<blink::mojom::blink::ResourceTimingInfoPtr>::Copy(
    const blink::mojom::blink::ResourceTimingInfoPtr& info) {
  return blink::mojom::blink::ResourceTimingInfo::New(
      info->name, info->start_time, info->alpn_negotiated_protocol,
      info->connection_info, info->timing ? info->timing->Clone() : nullptr,
      info->last_redirect_end_time, info->response_end, info->cache_state,
      info->encoded_body_size, info->decoded_body_size,
      info->did_reuse_connection, info->is_secure_transport,
      info->allow_timing_details, info->allow_negative_values,
      CloneServerTimingInfoArray(info->server_timing),
      info->render_blocking_status, info->response_status, info->content_type,
      info->service_worker_router_info
          ? info->service_worker_router_info->Clone()
          : nullptr,
      info->service_worker_response_source);
}

CrossThreadCopier<blink::mojom::blink::ServerTimingInfoPtr>::Type
CrossThreadCopier<blink::mojom::blink::ServerTimingInfoPtr>::Copy(
    const blink::mojom::blink::ServerTimingInfoPtr& info) {
  return blink::mojom::blink::ServerTimingInfo::New(info->name, info->duration,
                                                    info->description);
}

}  // namespace WTF
