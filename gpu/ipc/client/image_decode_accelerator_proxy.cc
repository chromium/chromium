// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/image_decode_accelerator_proxy.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "cc/paint/paint_image.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

ImageDecodeAcceleratorProxy::ImageDecodeAcceleratorProxy(GpuChannelHost* host,
                                                         int32_t route_id)
    : host_(host) {}

ImageDecodeAcceleratorProxy::~ImageDecodeAcceleratorProxy() {}

}  // namespace gpu
