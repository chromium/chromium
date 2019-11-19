// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/manifest.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"

namespace tracing {

namespace {

service_manager::Manifest::ExecutionMode GetTracingExecutionMode() {
  return base::FeatureList::IsEnabled(features::kTracingServiceInProcess)
             ? service_manager::Manifest::ExecutionMode::kInProcessBuiltin
             : service_manager::Manifest::ExecutionMode::kOutOfProcessBuiltin;
}

}  // namespace

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Tracing")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithExecutionMode(GetTracingExecutionMode())
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSingleton)
                           .CanConnectToInstancesWithAnyId(true)
                           .Build())
          .ExposeCapability(
              "tracing",
              service_manager::Manifest::InterfaceList<mojom::ConsumerHost>())
          .RequireCapability("service_manager",
                             "service_manager:service_manager")
          .WithInterfacesBindableOnAnyService(
              service_manager::Manifest::InterfaceList<mojom::TracedProcess>())
          .Build()};
  return *manifest;
}

}  // namespace tracing
