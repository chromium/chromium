// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/css_domains.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/dom_scenario_runner.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/html_domains.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/mathml_domains.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/svg_domains.h"

namespace blink {

class HTMLSpec : public DomScenarioDomainSpecification {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnyHtmlTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return AnyHtmlAttributeNameValuePair();
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
};

class SVGInHTMLSpec : public HTMLSpec {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnySvgTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return AnySvgAttributeNameValuePair();
  }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(svg_names::kSVGTag);
  }
};

class MathMLInHTMLSpec : public HTMLSpec {
 public:
  fuzztest::Domain<QualifiedName> AnyTag() override { return AnyMathMlTag(); }
  fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() override {
    return AnyMathMlAttributeNameValuePair();
  }
  fuzztest::Domain<QualifiedName> GetRootElementTag() override {
    return fuzztest::Just<QualifiedName>(mathml_names::kMathTag);
  }
};

class HTMLDomScenarioRunner : public DomScenarioRunner {
 public:
  HTMLDomScenarioRunner() = default;

  void HTML(const DomScenario& input) { RunTest(input); }
  void MathMLInHTML(const DomScenario& input) { RunTest(input); }
  void SVGInHTML(const DomScenario& input) { RunTest(input); }
};

FUZZ_TEST_F(HTMLDomScenarioRunner, HTML)
    .WithDomains(BuildDomScenarios<HTMLSpec>());

FUZZ_TEST_F(HTMLDomScenarioRunner, MathMLInHTML)
    .WithDomains(BuildDomScenarios<MathMLInHTMLSpec>());

FUZZ_TEST_F(HTMLDomScenarioRunner, SVGInHTML)
    .WithDomains(BuildDomScenarios<SVGInHTMLSpec>());

}  // namespace blink
