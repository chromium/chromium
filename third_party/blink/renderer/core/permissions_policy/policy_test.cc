// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/permissions_policy/dom_feature_policy.h"
#include "third_party/blink/renderer/core/permissions_policy/iframe_policy.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {
constexpr char kSelfOrigin[] = "https://selforigin.com";
constexpr char kOriginA[] = "https://example.com";
constexpr char kOriginASubdomain[] = "https://sub.example.com";
constexpr char kOriginB[] = "https://example.net";
constexpr char kOriginBSubdomain[] = "https://sub.example.net";
}  // namespace

using testing::UnorderedElementsAre;

class PolicyTest : public testing::Test {
 public:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>();

    auto origin = SecurityOrigin::CreateFromString(kSelfOrigin);

    PolicyParserMessageBuffer dummy_logger("", true /* discard_message */);
    auto header = PermissionsPolicyParser::ParseHeader(
        "fullscreen *; payment 'self'; midi 'none'; camera 'self' "
        "https://example.com https://example.net",
        "gyroscope=(self \"https://*.example.com\" \"https://example.net\")",
        origin.get(), dummy_logger, dummy_logger);
    auto permissions_policy = PermissionsPolicy::CreateFromParentPolicy(
        nullptr, header, {}, origin->ToUrlOrigin());

    auto& security_context =
        page_holder_->GetFrame().DomWindow()->GetSecurityContext();
    security_context.SetSecurityOriginForTesting(origin);
    security_context.SetPermissionsPolicy(std::move(permissions_policy));
  }

  DOMFeaturePolicy* GetPolicy() const { return policy_; }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<DOMFeaturePolicy> policy_;
};

class DOMFeaturePolicyTest : public PolicyTest {
 public:
  void SetUp() override {
    PolicyTest::SetUp();
    policy_ = MakeGarbageCollected<DOMFeaturePolicy>(
        page_holder_->GetFrame().DomWindow());
  }
};

class IFramePolicyTest : public PolicyTest {
 public:
  void SetUp() override {
    PolicyTest::SetUp();
    policy_ = MakeGarbageCollected<IFramePolicy>(
        page_holder_->GetFrame().DomWindow(), ParsedPermissionsPolicy(),
        SecurityOrigin::CreateFromString(kSelfOrigin));
  }
};

TEST_F(DOMFeaturePolicyTest, TestAllowsFeature) {
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "badfeature"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "payment"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginA));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginB));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginB));
  EXPECT_FALSE(
      GetPolicy()->allowsFeature(nullptr, "camera", "https://badorigin.com"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "geolocation", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "gyroscope"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "gyroscope", kOriginA));
  EXPECT_TRUE(
      GetPolicy()->allowsFeature(nullptr, "gyroscope", kOriginASubdomain));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "gyroscope", kOriginB));
  EXPECT_FALSE(
      GetPolicy()->allowsFeature(nullptr, "gyroscope", kOriginBSubdomain));
}

TEST_F(DOMFeaturePolicyTest, TestGetAllowList) {
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "camera"),
              UnorderedElementsAre(kSelfOrigin, kOriginA, kOriginB));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "payment"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "geolocation"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "fullscreen"),
              UnorderedElementsAre("*"));
  EXPECT_TRUE(
      GetPolicy()->getAllowlistForFeature(nullptr, "badfeature").empty());
  EXPECT_TRUE(GetPolicy()->getAllowlistForFeature(nullptr, "midi").empty());
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "sync-xhr"),
              UnorderedElementsAre("*"));
  EXPECT_THAT(
      GetPolicy()->getAllowlistForFeature(nullptr, "gyroscope"),
      UnorderedElementsAre(kSelfOrigin, kOriginB, "https://*.example.com"));
}

TEST_F(DOMFeaturePolicyTest, TestAllowedFeatures) {
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  EXPECT_TRUE(allowed_features.Contains("fullscreen"));
  EXPECT_TRUE(allowed_features.Contains("payment"));
  EXPECT_TRUE(allowed_features.Contains("camera"));
  EXPECT_TRUE(allowed_features.Contains("gyroscope"));
  // "geolocation" has default policy as allowed on self origin.
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
  EXPECT_FALSE(allowed_features.Contains("midi"));
  // "sync-xhr" is allowed on all origins
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
}

