// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_
#define CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_

#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "sandbox/policy/sandbox_type.h"

#if !defined(OS_MAC)
#include "base/feature_list.h"
#include "sandbox/policy/features.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

bool isNetworkSandboxEnabled() {
#if defined(OS_MAC)
  return true;
#else
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return false;
#endif  // defined(OS_WIN)
  return base::FeatureList::IsEnabled(
      sandbox::policy::features::kNetworkServiceSandbox);
#endif  // defined(OS_MAC)
}

}  // namespace

// This file maps service classes to sandbox types.  Services which
// require a non-utility sandbox can be added here.  See
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

// media::mojom::CdmService
namespace media {
namespace mojom {
class CdmService;
}
}  // namespace media
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<media::mojom::CdmService>() {
  return sandbox::policy::SandboxType::kCdm;
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
  return isNetworkSandboxEnabled() ? sandbox::policy::SandboxType::kNetwork
                                   : sandbox::policy::SandboxType::kNoSandbox;
}

// device::mojom::XRDeviceService
#if defined(OS_WIN)
namespace device {
namespace mojom {
class XRDeviceService;
}
}  // namespace device
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<device::mojom::XRDeviceService>() {
  return sandbox::policy::SandboxType::kXrCompositing;
}
#endif  // OS_WIN

// video_capture::mojom::VideoCaptureService
namespace video_capture {
namespace mojom {
class VideoCaptureService;
}
}  // namespace video_capture
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<video_capture::mojom::VideoCaptureService>() {
  return sandbox::policy::SandboxType::kVideoCapture;
}

#endif  // CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_
