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

#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

using network::mojom::ContentSecurityPolicySource;
using network::mojom::ContentSecurityPolicyType;

class LocalDOMWindowTest : public PageTestBase {};

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
    bool is_legacy;
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
      {"default", network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade,
       true},
      {"never", network::mojom::ReferrerPolicy::kNever, true},
      {"no-referrer", network::mojom::ReferrerPolicy::kNever, false},
      {"default", network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade,
       true},
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

  for (auto test : tests) {
    window->SetReferrerPolicy(network::mojom::ReferrerPolicy::kDefault);
    if (test.is_legacy) {
      // Legacy keyword support must be explicitly enabled for the policy to
      // parse successfully.
      window->ParseAndSetReferrerPolicy(test.policy);
      EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
                window->GetReferrerPolicy());
      window->ParseAndSetReferrerPolicy(test.policy, true);
    } else {
      window->ParseAndSetReferrerPolicy(test.policy);
    }
    EXPECT_EQ(test.expected, window->GetReferrerPolicy()) << test.policy;
  }
}

TEST_F(LocalDOMWindowTest, OutgoingReferrer) {
  NavigateTo(KURL("https://www.example.com/hoge#fuga?piyo"));
  EXPECT_EQ("https://www.example.com/hoge",
            GetFrame().DomWindow()->OutgoingReferrer());
}

TEST_F(LocalDOMWindowTest, OutgoingReferrerWithUniqueOrigin) {
  NavigateTo(KURL("https://www.example.com/hoge#fuga?piyo"),
             {{http_names::kContentSecurityPolicy, "sandbox allow-scripts"}});
  EXPECT_TRUE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
  EXPECT_EQ(String(), GetFrame().DomWindow()->OutgoingReferrer());
}

TEST_F(LocalDOMWindowTest, EnforceSandboxFlags) {
  NavigateTo(KURL("http://example.test/"), {{http_names::kContentSecurityPolicy,
                                             "sandbox allow-same-origin"}});
  EXPECT_FALSE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
  EXPECT_FALSE(
      GetFrame().DomWindow()->GetSecurityOrigin()->IsPotentiallyTrustworthy());

  NavigateTo(KURL("http://example.test/"),
             {{http_names::kContentSecurityPolicy, "sandbox"}});
  EXPECT_TRUE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
  EXPECT_FALSE(
      GetFrame().DomWindow()->GetSecurityOrigin()->IsPotentiallyTrustworthy());

  // A unique origin does not bypass secure context checks unless it
  // is also potentially trustworthy.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("very-special-scheme", url::SCHEME_WITH_HOST);
  SchemeRegistry::RegisterURLSchemeBypassingSecureContextCheck(
      "very-special-scheme");
  NavigateTo(KURL("very-special-scheme://example.test"),
             {{http_names::kContentSecurityPolicy, "sandbox"}});
  EXPECT_TRUE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
  EXPECT_FALSE(
      GetFrame().DomWindow()->GetSecurityOrigin()->IsPotentiallyTrustworthy());

  SchemeRegistry::RegisterURLSchemeAsSecure("very-special-scheme");
  NavigateTo(KURL("very-special-scheme://example.test"),
             {{http_names::kContentSecurityPolicy, "sandbox"}});
  EXPECT_TRUE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
  EXPECT_TRUE(
      GetFrame().DomWindow()->GetSecurityOrigin()->IsPotentiallyTrustworthy());

  NavigateTo(KURL("https://example.test"),
             {{http_names::kContentSecurityPolicy, "sandbox"}});
  EXPECT_TRUE(GetFrame().DomWindow()->GetSecurityOrigin()->IsOpaque());
  EXPECT_TRUE(
      GetFrame().DomWindow()->GetSecurityOrigin()->IsPotentiallyTrustworthy());
}

// Tests ExecutionContext::GetContentSecurityPolicyForCurrentWorld().
TEST_F(PageTestBase, CSPForWorld) {
  using ::testing::ElementsAre;

  // Set a CSP for the main world.
  const char* kMainWorldCSP = "connect-src https://google.com;";
  GetFrame().DomWindow()->GetContentSecurityPolicy()->DidReceiveHeader(
      kMainWorldCSP, ContentSecurityPolicyType::kEnforce,
      ContentSecurityPolicySource::kHTTP);

  LocalFrame* frame = &GetFrame();
  ScriptState* main_world_script_state = ToScriptStateForMainWorld(frame);
  v8::Isolate* isolate = main_world_script_state->GetIsolate();

  constexpr int kIsolatedWorldWithoutCSPId = 1;
  scoped_refptr<DOMWrapperWorld> world_without_csp =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldWithoutCSPId);
  ASSERT_TRUE(world_without_csp->IsIsolatedWorld());
  ScriptState* isolated_world_without_csp_script_state =
      ToScriptState(frame, *world_without_csp);

  const char* kIsolatedWorldCSP = "script-src 'none';";
  constexpr int kIsolatedWorldWithCSPId = 2;
  scoped_refptr<DOMWrapperWorld> world_with_csp =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldWithCSPId);
  ASSERT_TRUE(world_with_csp->IsIsolatedWorld());
  ScriptState* isolated_world_with_csp_script_state =
      ToScriptState(frame, *world_with_csp);
  IsolatedWorldCSP::Get().SetContentSecurityPolicy(
      kIsolatedWorldWithCSPId, kIsolatedWorldCSP,
      SecurityOrigin::Create(KURL("chrome-extension://123")));

  // Returns the csp headers being used for the current world.
  auto get_csp_headers = [this]() {
    auto* csp =
        GetFrame().DomWindow()->GetContentSecurityPolicyForCurrentWorld();
    return csp->Headers();
  };

  {
    SCOPED_TRACE("In main world.");
    ScriptState::Scope scope(main_world_script_state);
    EXPECT_THAT(get_csp_headers(),
                ElementsAre(CSPHeaderAndType(
                    {kMainWorldCSP, ContentSecurityPolicyType::kEnforce})));
  }

  {
    SCOPED_TRACE("In isolated world without csp.");
    ScriptState::Scope scope(isolated_world_without_csp_script_state);

    // If we are in an isolated world with no CSP defined, we use the main world
    // CSP.
    EXPECT_THAT(get_csp_headers(),
                ElementsAre(CSPHeaderAndType(
                    {kMainWorldCSP, ContentSecurityPolicyType::kEnforce})));
  }

  {
    SCOPED_TRACE("In isolated world with csp.");
    ScriptState::Scope scope(isolated_world_with_csp_script_state);
    // We use the isolated world's CSP if it specified one.
    EXPECT_THAT(get_csp_headers(),
                ElementsAre(CSPHeaderAndType(
                    {kIsolatedWorldCSP, ContentSecurityPolicyType::kEnforce})));
  }
}

}  // namespace blink
