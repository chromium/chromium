// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/net_log_capture_mode_type_converter.h"

namespace mojo {

net::NetLogCaptureMode
TypeConverter<net::NetLogCaptureMode, network::mojom::NetLogCaptureMode>::
    Convert(const network::mojom::NetLogCaptureMode capture_mode) {
  switch (capture_mode) {
    case network::mojom::NetLogCaptureMode::DEFAULT:
      return net::NetLogCaptureMode::Default();
    case network::mojom::NetLogCaptureMode::INCLUDE_COOKIES_AND_CREDENTIALS:
      return net::NetLogCaptureMode::IncludeCookiesAndCredentials();
    case network::mojom::NetLogCaptureMode::INCLUDE_SOCKET_BYTES:
      return net::NetLogCaptureMode::IncludeSocketBytes();
  }
  NOTREACHED();
  return net::NetLogCaptureMode::Default();
}

}  // namespace mojo
