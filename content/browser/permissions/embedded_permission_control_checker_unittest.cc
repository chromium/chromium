// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/embedded_permission_control_checker.h"

#include <optional>
#include <utility>

#include "base/run_loop.h"
#include "base/test/run_until.h"
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

enum class CapabilityElementSource {
  kUserMedia,
  kGeolocation,
  kInstall,
};

constexpr static int kMaxPEPCPerPage = 3;
constexpr static int kMaxInstallElementsPerPage = 24;

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
  EmbeddedPermissionControlCheckerTest() {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {blink::features::kUserMediaElement,
                                blink::features::kUserMediaElementLegacy,
                                blink::features::kGeolocationElement},
        /* disabled_features */ {});
  }
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
  CreateEmbeddedPermissionControlClient(
      std::vector<PermissionName> permissions,
      CapabilityElementSource source = CapabilityElementSource::kUserMedia) {
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
    switch (source) {
      case CapabilityElementSource::kUserMedia:
        request_descriptor->detail = blink::mojom::
            EmbeddedPermissionControlDescriptorExtension::NewUserMedia(
                blink::mojom::UserMediaEmbeddedPermissionRequestDescriptor::
                    New());
        break;
      case CapabilityElementSource::kGeolocation:
        request_descriptor->detail = blink::mojom::
            EmbeddedPermissionControlDescriptorExtension::NewGeolocation(
                blink::mojom::GeolocationEmbeddedPermissionRequestDescriptor::
                    New());
        break;
      case CapabilityElementSource::kInstall:
        request_descriptor->detail = blink::mojom::
            EmbeddedPermissionControlDescriptorExtension::NewInstall(
                blink::mojom::InstallEmbeddedPermissionRequestDescriptor::
                    New());
        break;
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
       IgnoreRegisterPageEmbeddedPermissionCheck) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kBypassPepcSecurityForTesting);
  for (const auto& [name, source] :
       {std::make_pair(PermissionName::AUDIO_CAPTURE,
                       CapabilityElementSource::kUserMedia),
        std::make_pair(PermissionName::VIDEO_CAPTURE,
                       CapabilityElementSource::kUserMedia),
        std::make_pair(PermissionName::GEOLOCATION,
                       CapabilityElementSource::kGeolocation)}) {
    std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>> clients(
        kMaxPEPCPerPage + 3);
    for (size_t i = 0; i < kMaxPEPCPerPage + 3; ++i) {
      clients[i] = CreateEmbeddedPermissionControlClient({name}, source);
      clients[i]->ExpectEmbeddedPermissionControlRegistered();
    }
  }
}

