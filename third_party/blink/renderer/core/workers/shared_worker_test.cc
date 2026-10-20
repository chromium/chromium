// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/shared_worker.h"

#include <algorithm>

#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class SharedWorkerPermissionsPolicyTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    NavigateTo(KURL("https://example.com/"));
  }

  network::PermissionsPolicyFeaturesBitset ParsePermissionsPolicy(
      SharedWorker* worker,
      const KURL& script_url,
      const String& allow) {
    return worker->ParsePermissionsPolicy(script_url, allow);
  }

 private:
  ScopedPermissionsPolicyForSharedWorkersForTest scoped_feature_{true};
};

TEST_F(SharedWorkerPermissionsPolicyTest, AllowedFeaturesSucceed) {
  auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
  worker->UpdateStateIfNeeded();
  KURL script_url("https://example.com/worker.js");

  network::PermissionsPolicyFeaturesBitset policy =
      ParsePermissionsPolicy(worker, script_url, "geolocation; usb");

  EXPECT_TRUE(
      policy.Contains(network::mojom::PermissionsPolicyFeature::kGeolocation));
  EXPECT_TRUE(policy.Contains(network::mojom::PermissionsPolicyFeature::kUsb));
  EXPECT_FALSE(
      policy.Contains(network::mojom::PermissionsPolicyFeature::kSerial));
}

TEST_F(SharedWorkerPermissionsPolicyTest,
       UnrecognizedFeatureLogsConsoleWarning) {
  auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
  worker->UpdateStateIfNeeded();
  KURL script_url("https://example.com/worker.js");

  ParsePermissionsPolicy(worker, script_url, "unknown-feature");
  ASSERT_EQ(GetPage().GetConsoleMessageStorage().size(), 1u);
  EXPECT_EQ(GetPage().GetConsoleMessageStorage().at(0)->Message(),
            "Unrecognized feature: 'unknown-feature'.");
}

}  // namespace blink
