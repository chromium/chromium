// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/aria_domains.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/css_domains.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/dom_scenario_runner.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/html_domains.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/mathml_domains.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/svg_domains.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {

class HtmlAndAria : public DomScenarioDomainSpecification {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnyHtmlTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return fuzztest::OneOf(AnyHtmlAttributeNameValuePair(),
                           AnyAriaAttributeNameValuePair());
  }
  fuzztest::Domain<std::string> AnyStyles() override {
    return AnyCssDeclaration();
  }
  fuzztest::Domain<std::string> AnyText() override {
    return fuzztest::PrintableAsciiString();
  }
  int GetMaxDomNodes() override { return 8; }
  int GetMaxAttributesPerNode() override { return 3; }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(
        html_names::TagToQualifiedName(html_names::HTMLTag::kBody));
  }
  fuzztest::Domain<std::string> AnyStylesheet() override {
    return fuzztest::OneOf(
        fuzztest::Just(std::string("")),
        fuzztest::Just(
            std::string("#id_0::before, #id_1::before { content: '['; } "
                        "#id_2::after, #id_3::after { content: ']'; } "
                        "#id_4::before, #id_5::before { content: '['; } "
                        "#id_4::after, #id_5::after { content: ']'; }")));
  }
};

class CanvasFallbackContent : public HtmlAndAria {
 public:
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(
        html_names::TagToQualifiedName(html_names::HTMLTag::kCanvas));
  }
};

class SvgAndAria : public DomScenarioDomainSpecification {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnySvgTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return fuzztest::OneOf(AnySvgAttributeNameValuePair(),
                           AnyAriaAttributeNameValuePair());
  }
  fuzztest::Domain<std::string> AnyStyles() override {
    return AnyCssDeclaration();
  }
  fuzztest::Domain<std::string> AnyText() override {
    return fuzztest::PrintableAsciiString();
  }
  int GetMaxDomNodes() override { return 8; }
  int GetMaxAttributesPerNode() override { return 3; }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(svg_names::kSVGTag);
  }
};

class MathMLAndAria : public DomScenarioDomainSpecification {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnyMathMlTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return fuzztest::OneOf(AnyMathMlAttributeNameValuePair(),
                           AnyAriaAttributeNameValuePair());
  }
  fuzztest::Domain<std::string> AnyStyles() override {
    return AnyCssDeclaration();
  }
  fuzztest::Domain<std::string> AnyText() override {
    return fuzztest::PrintableAsciiString();
  }
  int GetMaxDomNodes() override { return 8; }
  int GetMaxAttributesPerNode() override { return 3; }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(mathml_names::kMathTag);
  }
};

class AccessibilityDomScenarioRunner : public DomScenarioRunner {
 public:
  AccessibilityDomScenarioRunner() = default;

  void HtmlAndAria(const DomScenario& input) { RunTest(input); }
  void CanvasFallbackContent(const DomScenario& input) { RunTest(input); }
  void SvgAndAria(const DomScenario& input) { RunTest(input); }
  void MathMLAndAria(const DomScenario& input) { RunTest(input); }

 protected:
  // Observer hooks to add accessibility tree printing.
  void ObserveInitialDOM(
      const HeapVector<Member<Element>>& created_elements) override {
    std::string ax_tree = AccessibilityTest::PrintAXTree(GetDocument());
    LogIfEnabled(base::StrCat({"\n\nINITIAL ACCESSIBILITY TREE\n", ax_tree}));
  }
  void ObserveModifiedDOM(
      const HeapVector<Member<Element>>& created_elements) override {
    std::string ax_tree = AccessibilityTest::PrintAXTree(GetDocument());
    LogIfEnabled(base::StrCat({"\n\nMODIFIED ACCESSIBILITY TREE\n", ax_tree}));
  }
};

FUZZ_TEST_F(AccessibilityDomScenarioRunner, HtmlAndAria)
    .WithDomains(BuildDomScenarios<HtmlAndAria>());

FUZZ_TEST_F(AccessibilityDomScenarioRunner, CanvasFallbackContent)
    .WithDomains(BuildDomScenarios<CanvasFallbackContent>());

FUZZ_TEST_F(AccessibilityDomScenarioRunner, SvgAndAria)
    .WithDomains(BuildDomScenarios<SvgAndAria>());

FUZZ_TEST_F(AccessibilityDomScenarioRunner, MathMLAndAria)
    .WithDomains(BuildDomScenarios<MathMLAndAria>());

}  // namespace blink