TEST_F(EmbeddedPermissionControlCheckerTest,
       RegisterPageEmbeddedPermissionSinglePermission) {
  for (const auto& [name, source] :
       {std::make_pair(PermissionName::AUDIO_CAPTURE,
                       CapabilityElementSource::kUserMedia),
        std::make_pair(PermissionName::VIDEO_CAPTURE,
                       CapabilityElementSource::kUserMedia),
        std::make_pair(PermissionName::GEOLOCATION,
                       CapabilityElementSource::kGeolocation)}) {
    std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>> clients(
        kMaxPEPCPerPage);
    for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
      clients[i] = CreateEmbeddedPermissionControlClient({name}, source);
      clients[i]->ExpectEmbeddedPermissionControlRegistered();
    }

    auto pending_client_1 =
        CreateEmbeddedPermissionControlClient({name}, source);
    pending_client_1->ExpectEmbeddedPermissionControlNotRegistered();
    auto pending_client_2 =
        CreateEmbeddedPermissionControlClient({name}, source);
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
  for (const auto& [name, source] :
       {std::make_pair(PermissionName::AUDIO_CAPTURE,
                       CapabilityElementSource::kUserMedia),
        std::make_pair(PermissionName::VIDEO_CAPTURE,
                       CapabilityElementSource::kUserMedia),
        std::make_pair(PermissionName::GEOLOCATION,
                       CapabilityElementSource::kGeolocation)}) {
    std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>> clients(
        kMaxPEPCPerPage);
    for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
      clients[i] = CreateEmbeddedPermissionControlClient({name}, source);
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

TEST_F(EmbeddedPermissionControlCheckerTest, HasPageEmbeddedPermission) {
  auto* checker = EmbeddedPermissionControlChecker::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());
  ASSERT_TRUE(checker);

  EXPECT_FALSE(checker->HasPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE}));

  auto client = CreateEmbeddedPermissionControlClient(
      {PermissionName::AUDIO_CAPTURE}, CapabilityElementSource::kUserMedia);
  client->ExpectEmbeddedPermissionControlRegistered();

  EXPECT_TRUE(checker->HasPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE}));
  EXPECT_FALSE(checker->HasPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::VIDEO_CAPTURE}));
  EXPECT_FALSE(checker->HasPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kGeolocationElement,
      {PermissionName::AUDIO_CAPTURE}));

  auto client_2 = CreateEmbeddedPermissionControlClient(
      {PermissionName::AUDIO_CAPTURE}, CapabilityElementSource::kUserMedia);
  client_2->ExpectEmbeddedPermissionControlRegistered();

  EXPECT_TRUE(checker->HasPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE}));

  client.reset();
  base::RunLoop().RunUntilIdle();

  // Still true because client_2 is registered.
  EXPECT_TRUE(checker->HasPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE}));

  client_2.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(checker->HasPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE}));

  // Test combined permissions.
  auto combined_client = CreateEmbeddedPermissionControlClient(
      {PermissionName::AUDIO_CAPTURE, PermissionName::VIDEO_CAPTURE},
      CapabilityElementSource::kUserMedia);
  combined_client->ExpectEmbeddedPermissionControlRegistered();

  EXPECT_TRUE(checker->HasPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE, PermissionName::VIDEO_CAPTURE}));
  EXPECT_FALSE(checker->HasPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE}));

  combined_client.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(checker->HasPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE, PermissionName::VIDEO_CAPTURE}));
}

TEST_F(EmbeddedPermissionControlCheckerTest,
       ReentrantRegistrationOnClientDisconnect) {
  auto* checker = EmbeddedPermissionControlChecker::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());

  std::vector<mojo::PendingReceiver<EmbeddedPermissionControlClient>> receivers(
      kMaxPEPCPerPage);
  for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
    checker->CheckPageEmbeddedPermission(
        EmbeddedPermissionControlChecker::Source::kUserMediaElement,
        {PermissionName::AUDIO_CAPTURE},
        receivers[i].InitWithNewPipeAndPassRemote(),
        base::BindOnce(
            [](bool allow,
               const mojo::Remote<EmbeddedPermissionControlClient>&) {
              EXPECT_TRUE(allow);
            }));
  }

  mojo::PendingReceiver<EmbeddedPermissionControlClient> pending_receiver;
  mojo::PendingReceiver<EmbeddedPermissionControlClient> reentrant_receiver;
  bool pending_callback_called = false;
  bool reentrant_callback_called = false;

  checker->CheckPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE},
      pending_receiver.InitWithNewPipeAndPassRemote(),
      base::BindOnce(
          [](EmbeddedPermissionControlChecker* checker,
             mojo::PendingReceiver<EmbeddedPermissionControlClient>*
                 reentrant_rec,
             bool* pending_called, bool* reentrant_called, bool allow,
             const mojo::Remote<EmbeddedPermissionControlClient>&) {
            *pending_called = true;
            EXPECT_TRUE(allow);
            checker->CheckPageEmbeddedPermission(
                EmbeddedPermissionControlChecker::Source::kUserMediaElement,
                {PermissionName::AUDIO_CAPTURE},
                reentrant_rec->InitWithNewPipeAndPassRemote(),
                base::BindOnce(
                    [](bool* called, bool allow,
                       const mojo::Remote<EmbeddedPermissionControlClient>&) {
                      *called = true;
                      EXPECT_TRUE(allow);
                    },
                    reentrant_called));
          },
          checker, &reentrant_receiver, &pending_callback_called,
          &reentrant_callback_called));

  EXPECT_FALSE(pending_callback_called);
  EXPECT_FALSE(reentrant_callback_called);

  // Disconnect the first client.
  receivers[0].reset();
  EXPECT_TRUE(base::test::RunUntil([&]() { return pending_callback_called; }));
  EXPECT_FALSE(reentrant_callback_called);

  // Disconnect the second client to allow the reentrantly registered client.
  receivers[1].reset();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return reentrant_callback_called; }));
}

