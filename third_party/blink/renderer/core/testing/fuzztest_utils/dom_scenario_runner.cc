// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fuzztest_utils/dom_scenario_runner.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
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
  shadow_host_counter_ = 0;
  LogIfEnabled(base::StrCat({"\n\n", input.ToString()}));
  InjectKeyframesStylesheet();
  InjectCustomElementDefinitions();
  CreateInitialDOM(input, root, created_elements);
  AdvanceAnimations();
  CancelWebAnimations(created_elements);
  ApplyModifications(root, input, created_elements);
  AdvanceAnimations();
  CancelWebAnimations(created_elements);
  ExitFullscreen();
  GetDocument().body()->RemoveChildren();
  GetDocument().head()->RemoveChildren();
}

void DomScenarioRunner::CreateInitialDOM(
    const DomScenario& input,
    Element*& root,
    HeapVector<Member<Element>>& created_elements) {
  Document& document = GetDocument();

  if (!input.stylesheet.empty()) {
    Element* style = document.CreateRawElement(
        html_names::TagToQualifiedName(html_names::HTMLTag::kStyle));
    style->setTextContent(AtomicString::FromUtf8(input.stylesheet));
    document.head()->appendChild(style);
  }

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
    const AtomicString& local_name = node_spec.tag.LocalName();
    Element* element;
    if (local_name.contains('-')) {
      DummyExceptionStateForTesting exception_state;
      element = document.CreateElementForBinding(local_name, exception_state);
    } else {
      element = document.CreateRawElement(node_spec.tag);
    }
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
    SetParent(element, i, node_spec.initial_state.parent_index, root,
              created_elements, node_spec.initial_state.in_shadow_dom,
              node_spec.initial_state.use_slot_projection);
  }

  document.UpdateStyleAndLayoutTree();

  LogIfEnabled(base::StrCat({"\n\nINITIAL DOM\n", GetDOMTreeAsString()}));

  ObserveInitialDOM(created_elements);

  bool needs_update = false;
  for (size_t i = 0; i < input.node_specs.size(); ++i) {
    needs_update |= PerformElementActions(created_elements[i],
                                          input.node_specs[i].initial_state);
  }
  if (needs_update) {
    document.UpdateStyleAndLayoutTree();
  }
}

