// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
            AnyValueForHtmlAttribute(html_names::kHiddenAttr)),
        AttributeNameValuePairDomain(
            html_names::kTabindexAttr,
            AnyValueForHtmlAttribute(html_names::kTabindexAttr)));
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
  static constexpr int kNumNodes = 13;
  int GetMaxDomNodes() override { return kNumNodes; }
  int GetMaxAttributesPerNode() override { return 3; }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(
        html_names::TagToQualifiedName(html_names::HTMLTag::kBody));
  }
  std::optional<PredefinedNodesConfig> GetPredefinedNodes() override {
    // Create the table during test execution, due to use of QualifiedName.
    auto nodes = std::vector<NodeSpecification>{
        // <table> (parent: root) index 0.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTable),
         .initial_state = {.parent_index = kIndexOfRootElement}},
        // <tr> (parent: table) index 1.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTr),
         .initial_state = {.parent_index = 0}},
        // <th> (parent: tr) index 2.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTh),
         .initial_state = {.parent_index = 1}},
        // <th> (parent: tr) index 3.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTh),
         .initial_state = {.parent_index = 1}},
        // <th> (parent: tr) index 4.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTh),
         .initial_state = {.parent_index = 1}},
        // <tr> (parent: table) index 5.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTr),
         .initial_state = {.parent_index = 0}},
        // <td> (parent: tr) index 6.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 5}},
        // <td> (parent: tr) index 7.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 5}},
        // <td> (parent: tr) index 8.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 5}},
        // <tr> (parent: table) index 9.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTr),
         .initial_state = {.parent_index = 0}},
        // <td> (parent: tr) index 10.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 9}},
        // <td> (parent: tr) index 11.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 9}},
        // <td> (parent: tr) index 12.
        {.tag = html_names::TagToQualifiedName(html_names::HTMLTag::kTd),
         .initial_state = {.parent_index = 9}},
    };
    auto states_domain =
        fuzztest::VectorOf(AnyNodeState(this, kNumNodes,
                                        AnyAttributeNameValuePair(),
                                        AnyStyles()))
            .WithSize(kNumNodes);
    return PredefinedNodesConfig{std::move(nodes), states_domain,
                                 states_domain};
  }
};

class AnimationAndAria : public DomScenarioDomainSpecification {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnyHtmlTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return fuzztest::OneOf(AnyHtmlAttributeNameValuePair(),
                           AnyAriaAttributeNameValuePair());
  }
  fuzztest::Domain<std::string> AnyStyles() override {
    auto animation_styles = fuzztest::Map(
        [](const std::string& name, const std::string& duration,
           const std::string& delay, const std::string& direction,
           const std::string& fill_mode, const std::string& timing,
           const std::string& count) {
          return base::StrCat(
              {"animation-name:", name, ";animation-duration:", duration,
               ";animation-delay:", delay, ";animation-direction:", direction,
               ";animation-fill-mode:", fill_mode,
               ";animation-timing-function:", timing,
               ";animation-iteration-count:", count});
        },
        AnyCSSAnimationNameValue(), AnyCSSAnimationDurationValue(),
        AnyCSSAnimationDelayValue(), AnyCSSAnimationDirectionValue(),
        AnyCSSAnimationFillModeValue(), AnyCSSAnimationTimingFunctionValue(),
        AnyCSSAnimationIterationCountValue());
    return animation_styles;
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
};