TEST_F(EmbeddedPermissionControlCheckerTest,
       ReentrantDisconnectOnClientDisconnect) {
  auto* checker = EmbeddedPermissionControlChecker::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());

  std::vector<mojo::PendingReceiver<EmbeddedPermissionControlClient>> receivers(
      kMaxPEPCPerPage);
  for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
    checker->CheckPageEmbeddedPermission(
        EmbeddedPermissionControlChecker::Source::kUserMediaElement,
        {PermissionName::AUDIO_CAPTURE},
        receivers[i].InitWithNewPipeAndPassRemote(), base::DoNothing());
  }

  mojo::PendingReceiver<EmbeddedPermissionControlClient> pending_receiver_1;
  mojo::PendingReceiver<EmbeddedPermissionControlClient> pending_receiver_2;
  bool pending_1_called = false;
  bool pending_2_called = false;

  // When pending_receiver_1's callback runs, it disconnects pending_receiver_2.
  checker->CheckPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE},
      pending_receiver_1.InitWithNewPipeAndPassRemote(),
      base::BindOnce(
          [](mojo::PendingReceiver<EmbeddedPermissionControlClient>*
                 receiver_to_disconnect,
             bool* called, bool allow,
             const mojo::Remote<EmbeddedPermissionControlClient>&) {
            *called = true;
            EXPECT_TRUE(allow);
            receiver_to_disconnect->reset();
          },
          &pending_receiver_2, &pending_1_called));

  checker->CheckPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE},
      pending_receiver_2.InitWithNewPipeAndPassRemote(),
      base::BindOnce(
          [](bool* called, bool allow,
             const mojo::Remote<EmbeddedPermissionControlClient>&) {
            *called = true;
          },
          &pending_2_called));

  receivers[0].reset();
  EXPECT_TRUE(base::test::RunUntil([&]() { return pending_1_called; }));
  EXPECT_FALSE(pending_2_called);
}

TEST_F(EmbeddedPermissionControlCheckerTest,
       ReentrantRegistrationOnInitialCheck) {
  auto* checker = EmbeddedPermissionControlChecker::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());

  mojo::PendingReceiver<EmbeddedPermissionControlClient> receiver_1;
  mojo::PendingReceiver<EmbeddedPermissionControlClient> receiver_2;
  bool client_1_called = false;
  bool client_2_called = false;

  checker->CheckPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE},
      receiver_1.InitWithNewPipeAndPassRemote(),
      base::BindOnce(
          [](EmbeddedPermissionControlChecker* checker,
             mojo::PendingReceiver<EmbeddedPermissionControlClient>*
                 receiver_2_ptr,
             bool* c1_called, bool* c2_called, bool allow,
             const mojo::Remote<EmbeddedPermissionControlClient>&) {
            *c1_called = true;
            EXPECT_TRUE(allow);
            checker->CheckPageEmbeddedPermission(
                EmbeddedPermissionControlChecker::Source::kUserMediaElement,
                {PermissionName::AUDIO_CAPTURE},
                receiver_2_ptr->InitWithNewPipeAndPassRemote(),
                base::BindOnce(
                    [](bool* called, bool allow,
                       const mojo::Remote<EmbeddedPermissionControlClient>&) {
                      *called = true;
                      EXPECT_TRUE(allow);
                    },
                    c2_called));
          },
          checker, &receiver_2, &client_1_called, &client_2_called));

  EXPECT_TRUE(client_1_called);
  EXPECT_TRUE(client_2_called);
}

