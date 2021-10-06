// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_
#define CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_

#include "content/browser/network_service_instance_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "sandbox/policy/sandbox_type.h"

// This file maps service classes to sandbox types. See
// ServiceProcessHost::Launch() for how these templates are consumed.

// audio::mojom::AudioService
namespace audio {
namespace mojom {
class AudioService;
}
}  // namespace audio
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<audio::mojom::AudioService>() {
  return GetContentClient()->browser()->ShouldSandboxAudioService()
             ? sandbox::policy::SandboxType::kAudio
             : sandbox::policy::SandboxType::kNoSandbox;
}

// network::mojom::NetworkService
namespace network {
namespace mojom {
class NetworkService;
}
}  // namespace network
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<network::mojom::NetworkService>() {
  return GetContentClient()->browser()->ShouldSandboxNetworkService()
             ? sandbox::policy::SandboxType::kNetwork
             : sandbox::policy::SandboxType::kNoSandbox;
}

#endif  // CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_
