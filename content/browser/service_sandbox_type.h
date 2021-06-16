// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_
#define CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "sandbox/policy/sandbox_type.h"

// This file maps service classes to sandbox types. See
// ServiceProcessHost::Launch() for how these templates are consumed.

// auction_worklet::mojom::AuctionWorkletService
namespace auction_worklet {
namespace mojom {
class AuctionWorkletService;
}
}  // namespace auction_worklet
template <>
inline sandbox::policy::SandboxType content::GetServiceSandboxType<
    auction_worklet::mojom::AuctionWorkletService>() {
  return sandbox::policy::SandboxType::kService;
}

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

// data_decoder::mojom::DataDecoderService
namespace data_decoder {
namespace mojom {
class DataDecoderService;
}
}  // namespace data_decoder
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<data_decoder::mojom::DataDecoderService>() {
  return sandbox::policy::SandboxType::kService;
}

// device::mojom::XRDeviceService
namespace device {
namespace mojom {
class XRDeviceService;
}
}  // namespace device
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<device::mojom::XRDeviceService>() {
#if defined(OS_WIN)
  return sandbox::policy::SandboxType::kXrCompositing;
#else
  return sandbox::policy::SandboxType::kUtility;
#endif  // !OS_WIN
}

// media::mojom::CdmServiceBroker
namespace media {
namespace mojom {
class CdmServiceBroker;
}
}  // namespace media
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<media::mojom::CdmServiceBroker>() {
  return sandbox::policy::SandboxType::kCdm;
}

#if defined(OS_WIN)
// media::mojom::MediaFoundationServiceBroker
namespace media {
namespace mojom {
class MediaFoundationServiceBroker;
}
}  // namespace media
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<media::mojom::MediaFoundationServiceBroker>() {
  return sandbox::policy::SandboxType::kMediaFoundationCdm;
}
#endif  // defined(OS_WIN)

// network::mojom::NetworkService
namespace network {
namespace mojom {
class NetworkService;
}
}  // namespace network
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<network::mojom::NetworkService>() {
  return IsNetworkSandboxEnabled() ? sandbox::policy::SandboxType::kNetwork
                                   : sandbox::policy::SandboxType::kNoSandbox;
}

// storage::mojom::StorageService
namespace storage {
namespace mojom {
class StorageService;
}
}  // namespace storage
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<storage::mojom::StorageService>() {
  return sandbox::policy::SandboxType::kUtility;
}

// tracing::mojom::TracingService
namespace tracing {
namespace mojom {
class TracingService;
}
}  // namespace tracing
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<tracing::mojom::TracingService>() {
  return sandbox::policy::SandboxType::kUtility;
}

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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS_ASH)
// shape_detection::mojom::ShapeDetectionService
namespace shape_detection {
namespace mojom {
class ShapeDetectionService;
}  // namespace mojom
}  // namespace shape_detection
template <>
inline sandbox::policy::SandboxType content::GetServiceSandboxType<
    shape_detection::mojom::ShapeDetectionService>() {
  return sandbox::policy::SandboxType::kUtility;
}
#endif

#endif  // CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_