TEST_F(EmbeddedPermissionControlCheckerTest,
       ReentrantSelfDisconnectOnClientDisconnect) {
  auto* checker = EmbeddedPermissionControlChecker::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());

  std::vector<mojo::PendingReceiver<EmbeddedPermissionControlClient>> receivers(
      kMaxPEPCPerPage);
  for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
    checker->CheckPageEmbeddedPermission(
        EmbeddedPermissionControlChecker::Source::kUserMediaElement,
        {PermissionName::AUDIO_CAPTURE},
        receivers[i].InitWithNewPipeAndPassRemote(), base::DoNothing());
  }

  mojo::PendingReceiver<EmbeddedPermissionControlClient> pending_receiver;
  bool pending_called = false;

  checker->CheckPageEmbeddedPermission(
      EmbeddedPermissionControlChecker::Source::kUserMediaElement,
      {PermissionName::AUDIO_CAPTURE},
      pending_receiver.InitWithNewPipeAndPassRemote(),
      base::BindOnce(
          [](mojo::PendingReceiver<EmbeddedPermissionControlClient>*
                 self_receiver,
             bool* called, bool allow,
             const mojo::Remote<EmbeddedPermissionControlClient>&) {
            *called = true;
            EXPECT_TRUE(allow);
            self_receiver->reset();
          },
          &pending_receiver, &pending_called));

  receivers[0].reset();
  EXPECT_TRUE(base::test::RunUntil([&]() { return pending_called; }));
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
  // Register `kMaxPEPCPerPage` clients for the user-media element source.
  std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>>
      user_media_clients(kMaxPEPCPerPage);
  for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
    user_media_clients[i] = CreateEmbeddedPermissionControlClient(
        {PermissionName::AUDIO_CAPTURE}, CapabilityElementSource::kUserMedia);
    user_media_clients[i]->ExpectEmbeddedPermissionControlRegistered();
  }

  // Register `kMaxPEPCPerPage` clients for the geolocation element source.
  // These should also be registered, as the sources are decoupled.
  std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>>
      geolocation_clients(kMaxPEPCPerPage);
  for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
    geolocation_clients[i] = CreateEmbeddedPermissionControlClient(
        {PermissionName::GEOLOCATION}, CapabilityElementSource::kGeolocation);
    geolocation_clients[i]->ExpectEmbeddedPermissionControlRegistered();
  }

  // Create one more client for each source, which should not be registered yet.
  auto pending_user_media_client = CreateEmbeddedPermissionControlClient(
      {PermissionName::AUDIO_CAPTURE}, CapabilityElementSource::kUserMedia);
  pending_user_media_client->ExpectEmbeddedPermissionControlNotRegistered();

  auto pending_geolocation_client = CreateEmbeddedPermissionControlClient(
      {PermissionName::GEOLOCATION}, CapabilityElementSource::kGeolocation);
  pending_geolocation_client->ExpectEmbeddedPermissionControlNotRegistered();

  // Disconnect one client from the user-media element source.
  user_media_clients.pop_back();
  // The pending user-media client should now be registered.
  pending_user_media_client->ExpectEmbeddedPermissionControlRegistered();
  // The pending geolocation client should still not be registered.
  pending_geolocation_client->ExpectEmbeddedPermissionControlNotRegistered();

  // Disconnect one client from the geolocation element source.
  geolocation_clients.pop_back();
  // The pending geolocation client should now be registered.
  pending_geolocation_client->ExpectEmbeddedPermissionControlRegistered();
}

