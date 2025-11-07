// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fuzztest_utils/dom_scenario_runner.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/types/optional_ref.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/dom_scenario.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

const char kEnableDomFuzzerLogging[] = "enable-dom-fuzzer-logging";

namespace blink {

namespace {

// Finds and returns the first text child node of an element, or nullptr if
// no text child exists.
Text* FindFirstTextChild(Element* element) {
  for (Node* child = element->firstChild(); child;
       child = child->nextSibling()) {
    if (Text* text_node = DynamicTo<Text>(child)) {
      return text_node;
    }
  }
  return nullptr;
}

}  // namespace

DomScenarioRunner::DomScenarioRunner() {
  logging_enabled_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableDomFuzzerLogging);
}

void DomScenarioRunner::RunTest(const DomScenario& input) {
  Element* root = nullptr;
  HeapVector<Member<Element>> created_elements;
  LogIfEnabled(base::StrCat({"\n\n", input.ToString()}));
  CreateInitialDOM(input, root, created_elements);
  ApplyModifications(root, input.node_specs, created_elements);
  GetDocument().body()->RemoveChildren();
}

void DomScenarioRunner::CreateInitialDOM(
    const DomScenario& input,
    Element*& root,
    HeapVector<Member<Element>>& created_elements) {
  Document& document = GetDocument();

  // If root tag is body, use the document body, otherwise create and append
  // the root element to the body.
  if (input.root_tag ==
      html_names::TagToQualifiedName(html_names::HTMLTag::kBody)) {
    root = document.body();
  } else {
    root = document.CreateRawElement(input.root_tag);
    document.body()->appendChild(root);
  }
  created_elements.reserve(input.node_specs.size());

  for (size_t i = 0; i < input.node_specs.size(); ++i) {
    const auto& node_spec = input.node_specs[i];
    Element* element = document.CreateRawElement(node_spec.tag);
    element->setAttribute(html_names::kIdAttr,
                          AtomicString(StrCat({"id_", String::Number(i)})));
    // Set attributes first because there's a chance that one of the fuzzed
    // attributes is style. Should that occur we want the style domain to win.
    SetElementAttributes(element, node_spec.initial_state.attributes);
    SetElementStyle(element, node_spec.initial_state.styles.value_or(""));
    SetElementText(element, node_spec.initial_state.text.value_or(""));
    created_elements.push_back(element);
  }

  for (size_t i = 0; i < input.node_specs.size(); ++i) {
    const auto& node_spec = input.node_specs[i];
    Element* element = created_elements[i];
    SetParent(element, node_spec.initial_state.parent_index, root,
              created_elements);
  }

  document.UpdateStyleAndLayoutTree();

  LogIfEnabled(base::StrCat({"\n\nINITIAL DOM\n", GetDOMTreeAsString()}));

  ObserveInitialDOM(created_elements);
}

void DomScenarioRunner::ApplyModifications(
    Element* root,
    const std::vector<NodeSpecification>& node_specs,
    const HeapVector<Member<Element>>& created_elements) {
  CHECK_EQ(node_specs.size(), created_elements.size());

  for (size_t i = 0; i < node_specs.size(); ++i) {
    const auto& node_spec = node_specs[i];
    const auto& modified_state = node_spec.modified_state;
    Element* element = created_elements[i];
    SetParent(element, modified_state.parent_index, root, created_elements);
    // Set attributes first because there's a chance that one of the fuzzed
    // attributes is style. Should that occur we want the style domain to win.
    SetElementAttributes(element, modified_state.attributes);
    SetElementStyle(element, modified_state.styles.value_or(""));
    SetElementText(element, modified_state.text.value_or(""));
  }

  GetDocument().UpdateStyleAndLayoutTree();

  LogIfEnabled(base::StrCat({"\n\nMODIFIED DOM\n", GetDOMTreeAsString()}));

  ObserveModifiedDOM(created_elements);
}

void DomScenarioRunner::SetElementText(Element* element,
                                       const std::string& text) {
  if (auto* input_element = DynamicTo<HTMLInputElement>(element)) {
    if (input_element->IsTextField() || input_element->IsTextButton()) {
      input_element->setAttribute(html_names::kValueAttr,
                                  AtomicString(text.c_str()));
      return;
    }
  }

  Text* text_child = FindFirstTextChild(element);

  if (text.empty()) {
    if (text_child != nullptr) {
      element->removeChild(text_child);
    }
    return;
  }

  if (text_child == nullptr) {
    Text* text_node = GetDocument().createTextNode(AtomicString(text.c_str()));
    element->appendChild(text_node);
    return;
  }

  text_child->setData(AtomicString(text.c_str()));
}

void DomScenarioRunner::SetElementStyle(Element* element,
                                        const std::string& styles) {
  element->removeAttribute(html_names::kStyleAttr);
  if (!styles.empty()) {
    element->setAttribute(html_names::kStyleAttr, AtomicString(styles.c_str()));
  }
}