class ReadingFlowAndOrder : public DomScenarioDomainSpecification {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnyHtmlTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    auto known_idref = fuzztest::Map(
        [](int i) { return base::StrCat({"id_", base::NumberToString(i)}); },
        fuzztest::InRange(0, kNumNodes - 1));
    auto aria_owns_domain =
        fuzztest::OneOf(known_idref,
                        fuzztest::Map(
                            [](const std::string& id1, const std::string& id2) {
                              return base::StrCat({id1, " ", id2});
                            },
                            known_idref, known_idref),
                        fuzztest::Map(
                            [](const std::string& id1, const std::string& id2,
                               const std::string& id3) {
                              return base::StrCat({id1, " ", id2, " ", id3});
                            },
                            known_idref, known_idref, known_idref));
    auto tabindex_domain = fuzztest::Map(
        [](int tab) {
          return std::make_pair(html_names::kTabindexAttr.ToQualifiedName(),
                                base::NumberToString(tab));
        },
        fuzztest::Arbitrary<int>());
    return fuzztest::OneOf(
        tabindex_domain, AnyAriaAttributeNameValuePair(),
        AttributeNameValuePairDomain(html_names::kAriaOwnsAttr,
                                     std::move(aria_owns_domain)));
  }
  fuzztest::Domain<std::string> AnyStyles() override {
    auto grid_styles = fuzztest::Map(
        [](const std::string& area_template,
           const std::string& reading_flow_value) {
          return base::StrCat({"display: grid; grid-template-areas: \"",
                               area_template,
                               "\"; grid-template-columns: 1fr 1fr 1fr; "
                               "grid-template-rows: 1fr 1fr; reading-flow: ",
                               reading_flow_value});
        },
        fuzztest::ElementOf<std::string>({
            "a b c\" \"d e f",
            "c a b\" \"f d e",
            "b c a\" \"e f d",
            "d e f\" \"a b c",
            "f d e\" \"c a b",
            "e f d\" \"b c a",
        }),
        AnyCSSReadingFlowValue());
    auto flex_styles = fuzztest::Map(
        [](const std::string& direction,
           const std::string& reading_flow_value) {
          return base::StrCat({"display: flex; flex-direction: ", direction,
                               "; reading-flow: ", reading_flow_value});
        },
        AnyCSSFlexDirectionValue(), AnyCSSReadingFlowValue());
    return fuzztest::OneOf(grid_styles, flex_styles);
  }
  fuzztest::Domain<std::string> AnyText() override {
    return fuzztest::PrintableAsciiString();
  }
  static constexpr int kNumNodes = 7;
  int GetMaxDomNodes() override { return kNumNodes; }
  int GetMaxAttributesPerNode() override { return 2; }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(
        html_names::TagToQualifiedName(html_names::HTMLTag::kBody));
  }
  bool AllowReparenting() override { return false; }
  std::optional<PredefinedNodesConfig> GetPredefinedNodes() override {
    auto child_styles = []() {
      return fuzztest::OneOf(
          fuzztest::Map(
              [](const std::string& area) {
                return base::StrCat({"grid-area: ", area, ";"});
              },
              fuzztest::ElementOf<std::string>({"a", "b", "c", "d", "e", "f"})),
          fuzztest::Map(
              [](int order) {
                return base::StrCat(
                    {"order: ", base::NumberToString(order), ";"});
              },
              fuzztest::Arbitrary<int>()));
    };
    // Only tags and parent indices are fixed; everything else is fuzzed
    // via initial_states_domain and modified_states_domain.
    auto tag = [](html_names::HTMLTag t) {
      return html_names::TagToQualifiedName(t);
    };
    auto nodes = std::vector<NodeSpecification>{
        {.tag = tag(html_names::HTMLTag::kDiv),
         .initial_state = {.parent_index = kIndexOfRootElement}},
        {.tag = tag(html_names::HTMLTag::kButton),
         .initial_state = {.parent_index = 0}},
        {.tag = tag(html_names::HTMLTag::kDiv),
         .initial_state = {.parent_index = 0}},
        {.tag = tag(html_names::HTMLTag::kInput),
         .initial_state = {.parent_index = 0}},
        {.tag = tag(html_names::HTMLTag::kButton),
         .initial_state = {.parent_index = 0}},
        {.tag = tag(html_names::HTMLTag::kDiv),
         .initial_state = {.parent_index = 0}},
        {.tag = tag(html_names::HTMLTag::kButton),
         .initial_state = {.parent_index = 0}}};
    auto make_states_domain = [&]() {
      return fuzztest::Map(
          [](NodeState container, std::vector<NodeState> children) {
            children.insert(children.begin(), std::move(container));
            return children;
          },
          AnyNodeState(this, kNumNodes, AnyAriaAttributeNameValuePair(),
                       AnyStyles()),
          fuzztest::VectorOf(AnyNodeState(this, kNumNodes,
                                          AnyAttributeNameValuePair(),
                                          child_styles()))
              .WithSize(kNumNodes - 1));
    };
    return PredefinedNodesConfig{std::move(nodes), make_states_domain(),
                                 make_states_domain()};
  }
};

