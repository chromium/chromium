// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/integrity_policy.h"

#include "services/network/public/cpp/integrity_policy.h"
#include "services/network/public/mojom/integrity_policy.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/testing/main_thread_isolate.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

class IntegrityPolicyTest : public testing::Test {
 public:
  IntegrityPolicyTest()
      : secure_url("https://example.test/index.html"),
        secure_origin(SecurityOrigin::Create(secure_url)) {}
  ~IntegrityPolicyTest() override {
    execution_context->NotifyContextDestroyed();
  }

 protected:
  void SetUp() override { CreateExecutionContext(); }

  void CreateExecutionContext() {
    execution_context = MakeGarbageCollected<NullExecutionContext>();
    // Create a PolicyContainer with the right IntegrityPolicy
    MockPolicyContainerHost policy_container_host;
    mojom::blink::PolicyContainerPoliciesPtr policy_container_policies =
        mojom::blink::PolicyContainerPolicies::New();
    network::IntegrityPolicy integrity_policy;
    integrity_policy.blocked_destinations.emplace_back(
        network::mojom::IntegrityPolicy_Destination::kScript);
    integrity_policy.sources.emplace_back(
        network::mojom::IntegrityPolicy_Source::kInline);
    policy_container_policies->integrity_policy = integrity_policy;
    std::unique_ptr<PolicyContainer> policy_container =
        std::make_unique<PolicyContainer>(
            policy_container_host.BindNewEndpointAndPassDedicatedRemote(),
            std::move(policy_container_policies));
    execution_context->SetUpSecurityContextForTesting();
    execution_context->GetSecurityContext().SetSecurityOriginForTesting(
        secure_origin);
    execution_context->SetPolicyContainer(std::move(policy_container));
  }

  test::TaskEnvironment task_environment;
  KURL secure_url;
  scoped_refptr<SecurityOrigin> secure_origin;
  Persistent<NullExecutionContext> execution_context;
};

TEST_F(IntegrityPolicyTest, AllowRequestTest) {
  Vector<uint8_t> decoded;
  ASSERT_TRUE(Base64Decode("foobar", decoded));
  IntegrityMetadataSet kNonEmptyIntegrityMetadata;
  IntegrityMetadata m(network::mojom::blink::IntegrityAlgorithm::kSha256,
                      decoded);
  kNonEmptyIntegrityMetadata.Insert(std::move(m));

  struct TestCase {
    network::mojom::RequestDestination destination;
    network::mojom::RequestMode mode;
    const IntegrityMetadataSet& metadata;
    const KURL& url;
    bool allow;
  } cases[] = {
      {network::mojom::RequestDestination::kScript,
       network::mojom::RequestMode::kCors, IntegrityMetadataSet(),
       KURL("https://www.example.com"), false},
      {network::mojom::RequestDestination::kStyle,
       network::mojom::RequestMode::kCors, IntegrityMetadataSet(),
       KURL("https://www.example.com"), true},
      {network::mojom::RequestDestination::kScript,
       network::mojom::RequestMode::kNoCors, kNonEmptyIntegrityMetadata,
       KURL("https://www.example.com"), false},
      {network::mojom::RequestDestination::kScript,
       network::mojom::RequestMode::kCors, kNonEmptyIntegrityMetadata,
       KURL("https://www.example.com"), true},
      {network::mojom::RequestDestination::kScript,
       network::mojom::RequestMode::kCors, IntegrityMetadataSet(),
       KURL("data:text/html;charset=utf-8,Hello%20World"), true},
      {network::mojom::RequestDestination::kScript,
       network::mojom::RequestMode::kCors, IntegrityMetadataSet(),
       KURL(
           "blob:https://www.example.com/31de0587-36d7-4939-848f-8d13fdfd400c"),
       true},
  };

  // Enforced
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "destination: " << test.destination
                                    << ", mode: " << test.mode);
    bool result = IntegrityPolicy::AllowRequest(
        execution_context, execution_context->GetCurrentWorld(),
        test.destination, test.mode, test.metadata, test.url);
    EXPECT_EQ(test.allow, result);
  }
}

}  // namespace blink
