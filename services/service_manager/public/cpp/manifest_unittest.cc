// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/manifest.h"

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace service_manager {

const char kTestServiceName[] = "test_service";

const Manifest& GetPackagedService1Manifest() {
  static base::NoDestructor<Manifest> manifest{ManifestBuilder()
                                                   .WithServiceName("service_1")
                                                   .WithDisplayName("Service 1")
                                                   .Build()};
  return *manifest;
}

const Manifest& GetPackagedService2Manifest() {
  static base::NoDestructor<Manifest> manifest{ManifestBuilder()
                                                   .WithServiceName("service_2")
                                                   .WithDisplayName("Service 2")
                                                   .Build()};
  return *manifest;
}

Manifest CreateTestManifest() {
  static base::NoDestructor<Manifest> manifest{
      ManifestBuilder()
          .WithServiceName(kTestServiceName)
          .WithDisplayName("The Test Service, Obviously")
          .WithOptions(
              ManifestOptionsBuilder()
                  .WithSandboxType("none")
                  .WithInstanceSharingPolicy(
                      Manifest::InstanceSharingPolicy::kSharedAcrossGroups)
                  .CanConnectToInstancesWithAnyId(true)
                  .CanConnectToInstancesInAnyGroup(true)
                  .CanRegisterOtherServiceInstances(false)
                  .Build())
          .ExposeCapability("capability_1",
                            Manifest::InterfaceList<mojom::Connector,
                                                    mojom::ProcessMetadata>())
          .ExposeCapability("capability_2",
                            Manifest::InterfaceList<mojom::Connector>())
          .RequireCapability("service_42", "computation")
          .RequireCapability("frobinator", "frobination")
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:frame", "filter_capability_1",
              Manifest::InterfaceList<mojom::Connector>())
          .RequireInterfaceFilterCapability_Deprecated(
              "browser", "navigation:frame", "some_filter_capability")
          .RequireInterfaceFilterCapability_Deprecated(
              "browser", "navigation:frame", "another_filter_capability")
          .PackageService(GetPackagedService1Manifest())
          .PackageService(GetPackagedService2Manifest())
          .PreloadFile("file1_key",
                       base::FilePath(FILE_PATH_LITERAL("AUTOEXEC.BAT")))
          .PreloadFile("file2_key",
                       base::FilePath(FILE_PATH_LITERAL("CONFIG.SYS")))
          .PreloadFile("file3_key", base::FilePath(FILE_PATH_LITERAL(".vimrc")))
          .Build()};
  return *manifest;
}

TEST(ManifestTest, BasicBuilder) {
  auto manifest = CreateTestManifest();
  EXPECT_EQ(kTestServiceName, manifest.service_name);
  EXPECT_EQ("none", manifest.options.sandbox_type);
  EXPECT_TRUE(manifest.options.can_connect_to_instances_in_any_group);
  EXPECT_TRUE(manifest.options.can_connect_to_instances_with_any_id);
  EXPECT_FALSE(manifest.options.can_register_other_service_instances);
  EXPECT_EQ(Manifest::InstanceSharingPolicy::kSharedAcrossGroups,
            manifest.options.instance_sharing_policy);

  EXPECT_EQ(2u, manifest.exposed_capabilities.size());
  EXPECT_THAT(
      manifest.exposed_capabilities["capability_1"],
      ElementsAre(mojom::Connector::Name_, mojom::ProcessMetadata::Name_));
  EXPECT_THAT(manifest.exposed_capabilities["capability_2"],
              ElementsAre(mojom::Connector::Name_));

  EXPECT_EQ(2u, manifest.required_capabilities.size());
  EXPECT_THAT(manifest.required_capabilities["service_42"],
              ElementsAre("computation"));
  EXPECT_THAT(manifest.required_capabilities["frobinator"],
              ElementsAre("frobination"));

  EXPECT_EQ(1u, manifest.exposed_interface_filter_capabilities.size());
  EXPECT_THAT(
      manifest.exposed_interface_filter_capabilities["navigation:frame"]
                                                    ["filter_capability_1"],
      ElementsAre(mojom::Connector::Name_));

  EXPECT_EQ(1u, manifest.required_interface_filter_capabilities.size());
  EXPECT_THAT(
      manifest.required_interface_filter_capabilities["navigation:frame"]
                                                     ["browser"],
      ElementsAre("another_filter_capability", "some_filter_capability"));

  EXPECT_EQ(2u, manifest.packaged_services.size());
  EXPECT_EQ(manifest.packaged_services[0].service_name,
            GetPackagedService1Manifest().service_name);
  EXPECT_EQ(3u, manifest.preloaded_files.size());
}

TEST(ManifestTest, Amend) {
  // Verify that everything is properly merged when amending potentially
  // overlapping capability metadata.
  Manifest manifest =
      ManifestBuilder()
          .ExposeCapability("cap1", {"interface1", "interface2"})
          .RequireCapability("service1", "cap2")
          .RequireCapability("service2", "cap3")
          .ExposeInterfaceFilterCapability_Deprecated(
              "filter1", "filtercap1", {"interface3", "interface4"})
          .RequireInterfaceFilterCapability_Deprecated("service3", "filter2",
                                                       "filtercap2")
          .Build();

  Manifest overlay = ManifestBuilder()
                         .ExposeCapability("cap1", {"xinterface1"})
                         .ExposeCapability("xcap1", {"xinterface2"})
                         .RequireCapability("xservice1", "xcap2")
                         .ExposeInterfaceFilterCapability_Deprecated(
                             "filter1", "filtercap1", {"xinterface3"})
                         .ExposeInterfaceFilterCapability_Deprecated(
                             "xfilter1", "xfiltercap1", {"xinterface4"})
                         .RequireInterfaceFilterCapability_Deprecated(
                             "xservice2", "xfilter2", "xfiltercap2")
                         .Build();

  manifest.Amend(std::move(overlay));

  auto& exposed_capabilities = manifest.exposed_capabilities;
  ASSERT_EQ(2u, exposed_capabilities.size());
  EXPECT_THAT(exposed_capabilities["cap1"],
              ElementsAre("interface1", "interface2", "xinterface1"));
  EXPECT_THAT(exposed_capabilities["xcap1"], ElementsAre("xinterface2"));

  auto& required_capabilities = manifest.required_capabilities;
  ASSERT_EQ(3u, required_capabilities.size());
  EXPECT_THAT(required_capabilities["service1"], ElementsAre("cap2"));
  EXPECT_THAT(required_capabilities["service2"], ElementsAre("cap3"));
  EXPECT_THAT(required_capabilities["xservice1"], ElementsAre("xcap2"));

  auto& exposed_filters = manifest.exposed_interface_filter_capabilities;
  ASSERT_EQ(2u, exposed_filters.size());
  EXPECT_THAT(exposed_filters["filter1"]["filtercap1"],
              ElementsAre("interface3", "interface4", "xinterface3"));
  EXPECT_THAT(exposed_filters["xfilter1"]["xfiltercap1"],
              ElementsAre("xinterface4"));

  auto& required_filters = manifest.required_interface_filter_capabilities;
  ASSERT_EQ(2u, required_filters.size());
  EXPECT_THAT(required_filters["filter2"]["service3"],
              ElementsAre("filtercap2"));
  EXPECT_THAT(required_filters["xfilter2"]["xservice2"],
              ElementsAre("xfiltercap2"));
}

}  // namespace service_manager