TEST_F(IFramePolicyTest, TestAllowsFeature) {
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "badfeature"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "fullscreen", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "payment"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginA));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginB));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginA));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginB));
  EXPECT_FALSE(
      GetPolicy()->allowsFeature(nullptr, "camera", "https://badorigin.com"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "geolocation", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "gyroscope"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "gyroscope", kOriginA));
  EXPECT_FALSE(
      GetPolicy()->allowsFeature(nullptr, "gyroscope", kOriginASubdomain));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "gyroscope", kOriginB));
  EXPECT_FALSE(
      GetPolicy()->allowsFeature(nullptr, "gyroscope", kOriginBSubdomain));
}

TEST_F(IFramePolicyTest, TestGetAllowList) {
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "camera"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "payment"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "geolocation"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "fullscreen"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_TRUE(
      GetPolicy()->getAllowlistForFeature(nullptr, "badfeature").empty());
  EXPECT_TRUE(GetPolicy()->getAllowlistForFeature(nullptr, "midi").empty());
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "sync-xhr"),
              UnorderedElementsAre("*"));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "gyroscope"),
              UnorderedElementsAre(kSelfOrigin));
}

TEST_F(IFramePolicyTest, TestSameOriginAllowedFeatures) {
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  // These features are allowed in a same origin context, and not restricted by
  // the parent document's policy.
  EXPECT_TRUE(allowed_features.Contains("fullscreen"));
  EXPECT_TRUE(allowed_features.Contains("payment"));
  EXPECT_TRUE(allowed_features.Contains("camera"));
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  EXPECT_TRUE(allowed_features.Contains("gyroscope"));
  // "midi" is restricted by the parent document's policy.
  EXPECT_FALSE(allowed_features.Contains("midi"));
  // "sync-xhr" is allowed on all origins.
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
  // This feature does not exist, so should not be advertised as allowed.
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
}

TEST_F(IFramePolicyTest, TestCrossOriginAllowedFeatures) {
  // Update the iframe's policy, given a new origin.
  GetPolicy()->UpdateContainerPolicy(
      ParsedPermissionsPolicy(), SecurityOrigin::CreateFromString(kOriginA));
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  // None of these features should be allowed in a cross-origin context.
  EXPECT_FALSE(allowed_features.Contains("fullscreen"));
  EXPECT_FALSE(allowed_features.Contains("payment"));
  EXPECT_FALSE(allowed_features.Contains("camera"));
  EXPECT_FALSE(allowed_features.Contains("geolocation"));
  EXPECT_FALSE(allowed_features.Contains("midi"));
  EXPECT_FALSE(allowed_features.Contains("gyroscope"));
  // "sync-xhr" is allowed on all origins.
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
  // This feature does not exist, so should not be advertised as allowed.
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
}

TEST_F(IFramePolicyTest, TestCombinedPolicyOnOriginA) {
  PolicyParserMessageBuffer dummy_logger("", true /* discard_message */);
  ParsedPermissionsPolicy container_policy =
      PermissionsPolicyParser::ParseAttribute(
          "geolocation 'src'; payment 'none'; midi; camera 'src'; gyroscope "
          "'src'",
          SecurityOrigin::CreateFromString(kSelfOrigin),
          SecurityOrigin::CreateFromString(kOriginA), dummy_logger);
  GetPolicy()->UpdateContainerPolicy(
      container_policy, SecurityOrigin::CreateFromString(kOriginA));
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  // These features are not explicitly allowed.
  EXPECT_FALSE(allowed_features.Contains("fullscreen"));
  EXPECT_FALSE(allowed_features.Contains("payment"));
  EXPECT_FALSE(allowed_features.Contains("gyroscope"));
  // These features are explicitly allowed.
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  EXPECT_TRUE(allowed_features.Contains("camera"));
  // "midi" is allowed by the attribute, but still blocked by the parent
  // document's policy.
  EXPECT_FALSE(allowed_features.Contains("midi"));
  // "sync-xhr" is still implicitly allowed on all origins.
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
  // This feature does not exist, so should not be advertised as allowed.
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
}

