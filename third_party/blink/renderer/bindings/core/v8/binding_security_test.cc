// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"

#include "services/network/public/mojom/cors_origin_pattern.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"

namespace blink {

namespace {
const char kMainFrame[] = "https://example.com/main.html";
const char kSameOriginTarget[] = "https://example.com/target.html";
const char kSameOriginDomainTarget[] = "https://sub.example.com/target.html";
const char kCrossOriginTarget[] = "https://not-example.com/target.html";

const char kTargetHTML[] =
    "<!DOCTYPE html>"
    "<script>"
    "  (window.opener || window.top).postMessage('yay', '*');"
    "</script>";
const char kSameOriginDomainTargetHTML[] =
    "<!DOCTYPE html>"
    "<script>"
    "  document.domain = 'example.com';"
    "  (window.opener || window.top).postMessage('yay', '*');"
    "</script>";
}

class BindingSecurityCounterTest
    : public SimTest,
      public testing::WithParamInterface<const char*> {
 public:
  enum class OriginDisposition { CrossOrigin, SameOrigin, SameOriginDomain };

  BindingSecurityCounterTest() = default;

  void LoadWindowAndAccessProperty(OriginDisposition which_origin,
                                   const String& property) {
    const char* target_url;
    const char* target_html;
    switch (which_origin) {
      case OriginDisposition::CrossOrigin:
        target_url = kCrossOriginTarget;
        target_html = kTargetHTML;
        break;
      case OriginDisposition::SameOrigin:
        target_url = kSameOriginTarget;
        target_html = kTargetHTML;
        break;
      case OriginDisposition::SameOriginDomain:
        target_url = kSameOriginDomainTarget;
        target_html = kSameOriginDomainTargetHTML;
        break;
    }

    SimRequest main(kMainFrame, "text/html");
    SimRequest target(target_url, "text/html");
    const String document = Format(
        "<!DOCTYPE html>"
        "<script>"
        "  {}"
        "  window.addEventListener('message', e => {{"
        "    window.other = e.source.{};"
        "    console.log('yay');"
        "  }});"
        "  var w = window.open('{}');"
        "</script>",
        which_origin == OriginDisposition::SameOriginDomain
            ? "document.domain = 'example.com';"
            : "",
        property, target_url);

    LoadURL(kMainFrame);
    main.Complete(document);
    target.Complete(target_html);
    test::RunPendingTasks();
  }

  void LoadFrameAndAccessProperty(OriginDisposition which_origin,
                                  const String& property) {
    const char* target_url;
    const char* target_html;
    switch (which_origin) {
      case OriginDisposition::CrossOrigin:
        target_url = kCrossOriginTarget;
        target_html = kTargetHTML;
        break;
      case OriginDisposition::SameOrigin:
        target_url = kSameOriginTarget;
        target_html = kTargetHTML;
        break;
      case OriginDisposition::SameOriginDomain:
        target_url = kSameOriginDomainTarget;
        target_html = kSameOriginDomainTargetHTML;
        break;
    }
    SimRequest main(kMainFrame, "text/html");
    SimRequest target(target_url, "text/html");
    const String document = Format(
        "<!DOCTYPE html>"
        "<body>"
        "<script>"
        "  {}"
        "  var i = document.createElement('iframe');"
        "  window.addEventListener('message', e => {{"
        "    window.other = e.source.{};"
        "    console.log('yay');"
        "  }});"
        "  i.src = '{}';"
        "  document.body.appendChild(i);"
        "</script>",
        which_origin == OriginDisposition::SameOriginDomain
            ? "document.domain = 'example.com';"
            : "",
        property, target_url);

    LoadURL(kMainFrame);
    main.Complete(document);
    target.Complete(target_html);
    test::RunPendingTasks();
  }
};

INSTANTIATE_TEST_SUITE_P(WindowProperties,
                         BindingSecurityCounterTest,
                         testing::Values("window",
                                         "self",
                                         "location",
                                         "close",
                                         "closed",
                                         "focus",
                                         "blur",
                                         "frames",
                                         "length",
                                         "top",
                                         "opener",
                                         "parent",
                                         "postMessage"));

TEST_P(BindingSecurityCounterTest, CrossOriginWindow) {
  LoadWindowAndAccessProperty(OriginDisposition::CrossOrigin, GetParam());
  EXPECT_TRUE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccess));
  EXPECT_TRUE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

TEST_P(BindingSecurityCounterTest, SameOriginWindow) {
  LoadWindowAndAccessProperty(OriginDisposition::SameOrigin, GetParam());
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccess));
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

TEST_P(BindingSecurityCounterTest, SameOriginDomainWindow) {
  LoadWindowAndAccessProperty(OriginDisposition::SameOriginDomain, GetParam());
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccess));
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_TRUE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

TEST_P(BindingSecurityCounterTest, CrossOriginFrame) {
  LoadFrameAndAccessProperty(OriginDisposition::CrossOrigin, GetParam());
  EXPECT_TRUE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccess));
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

TEST_P(BindingSecurityCounterTest, SameOriginFrame) {
  LoadFrameAndAccessProperty(OriginDisposition::SameOrigin, GetParam());
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccess));
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

