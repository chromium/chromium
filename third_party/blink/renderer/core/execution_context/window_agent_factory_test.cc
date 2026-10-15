// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/window_agent_factory.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/core/execution_context/agent_cluster_key.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "url/url_util.h"

namespace blink {

namespace {

class CustomTestingPlatformSupport : public TestingPlatformSupport {
 public:
  bool ShouldUsePerBrowsingInstanceWindowAgent(
      const WebSecurityOrigin& origin) override {
    return String(origin.Host()) == opted_in_extension_id_;
  }

  void SetOptedInExtensionId(const String& extension_id) {
    opted_in_extension_id_ = extension_id;
  }

 private:
  String opted_in_extension_id_;
};

}  // namespace

class WindowAgentFactoryTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<CustomTestingPlatformSupport> platform_;
};

TEST_F(WindowAgentFactoryTest,
       ExtensionWindowAgentIsolationWithOriginRegistration) {
  const char* kExtensionScheme = "chrome-extension";
  const char* kOptedInExtensionId = "glbjnfimcajjenihimblfaponejbkoph";
  const char* kStandardExtensionId = "abcdefghijklmnopqrstuvwxyzabcdef";

  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kExtensionScheme, url::SCHEME_WITH_HOST);
#if DCHECK_IS_ON()
  SetIsBeforeThreadCreatedForTest();
#endif
  CommonSchemeRegistry::RegisterURLSchemeAsExtension(kExtensionScheme);

  auto page_holder1 = std::make_unique<DummyPageHolder>();
  auto page_holder2 = std::make_unique<DummyPageHolder>();
  WindowAgentFactory& factory1 =
      page_holder1->GetFrame().window_agent_factory();
  WindowAgentFactory& factory2 =
      page_holder2->GetFrame().window_agent_factory();

  AgentClusterKey opted_in_key =
      AgentClusterKey::CreateOriginKeyed(SecurityOrigin::CreateFromString(
          String("chrome-extension://") + kOptedInExtensionId));
  AgentClusterKey standard_key =
      AgentClusterKey::CreateOriginKeyed(SecurityOrigin::CreateFromString(
          String("chrome-extension://") + kStandardExtensionId));

  // 1. By default (not opted-in via `Platform`), all extension frames in the
  // same process share a single WindowAgent.
  {
    WindowAgent* agent1 =
        factory1.GetAgentForAgentClusterKey(false, standard_key);
    WindowAgent* agent2 =
        factory2.GetAgentForAgentClusterKey(false, standard_key);
    EXPECT_EQ(agent1, agent2);
  }

  // 2. When the extension ID is indicated by `Platform` for BackForwardCache,
  // extension frames in different BrowsingInstances get isolated WindowAgents
  // per WindowAgentFactory.
  {
    platform_->SetOptedInExtensionId(kOptedInExtensionId);

    WindowAgent* agent1 =
        factory1.GetAgentForAgentClusterKey(false, opted_in_key);
    WindowAgent* agent2 =
        factory2.GetAgentForAgentClusterKey(false, opted_in_key);
    EXPECT_NE(agent1, agent2);

    // Multiple lookups within the same factory still return the same agent.
    EXPECT_EQ(agent1, factory1.GetAgentForAgentClusterKey(false, opted_in_key));
    EXPECT_EQ(agent2, factory2.GetAgentForAgentClusterKey(false, opted_in_key));

    // Standard extensions in the same process remain unaffected and shared.
    WindowAgent* standard_agent1 =
        factory1.GetAgentForAgentClusterKey(false, standard_key);
    WindowAgent* standard_agent2 =
        factory2.GetAgentForAgentClusterKey(false, standard_key);
    EXPECT_EQ(standard_agent1, standard_agent2);
  }

  CommonSchemeRegistry::RemoveURLSchemeAsExtensionForTest(kExtensionScheme);
}

}  // namespace blink
