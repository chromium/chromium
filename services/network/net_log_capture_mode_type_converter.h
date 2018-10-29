// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NET_LOG_CAPTURE_MODE_TYPE_CONVERTER_H_
#define SERVICES_NETWORK_NET_LOG_CAPTURE_MODE_TYPE_CONVERTER_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "net/log/net_log_capture_mode.h"
#include "services/network/public/mojom/net_log.mojom.h"

namespace mojo {

// Converts a network::mojom::NetLogCaptureMode to a net::NetLogCaptureMode.
template <>
struct TypeConverter<net::NetLogCaptureMode,
                     network::mojom::NetLogCaptureMode> {
  static net::NetLogCaptureMode Convert(
      network::mojom::NetLogCaptureMode capture_mode);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_NET_LOG_CAPTURE_MODE_TYPE_CONVERTER_H_
