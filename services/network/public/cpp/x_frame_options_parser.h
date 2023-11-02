// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_X_FRAME_OPTIONS_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_X_FRAME_OPTIONS_PARSER_H_

#include "base/component_export.h"
#include "services/network/public/mojom/x_frame_options.mojom-forward.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {

COMPONENT_EXPORT(NETWORK_CPP)
mojom::XFrameOptionsValue ParseXFrameOptions(const net::HttpResponseHeaders&);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_X_FRAME_OPTIONS_PARSER_H_
