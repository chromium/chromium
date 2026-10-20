// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/shared_worker.h"

#include <optional>

#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

class ErrorEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override {
    EXPECT_EQ(event->type(), event_type_names::kError);
    error_count_++;
  }
  int ErrorCount() const { return error_count_; }

 private:
  int error_count_ = 0;
};

}  // namespace

class SharedWorkerPermissionsPolicyTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    NavigateTo(KURL("https://example.com/"));
  }

  std::optional<network::PermissionsPolicyFeaturesBitset>
  ParsePermissionsPolicy(SharedWorker* worker,
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

  std::optional<network::PermissionsPolicyFeaturesBitset> policy =
      ParsePermissionsPolicy(worker, script_url, "geolocation; usb");
  ASSERT_TRUE(policy.has_value());

  EXPECT_TRUE(
      policy->Contains(network::mojom::PermissionsPolicyFeature::kGeolocation));
  EXPECT_TRUE(policy->Contains(network::mojom::PermissionsPolicyFeature::kUsb));
  EXPECT_FALSE(
      policy->Contains(network::mojom::PermissionsPolicyFeature::kSerial));
}

TEST_F(SharedWorkerPermissionsPolicyTest,
       UnrecognizedFeatureLogsConsoleWarning) {
  auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
  worker->UpdateStateIfNeeded();
  KURL script_url("https://example.com/worker.js");

  std::optional<network::PermissionsPolicyFeaturesBitset> policy =
      ParsePermissionsPolicy(worker, script_url, "unknown-feature");
  EXPECT_TRUE(policy.has_value());
  ASSERT_EQ(GetPage().GetConsoleMessageStorage().size(), 1u);
  EXPECT_EQ(GetPage().GetConsoleMessageStorage().at(0)->Message(),
            "Unrecognized feature: 'unknown-feature'.");
}

TEST_F(SharedWorkerPermissionsPolicyTest,
       DisallowedFeatureInCreatorFiresErrorEventAndLogsConsoleError) {
  // Disable geolocation in the creator frame's PermissionsPolicy.
  network::ParsedPermissionsPolicy header_policy = {
      network::ParsedPermissionsPolicyDeclaration(
          network::mojom::PermissionsPolicyFeature::kGeolocation)};
  GetFrame().DomWindow()->GetSecurityContext().SetPermissionsPolicy(
      network::PermissionsPolicy::CreateFromParentPolicy(
          /*parent_policy=*/nullptr, header_policy, /*container_policy=*/{},
          GetFrame().DomWindow()->GetSecurityOrigin()->ToUrlOrigin()));
  ASSERT_FALSE(GetFrame().DomWindow()->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kGeolocation));

  auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
  worker->UpdateStateIfNeeded();
  auto* listener = MakeGarbageCollected<ErrorEventListener>();
  worker->addEventListener(event_type_names::kError, listener);
  KURL script_url("https://example.com/worker.js");

  // Creating a worker without requesting 'geolocation' (or explicitly setting
  // "geolocation 'none'") should succeed even though geolocation is disabled in
  // the creator context.
  EXPECT_TRUE(
      ParsePermissionsPolicy(worker, script_url, g_empty_string).has_value());
  EXPECT_TRUE(ParsePermissionsPolicy(worker, script_url, "geolocation 'none'")
                  .has_value());
  test::RunPendingTasks();
  EXPECT_EQ(listener->ErrorCount(), 0);
  EXPECT_EQ(GetPage().GetConsoleMessageStorage().size(), 0u);

  // Requesting 'geolocation' when it is disabled in the creator context logs a
  // console error naming the feature and dispatches an error event
  // asynchronously.
  EXPECT_FALSE(
      ParsePermissionsPolicy(worker, script_url, "geolocation").has_value());
  ASSERT_EQ(GetPage().GetConsoleMessageStorage().size(), 1u);
  EXPECT_EQ(GetPage().GetConsoleMessageStorage().at(0)->Message(),
            "Permissions-Policy feature 'geolocation' specified in the "
            "'allow' option cannot be delegated by the creating context "
            "to the worker script's origin.");
  test::RunPendingTasks();
  EXPECT_EQ(listener->ErrorCount(), 1);

  // Same check for a data: URL worker.
  EXPECT_FALSE(ParsePermissionsPolicy(worker, KURL("data:text/javascript,"),
                                      "geolocation")
                   .has_value());
  ASSERT_EQ(GetPage().GetConsoleMessageStorage().size(), 2u);
  EXPECT_EQ(GetPage().GetConsoleMessageStorage().at(1)->Message(),
            "Permissions-Policy feature 'geolocation' specified in the "
            "'allow' option cannot be delegated by the creating context "
            "to the worker script's origin.");
  test::RunPendingTasks();
  EXPECT_EQ(listener->ErrorCount(), 2);
}

