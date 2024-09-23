/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/local_dom_window.h"

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

using network::mojom::ContentSecurityPolicySource;
using network::mojom::ContentSecurityPolicyType;
using network::mojom::WebSandboxFlags;

class LocalDOMWindowTest : public PageTestBase {
 protected:
  void NavigateWithSandbox(
      const KURL& url,
      WebSandboxFlags sandbox_flags = WebSandboxFlags::kAll) {
    auto params = WebNavigationParams::CreateWithEmptyHTMLForTesting(url);
    MockPolicyContainerHost mock_policy_container_host;
    params->policy_container = std::make_unique<blink::WebPolicyContainer>(
        blink::WebPolicyContainerPolicies(),
        mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
    params->policy_container->policies.sandbox_flags = sandbox_flags;
    GetFrame().Loader().CommitNavigation(std::move(params),
                                         /*extra_data=*/nullptr);
    test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }
};

TEST_F(LocalDOMWindowTest, AttachExecutionContext) {
  auto* scheduler = GetFrame().GetFrameScheduler();
  auto* window = GetFrame().DomWindow();
  EXPECT_TRUE(
      window->GetAgent()->event_loop()->IsSchedulerAttachedForTest(scheduler));
  window->FrameDestroyed();
  EXPECT_FALSE(
      window->GetAgent()->event_loop()->IsSchedulerAttachedForTest(scheduler));
}

TEST_F(LocalDOMWindowTest, referrerPolicyParsing) {
  LocalDOMWindow* window = GetFrame().DomWindow();
  EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
            window->GetReferrerPolicy());

  struct TestCase {
    const char* policy;
    network::mojom::ReferrerPolicy expected;
    bool uses_legacy_tokens;
  } tests[] = {
      {"", network::mojom::ReferrerPolicy::kDefault, false},
      // Test that invalid policy values are ignored.
      {"not-a-real-policy", network::mojom::ReferrerPolicy::kDefault, false},
      {"not-a-real-policy,also-not-a-real-policy",
       network::mojom::ReferrerPolicy::kDefault, false},
      {"not-a-real-policy,unsafe-url", network::mojom::ReferrerPolicy::kAlways,
       false},
      {"unsafe-url,not-a-real-policy", network::mojom::ReferrerPolicy::kAlways,
       false},
      // Test parsing each of the policy values.
      {"always", network::mojom::ReferrerPolicy::kAlways, true},
      {"default",
       ReferrerUtils::MojoReferrerPolicyResolveDefault(
           network::mojom::ReferrerPolicy::kDefault),
       true},
      {"never", network::mojom::ReferrerPolicy::kNever, true},
      {"no-referrer", network::mojom::ReferrerPolicy::kNever, false},
      {"no-referrer-when-downgrade",
       network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, false},
      {"origin", network::mojom::ReferrerPolicy::kOrigin, false},
      {"origin-when-crossorigin",
       network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, true},
      {"origin-when-cross-origin",
       network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, false},
      {"same-origin", network::mojom::ReferrerPolicy::kSameOrigin, false},
      {"strict-origin", network::mojom::ReferrerPolicy::kStrictOrigin, false},
      {"strict-origin-when-cross-origin",
       network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin, false},
      {"unsafe-url", network::mojom::ReferrerPolicy::kAlways},
  };

  for (const auto test : tests) {
    window->SetReferrerPolicy(network::mojom::ReferrerPolicy::kDefault);
    if (test.uses_legacy_tokens) {
      // Legacy tokens are supported only for meta-specified policy.
      window->ParseAndSetReferrerPolicy(test.policy, kPolicySourceHttpHeader);
      EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
                window->GetReferrerPolicy());
      window->ParseAndSetReferrerPolicy(test.policy, kPolicySourceMetaTag);
    } else {
      window->ParseAndSetReferrerPolicy(test.policy, kPolicySourceHttpHeader);
    }
    EXPECT_EQ(test.expected, window->GetReferrerPolicy()) << test.policy;
  }
}

TEST_F(LocalDOMWindowTest, referrerPolicyParsingWithCommas) {
  LocalDOMWindow* window = GetFrame().DomWindow();
  EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
            window->GetReferrerPolicy());

  struct TestCase {
    const char* policy;
    network::mojom::ReferrerPolicy expected;
  } tests[] = {
      {"same-origin,strict-origin",
       network::mojom::ReferrerPolicy::kStrictOrigin},
      {"same-origin,not-a-real-policy,strict-origin",
       network::mojom::ReferrerPolicy::kStrictOrigin},
      {"strict-origin, same-origin, not-a-real-policy",
       network::mojom::ReferrerPolicy::kSameOrigin},
  };

  for (const auto test : tests) {
    window->SetReferrerPolicy(network::mojom::ReferrerPolicy::kDefault);
    // Policies containing commas are ignored when specified by a Meta element.
    window->ParseAndSetReferrerPolicy(test.policy, kPolicySourceMetaTag);
    EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
              window->GetReferrerPolicy());

    // Header-specified policy permits commas and returns the last valid policy.
    window->ParseAndSetReferrerPolicy(test.policy, kPolicySourceHttpHeader);
    EXPECT_EQ(test.expected, window->GetReferrerPolicy()) << test.policy;
  }
}

