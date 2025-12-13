// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_DOM_SCENARIO_RUNNER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_DOM_SCENARIO_RUNNER_H_

#include <string>

#include "base/types/optional_ref.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/dom_scenario.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/fuzztest/src/fuzztest/googletest_fixture_adapter.h"

namespace blink {

class Element;
class Node;

class DomScenarioRunner
    : public fuzztest::PerFuzzTestFixtureAdapter<RenderingTest> {
 public:
  DomScenarioRunner();
  ~DomScenarioRunner() override = default;

 protected:
  // Runs the test by creating the initial DOM, then applies modifications.
  void RunTest(const DomScenario& input);

  // Observer hooks for subclasses to add custom behavior.

  // Called after the initial DOM is created but before modifications are
  // applied. Subclasses can override to add custom logic such as dumping
  // accessibility trees.
  virtual void ObserveInitialDOM(
      const HeapVector<Member<Element>>& created_elements) {}

  // Called after modifications are applied to the DOM. Subclasses can override
  // to add custom logic such as dumping accessibility trees.
  virtual void ObserveModifiedDOM(
      const HeapVector<Member<Element>>& created_elements) {}

  // Sets the text content of an element. Subclasses can override to customize
  // text handling for specific element types. Default implementation handles
  // input elements and regular text nodes.
  virtual void SetElementText(Element* element, const std::string& text);

  // Sets the inline style attribute of an element. Subclasses can override to
  // customize style handling. Default implementation sets/removes the style
  // attribute.
  virtual void SetElementStyle(Element* element, const std::string& styles);

  // Logs a message if fuzzer logging is enabled. Subclasses can override to
  // customize logging behavior. Default implementation checks for
  // --enable-dom-fuzzer-logging flag.
  virtual void LogIfEnabled(const std::string& message);

 private:
  // Creates the initial DOM from the scenario specification.
  void CreateInitialDOM(const DomScenario& input,
                        Element*& root,
                        HeapVector<Member<Element>>& created_elements);

  // Applies the modifications from the scenario to the existing DOM.
  void ApplyModifications(Element* root,
                          const std::vector<NodeSpecification>& node_specs,
                          const HeapVector<Member<Element>>& created_elements);

  // Helper functions to set element properties during creation and
  // modification.
  void SetElementAttributes(
      Element* element,
      base::optional_ref<
          const std::vector<std::pair<QualifiedName, std::string>>> attributes);
  void SetParent(Element* child,
                 size_t child_index,
                 int parent_index,
                 Element* root,
                 const HeapVector<Member<Element>>& created_elements,
                 bool in_shadow_dom,
                 bool use_slot_projection);

  // Get DOM tree as string with pretty-printing.
  std::string GetDOMTreeAsString();

 private:
  // Helper functions for `GetDOMTreeAsString`.
  void SerializeNode(Node* node, std::string& result, int indent);
  std::string EscapeString(const std::string& str);

  // Creates a shadow host wrapper for an element and returns the shadow host.
  // Called by `SetParent` when shadow DOM usage is indicated.
  Element* WrapInShadowDOM(Element* element,
                           bool use_slot_projection,
                           size_t child_index);

  // Set from --enable-dom-fuzzer-logging at construction time.
  bool logging_enabled_ = false;

  // Counter for generating unique shadow host IDs within a test case.
  inline static int shadow_host_counter_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_DOM_SCENARIO_RUNNER_H_