TEST_F(SharedWorkerPermissionsPolicyTest,
       AllowedFeaturesForOpaqueOriginWithDefaultPolicySucceed) {
  // When the creator context has no declared policy for 'geolocation' or 'usb'
  // (which have a default allowlist of 'self') and the features are enabled in
  // the creator context, the creator can delegate them to an opaque origin
  // (such as a data: URL), even though IsFeatureEnabledForOrigin() returns
  // false for the opaque origin.
  const network::PermissionsPolicy* creator_policy =
      GetFrame().DomWindow()->GetSecurityContext().GetPermissionsPolicy();
  ASSERT_TRUE(creator_policy);
  KURL data_url("data:text/javascript,");
  url::Origin opaque_origin = SecurityOrigin::Create(data_url)->ToUrlOrigin();
  ASSERT_TRUE(opaque_origin.opaque());
  ASSERT_FALSE(creator_policy->IsFeatureEnabledForOrigin(
      network::mojom::PermissionsPolicyFeature::kGeolocation, opaque_origin));

  auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
  worker->UpdateStateIfNeeded();

  std::optional<network::PermissionsPolicyFeaturesBitset> policy =
      ParsePermissionsPolicy(worker, data_url, "geolocation; usb");
  ASSERT_TRUE(policy.has_value());

  EXPECT_TRUE(
      policy->Contains(network::mojom::PermissionsPolicyFeature::kGeolocation));
  EXPECT_TRUE(policy->Contains(network::mojom::PermissionsPolicyFeature::kUsb));
  EXPECT_FALSE(
      policy->Contains(network::mojom::PermissionsPolicyFeature::kSerial));
}

TEST_F(SharedWorkerPermissionsPolicyTest,
       DisallowedFeatureForScriptOriginFiresErrorEventAndLogsConsoleError) {
  // Declare `Permissions-Policy: geolocation=(self)` on the creator frame.
  // 'geolocation' is enabled for the document's own origin, but because there
  // is an explicit declared policy only matching 'self', it cannot be delegated
  // to an opaque origin (such as a data: URL).
  url::Origin creator_origin =
      GetFrame().DomWindow()->GetSecurityOrigin()->ToUrlOrigin();
  network::ParsedPermissionsPolicyDeclaration decl(
      network::mojom::PermissionsPolicyFeature::kGeolocation);
  decl.self_if_matches = creator_origin;
  network::ParsedPermissionsPolicy header_policy = {decl};
  GetFrame().DomWindow()->GetSecurityContext().SetPermissionsPolicy(
      network::PermissionsPolicy::CreateFromParentPolicy(
          /*parent_policy=*/nullptr, header_policy, /*container_policy=*/{},
          creator_origin));
  ASSERT_TRUE(GetFrame().DomWindow()->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kGeolocation));

  auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
  worker->UpdateStateIfNeeded();
  auto* listener = MakeGarbageCollected<ErrorEventListener>();
  worker->addEventListener(event_type_names::kError, listener);

  // Same-origin worker can still be delegated 'geolocation'.
  EXPECT_TRUE(ParsePermissionsPolicy(worker, KURL("https://example.com/w.js"),
                                     "geolocation")
                  .has_value());

  // Opaque-origin (data: URL) worker cannot be delegated 'geolocation'.
  EXPECT_FALSE(ParsePermissionsPolicy(worker, KURL("data:text/javascript,"),
                                      "geolocation")
                   .has_value());
  ASSERT_EQ(GetPage().GetConsoleMessageStorage().size(), 1u);
  EXPECT_EQ(GetPage().GetConsoleMessageStorage().at(0)->Message(),
            "Permissions-Policy feature 'geolocation' specified in the "
            "'allow' option cannot be delegated by the creating context "
            "to the worker script's origin.");
  test::RunPendingTasks();
  EXPECT_EQ(listener->ErrorCount(), 1);
}

}  // namespace blink