TEST_F(LocalDOMWindowTest, OutgoingReferrer) {
  NavigateTo(KURL("https://www.example.com/hoge#fuga?piyo"));
  EXPECT_EQ("https://www.example.com/hoge",
            GetFrame().DomWindow()->OutgoingReferrer());
}

TEST_F(LocalDOMWindowTest, OutgoingReferrerWithUniqueOrigin) {
  NavigateWithSandbox(
      KURL("https://www.example.com/hoge#fuga?piyo"),
      ~WebSandboxFlags::kAutomaticFeatures & ~WebSandboxFlags::kScripts);
  EXPECT_TRUE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
  EXPECT_EQ(String(), GetFrame().DomWindow()->OutgoingReferrer());
}

TEST_F(LocalDOMWindowTest, EnforceSandboxFlags) {
  NavigateWithSandbox(KURL("http://example.test/"), ~WebSandboxFlags::kOrigin);
  EXPECT_FALSE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
  EXPECT_FALSE(
      GetFrame().DomWindow()->GetSecurityOrigin()->IsPotentiallyTrustworthy());

  NavigateWithSandbox(KURL("http://example.test/"));
  EXPECT_TRUE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
  EXPECT_FALSE(
      GetFrame().DomWindow()->GetSecurityOrigin()->IsPotentiallyTrustworthy());

  // A unique origin does not bypass secure context checks unless it
  // is also potentially trustworthy.
  {
    url::ScopedSchemeRegistryForTests scoped_registry;
    url::AddStandardScheme("very-special-scheme", url::SCHEME_WITH_HOST);
#if DCHECK_IS_ON()
    WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
    SchemeRegistry::RegisterURLSchemeBypassingSecureContextCheck(
        "very-special-scheme");
    NavigateWithSandbox(KURL("very-special-scheme://example.test"));
    EXPECT_TRUE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
    EXPECT_FALSE(GetFrame()
                     .DomWindow()
                     ->GetSecurityOrigin()
                     ->IsPotentiallyTrustworthy());
  }

  {
    url::ScopedSchemeRegistryForTests scoped_registry;
    url::AddStandardScheme("very-special-scheme", url::SCHEME_WITH_HOST);
    url::AddSecureScheme("very-special-scheme");
    NavigateWithSandbox(KURL("very-special-scheme://example.test"));
    EXPECT_TRUE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
    EXPECT_TRUE(GetFrame()
                    .DomWindow()
                    ->GetSecurityOrigin()
                    ->IsPotentiallyTrustworthy());

    NavigateWithSandbox(KURL("https://example.test"));
    EXPECT_TRUE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
    EXPECT_TRUE(GetFrame()
                    .DomWindow()
                    ->GetSecurityOrigin()
                    ->IsPotentiallyTrustworthy());
  }
}

TEST_F(LocalDOMWindowTest, UserAgent) {
  EXPECT_EQ(GetFrame().DomWindow()->UserAgent(),
            GetFrame().Loader().UserAgent());
}

