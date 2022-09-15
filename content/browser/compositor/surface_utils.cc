// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/surface_utils.h"

#include "build/build_config.h"
#include "components/viz/host/host_frame_sink_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/compositor_dependencies_android.h"
#else
#include "content/browser/compositor/image_transport_factory.h"
#include "ui/compositor/compositor.h"  // nogncheck
#endif

namespace content {

viz::FrameSinkId AllocateFrameSinkId() {
#if BUILDFLAG(IS_ANDROID)
  return CompositorDependenciesAndroid::Get().AllocateFrameSinkId();
#else
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  return factory->GetContextFactory()->AllocateFrameSinkId();
#endif
}

viz::HostFrameSinkManager* GetHostFrameSinkManager() {
#if BUILDFLAG(IS_ANDROID)
  return CompositorDependenciesAndroid::Get().host_frame_sink_manager();
#else
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  if (!factory)
    return nullptr;
  return factory->GetContextFactory()->GetHostFrameSinkManager();
#endif
}

}  // namespace content
