// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
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

namespace {

// Helper to create a domain for an attribute name-value pair.
fuzztest::Domain<std::pair<QualifiedName, std::string>>
AttributeNameValuePairDomain(const QualifiedName& attribute,
                             fuzztest::Domain<std::string> value_domain) {
  return fuzztest::Map(
      [attribute](const std::string& value) {
        return std::make_pair(attribute, value);
      },
      std::move(value_domain));
}

}  // namespace

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
  bool UseShadowDOM() override { return true; }
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

class ProblematicMarkup : public DomScenarioDomainSpecification {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override {
    return fuzztest::Map(
        [](html_names::HTMLTag tag) -> QualifiedName {
          return html_names::TagToQualifiedName(tag);
        },
        fuzztest::ElementOf<html_names::HTMLTag>(
            {html_names::HTMLTag::kArea, html_names::HTMLTag::kDialog,
             html_names::HTMLTag::kIFrame, html_names::HTMLTag::kInput,
             html_names::HTMLTag::kLabel, html_names::HTMLTag::kMap,
             html_names::HTMLTag::kRp, html_names::HTMLTag::kRt,
             html_names::HTMLTag::kRuby, html_names::HTMLTag::kWbr}));
  }

  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return fuzztest::OneOf(
        AttributeNameValuePairDomain(
            html_names::kRoleAttr,
            fuzztest::OneOf(AnyNonAbstractAriaRole(),
                            fuzztest::Just(std::string("none")))),
        AttributeNameValuePairDomain(
            html_names::kAriaOwnsAttr,
            AnyValueForAriaAttribute(html_names::kAriaOwnsAttr)),
        AttributeNameValuePairDomain(
            html_names::kAriaActivedescendantAttr,
            AnyValueForAriaAttribute(html_names::kAriaActivedescendantAttr)),
        AttributeNameValuePairDomain(
            html_names::kAriaHiddenAttr,
            AnyValueForAriaAttribute(html_names::kAriaHiddenAttr)),
        AttributeNameValuePairDomain(
            html_names::kContenteditableAttr,
            AnyValueForHtmlAttribute(html_names::kContenteditableAttr)),
        AttributeNameValuePairDomain(
            html_names::kTypeAttr,
            AnyValueForHtmlAttribute(html_names::kTypeAttr)),
        AttributeNameValuePairDomain(
            html_names::kInertAttr,
            AnyValueForHtmlAttribute(html_names::kInertAttr)),
        AttributeNameValuePairDomain(
            html_names::kHiddenAttr,
            AnyValueForHtmlAttribute(html_names::kHiddenAttr)));
  }

  fuzztest::Domain<std::string> AnyStyles() override {
    return fuzztest::FlatMap(
        [](CSSPropertyID property) {
          return fuzztest::Map(
              [property](const std::string& value) {
                CSSPropertyName prop_name(property);
                return base::StrCat(
                    {prop_name.ToAtomicString().Utf8(), ":", value});
              },
              AnyPlausibleValueForCSSProperty(property));
        },
        fuzztest::ElementOf<CSSPropertyID>(
            {CSSPropertyID::kDisplay, CSSPropertyID::kVisibility,
             CSSPropertyID::kContentVisibility}));
  }
  fuzztest::Domain<std::string> AnyText() override {
    // Include problematic text content like soft hyphens (U+00AD).
    return fuzztest::OneOf(fuzztest::PrintableAsciiString(),
                           fuzztest::Map(
                               [](const std::string& base_text) {
                                 std::string result = base_text;
                                 // Insert a soft hyphen somewhere in the middle
                                 // if possible.
                                 size_t mid = result.length() / 2;
                                 if (mid > 0 && mid < result.length()) {
                                   result.insert(mid, "\u00AD");
                                 }
                                 return result;
                               },
                               fuzztest::PrintableAsciiString()));
  }
  int GetMaxDomNodes() override { return 20; }
  int GetMaxAttributesPerNode() override { return 8; }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(
        html_names::TagToQualifiedName(html_names::HTMLTag::kBody));
  }
};

class ValidTableAndAria : public DomScenarioDomainSpecification {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnyHtmlTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return fuzztest::OneOf(AnyHtmlTableAttributeNameValuePair(),
                           AnyAriaTableAttributeNameValuePair(),
                           AnyAriaTableRoleNameValuePair());
  }
  fuzztest::Domain<std::string> AnyStyles() override {
    return AnyCssDeclaration();
  }
  fuzztest::Domain<std::string> AnyText() override {
    return fuzztest::PrintableAsciiString();
  }
  int GetMaxDomNodes() override { return GetPredefinedNodes()->size(); }
  int GetMaxAttributesPerNode() override { return 3; }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(
        html_names::TagToQualifiedName(html_names::HTMLTag::kBody));
  }
  std::optional<std::vector<NodeSpecification>> GetPredefinedNodes() override {
    // Create the table during test execution, due to use of QualifiedName.
    return std::vector<NodeSpecification>{
        // <table> (parent: root) index 0.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTable),
         .initial_state = {.parent_index = kIndexOfRootElement}},
        // <tr> (parent: table) index 1.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTr),
         .initial_state = {.parent_index = 0}},
        // <th> (parent: tr) index 2.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTh),
         .initial_state = {.parent_index = 1, .text = "Header 1"}},
        // <th> (parent: tr) index 3.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTh),
         .initial_state = {.parent_index = 1, .text = "Header 2"}},
        // <th> (parent: tr) index 4.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTh),
         .initial_state = {.parent_index = 1, .text = "Header 3"}},
        // <tr> (parent: table) index 5.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTr),
         .initial_state = {.parent_index = 0}},
        // <td> (parent: tr) index 6.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 5, .text = "Cell 1"}},
        // <td> (parent: tr) index 7.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 5, .text = "Cell 2"}},
        // <td> (parent: tr) index 8.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 5, .text = "Cell 3"}},
        // <tr> (parent: table) index 9.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTr),
         .initial_state = {.parent_index = 0}},
        // <td> (parent: tr) index 10.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 9, .text = "Cell 1"}},
        // <td> (parent: tr) index 11.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 9, .text = "Cell 2"}},
        // <td> (parent: tr) index 12.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 9, .text = "Cell 3"}},
    };
  }
};

class AccessibilityDomScenarioRunner : public DomScenarioRunner {
 public:
  AccessibilityDomScenarioRunner() = default;

  void HtmlAndAria(const DomScenario& input) { RunTest(input); }
  void CanvasFallbackContent(const DomScenario& input) { RunTest(input); }
  void SvgAndAria(const DomScenario& input) { RunTest(input); }
  void MathMLAndAria(const DomScenario& input) { RunTest(input); }
  void ProblematicMarkup(const DomScenario& input) { RunTest(input); }
  void ValidTableAndAria(const DomScenario& input) { RunTest(input); }

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

FUZZ_TEST_F(AccessibilityDomScenarioRunner, ProblematicMarkup)
    .WithDomains(BuildDomScenarios<ProblematicMarkup>());

FUZZ_TEST_F(AccessibilityDomScenarioRunner, ValidTableAndAria)
    .WithDomains(BuildDomScenarios<ValidTableAndAria>());

}  // namespace blink
