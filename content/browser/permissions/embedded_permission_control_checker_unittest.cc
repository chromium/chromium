// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/embedded_permission_control_checker.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"

using blink::mojom::EmbeddedPermissionControlClient;
using blink::mojom::PermissionDescriptor;
using blink::mojom::PermissionDescriptorPtr;
using blink::mojom::PermissionName;
using blink::mojom::PermissionService;
using blink::mojom::PermissionStatus;
using ::testing::_;

namespace content {

namespace {

constexpr static int kMaxPEPCPerPage = 3;

class MockEmbeddedPermissionControlClient
    : public EmbeddedPermissionControlClient {
 public:
  explicit MockEmbeddedPermissionControlClient(
      mojo::PendingReceiver<EmbeddedPermissionControlClient> pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }
  ~MockEmbeddedPermissionControlClient() override = default;

  MOCK_METHOD2(
      OnEmbeddedPermissionControlRegistered,
      void(bool allowed,
           const std::optional<std::vector<PermissionStatus>>& statuses));

  void ExpectEmbeddedPermissionControlRegistered() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnEmbeddedPermissionControlRegistered(/*allow*/ true, _))
        .Times(1);
    run_loop.RunUntilIdle();
  }

  void ExpectEmbeddedPermissionControlNotRegistered() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnEmbeddedPermissionControlRegistered(/*allow*/ true, _))
        .Times(0);
    run_loop.RunUntilIdle();
  }

 private:
  mojo::Receiver<EmbeddedPermissionControlClient> receiver_{this};
};

}  // namespace

class EmbeddedPermissionControlCheckerTest
    : public content::RenderViewHostTestHarness {
 public:
  EmbeddedPermissionControlCheckerTest()
      : scoped_feature_list_(blink::features::kPermissionElement) {}
  EmbeddedPermissionControlCheckerTest(
      const EmbeddedPermissionControlCheckerTest&) = delete;
  EmbeddedPermissionControlCheckerTest& operator=(
      const EmbeddedPermissionControlCheckerTest&) = delete;
  ~EmbeddedPermissionControlCheckerTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL("https://www.google.com"));
    content::CreatePermissionService(
        main_rfh(), permission_service_.BindNewPipeAndPassReceiver());
  }

  PermissionService* permission_service() { return permission_service_.get(); }

  std::unique_ptr<MockEmbeddedPermissionControlClient>
  CreateEmbeddedPermissionControlClient(std::vector<PermissionName> permissions,
                                        bool is_geolocation_source = false) {
    mojo::PendingRemote<EmbeddedPermissionControlClient> mojo_client;
    auto client = std::make_unique<MockEmbeddedPermissionControlClient>(
        mojo_client.InitWithNewPipeAndPassReceiver());

    std::vector<PermissionDescriptorPtr> permission_descriptors;
    permission_descriptors.reserve(permissions.size());
    std::ranges::transform(permissions,
                           std::back_inserter(permission_descriptors),
                           [](const auto& permission) {
                             auto descriptor = PermissionDescriptor::New();
                             descriptor->name = permission;
                             return descriptor;
                           });

    auto request_descriptor =
        blink::mojom::EmbeddedPermissionRequestDescriptor::New();
    if (is_geolocation_source) {
      request_descriptor->geolocation =
          blink::mojom::GeolocationEmbeddedPermissionRequestDescriptor::New();
    }

    permission_service()->RegisterPageEmbeddedPermissionControl(
        std::move(permission_descriptors), std::move(request_descriptor),
        std::move(mojo_client));
    return client;
  }

 private:
  mojo::Remote<PermissionService> permission_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(EmbeddedPermissionControlCheckerTest,
       IgnoreRegisteregisterPageEmbeddedPermissionCheck) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kBypassPepcSecurityForTesting);
  for (PermissionName name :
       {PermissionName::AUDIO_CAPTURE, PermissionName::VIDEO_CAPTURE,
        PermissionName::GEOLOCATION}) {
    std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>> clients(
        kMaxPEPCPerPage + 3);
    for (size_t i = 0; i < kMaxPEPCPerPage + 3; ++i) {
      clients[i] = CreateEmbeddedPermissionControlClient({name});
      clients[i]->ExpectEmbeddedPermissionControlRegistered();
    }
  }
}

TEST_F(EmbeddedPermissionControlCheckerTest,
       RegisterPageEmbeddedPermissionSinglePermission) {
  for (PermissionName name :
       {PermissionName::AUDIO_CAPTURE, PermissionName::VIDEO_CAPTURE,
        PermissionName::GEOLOCATION}) {
    std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>> clients(
        kMaxPEPCPerPage);
    for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
      clients[i] = CreateEmbeddedPermissionControlClient({name});
      clients[i]->ExpectEmbeddedPermissionControlRegistered();
    }

    auto pending_client_1 = CreateEmbeddedPermissionControlClient({name});
    pending_client_1->ExpectEmbeddedPermissionControlNotRegistered();
    auto pending_client_2 = CreateEmbeddedPermissionControlClient({name});
    pending_client_2->ExpectEmbeddedPermissionControlNotRegistered();
    clients.pop_back();
    pending_client_1->ExpectEmbeddedPermissionControlRegistered();
    pending_client_2->ExpectEmbeddedPermissionControlNotRegistered();
    clients.pop_back();
    pending_client_2->ExpectEmbeddedPermissionControlRegistered();
  }
}