void DomScenarioRunner::SetElementAttributes(
    Element* element,
    base::optional_ref<const std::vector<std::pair<QualifiedName, std::string>>>
        attributes) {
  // Remove existing attributes (except protected ones).
  // We need the ID to remain valid; we want styles to be managed separately.
  Vector<QualifiedName> attributes_to_remove;
  for (const auto& attr : element->Attributes()) {
    if (attr.GetName() != html_names::kIdAttr &&
        attr.GetName() != html_names::kStyleAttr) {
      attributes_to_remove.push_back(attr.GetName());
    }
  }
  for (const auto& attr_name : attributes_to_remove) {
    element->removeAttribute(attr_name);
  }

  if (attributes.has_value()) {
    for (const auto& attr_pair : *attributes) {
      element->setAttribute(attr_pair.first,
                            AtomicString(attr_pair.second.c_str()));
    }
  }
}

// TODO(crbug.com/445771451): Remove this helper when slot assignment bug is
// fixed.
static bool HasDirAutoAncestor(Element* element) {
  for (Element* ancestor = element->parentElement(); ancestor;
       ancestor = ancestor->parentElement()) {
    if (ancestor->FastGetAttribute(html_names::kDirAttr) == "auto") {
      return true;
    }
  }
  return false;
}

void DomScenarioRunner::SetParent(
    Element* child,
    int parent_index,
    Element* root,
    const HeapVector<Member<Element>>& created_elements) {
  DCHECK(child);
  DCHECK(root);

  // TODO(crbug.com/445771451): Remove this temporary workaround for slot
  // assignment recursion bug. Skip moving slot elements that would trigger
  // recursive slot assignment recalculation during appendChild operations.
  if (IsA<HTMLSlotElement>(child) && child->parentNode() &&
      HasDirAutoAncestor(child)) {
    return;
  }

  Element* target_parent = root;
  if (parent_index >= 0 &&
      parent_index < static_cast<int>(created_elements.size())) {
    Element* potential_parent = created_elements[parent_index];
    if (potential_parent != child) {
      target_parent = potential_parent;
    }
  }

  DummyExceptionStateForTesting exception_state;
  target_parent->appendChild(child, exception_state);

  // If appendChild failed and we're not already using root, fall back to root.
  if ((exception_state.HadException() ||
       child->parentNode() != target_parent) &&
      target_parent != root) {
    DummyExceptionStateForTesting root_exception_state;
    root->appendChild(child, root_exception_state);
  }
}

void DomScenarioRunner::LogIfEnabled(const std::string& message) {
  if (logging_enabled_) {
    LOG(INFO) << message;
  }
}

std::string DomScenarioRunner::GetDOMTreeAsString() {
  std::string result;
  SerializeNode(GetDocument().body(), result, 0);
  return result;
}

void DomScenarioRunner::SerializeNode(Node* node,
                                      std::string& result,
                                      int indent) {
  std::string indent_str(indent * 2, ' ');

  if (Element* element = DynamicTo<Element>(node)) {
    // Opening tag.
    base::StrAppend(&result, {indent_str, "<", element->tagName().Utf8()});
    if (element->hasAttributes()) {
      for (const auto& attr : element->Attributes()) {
        base::StrAppend(&result, {" ", attr.GetName().ToString().Utf8(), "=\"",
                                  EscapeString(attr.Value().Utf8()), "\""});
      }
    }
    base::StrAppend(&result, {">"});

    // Children.
    bool has_children = false;
    for (Node* child = element->firstChild(); child;
         child = child->nextSibling()) {
      if (!has_children) {
        base::StrAppend(&result, {"\n"});
        has_children = true;
      }
      SerializeNode(child, result, indent + 1);
      base::StrAppend(&result, {"\n"});
    }

    // Closing tag.
    if (has_children) {
      base::StrAppend(&result, {indent_str});
    }
    base::StrAppend(&result, {"</", element->tagName().Utf8(), ">"});
  } else if (Text* text = DynamicTo<Text>(node)) {
    base::StrAppend(&result, {indent_str, EscapeString(text->data().Utf8())});
  }
}

std::string DomScenarioRunner::EscapeString(const std::string& str) {
  std::string result;
  result.reserve(str.size());
  for (char c : str) {
    if (c == '\n') {
      result += "\\n";
    } else if (c == '\r') {
      result += "\\r";
    } else if (c == '\t') {
      result += "\\t";
    } else if (c < 32 || c > 126) {
      // Non-printable or non-ASCII characters
      base::StringAppendF(&result, "\\x%02X", static_cast<unsigned char>(c));
    } else {
      result += c;
    }
  }
  return result;
}

}  // namespace blink
