// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_CPP_XR_FRAME_SINK_CLIENT_H_
#define DEVICE_VR_PUBLIC_CPP_XR_FRAME_SINK_CLIENT_H_

#include <memory>
#include "base/callback_forward.h"
#include "base/component_export.h"

namespace device {
// There are a handful of methods to create/register RootCompositorFrameSinks
// that must be run on the UI thread; however, the interfaces that need to be
// called are restricted to //content. This interface is designed to allow
// injection and encapsulation of those calls for XR Runtimes that are creating
// a RootCompositorFrameSink.
class COMPONENT_EXPORT(VR_PUBLIC_CPP) XrFrameSinkClient {
 public:
  XrFrameSinkClient();
  virtual ~XrFrameSinkClient();

  // TODO(https://crbug.com/1178028): Add needed methods to this interface.
};

using XrFrameSinkClientFactory =
    base::RepeatingCallback<std::unique_ptr<XrFrameSinkClient>()>;
}  // namespace device

#endif  // DEVICE_VR_PUBLIC_CPP_XR_FRAME_SINK_CLIENT_H_