class SelectAndAria : public DomScenarioDomainSpecification {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnyHtmlTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return fuzztest::OneOf(
        AnyAriaAttributeNameValuePair(),
        AttributeNameValuePairDomain(html_names::kForAttr,
                                     fuzztest::Just(std::string("id_0"))),
        AttributeNameValuePairDomain(
            html_names::kSizeAttr,
            AnyValueForHtmlAttribute(html_names::kSizeAttr)),
        AttributeNameValuePairDomain(
            html_names::kMultipleAttr,
            AnyValueForHtmlAttribute(html_names::kMultipleAttr)),
        AttributeNameValuePairDomain(
            html_names::kDisabledAttr,
            AnyValueForHtmlAttribute(html_names::kDisabledAttr)),
        AttributeNameValuePairDomain(
            html_names::kSelectedAttr,
            AnyValueForHtmlAttribute(html_names::kSelectedAttr)),
        AttributeNameValuePairDomain(html_names::kValueAttr,
                                     fuzztest::PrintableAsciiString()),
        AttributeNameValuePairDomain(html_names::kLabelAttr,
                                     fuzztest::PrintableAsciiString()));
  }
  fuzztest::Domain<std::string> AnyStyles() override {
    return AnyCssDeclaration();
  }
  fuzztest::Domain<std::string> AnyText() override {
    return fuzztest::PrintableAsciiString();
  }
  static constexpr int kNumNodes = 8;
  int GetMaxDomNodes() override { return kNumNodes; }
  int GetMaxAttributesPerNode() override { return 4; }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(
        html_names::TagToQualifiedName(html_names::HTMLTag::kBody));
  }
  bool AllowReparenting() override { return false; }
  std::optional<PredefinedNodesConfig> GetPredefinedNodes() override {
    auto tag = [](html_names::HTMLTag t) {
      return html_names::TagToQualifiedName(t);
    };
    auto nodes = std::vector<NodeSpecification>{
        // <select> (parent: root) index 0.
        {.tag = tag(html_names::HTMLTag::kSelect),
         .initial_state = {.parent_index = kIndexOfRootElement}},
        // <optgroup> (parent: select) index 1.
        {.tag = tag(html_names::HTMLTag::kOptgroup),
         .initial_state = {.parent_index = 0}},
        // <option> (parent: select) index 2.
        {.tag = tag(html_names::HTMLTag::kOption),
         .initial_state = {.parent_index = 0}},
        // <option> (parent: select) index 3.
        {.tag = tag(html_names::HTMLTag::kOption),
         .initial_state = {.parent_index = 0}},
        // <option> (parent: optgroup) index 4.
        {.tag = tag(html_names::HTMLTag::kOption),
         .initial_state = {.parent_index = 1}},
        // <option> (parent: optgroup) index 5.
        {.tag = tag(html_names::HTMLTag::kOption),
         .initial_state = {.parent_index = 1}},
        // <option> (parent: optgroup) index 6.
        {.tag = tag(html_names::HTMLTag::kOption),
         .initial_state = {.parent_index = 1}},
        // <label> (parent: root) index 7 — associated via for= attr.
        {.tag = tag(html_names::HTMLTag::kLabel),
         .initial_state = {.parent_index = kIndexOfRootElement}},
    };
    auto states_domain =
        fuzztest::VectorOf(AnyNodeState(this, kNumNodes,
                                        AnyAttributeNameValuePair(),
                                        AnyStyles()))
            .WithSize(kNumNodes);
    return PredefinedNodesConfig{std::move(nodes), states_domain,
                                 states_domain};
  }
};

