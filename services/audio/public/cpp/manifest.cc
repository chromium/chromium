// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/manifest.h"

#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"
#include "services/audio/public/mojom/device_notifications.mojom.h"
#include "services/audio/public/mojom/log_factory_manager.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"
#include "services/audio/public/mojom/system_info.mojom.h"
#include "services/audio/public/mojom/testing_api.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace audio {

service_manager::Manifest GetManifest(
    service_manager::Manifest::ExecutionMode execution_mode) {
  return service_manager::Manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Audio Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithExecutionMode(execution_mode)
                           .WithSandboxType("audio")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability(
              "debug_recording",
              service_manager::Manifest::InterfaceList<mojom::DebugRecording>())
          .ExposeCapability(
              "device_notifier",
              service_manager::Manifest::InterfaceList<mojom::DeviceNotifier>())
          .ExposeCapability(
              "info",
              service_manager::Manifest::InterfaceList<mojom::SystemInfo>())
          .ExposeCapability("log_factory_manager",
                            service_manager::Manifest::InterfaceList<
                                mojom::LogFactoryManager>())
          .ExposeCapability(
              "stream_factory",
              service_manager::Manifest::InterfaceList<mojom::StreamFactory>())
          .ExposeCapability(
              "testing_api",
              service_manager::Manifest::InterfaceList<mojom::TestingApi>())
          .Build()};
}

}  // namespace audio