TEST_F(IFramePolicyTest, TestCombinedPolicyOnOriginASubdomain) {
  PolicyParserMessageBuffer dummy_logger("", true /* discard_message */);
  ParsedPermissionsPolicy container_policy =
      PermissionsPolicyParser::ParseAttribute(
          "geolocation 'src'; payment 'none'; midi; camera 'src'; gyroscope "
          "'src'",
          SecurityOrigin::CreateFromString(kSelfOrigin),
          SecurityOrigin::CreateFromString(kOriginASubdomain), dummy_logger);
  GetPolicy()->UpdateContainerPolicy(
      container_policy, SecurityOrigin::CreateFromString(kOriginASubdomain));
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  // These features are not explicitly allowed.
  EXPECT_FALSE(allowed_features.Contains("fullscreen"));
  EXPECT_FALSE(allowed_features.Contains("payment"));
  // These features are explicitly allowed.
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  // These are allowed by the attribute, but still blocked by the parent policy.
  EXPECT_FALSE(allowed_features.Contains("midi"));
  EXPECT_FALSE(allowed_features.Contains("camera"));
  // These features are allowed via wildcard matching.
  EXPECT_TRUE(allowed_features.Contains("gyroscope"));
  // "sync-xhr" is still implicitly allowed on all origins.
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
  // This feature does not exist, so should not be advertised as allowed.
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
}

TEST_F(IFramePolicyTest, TestCombinedPolicyOnOriginB) {
  PolicyParserMessageBuffer dummy_logger("", true /* discard_message */);
  ParsedPermissionsPolicy container_policy =
      PermissionsPolicyParser::ParseAttribute(
          "geolocation 'src'; payment 'none'; midi; camera 'src'; gyroscope "
          "'src'",
          SecurityOrigin::CreateFromString(kSelfOrigin),
          SecurityOrigin::CreateFromString(kOriginB), dummy_logger);
  GetPolicy()->UpdateContainerPolicy(
      container_policy, SecurityOrigin::CreateFromString(kOriginB));
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  // These features are not explicitly allowed.
  EXPECT_FALSE(allowed_features.Contains("fullscreen"));
  EXPECT_FALSE(allowed_features.Contains("payment"));
  // These features are explicitly allowed.
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  EXPECT_TRUE(allowed_features.Contains("camera"));
  EXPECT_TRUE(allowed_features.Contains("gyroscope"));
  // These are allowed by the attribute, but still blocked by the parent policy.
  EXPECT_FALSE(allowed_features.Contains("midi"));
  // "sync-xhr" is still implicitly allowed on all origins.
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
  // This feature does not exist, so should not be advertised as allowed.
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
}

TEST_F(IFramePolicyTest, TestCombinedPolicyOnOriginBSubdomain) {
  PolicyParserMessageBuffer dummy_logger("", true /* discard_message */);
  ParsedPermissionsPolicy container_policy =
      PermissionsPolicyParser::ParseAttribute(
          "geolocation 'src'; payment 'none'; midi; camera 'src'; gyroscope "
          "'src'",
          SecurityOrigin::CreateFromString(kSelfOrigin),
          SecurityOrigin::CreateFromString(kOriginBSubdomain), dummy_logger);
  GetPolicy()->UpdateContainerPolicy(
      container_policy, SecurityOrigin::CreateFromString(kOriginBSubdomain));
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  // These features are not explicitly allowed.
  EXPECT_FALSE(allowed_features.Contains("fullscreen"));
  EXPECT_FALSE(allowed_features.Contains("payment"));
  EXPECT_FALSE(allowed_features.Contains("gyroscope"));
  // These features are explicitly allowed.
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  // These are allowed by the attribute, but still blocked by the parent policy.
  EXPECT_FALSE(allowed_features.Contains("midi"));
  EXPECT_FALSE(allowed_features.Contains("camera"));
  // "sync-xhr" is still implicitly allowed on all origins.
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
  // This feature does not exist, so should not be advertised as allowed.
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
}

}  // namespace blink