class CustomizableSelectAndAria : public DomScenarioDomainSpecification {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnyHtmlTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return fuzztest::OneOf(
        AnyAriaAttributeNameValuePair(),
        AttributeNameValuePairDomain(
            html_names::kSizeAttr,
            AnyValueForHtmlAttribute(html_names::kSizeAttr)),
        AttributeNameValuePairDomain(
            html_names::kMultipleAttr,
            AnyValueForHtmlAttribute(html_names::kMultipleAttr)),
        AttributeNameValuePairDomain(
            html_names::kDisabledAttr,
            AnyValueForHtmlAttribute(html_names::kDisabledAttr)),
        AttributeNameValuePairDomain(
            html_names::kSelectedAttr,
            AnyValueForHtmlAttribute(html_names::kSelectedAttr)),
        AttributeNameValuePairDomain(html_names::kValueAttr,
                                     fuzztest::PrintableAsciiString()),
        AttributeNameValuePairDomain(html_names::kLabelAttr,
                                     fuzztest::PrintableAsciiString()));
  }
  fuzztest::Domain<std::string> AnyStyles() override {
    return AnyCssDeclaration();
  }
  fuzztest::Domain<std::string> AnyText() override {
    return fuzztest::PrintableAsciiString();
  }
  static constexpr int kNumNodes = 11;
  int GetMaxDomNodes() override { return kNumNodes; }
  int GetMaxAttributesPerNode() override { return 4; }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(
        html_names::TagToQualifiedName(html_names::HTMLTag::kBody));
  }
  fuzztest::Domain<std::string> AnyStylesheet() override {
    return fuzztest::OneOf(
        fuzztest::Just(std::string(
            "select, ::picker(select) { appearance: base-select; }")),
        fuzztest::Just(
            std::string("select, ::picker(select) { appearance: base-select; } "
                        "option::checkmark { content: '\\2713'; } "
                        "select::picker-icon { content: '\\25BC'; }")));
  }
  bool AllowReparenting() override { return false; }
  std::optional<PredefinedNodesConfig> GetPredefinedNodes() override {
    auto tag = [](html_names::HTMLTag t) {
      return html_names::TagToQualifiedName(t);
    };
    auto nodes = std::vector<NodeSpecification>{
        // <select> (parent: root) index 0.
        {.tag = tag(html_names::HTMLTag::kSelect),
         .initial_state = {.parent_index = kIndexOfRootElement}},
        // <button> (parent: select) index 1.
        {.tag = tag(html_names::HTMLTag::kButton),
         .initial_state = {.parent_index = 0}},
        // <selectedcontent> (parent: button) index 2.
        {.tag = tag(html_names::HTMLTag::kSelectedcontent),
         .initial_state = {.parent_index = 1}},
        // <div> (parent: select) index 3 — rich picker content.
        {.tag = tag(html_names::HTMLTag::kDiv),
         .initial_state = {.parent_index = 0}},
        // <option> (parent: select) index 4.
        {.tag = tag(html_names::HTMLTag::kOption),
         .initial_state = {.parent_index = 0}},
        // <option> (parent: select) index 5.
        {.tag = tag(html_names::HTMLTag::kOption),
         .initial_state = {.parent_index = 0}},
        // <optgroup> (parent: select) index 6.
        {.tag = tag(html_names::HTMLTag::kOptgroup),
         .initial_state = {.parent_index = 0}},
        // <option> (parent: optgroup) index 7.
        {.tag = tag(html_names::HTMLTag::kOption),
         .initial_state = {.parent_index = 6}},
        // <datalist> (parent: select) index 8.
        {.tag = tag(html_names::HTMLTag::kDatalist),
         .initial_state = {.parent_index = 0}},
        // <option> (parent: datalist) index 9.
        {.tag = tag(html_names::HTMLTag::kOption),
         .initial_state = {.parent_index = 8}},
        // <label> (parent: root) index 10.
        {.tag = tag(html_names::HTMLTag::kLabel),
         .initial_state = {.parent_index = kIndexOfRootElement}},
    };
    auto states_domain =
        fuzztest::VectorOf(AnyNodeState(this, kNumNodes,
                                        AnyAttributeNameValuePair(),
                                        AnyStyles()))
            .WithSize(kNumNodes);
    return PredefinedNodesConfig{std::move(nodes), states_domain,
                                 states_domain};
  }
};

