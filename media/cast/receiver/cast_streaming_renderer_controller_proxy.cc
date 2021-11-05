// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/cast_streaming_renderer_controller_proxy.h"

namespace media {
namespace cast {

// static
CastStreamingRendererControllerProxy*
    CastStreamingRendererControllerProxy::singleton_instance_ = nullptr;

// static
CastStreamingRendererControllerProxy*
CastStreamingRendererControllerProxy::GetInstance() {
  return singleton_instance_;
}

CastStreamingRendererControllerProxy::CastStreamingRendererControllerProxy() {
  DCHECK(!singleton_instance_);
  singleton_instance_ = this;
}

CastStreamingRendererControllerProxy::~CastStreamingRendererControllerProxy() {
  DCHECK(singleton_instance_);
  singleton_instance_ = nullptr;
}

}  // namespace cast
}  // namespace media
