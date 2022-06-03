// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"

#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker_mode.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"

namespace blink {

namespace {

class DetachableResourceFetcherPropertiesTest : public testing::Test {
 public:
  const FetchClientSettingsObjectSnapshot& CreateFetchClientSettingsObject(
      network::mojom::IPAddressSpace address_space) {
    return *MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
        KURL("https://example.com/foo.html"),
        KURL("https://example.com/foo.html"),
        SecurityOrigin::Create(KURL("https://example.com/")),
        network::mojom::ReferrerPolicy::kDefault,
        "https://example.com/foo.html", HttpsState::kModern,
        AllowedByNosniff::MimeTypeCheck::kStrict, address_space,
        mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
        FetchClientSettingsObject::InsecureNavigationsSet());
  }
};

TEST_F(DetachableResourceFetcherPropertiesTest, DetachWithDefaultValues) {
  const auto& original_client_settings_object =
      CreateFetchClientSettingsObject(network::mojom::IPAddressSpace::kPublic);
  auto& properties = *MakeGarbageCollected<DetachableResourceFetcherProperties>(
      *MakeGarbageCollected<TestResourceFetcherProperties>(
          original_client_settings_object));

  const auto& client_settings_object =
      properties.GetFetchClientSettingsObject();
  EXPECT_EQ(&original_client_settings_object, &client_settings_object);
  EXPECT_FALSE(properties.IsMainFrame());
  EXPECT_EQ(properties.GetControllerServiceWorkerMode(),
            mojom::ControllerServiceWorkerMode::kNoController);
  // We cannot call ServiceWorkerId as the service worker mode is kNoController.
  EXPECT_FALSE(properties.IsPaused());
  EXPECT_FALSE(properties.IsDetached());
  EXPECT_FALSE(properties.IsLoadComplete());
  EXPECT_FALSE(properties.ShouldBlockLoadingSubResource());
  EXPECT_FALSE(properties.IsSubframeDeprioritizationEnabled());
  EXPECT_EQ(scheduler::FrameStatus::kNone, properties.GetFrameStatus());

  properties.Detach();

  EXPECT_NE(&client_settings_object,
            &properties.GetFetchClientSettingsObject());
  EXPECT_EQ(properties.GetFetchClientSettingsObject().BaseUrl(),
            KURL("https://example.com/foo.html"));
  EXPECT_FALSE(properties.IsMainFrame());
  EXPECT_EQ(properties.GetControllerServiceWorkerMode(),
            mojom::ControllerServiceWorkerMode::kNoController);
  // We cannot call ServiceWorkerId as the service worker mode is kNoController.
  EXPECT_FALSE(properties.IsPaused());
  EXPECT_TRUE(properties.IsDetached());
  EXPECT_FALSE(properties.IsLoadComplete());
  EXPECT_TRUE(properties.ShouldBlockLoadingSubResource());
  EXPECT_FALSE(properties.IsSubframeDeprioritizationEnabled());
  EXPECT_EQ(scheduler::FrameStatus::kNone, properties.GetFrameStatus());
}

TEST_F(DetachableResourceFetcherPropertiesTest, DetachWithNonDefaultValues) {
  const auto& original_client_settings_object =
      CreateFetchClientSettingsObject(network::mojom::IPAddressSpace::kPublic);
  auto& original_properties =
      *MakeGarbageCollected<TestResourceFetcherProperties>(
          original_client_settings_object);
  auto& properties = *MakeGarbageCollected<DetachableResourceFetcherProperties>(
      original_properties);

  original_properties.SetIsMainFrame(true);
  original_properties.SetControllerServiceWorkerMode(
      mojom::ControllerServiceWorkerMode::kControlled);
  original_properties.SetServiceWorkerId(133);
  original_properties.SetIsPaused(true);
  original_properties.SetIsLoadComplete(true);
  original_properties.SetShouldBlockLoadingSubResource(true);
  original_properties.SetIsSubframeDeprioritizationEnabled(true);
  original_properties.SetFrameStatus(scheduler::FrameStatus::kMainFrameVisible);

  const auto& client_settings_object =
      properties.GetFetchClientSettingsObject();
  EXPECT_EQ(&original_client_settings_object, &client_settings_object);
  EXPECT_TRUE(properties.IsMainFrame());
  EXPECT_EQ(properties.GetControllerServiceWorkerMode(),
            mojom::ControllerServiceWorkerMode::kControlled);
  EXPECT_EQ(properties.ServiceWorkerId(), 133);
  EXPECT_TRUE(properties.IsPaused());
  EXPECT_FALSE(properties.IsDetached());
  EXPECT_TRUE(properties.IsLoadComplete());
  EXPECT_TRUE(properties.ShouldBlockLoadingSubResource());
  EXPECT_TRUE(properties.IsSubframeDeprioritizationEnabled());
  EXPECT_EQ(scheduler::FrameStatus::kMainFrameVisible,
            properties.GetFrameStatus());

  properties.Detach();

  EXPECT_NE(&client_settings_object,
            &properties.GetFetchClientSettingsObject());
  EXPECT_EQ(properties.GetFetchClientSettingsObject().BaseUrl(),
            KURL("https://example.com/foo.html"));
  EXPECT_TRUE(properties.IsMainFrame());
  EXPECT_EQ(properties.GetControllerServiceWorkerMode(),
            mojom::ControllerServiceWorkerMode::kNoController);
  // We cannot call ServiceWorkerId as the service worker mode is kNoController.
  EXPECT_TRUE(properties.IsPaused());
  EXPECT_TRUE(properties.IsDetached());
  EXPECT_TRUE(properties.IsLoadComplete());
  EXPECT_TRUE(properties.ShouldBlockLoadingSubResource());
  EXPECT_TRUE(properties.IsSubframeDeprioritizationEnabled());
  EXPECT_EQ(scheduler::FrameStatus::kNone, properties.GetFrameStatus());
}

}  // namespace

}  // namespace blink