void DomScenarioRunner::ApplyModifications(
    Element* root,
    const DomScenario& input,
    const HeapVector<Member<Element>>& created_elements) {
  const auto& node_specs = input.node_specs;
  CHECK_EQ(node_specs.size(), created_elements.size());

  for (size_t i = 0; i < node_specs.size(); ++i) {
    const auto& node_spec = node_specs[i];
    const auto& modified_state = node_spec.modified_state;
    Element* element = created_elements[i];
    int parent_index = input.allow_reparenting
                           ? modified_state.parent_index
                           : node_spec.initial_state.parent_index;
    SetParent(element, i, parent_index, root, created_elements,
              modified_state.in_shadow_dom, modified_state.use_slot_projection);
    // Set attributes first because there's a chance that one of the fuzzed
    // attributes is style. Should that occur we want the style domain to win.
    SetElementAttributes(element, modified_state.attributes);
    SetElementStyle(element, modified_state.styles.value_or(""));
    SetElementText(element, modified_state.text.value_or(""));
  }

  GetDocument().UpdateStyleAndLayoutTree();

  LogIfEnabled(base::StrCat({"\n\nMODIFIED DOM\n", GetDOMTreeAsString()}));

  ObserveModifiedDOM(created_elements);

  bool needs_update = false;
  for (size_t i = 0; i < node_specs.size(); ++i) {
    needs_update |= PerformElementActions(created_elements[i],
                                          node_specs[i].modified_state);
  }
  if (needs_update) {
    GetDocument().UpdateStyleAndLayoutTree();
  }
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
    size_t child_index,
    int parent_index,
    Element* root,
    const HeapVector<Member<Element>>& created_elements,
    bool in_shadow_dom,
    bool use_slot_projection) {
  DCHECK(child);
  DCHECK(root);

  // If shadow DOM usage is indicated, wrap the element and update child to
  // point to the shadow host.
  if (in_shadow_dom) {
    child = WrapInShadowDOM(child, use_slot_projection, child_index);
  }

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

bool DomScenarioRunner::PerformElementActions(Element* element,
                                              const NodeState& state) {
  bool acted = false;
  if (state.should_focus) {
    const bool added_tabindex =
        !element->FastHasAttribute(html_names::kTabindexAttr);
    if (added_tabindex) {
      element->setAttribute(html_names::kTabindexAttr, AtomicString("-1"));
    }
    element->Focus();
    if (added_tabindex) {
      element->removeAttribute(html_names::kTabindexAttr);
    }
    acted = true;
  }
  if (state.should_scroll_into_view) {
    element->scrollIntoViewForTesting();
    acted = true;
  }
  if (state.should_enter_fullscreen) {
    ExitFullscreen();
    EnterFullscreen(element);
    acted = true;
  }
  if (state.web_animation.has_value()) {
    CreateWebAnimation(element, *state.web_animation);
    acted = true;
  }
  if (auto* select = DynamicTo<HTMLSelectElement>(element)) {
    if (select->UsesMenuList()) {
      select->ShowPopup();
      acted = true;
    }
  }
  if (auto* dialog = DynamicTo<HTMLDialogElement>(element)) {
    if (dialog->IsModal() || dialog->IsOpen()) {
      dialog->close();
    } else {
      DummyExceptionStateForTesting exception_state;
      dialog->showModal(exception_state);
    }
    acted = true;
  }
  if (acted) {
    ObserveElementAction(element);
  }
  return acted;
}

void DomScenarioRunner::CreateWebAnimation(Element* element,
                                           const WebAnimationParams& params) {
  auto* start_keyframe = MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(
      params.property, String(params.from_value),
      SecureContextMode::kInsecureContext, nullptr);
  auto* end_keyframe = MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(params.property, String(params.to_value),
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);

  auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  auto* effect = MakeGarbageCollected<KeyframeEffect>(element, model, timing);
  Animation* animation =
      Animation::Create(effect, &GetDocument().Timeline(), ASSERT_NO_EXCEPTION);
  animation->play();
}

void DomScenarioRunner::CancelWebAnimations(
    const HeapVector<Member<Element>>& created_elements) {
  for (Element* element : created_elements) {
    for (Animation* animation : element->getAnimations()) {
      animation->cancel();
    }
  }
}

void DomScenarioRunner::EnterFullscreen(Element* element) {
  Document& document = GetDocument();
  LocalFrame::NotifyUserActivation(
      document.GetFrame(), mojom::blink::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*element);
  Fullscreen::DidResolveEnterFullscreenRequest(document, /*granted=*/true);
  UpdateAllLifecyclePhasesForTest();
}

void DomScenarioRunner::ExitFullscreen() {
  Document& document = GetDocument();
  Fullscreen::FullyExitFullscreen(document);
  Fullscreen::DidExitFullscreen(document);
  UpdateAllLifecyclePhasesForTest();
}

void DomScenarioRunner::InjectCustomElementDefinitions() {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  static const char kCustomElementsJS[] =
      R"js(customElements.define('fuzz-plain', class extends HTMLElement {});

    customElements.define('fuzz-shadow', class extends HTMLElement {
      connectedCallback() {
        if (!this.shadowRoot) {
          const shadow = this.attachShadow({mode: 'open'});
          shadow.innerHTML = '<span></span><slot></slot>';
        }
      }
    });

    customElements.define('fuzz-attrs', class extends HTMLElement {
      constructor() {
        super();
        this.attachShadow({mode: 'open'});
        this.shadowRoot.innerHTML = '<span id="label"></span><slot></slot>';
      }
      static get observedAttributes() {
        return ['title', 'data-label', 'data-state', 'hidden'];
      }
      attributeChangedCallback(name, oldValue, newValue) {
        const label = this.shadowRoot.querySelector('#label');
        if (label) {
          label.textContent = newValue || '';
        }
      }
    });)js";
  Document& document = GetDocument();
  Element* script = document.CreateRawElement(
      html_names::TagToQualifiedName(html_names::HTMLTag::kScript));
  script->setTextContent(AtomicString(kCustomElementsJS));
  document.head()->appendChild(script);
  document.UpdateStyleAndLayoutTree();
}

void DomScenarioRunner::InjectKeyframesStylesheet() {
  static const char kKeyframesCSS[] = R"css(
    @keyframes fuzz-fade {
      from { opacity: 1 }
      to { opacity: 0 }
    }
    @keyframes fuzz-hide {
      from { visibility: visible }
      to { visibility: hidden }
    }
    @keyframes fuzz-reveal {
      from { content-visibility: hidden }
      to { content-visibility: visible }
    }
    @keyframes fuzz-shrink-to-zero {
      from { width: 100px; height: 100px }
      to { width: 0; height: 0 }
    }
    @keyframes fuzz-shrink {
      from { width: 100px; height: 100px }
      to { width: 50px; height: 50px }
    }
    @keyframes fuzz-move {
      from { transform: none }
      to { transform: translateX(100px) }
    }
    @keyframes fuzz-move-offscreen {
      from { transform: none }
      to { transform: translateX(5000px) }
    }
    @keyframes fuzz-spin {
      to { transform: rotate(360deg) }
    }
    @keyframes fuzz-tilt {
      to { transform: rotate(45deg) }
    }
    @keyframes fuzz-pulse {
      0%, 100% { transform: scale(1); opacity: 1 }
      50% { transform: scale(1.15); opacity: 0.7 }
    }
    @keyframes fuzz-recolor {
      0% { color: red }
      25% { color: green }
      50% { color: blue }
      75% { color: yellow }
      100% { color: red }
    }
    @keyframes fuzz-toggle-display {
      from { display: block }
      to { display: none }
    }
  )css";
  Document& document = GetDocument();
  Element* style = document.CreateRawElement(
      html_names::TagToQualifiedName(html_names::HTMLTag::kStyle));
  style->setTextContent(AtomicString(kKeyframesCSS));
  document.head()->appendChild(style);
}