TEST_P(BindingSecurityCounterTest, SameOriginDomainFrame) {
  LoadFrameAndAccessProperty(OriginDisposition::SameOriginDomain, GetParam());
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccess));
  EXPECT_FALSE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_TRUE(GetDocument().Loader()->GetUseCounter().IsCounted(
      WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

namespace {

constexpr int32_t kTestIsolatedWorldId = 17;
// Stands in for an extension origin (chrome-extension://<id>). A standard
// scheme is used because non-standard schemes are not registered in unit
// tests and would produce an opaque origin; the logic under test does not
// depend on the scheme.
constexpr char kExtensionOrigin[] = "https://extension.test";

}  // namespace

// Tests for cross-context access originating from an isolated world (e.g. an
// extension content script). Such access is governed by the same-origin policy
// of the frames themselves, matching the main world, regardless of the
// isolated world's security origin or origin access allowlist entries.
class BindingSecurityIsolatedWorldTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    SimRequest main(kMainFrame, "text/html");
    SimRequest same_origin_child(kSameOriginTarget, "text/html");
    SimRequest cross_origin_child(kCrossOriginTarget, "text/html");
    LoadURL(kMainFrame);
    main.Complete(
        Format("<!DOCTYPE html>"
               "<iframe src='{}'></iframe>"
               "<iframe src='{}'></iframe>",
               kSameOriginTarget, kCrossOriginTarget));
    same_origin_child.Complete("<!DOCTYPE html>");
    cross_origin_child.Complete("<!DOCTYPE html>");
    test::RunPendingTasks();

    world_ = DOMWrapperWorld::EnsureIsolatedWorld(Window().GetIsolate(),
                                                  kTestIsolatedWorldId);
  }

  void TearDown() override {
    SecurityPolicy::ClearOriginAccessList();
    DOMWrapperWorld::SetIsolatedWorldSecurityOrigin(kTestIsolatedWorldId,
                                                    nullptr);
    world_ = nullptr;
    SimTest::TearDown();
  }

  void RegisterExtensionOrigin() {
    DOMWrapperWorld::SetIsolatedWorldSecurityOrigin(
        kTestIsolatedWorldId,
        SecurityOrigin::CreateFromString(kExtensionOrigin));
  }

  void AllowHost(const String& host) {
    SecurityPolicy::AddOriginAccessAllowListEntry(
        *SecurityOrigin::CreateFromString(kExtensionOrigin), "https", host,
        /*port=*/0, network::mojom::CorsDomainMatchMode::kAllowSubdomains,
        network::mojom::CorsPortMatchMode::kAllowAnyPort,
        network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  }

  LocalDOMWindow* SameOriginChild() {
    return To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild())
        ->DomWindow();
  }

  LocalDOMWindow* CrossOriginChild() {
    return To<LocalFrame>(GetDocument()
                              .GetFrame()
                              ->Tree()
                              .FirstChild()
                              ->Tree()
                              .NextSibling())
        ->DomWindow();
  }

  bool ShouldAllowAccessToContext(LocalDOMWindow* target) {
    ScriptState* accessing = ToScriptState(&Window(), *world_);
    ScriptState* target_state = ToScriptState(target, *world_);
    return BindingSecurity::ShouldAllowAccessToV8Context(accessing,
                                                         target_state);
  }

 protected:
  Persistent<DOMWrapperWorld> world_;
};

TEST_F(BindingSecurityIsolatedWorldTest, SameOriginAllowedCrossOriginDenied) {
  EXPECT_TRUE(ShouldAllowAccessToContext(SameOriginChild()));
  EXPECT_FALSE(ShouldAllowAccessToContext(CrossOriginChild()));
}

TEST_F(BindingSecurityIsolatedWorldTest,
       IsolatedWorldOriginAndAllowlistDoNotGrantCrossOriginAccess) {
  RegisterExtensionOrigin();
  AllowHost("example.com");
  AllowHost("not-example.com");
  // Even when the isolated world's security origin has allowlist access to
  // the cross-origin target, cross-origin frame access is denied.
  EXPECT_TRUE(ShouldAllowAccessToContext(SameOriginChild()));
  EXPECT_FALSE(ShouldAllowAccessToContext(CrossOriginChild()));
}

TEST_F(BindingSecurityIsolatedWorldTest,
       IsolatedWorldMatchingTargetOriginDoesNotGrantCrossOriginAccess) {
  // Even if the isolated world's own security origin matches the cross-origin
  // child frame, access from the main frame's isolated world to the
  // cross-origin child's isolated world is still denied.
  DOMWrapperWorld::SetIsolatedWorldSecurityOrigin(
      kTestIsolatedWorldId,
      SecurityOrigin::CreateFromString(kCrossOriginTarget));
  EXPECT_TRUE(ShouldAllowAccessToContext(SameOriginChild()));
  EXPECT_FALSE(ShouldAllowAccessToContext(CrossOriginChild()));
}

TEST_F(BindingSecurityIsolatedWorldTest, FeatureDisabledAllowsEverything) {
  ScopedIsolatedWorldCrossContextAccessCheckForTest disabled(false);
  // Previously isolated worlds unconditionally bypassed the cross-context
  // access check; the kill switch must restore that behaviour.
  EXPECT_TRUE(ShouldAllowAccessToContext(SameOriginChild()));
  EXPECT_TRUE(ShouldAllowAccessToContext(CrossOriginChild()));
}

}  // namespace blink