class DialogAndAria : public DomScenarioDomainSpecification {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnyHtmlTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return fuzztest::OneOf(AnyAriaAttributeNameValuePair(),
                           AnyHtmlAttributeNameValuePair());
  }
  fuzztest::Domain<std::string> AnyStyles() override {
    return AnyCssDeclaration();
  }
  fuzztest::Domain<std::string> AnyText() override {
    return fuzztest::PrintableAsciiString();
  }
  static constexpr int kNumNodes = 8;
  int GetMaxDomNodes() override { return kNumNodes; }
  int GetMaxAttributesPerNode() override { return 4; }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(
        html_names::TagToQualifiedName(html_names::HTMLTag::kBody));
  }
  bool AllowReparenting() override { return false; }
  std::optional<PredefinedNodesConfig> GetPredefinedNodes() override {
    auto tag = [](html_names::HTMLTag t) {
      return html_names::TagToQualifiedName(t);
    };
    auto nodes = std::vector<NodeSpecification>{
        // <div> (parent: root) index 0.
        {.tag = tag(html_names::HTMLTag::kDiv),
         .initial_state = {.parent_index = kIndexOfRootElement}},
        // <dialog> (parent: root) index 1.
        {.tag = tag(html_names::HTMLTag::kDialog),
         .initial_state = {.parent_index = kIndexOfRootElement}},
        // <button> (parent: dialog 1) index 2 — focusable dialog content.
        {.tag = tag(html_names::HTMLTag::kButton),
         .initial_state = {.parent_index = 1}},
        // <span> (parent: root) index 3.
        {.tag = tag(html_names::HTMLTag::kSpan),
         .initial_state = {.parent_index = kIndexOfRootElement}},
        // <dialog> (parent: root) index 4.
        {.tag = tag(html_names::HTMLTag::kDialog),
         .initial_state = {.parent_index = kIndexOfRootElement}},
        // <input> (parent: dialog 4) index 5 — focusable dialog content.
        {.tag = tag(html_names::HTMLTag::kInput),
         .initial_state = {.parent_index = 4}},
        // <button> (parent: root) index 6 — focusable sibling.
        {.tag = tag(html_names::HTMLTag::kButton),
         .initial_state = {.parent_index = kIndexOfRootElement}},
        // <div> (parent: root) index 7.
        {.tag = tag(html_names::HTMLTag::kDiv),
         .initial_state = {.parent_index = kIndexOfRootElement}},
    };
    // Dialog nodes get an attribute domain that includes `open`, so the
    // toggle logic exercises both close and showModal paths. The `open`
    // entry is repeated to weight it higher than ARIA/HTML attributes,
    // since it's the only way to start a dialog in the open state.
    auto dialog_attrs = fuzztest::OneOf(
        AnyAriaAttributeNameValuePair(), AnyHtmlAttributeNameValuePair(),
        AttributeNameValuePairDomain(html_names::kOpenAttr,
                                     fuzztest::Just(std::string(""))),
        AttributeNameValuePairDomain(html_names::kOpenAttr,
                                     fuzztest::Just(std::string(""))),
        AttributeNameValuePairDomain(html_names::kOpenAttr,
                                     fuzztest::Just(std::string(""))));
    auto make_states_domain = [&]() {
      return fuzztest::Map(
          [](std::vector<NodeState> regular, NodeState dialog1,
             NodeState dialog4) {
            std::vector<NodeState> result;
            result.push_back(std::move(regular[0]));
            result.push_back(std::move(dialog1));
            result.push_back(std::move(regular[1]));
            result.push_back(std::move(regular[2]));
            result.push_back(std::move(dialog4));
            result.push_back(std::move(regular[3]));
            result.push_back(std::move(regular[4]));
            result.push_back(std::move(regular[5]));
            return result;
          },
          fuzztest::VectorOf(AnyNodeState(this, kNumNodes,
                                          AnyAttributeNameValuePair(),
                                          AnyStyles()))
              .WithSize(kNumNodes - 2),
          AnyNodeState(this, kNumNodes, dialog_attrs, AnyStyles()),
          AnyNodeState(this, kNumNodes, dialog_attrs, AnyStyles()));
    };
    return PredefinedNodesConfig{std::move(nodes), make_states_domain(),
                                 make_states_domain()};
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
  void AnimationAndAria(const DomScenario& input) { RunTest(input); }
  void ReadingFlowAndOrder(const DomScenario& input) { RunTest(input); }
  void SelectAndAria(const DomScenario& input) { RunTest(input); }
  void CustomizableSelectAndAria(const DomScenario& input) { RunTest(input); }
  void DialogAndAria(const DomScenario& input) { RunTest(input); }

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
  void ObserveElementAction(Element* element) override {
    std::string ax_tree = AccessibilityTest::PrintAXTree(GetDocument());
    LogIfEnabled(
        base::StrCat({"\n\nACCESSIBILITY TREE AFTER ELEMENT ACTION: ",
                      element->GetIdAttribute().Utf8(), "\n", ax_tree}));
  }
  void ObserveAnimationsAdvanced() override {
    std::string ax_tree = AccessibilityTest::PrintAXTree(GetDocument());
    LogIfEnabled(base::StrCat(
        {"\n\nACCESSIBILITY TREE AFTER ANIMATIONS ADVANCED\n", ax_tree}));
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

FUZZ_TEST_F(AccessibilityDomScenarioRunner, AnimationAndAria)
    .WithDomains(BuildDomScenarios<AnimationAndAria>());

FUZZ_TEST_F(AccessibilityDomScenarioRunner, ReadingFlowAndOrder)
    .WithDomains(BuildDomScenarios<ReadingFlowAndOrder>());

FUZZ_TEST_F(AccessibilityDomScenarioRunner, SelectAndAria)
    .WithDomains(BuildDomScenarios<SelectAndAria>());

FUZZ_TEST_F(AccessibilityDomScenarioRunner, CustomizableSelectAndAria)
    .WithDomains(BuildDomScenarios<CustomizableSelectAndAria>());

FUZZ_TEST_F(AccessibilityDomScenarioRunner, DialogAndAria)
    .WithDomains(BuildDomScenarios<DialogAndAria>());

}  // namespace blink