TEST_F(EmbeddedPermissionControlCheckerTest,
       RegisterPageEmbeddedPermissionMultiplePermissions) {
  std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>>
      grouped_clients(kMaxPEPCPerPage);
  for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
    grouped_clients[i] = CreateEmbeddedPermissionControlClient(
        {PermissionName::AUDIO_CAPTURE, PermissionName::VIDEO_CAPTURE});
    grouped_clients[i]->ExpectEmbeddedPermissionControlRegistered();
  }

  // Embedded permission control of a single permission will not count towards
  // the grouped one.
  for (PermissionName name :
       {PermissionName::AUDIO_CAPTURE, PermissionName::VIDEO_CAPTURE,
        PermissionName::GEOLOCATION}) {
    std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>> clients(
        kMaxPEPCPerPage);
    for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
      clients[i] = CreateEmbeddedPermissionControlClient({name});
      clients[i]->ExpectEmbeddedPermissionControlRegistered();
    }
  }

  // Changing order of permissions does not matter.
  auto pending_client_1 = CreateEmbeddedPermissionControlClient(
      {PermissionName::VIDEO_CAPTURE, PermissionName::AUDIO_CAPTURE});
  pending_client_1->ExpectEmbeddedPermissionControlNotRegistered();
  auto pending_client_2 = CreateEmbeddedPermissionControlClient(
      {PermissionName::VIDEO_CAPTURE, PermissionName::AUDIO_CAPTURE});
  pending_client_2->ExpectEmbeddedPermissionControlNotRegistered();
  grouped_clients.pop_back();
  pending_client_1->ExpectEmbeddedPermissionControlRegistered();
  pending_client_2->ExpectEmbeddedPermissionControlNotRegistered();
  grouped_clients.pop_back();
  pending_client_2->ExpectEmbeddedPermissionControlRegistered();
}

class GeolocationEmbeddedPermissionControlCheckerTest
    : public EmbeddedPermissionControlCheckerTest {
 public:
  GeolocationEmbeddedPermissionControlCheckerTest() = default;
  GeolocationEmbeddedPermissionControlCheckerTest(
      const GeolocationEmbeddedPermissionControlCheckerTest&) = delete;
  ~GeolocationEmbeddedPermissionControlCheckerTest() override = default;

 private:
  base::test::ScopedFeatureList features_{blink::features::kGeolocationElement};
};

TEST_F(GeolocationEmbeddedPermissionControlCheckerTest,
       DecouplePermissionSources) {
  // Register `kMaxPEPCPerPage` clients for the permission element source.
  std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>>
      permission_clients(kMaxPEPCPerPage);
  for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
    permission_clients[i] = CreateEmbeddedPermissionControlClient(
        {PermissionName::GEOLOCATION}, /*is_geolocation_source=*/false);
    permission_clients[i]->ExpectEmbeddedPermissionControlRegistered();
  }

  // Register `kMaxPEPCPerPage` clients for the geolocation element source.
  // These should also be registered, as the sources are decoupled.
  std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>>
      geolocation_clients(kMaxPEPCPerPage);
  for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
    geolocation_clients[i] = CreateEmbeddedPermissionControlClient(
        {PermissionName::GEOLOCATION}, /*is_geolocation_source=*/true);
    geolocation_clients[i]->ExpectEmbeddedPermissionControlRegistered();
  }

  // Create one more client for each source, which should not be registered yet.
  auto pending_permission_client = CreateEmbeddedPermissionControlClient(
      {PermissionName::GEOLOCATION}, /*is_geolocation_source=*/false);
  pending_permission_client->ExpectEmbeddedPermissionControlNotRegistered();

  auto pending_geolocation_client = CreateEmbeddedPermissionControlClient(
      {PermissionName::GEOLOCATION}, /*is_geolocation_source=*/true);
  pending_geolocation_client->ExpectEmbeddedPermissionControlNotRegistered();

  // Disconnect one client from the permission element source.
  permission_clients.pop_back();
  // The pending permission client should now be registered.
  pending_permission_client->ExpectEmbeddedPermissionControlRegistered();
  // The pending geolocation client should still not be registered.
  pending_geolocation_client->ExpectEmbeddedPermissionControlNotRegistered();

  // Disconnect one client from the geolocation element source.
  geolocation_clients.pop_back();
  // The pending geolocation client should now be registered.
  pending_geolocation_client->ExpectEmbeddedPermissionControlRegistered();
}

}  // namespace content