class InstallEmbeddedPermissionControlCheckerTest
    : public EmbeddedPermissionControlCheckerTest {
 public:
  InstallEmbeddedPermissionControlCheckerTest() = default;
  InstallEmbeddedPermissionControlCheckerTest(
      const InstallEmbeddedPermissionControlCheckerTest&) = delete;
  ~InstallEmbeddedPermissionControlCheckerTest() override = default;

 private:
  base::test::ScopedFeatureList features_{blink::features::kInstallElement};
};

TEST_F(InstallEmbeddedPermissionControlCheckerTest, InstallElementHigherLimit) {
  // Install elements should support up to 24 registrations per page.
  std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>>
      install_clients(kMaxInstallElementsPerPage);
  for (size_t i = 0; i < kMaxInstallElementsPerPage; ++i) {
    install_clients[i] = CreateEmbeddedPermissionControlClient(
        {PermissionName::WEB_APP_INSTALLATION},
        CapabilityElementSource::kInstall);
    install_clients[i]->ExpectEmbeddedPermissionControlRegistered();
  }

  // The 25th install element should not be registered yet.
  auto pending_install_client = CreateEmbeddedPermissionControlClient(
      {PermissionName::WEB_APP_INSTALLATION},
      CapabilityElementSource::kInstall);
  pending_install_client->ExpectEmbeddedPermissionControlNotRegistered();

  // Disconnect one install element.
  install_clients.pop_back();
  // The pending install element should now be registered.
  pending_install_client->ExpectEmbeddedPermissionControlRegistered();
}

TEST_F(InstallEmbeddedPermissionControlCheckerTest,
       DecoupleInstallFromOtherSources) {
  // Register `kMaxPEPCPerPage` (3) clients for the user-media element source.
  std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>>
      user_media_clients(kMaxPEPCPerPage);
  for (size_t i = 0; i < kMaxPEPCPerPage; ++i) {
    user_media_clients[i] = CreateEmbeddedPermissionControlClient(
        {PermissionName::AUDIO_CAPTURE}, CapabilityElementSource::kUserMedia);
    user_media_clients[i]->ExpectEmbeddedPermissionControlRegistered();
  }

  // Register `kMaxInstallElementsPerPage` (24) clients for the install element
  // source. These should also be registered, as the sources are decoupled.
  std::vector<std::unique_ptr<MockEmbeddedPermissionControlClient>>
      install_clients(kMaxInstallElementsPerPage);
  for (size_t i = 0; i < kMaxInstallElementsPerPage; ++i) {
    install_clients[i] = CreateEmbeddedPermissionControlClient(
        {PermissionName::WEB_APP_INSTALLATION},
        CapabilityElementSource::kInstall);
    install_clients[i]->ExpectEmbeddedPermissionControlRegistered();
  }

  // Create one more client for each source, which should not be registered yet.
  auto pending_user_media_client = CreateEmbeddedPermissionControlClient(
      {PermissionName::AUDIO_CAPTURE}, CapabilityElementSource::kUserMedia);
  pending_user_media_client->ExpectEmbeddedPermissionControlNotRegistered();

  auto pending_install_client = CreateEmbeddedPermissionControlClient(
      {PermissionName::WEB_APP_INSTALLATION},
      CapabilityElementSource::kInstall);
  pending_install_client->ExpectEmbeddedPermissionControlNotRegistered();

  // Disconnect one client from the user-media element source.
  user_media_clients.pop_back();
  // The pending user-media client should now be registered.
  pending_user_media_client->ExpectEmbeddedPermissionControlRegistered();
  // The pending install client should still not be registered.
  pending_install_client->ExpectEmbeddedPermissionControlNotRegistered();

  // Disconnect one client from the install element source.
  install_clients.pop_back();
  // The pending install client should now be registered.
  pending_install_client->ExpectEmbeddedPermissionControlRegistered();
}

}  // namespace content
