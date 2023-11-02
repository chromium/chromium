// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/tests/service_manager/test_manifests.h"

#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/tests/service_manager/service_manager.test-mojom.h"

namespace service_manager {

const char kTestServiceName[] = "service_manager_unittest";
const char kTestTargetName[] = "service_manager_unittest_target";
const char kTestEmbedderName[] = "service_manager_unittest_embedder";
const char kTestRegularServiceName[] = "service_manager_unittest_regular";
const char kTestSharedServiceName[] =
    "service_manager_unittest_shared_across_groups";
const char kTestSingletonServiceName[] = "service_manager_unittest_singleton";

const char kCreateInstanceTestCapability[] = "create_instance_test";

const std::vector<Manifest>& GetTestManifests() {
  static base::NoDestructor<std::vector<Manifest>> manifests{
      {ManifestBuilder()
           .WithServiceName(kTestEmbedderName)
           .WithOptions(ManifestOptionsBuilder()
                            .WithInstanceSharingPolicy(
                                Manifest::InstanceSharingPolicy::kSingleton)
                            .WithExecutionMode(
                                Manifest::ExecutionMode::kStandaloneExecutable)
                            .WithSandboxType("none")
                            .Build())
           .RequireCapability(kTestTargetName, "")
           .PackageService(ManifestBuilder()
                               .WithServiceName(kTestRegularServiceName)
                               .RequireCapability(kTestTargetName, "")
                               .RequireCapability(mojom::kServiceName, "")
                               .Build())
           .PackageService(
               ManifestBuilder()
                   .WithServiceName(kTestSharedServiceName)
                   .WithOptions(ManifestOptionsBuilder()
                                    .WithInstanceSharingPolicy(
                                        Manifest::InstanceSharingPolicy::
                                            kSharedAcrossGroups)
                                    .Build())
                   .Build())
           .PackageService(
               ManifestBuilder()
                   .WithServiceName(kTestSingletonServiceName)
                   .WithOptions(
                       ManifestOptionsBuilder()
                           .WithInstanceSharingPolicy(
                               Manifest::InstanceSharingPolicy::kSingleton)
                           .Build())
                   .RequireCapability(kTestTargetName, "")
                   .Build())
           .Build(),
       ManifestBuilder()
           .WithServiceName(kTestServiceName)
           .WithOptions(ManifestOptionsBuilder()
                            .WithInstanceSharingPolicy(
                                Manifest::InstanceSharingPolicy::kSingleton)
                            .CanConnectToInstancesWithAnyId(true)
                            .CanRegisterOtherServiceInstances(true)
                            .Build())
           .ExposeCapability(
               kCreateInstanceTestCapability,
               Manifest::InterfaceList<test::mojom::CreateInstanceTest>())
           .RequireCapability(kTestTargetName, "")
           .RequireCapability(kTestEmbedderName, "")
           .RequireCapability(kTestSharedServiceName, "")
           .RequireCapability(kTestSingletonServiceName, "")
           .RequireCapability(kTestRegularServiceName, "")
           .RequireCapability(mojom::kServiceName,
                              "service_manager:service_manager")
           .Build(),
       ManifestBuilder()
           .WithServiceName(kTestTargetName)
           .WithOptions(ManifestOptionsBuilder()
                            .WithExecutionMode(
                                Manifest::ExecutionMode::kStandaloneExecutable)
                            .WithSandboxType("none")
                            .Build())
           .RequireCapability(kTestTargetName, "")
           .RequireCapability(kTestServiceName, kCreateInstanceTestCapability)
           .Build()}};
  return *manifests;
}

}  // namespace service_manager
