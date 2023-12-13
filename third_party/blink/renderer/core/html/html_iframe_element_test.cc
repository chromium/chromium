// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_iframe_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_runtime_features_base.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

class HTMLIFrameElementTest : public testing::Test {
 public:
  scoped_refptr<const SecurityOrigin> GetOriginForPermissionsPolicy(
      HTMLIFrameElement* element) {
    return element->GetOriginForPermissionsPolicy();
  }

  void SetUp() final {
    const KURL document_url("http://example.com");
    page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
    window_ = page_holder_->GetFrame().DomWindow();
    window_->document()->SetURL(document_url);
    window_->GetSecurityContext().SetSecurityOriginForTesting(
        SecurityOrigin::Create(document_url));
    frame_element_ =
        MakeGarbageCollected<HTMLIFrameElement>(*window_->document());
  }

  void TearDown() final {
    frame_element_.Clear();
    window_.Clear();
    page_holder_.reset();
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<LocalDOMWindow> window_;
  Persistent<HTMLIFrameElement> frame_element_;
};

// Test that the correct origin is used when constructing the container policy,
// and that frames which should inherit their parent document's origin do so.
TEST_F(HTMLIFrameElementTest, FramesUseCorrectOrigin) {
  frame_element_->setAttribute(html_names::kSrcAttr,
                               AtomicString("about:blank"));
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForPermissionsPolicy(frame_element_);
  EXPECT_TRUE(effective_origin->IsSameOriginWith(window_->GetSecurityOrigin()));

  frame_element_->setAttribute(
      html_names::kSrcAttr,
      AtomicString("data:text/html;base64,PHRpdGxlPkFCQzwvdGl0bGU+"));
  effective_origin = GetOriginForPermissionsPolicy(frame_element_);
  EXPECT_FALSE(
      effective_origin->IsSameOriginWith(window_->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsOpaque());

  frame_element_->setAttribute(html_names::kSrcAttr,
                               AtomicString("http://example.net/"));
  effective_origin = GetOriginForPermissionsPolicy(frame_element_);
  EXPECT_FALSE(
      effective_origin->IsSameOriginWith(window_->GetSecurityOrigin()));
  EXPECT_FALSE(effective_origin->IsOpaque());
}

// Test that a unique origin is used when constructing the container policy in a
// sandboxed iframe.
TEST_F(HTMLIFrameElementTest, SandboxFramesUseCorrectOrigin) {
  frame_element_->setAttribute(html_names::kSandboxAttr, g_empty_atom);
  frame_element_->setAttribute(html_names::kSrcAttr,
                               AtomicString("http://example.com/"));
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForPermissionsPolicy(frame_element_);
  EXPECT_FALSE(
      effective_origin->IsSameOriginWith(window_->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsOpaque());

  frame_element_->setAttribute(html_names::kSrcAttr,
                               AtomicString("http://example.net/"));
  effective_origin = GetOriginForPermissionsPolicy(frame_element_);
  EXPECT_FALSE(
      effective_origin->IsSameOriginWith(window_->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsOpaque());
}

// Test that a sandboxed iframe with the allow-same-origin sandbox flag uses the
// parent document's origin for the container policy.
TEST_F(HTMLIFrameElementTest, SameOriginSandboxFramesUseCorrectOrigin) {
  frame_element_->setAttribute(html_names::kSandboxAttr,
                               AtomicString("allow-same-origin"));
  frame_element_->setAttribute(html_names::kSrcAttr,
                               AtomicString("http://example.com/"));
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForPermissionsPolicy(frame_element_);
  EXPECT_TRUE(effective_origin->IsSameOriginWith(window_->GetSecurityOrigin()));
  EXPECT_FALSE(effective_origin->IsOpaque());
}

// Test that the parent document's origin is used when constructing the
// container policy in a srcdoc iframe.
TEST_F(HTMLIFrameElementTest, SrcdocFramesUseCorrectOrigin) {
  frame_element_->setAttribute(html_names::kSrcdocAttr,
                               AtomicString("<title>title</title>"));
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForPermissionsPolicy(frame_element_);
  EXPECT_TRUE(effective_origin->IsSameOriginWith(window_->GetSecurityOrigin()));
}

// Test that a unique origin is used when constructing the container policy in a
// sandboxed iframe with a srcdoc.
TEST_F(HTMLIFrameElementTest, SandboxedSrcdocFramesUseCorrectOrigin) {
  frame_element_->setAttribute(html_names::kSandboxAttr, g_empty_atom);
  frame_element_->setAttribute(html_names::kSrcdocAttr,
                               AtomicString("<title>title</title>"));
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForPermissionsPolicy(frame_element_);
  EXPECT_FALSE(
      effective_origin->IsSameOriginWith(window_->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsOpaque());
}

// Test that iframes with relative src urls correctly construct their origin
// relative to the parent document.
TEST_F(HTMLIFrameElementTest, RelativeURLsUseCorrectOrigin) {
  // Host-relative URLs should resolve to the same domain as the parent.
  frame_element_->setAttribute(html_names::kSrcAttr,
                               AtomicString("index2.html"));
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForPermissionsPolicy(frame_element_);
  EXPECT_TRUE(effective_origin->IsSameOriginWith(window_->GetSecurityOrigin()));

  // Scheme-relative URLs should not resolve to the same domain as the parent.
  frame_element_->setAttribute(html_names::kSrcAttr,
                               AtomicString("//example.net/index2.html"));
  effective_origin = GetOriginForPermissionsPolicy(frame_element_);
  EXPECT_FALSE(
      effective_origin->IsSameOriginWith(window_->GetSecurityOrigin()));
}

// Test that various iframe attribute configurations result in the correct
// container policies.

// Test that the correct container policy is constructed on an iframe element.
TEST_F(HTMLIFrameElementTest, DefaultContainerPolicy) {
  frame_element_->setAttribute(html_names::kSrcAttr,
                               AtomicString("http://example.net/"));
  frame_element_->UpdateContainerPolicyForTests();

  const ParsedPermissionsPolicy& container_policy =
      frame_element_->GetFramePolicy().container_policy;
  EXPECT_EQ(0UL, container_policy.size());
}

// Test that the allow attribute results in a container policy which is
// restricted to the domain in the src attribute.
TEST_F(HTMLIFrameElementTest, AllowAttributeContainerPolicy) {
  frame_element_->setAttribute(html_names::kSrcAttr,
                               AtomicString("http://example.net/"));
  frame_element_->setAttribute(html_names::kAllowAttr,
                               AtomicString("fullscreen"));
  frame_element_->UpdateContainerPolicyForTests();

  const ParsedPermissionsPolicy& container_policy1 =
      frame_element_->GetFramePolicy().container_policy;

  EXPECT_EQ(1UL, container_policy1.size());
  EXPECT_EQ(mojom::blink::PermissionsPolicyFeature::kFullscreen,
            container_policy1[0].feature);
  EXPECT_FALSE(container_policy1[0].matches_all_origins);
  EXPECT_EQ(1UL, container_policy1[0].allowed_origins.size());
  EXPECT_EQ("http://example.net",
            container_policy1[0].allowed_origins.begin()->Serialize());

  frame_element_->setAttribute(html_names::kAllowAttr,
                               AtomicString("payment; fullscreen"));
  frame_element_->UpdateContainerPolicyForTests();

  const ParsedPermissionsPolicy& container_policy2 =
      frame_element_->GetFramePolicy().container_policy;
  EXPECT_EQ(2UL, container_policy2.size());
  EXPECT_TRUE(container_policy2[0].feature ==
                  mojom::blink::PermissionsPolicyFeature::kFullscreen ||
              container_policy2[1].feature ==
                  mojom::blink::PermissionsPolicyFeature::kFullscreen);
  EXPECT_TRUE(container_policy2[0].feature ==
                  mojom::blink::PermissionsPolicyFeature::kPayment ||
              container_policy2[1].feature ==
                  mojom::blink::PermissionsPolicyFeature::kPayment);
  EXPECT_EQ(1UL, container_policy2[0].allowed_origins.size());
  EXPECT_EQ("http://example.net",
            container_policy2[0].allowed_origins.begin()->Serialize());
  EXPECT_FALSE(container_policy2[1].matches_all_origins);
  EXPECT_EQ(1UL, container_policy2[1].allowed_origins.size());
  EXPECT_EQ("http://example.net",
            container_policy2[1].allowed_origins.begin()->Serialize());
}

// Test the ConstructContainerPolicy method when no attributes are set on the
// iframe element.
TEST_F(HTMLIFrameElementTest, ConstructEmptyContainerPolicy) {
  ParsedPermissionsPolicy container_policy =
      frame_element_->ConstructContainerPolicy();
  EXPECT_EQ(0UL, container_policy.size());
}

// Test the ConstructContainerPolicy method when the "allow" attribute is used
// to enable features in the frame.
TEST_F(HTMLIFrameElementTest, ConstructContainerPolicy) {
  frame_element_->setAttribute(html_names::kAllowAttr,
                               AtomicString("payment; usb"));
  ParsedPermissionsPolicy container_policy =
      frame_element_->ConstructContainerPolicy();
  EXPECT_EQ(2UL, container_policy.size());
  EXPECT_EQ(mojom::blink::PermissionsPolicyFeature::kPayment,
            container_policy[0].feature);
  EXPECT_FALSE(container_policy[0].matches_all_origins);
  EXPECT_EQ(1UL, container_policy[0].allowed_origins.size());
  EXPECT_TRUE(container_policy[0].allowed_origins.begin()->DoesMatchOrigin(
      GetOriginForPermissionsPolicy(frame_element_)->ToUrlOrigin()));
  EXPECT_EQ(mojom::blink::PermissionsPolicyFeature::kUsb,
            container_policy[1].feature);
  EXPECT_EQ(1UL, container_policy[1].allowed_origins.size());
  EXPECT_TRUE(container_policy[1].allowed_origins.begin()->DoesMatchOrigin(
      GetOriginForPermissionsPolicy(frame_element_)->ToUrlOrigin()));
}

// Test the ConstructContainerPolicy method when the "allowfullscreen" attribute
// is used to enable fullscreen in the frame.
TEST_F(HTMLIFrameElementTest, ConstructContainerPolicyWithAllowFullscreen) {
  frame_element_->SetBooleanAttribute(html_names::kAllowfullscreenAttr, true);

  ParsedPermissionsPolicy container_policy =
      frame_element_->ConstructContainerPolicy();
  EXPECT_EQ(1UL, container_policy.size());
  EXPECT_EQ(mojom::blink::PermissionsPolicyFeature::kFullscreen,
            container_policy[0].feature);
  EXPECT_TRUE(container_policy[0].matches_all_origins);
}

// Test the ConstructContainerPolicy method when the "allowpaymentrequest"
// attribute is used to enable the paymentrequest API in the frame.
TEST_F(HTMLIFrameElementTest, ConstructContainerPolicyWithAllowPaymentRequest) {
  frame_element_->setAttribute(html_names::kAllowAttr, AtomicString("usb"));
  frame_element_->SetBooleanAttribute(html_names::kAllowpaymentrequestAttr,
                                      true);

  ParsedPermissionsPolicy container_policy =
      frame_element_->ConstructContainerPolicy();
  EXPECT_EQ(2UL, container_policy.size());
  EXPECT_EQ(mojom::blink::PermissionsPolicyFeature::kUsb,
            container_policy[0].feature);
  EXPECT_FALSE(container_policy[0].matches_all_origins);
  EXPECT_EQ(1UL, container_policy[0].allowed_origins.size());
  EXPECT_TRUE(container_policy[0].allowed_origins.begin()->DoesMatchOrigin(
      GetOriginForPermissionsPolicy(frame_element_)->ToUrlOrigin()));
  EXPECT_EQ(mojom::blink::PermissionsPolicyFeature::kPayment,
            container_policy[1].feature);
}

// Test the ConstructContainerPolicy method when both "allowfullscreen" and
// "allowpaymentrequest" attributes are set on the iframe element, and the
// "allow" attribute is also used to override the paymentrequest feature. In the
// resulting container policy, the payment and usb features should be enabled
// only for the frame's origin, (since the allow attribute overrides
// allowpaymentrequest,) while fullscreen should be enabled for all origins.
TEST_F(HTMLIFrameElementTest, ConstructContainerPolicyWithAllowAttributes) {
  frame_element_->setAttribute(html_names::kAllowAttr,
                               AtomicString("payment; usb"));
  frame_element_->SetBooleanAttribute(html_names::kAllowfullscreenAttr, true);
  frame_element_->SetBooleanAttribute(html_names::kAllowpaymentrequestAttr,
                                      true);

  ParsedPermissionsPolicy container_policy =
      frame_element_->ConstructContainerPolicy();
  EXPECT_EQ(3UL, container_policy.size());
  EXPECT_EQ(mojom::blink::PermissionsPolicyFeature::kPayment,
            container_policy[0].feature);
  EXPECT_FALSE(container_policy[0].matches_all_origins);
  EXPECT_EQ(1UL, container_policy[0].allowed_origins.size());
  EXPECT_TRUE(container_policy[0].allowed_origins.begin()->DoesMatchOrigin(
      GetOriginForPermissionsPolicy(frame_element_)->ToUrlOrigin()));
  EXPECT_EQ(mojom::blink::PermissionsPolicyFeature::kUsb,
            container_policy[1].feature);
  EXPECT_EQ(1UL, container_policy[1].allowed_origins.size());
  EXPECT_TRUE(container_policy[1].allowed_origins.begin()->DoesMatchOrigin(
      GetOriginForPermissionsPolicy(frame_element_)->ToUrlOrigin()));
  EXPECT_EQ(mojom::blink::PermissionsPolicyFeature::kFullscreen,
            container_policy[2].feature);
}

using HTMLIFrameElementSimTest = SimTest;

TEST_F(HTMLIFrameElementSimTest, PolicyAttributeParsingError) {
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe policy="bad-feature-name"></iframe>
  )");

  // Note: Parsing of policy attribute string, i.e. call to
  // HTMLFrameOwnerElement::UpdateRequiredPolicy(), happens twice in above
  // situation:
  // - HTMLFrameOwnerElement::LoadOrRedirectSubframe()
  // - HTMLIFrameElement::ParseAttribute()
  EXPECT_EQ(ConsoleMessages().size(), 2u);
  for (const auto& message : ConsoleMessages()) {
    EXPECT_TRUE(
        message.StartsWith("Unrecognized document policy feature name"));
  }
}

TEST_F(HTMLIFrameElementSimTest, AllowAttributeParsingError) {
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe
      allow="bad-feature-name"
      allowfullscreen
      allowpayment
      sandbox=""></iframe>
  )");

  EXPECT_EQ(ConsoleMessages().size(), 1u)
      << "Allow attribute parsing should only generate console message once, "
         "even though there might be multiple call to "
         "PermissionsPolicyParser::ParseAttribute.";
  EXPECT_TRUE(ConsoleMessages().front().StartsWith("Unrecognized feature"))
      << "Expect permissions policy parser raising error for unrecognized "
         "feature but got: "
      << ConsoleMessages().front();
}

TEST_F(HTMLIFrameElementSimTest, Adauctionheaders_SecureContext_Allowed) {
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe adauctionheaders></iframe>
  )");

  EXPECT_TRUE(ConsoleMessages().empty());
}

TEST_F(HTMLIFrameElementSimTest, Adauctionheaders_InsecureContext_NotAllowed) {
  SimRequest main_resource("http://example.com", "text/html");
  LoadURL("http://example.com");
  main_resource.Complete(R"(
    <iframe adauctionheaders></iframe>
  )");

  EXPECT_EQ(ConsoleMessages().size(), 1u);
  EXPECT_TRUE(ConsoleMessages().front().StartsWith(
      "adAuctionHeaders: Protected Audience APIs "
      "are only available in secure contexts."))
      << "Unexpected error; got: " << ConsoleMessages().front();
}

TEST_F(HTMLIFrameElementSimTest, Sharedstoragewritable_SecureContext_Allowed) {
  WebRuntimeFeaturesBase::EnableSharedStorageAPI(true);
  WebRuntimeFeaturesBase::EnableSharedStorageAPIM118(true);
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe
      allow="shared-storage"
      sharedstoragewritable></iframe>
  )");

  EXPECT_TRUE(ConsoleMessages().empty());
}

TEST_F(HTMLIFrameElementSimTest,
       Sharedstoragewritable_InsecureContext_NotAllowed) {
  WebRuntimeFeaturesBase::EnableSharedStorageAPI(true);
  WebRuntimeFeaturesBase::EnableSharedStorageAPIM118(true);
  SimRequest main_resource("http://example.com", "text/html");
  LoadURL("http://example.com");
  main_resource.Complete(R"(
    <iframe
      allow="shared-storage"
      sharedstoragewritable></iframe>
  )");

  EXPECT_EQ(ConsoleMessages().size(), 1u);
  EXPECT_TRUE(ConsoleMessages().front().StartsWith(
      "sharedStorageWritable: sharedStorage operations are only available in "
      "secure contexts."))
      << "Expect error that Shared Storage operations are not allowed in "
         "insecure contexts but got: "
      << ConsoleMessages().front();
}

}  // namespace blink