void DomScenarioRunner::AdvanceAnimations() {
  auto& clock = GetAnimationClock();
  clock.UpdateTime(clock.CurrentTime() + base::Milliseconds(500));
  UpdateAllLifecyclePhasesForTest();
  ObserveAnimationsAdvanced();
}

void DomScenarioRunner::LogIfEnabled(const std::string& message) {
  if (logging_enabled_) {
    LOG(INFO) << message;
  }
}

std::string DomScenarioRunner::GetDOMTreeAsString() {
  std::string result;
  if (GetDocument().head() && GetDocument().head()->hasChildren()) {
    SerializeNode(GetDocument().head(), result, 0);
    base::StrAppend(&result, {"\n"});
  }
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

    // Shadow root content if present.
    bool has_shadow_root = element->GetShadowRoot() != nullptr;
    if (has_shadow_root) {
      ShadowRoot* shadow_root = element->GetShadowRoot();
      base::StrAppend(&result,
                      {"\n", indent_str, "  #shadow-root (",
                       (shadow_root->IsOpen() ? "open" : "closed"), ")"});
      for (Node* child = shadow_root->firstChild(); child;
           child = child->nextSibling()) {
        base::StrAppend(&result, {"\n"});
        SerializeNode(child, result, indent + 2);
      }
      base::StrAppend(&result, {"\n", indent_str, "  #end-shadow-root"});
    }

    // Children.
    bool has_children = false;
    bool is_style_element = element->HasTagName(html_names::kStyleTag);
    bool is_script_element = element->HasTagName(html_names::kScriptTag);
    for (Node* child = element->firstChild(); child;
         child = child->nextSibling()) {
      if (!has_children) {
        base::StrAppend(&result, {"\n"});
        has_children = true;
      }
      if ((is_style_element || is_script_element) && IsA<Text>(child)) {
        String text = To<Text>(*child).data();
        text = StrCat({"    ", text});
        text.Replace("} ", "}\n    ");
        base::StrAppend(&result, {text.Utf8()});
      } else {
        SerializeNode(child, result, indent + 1);
      }
      base::StrAppend(&result, {"\n"});
    }

    // Closing tag.
    if (has_children) {
      base::StrAppend(&result, {indent_str});
    } else if (has_shadow_root) {
      base::StrAppend(&result, {"\n", indent_str});
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

Element* DomScenarioRunner::WrapInShadowDOM(Element* element,
                                            bool use_slot_projection,
                                            size_t child_index) {
  Document& document = GetDocument();
  Element* shadow_host = document.CreateRawElement(
      html_names::TagToQualifiedName(html_names::HTMLTag::kDiv));
  // Use a counter to ensure unique IDs across initial and modification phases.
  shadow_host->setAttribute(
      html_names::kIdAttr,
      AtomicString(StrCat({"shadow-host-", String::Number(child_index), "-",
                           String::Number(shadow_host_counter_++)})));
  ShadowRoot& shadow_root =
      shadow_host->AttachShadowRootForTesting(ShadowRootMode::kOpen);

  // Projection: element appended to shadow_host, projected through <slot>.
  // Otherwise: element directly in shadow root.
  if (use_slot_projection) {
    Element* slot = document.CreateRawElement(
        html_names::TagToQualifiedName(html_names::HTMLTag::kSlot));
    AtomicString slot_name =
        AtomicString(StrCat({"slot-", String::Number(child_index)}));
    slot->setAttribute(html_names::kNameAttr, slot_name);
    shadow_root.appendChild(slot);
    element->setAttribute(html_names::kSlotAttr, slot_name);
  }

  // Try to append element to the appropriate parent.
  ContainerNode* target = use_slot_projection
                              ? static_cast<ContainerNode*>(shadow_host)
                              : static_cast<ContainerNode*>(&shadow_root);
  DummyExceptionStateForTesting exception_state;
  target->appendChild(element, exception_state);

  // If `appendChild` failed, return the unwrapped element so `SetParent` can
  // add it directly to the DOM.
  if (exception_state.HadException() || element->parentNode() == nullptr) {
    LogIfEnabled(base::StrCat({"appendChild failed for element ",
                               base::NumberToString(child_index), ": ",
                               exception_state.Message().Utf8(),
                               ", returning unwrapped element"}));
    return element;
  }

  return shadow_host;
}

}  // namespace blink
