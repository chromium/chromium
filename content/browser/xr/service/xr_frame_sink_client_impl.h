// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_SERVICE_XR_FRAME_SINK_CLIENT_IMPL_H_
#define CONTENT_BROWSER_XR_SERVICE_XR_FRAME_SINK_CLIENT_IMPL_H_

#include <memory>

#include "device/vr/public/cpp/xr_frame_sink_client.h"

namespace content {
class XrFrameSinkClientImpl : public device::XrFrameSinkClient {
 public:
  XrFrameSinkClientImpl();
  ~XrFrameSinkClientImpl() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_SERVICE_XR_FRAME_SINK_CLIENT_IMPL_H_