// Tests ExecutionContext::GetContentSecurityPolicyForCurrentWorld().
TEST_F(PageTestBase, CSPForWorld) {
  using ::testing::ElementsAre;

  // Set a CSP for the main world.
  const char* kMainWorldCSP = "connect-src https://google.com;";
  GetFrame().DomWindow()->GetContentSecurityPolicy()->AddPolicies(
      ParseContentSecurityPolicies(
          kMainWorldCSP, ContentSecurityPolicyType::kEnforce,
          ContentSecurityPolicySource::kHTTP,
          *(GetFrame().DomWindow()->GetSecurityOrigin())));
  const Vector<
      network::mojom::blink::ContentSecurityPolicyPtr>& parsed_main_world_csp =
      GetFrame().DomWindow()->GetContentSecurityPolicy()->GetParsedPolicies();

  LocalFrame* frame = &GetFrame();
  ScriptState* main_world_script_state = ToScriptStateForMainWorld(frame);
  v8::Isolate* isolate = main_world_script_state->GetIsolate();

  constexpr int kIsolatedWorldWithoutCSPId = 1;
  DOMWrapperWorld* world_without_csp =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldWithoutCSPId);
  ASSERT_TRUE(world_without_csp->IsIsolatedWorld());
  ScriptState* isolated_world_without_csp_script_state =
      ToScriptState(frame, *world_without_csp);

  const char* kIsolatedWorldCSP = "script-src 'none';";
  constexpr int kIsolatedWorldWithCSPId = 2;
  DOMWrapperWorld* world_with_csp =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldWithCSPId);
  ASSERT_TRUE(world_with_csp->IsIsolatedWorld());
  ScriptState* isolated_world_with_csp_script_state =
      ToScriptState(frame, *world_with_csp);
  IsolatedWorldCSP::Get().SetContentSecurityPolicy(
      kIsolatedWorldWithCSPId, kIsolatedWorldCSP,
      SecurityOrigin::Create(KURL("chrome-extension://123")));

  // Returns the csp headers being used for the current world.
  auto get_csp = [this]()
      -> const Vector<network::mojom::blink::ContentSecurityPolicyPtr>& {
    auto* csp =
        GetFrame().DomWindow()->GetContentSecurityPolicyForCurrentWorld();
    return csp->GetParsedPolicies();
  };

  {
    SCOPED_TRACE("In main world.");
    ScriptState::Scope scope(main_world_script_state);
    EXPECT_EQ(get_csp(), parsed_main_world_csp);
  }

  {
    SCOPED_TRACE("In isolated world without csp.");
    ScriptState::Scope scope(isolated_world_without_csp_script_state);

    // If we are in an isolated world with no CSP defined, we use the main world
    // CSP.
    EXPECT_EQ(get_csp(), parsed_main_world_csp);
  }

  {
    SCOPED_TRACE("In isolated world with csp.");
    ScriptState::Scope scope(isolated_world_with_csp_script_state);
    // We use the isolated world's CSP if it specified one.
    EXPECT_EQ(get_csp()[0]->header->header_value, kIsolatedWorldCSP);
  }
}

TEST_F(LocalDOMWindowTest, ConsoleMessageCategory) {
  auto unknown_location = CaptureSourceLocation(String(), 0, 0);
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kError, "Kaboom!",
      std::move(unknown_location));
  console_message->SetCategory(mojom::blink::ConsoleMessageCategory::Cors);
  auto* window = GetFrame().DomWindow();
  window->AddConsoleMessageImpl(console_message, false);
  auto* message_storage = &GetFrame().GetPage()->GetConsoleMessageStorage();
  EXPECT_EQ(1u, message_storage->size());
  for (WTF::wtf_size_t i = 0; i < message_storage->size(); ++i) {
    EXPECT_EQ(mojom::blink::ConsoleMessageCategory::Cors,
              *message_storage->at(i)->Category());
  }
}
TEST_F(LocalDOMWindowTest, NavigationId) {
  String navigation_id1 = GetFrame().DomWindow()->GetNavigationId();
  GetFrame().DomWindow()->GenerateNewNavigationId();
  String navigation_id2 = GetFrame().DomWindow()->GetNavigationId();
  GetFrame().DomWindow()->GenerateNewNavigationId();
  String navigation_id3 = GetFrame().DomWindow()->GetNavigationId();
  EXPECT_NE(navigation_id1, navigation_id2);
  EXPECT_NE(navigation_id1, navigation_id3);
  EXPECT_NE(navigation_id2, navigation_id3);
}

TEST_F(LocalDOMWindowTest, StorageAccessApiStatus) {
  EXPECT_EQ(GetFrame().DomWindow()->GetStorageAccessApiStatus(),
            net::StorageAccessApiStatus::kNone);
  GetFrame().DomWindow()->SetStorageAccessApiStatus(
      net::StorageAccessApiStatus::kAccessViaAPI);
  EXPECT_EQ(GetFrame().DomWindow()->GetStorageAccessApiStatus(),
            net::StorageAccessApiStatus::kAccessViaAPI);
}

TEST_F(LocalDOMWindowTest, CanExecuteScriptsDuringDetach) {
  GetFrame().Loader().DetachDocument();
  EXPECT_NE(GetFrame().DomWindow(), nullptr);

  // When detach has started and FrameLoader::document_loader_ is nullptr, but
  // the window hasn't been detached from its frame yet, CanExecuteScripts()
  // should return false and not crash.
  // This case is reachable when the only thing blocking a main frame's load
  // event from firing is an iframe's load event, and that iframe is detached,
  // thus unblocking the load event. If the detaching window is accessed inside
  // a load event listener in that case, we may call CanExecuteScripts() in this
  // partially-detached state.
  // See crbug.com/350874762, crbug.com/41482536 and crbug.com/41484859.
  EXPECT_FALSE(
      GetFrame().DomWindow()->CanExecuteScripts(kAboutToExecuteScript));
}

}  // namespace blink
