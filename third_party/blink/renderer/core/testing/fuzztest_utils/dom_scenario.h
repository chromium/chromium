// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_DOM_SCENARIO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_DOM_SCENARIO_H_

#include <optional>
#include <string>
#include <vector>

#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

class QualifiedName;

// Sentinel value indicating that a node's parent should be the root element.
// The root element by default is the <body> element, but can be overridden
// by subclasses of DomFuzzer. For instance, an Accessibility Fuzzer can
// set the root to be a <canvas> element for the purpose of fuzz testing
// canvas fallback content.
inline constexpr int kIndexOfRootElement = -1;

// Holds the state of a DOM node (parent index, attributes, styles, text).
struct NodeState {
  int parent_index = kIndexOfRootElement;
  std::optional<std::vector<std::pair<QualifiedName, std::string>>> attributes;
  std::optional<std::string> styles;
  std::optional<std::string> text;
};

// Specification for a single DOM node, including its tag, initial state,
// and modifications to be applied to it.
struct NodeSpecification {
  QualifiedName tag;
  NodeState initial_state;
  NodeState modified_state;
};

// Represents the test case that will be used by the test runner, namely
// a root element tag and a set of node specifications (each containing
// initial state and modifications).
struct DomScenario {
  QualifiedName root_tag;
  std::vector<NodeSpecification> node_specs;
  std::string ToString() const;
};

// Specification interface for generating DOM scenarios to be used by the
// DomScenario test runner.
class DomScenarioDomainSpecification {
 public:
  virtual ~DomScenarioDomainSpecification() = default;
  virtual fuzztest::Domain<QualifiedName> AnyTag() = 0;
  virtual fuzztest::Domain<std::pair<QualifiedName, std::string>>
  AnyAttributeNameValuePair() = 0;
  virtual fuzztest::Domain<std::string> AnyStyles() = 0;
  virtual fuzztest::Domain<std::string> AnyText() = 0;
  virtual int GetMaxDomNodes() = 0;
  virtual int GetMaxAttributesPerNode() = 0;
  virtual fuzztest::Domain<QualifiedName> GetRootElementTag() = 0;
};

// Domain building functions
fuzztest::Domain<DomScenario> AnyDomScenarioForSpec(
    DomScenarioDomainSpecification* spec);

template <typename SpecificationType>
auto BuildDomScenarios() {
  static SpecificationType specification;
  return AnyDomScenarioForSpec(&specification);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_DOM_SCENARIO_H_
