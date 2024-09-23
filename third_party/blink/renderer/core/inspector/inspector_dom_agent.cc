/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"

#include <memory>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/fenced_frame/document_fenced_frames.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/dom_editor.h"
#include "third_party/blink/renderer/core/inspector/dom_patch_support.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_highlight.h"
#include "third_party/blink/renderer/core/inspector/inspector_history.h"
#include "third_party/blink/renderer/core/inspector/resolve_node.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/core/xml/document_xpath_evaluator.h"
#include "third_party/blink/renderer/core/xml/xpath_result.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using mojom::blink::FormControlType;
using protocol::Maybe;

namespace {

const size_t kMaxTextSize = 10000;
const UChar kEllipsisUChar[] = {0x2026, 0};

template <typename Functor>
void ForEachSupportedPseudo(const Element* element, Functor& func) {
  for (PseudoId pseudo_id :
       {kPseudoIdBefore, kPseudoIdAfter, kPseudoIdMarker, kPseudoIdBackdrop}) {
    if (!PseudoElement::IsWebExposed(pseudo_id, element))
      continue;
    if (PseudoElement* pseudo_element = element->GetPseudoElement(pseudo_id))
      func(pseudo_element);
  }
  ViewTransitionUtils::ForEachDirectTransitionPseudo(element, func);
}

}  // namespace

class InspectorRevalidateDOMTask final
    : public GarbageCollected<InspectorRevalidateDOMTask> {
 public:
  explicit InspectorRevalidateDOMTask(InspectorDOMAgent*);
  void ScheduleStyleAttrRevalidationFor(Element*);
  void Reset() { timer_.Stop(); }
  void OnTimer(TimerBase*);
  void Trace(Visitor*) const;

 private:
  Member<InspectorDOMAgent> dom_agent_;
  HeapTaskRunnerTimer<InspectorRevalidateDOMTask> timer_;
  HeapHashSet<Member<Element>> style_attr_invalidated_elements_;
};

InspectorRevalidateDOMTask::InspectorRevalidateDOMTask(
    InspectorDOMAgent* dom_agent)
    : dom_agent_(dom_agent),
      timer_(
          dom_agent->GetDocument()->GetTaskRunner(TaskType::kDOMManipulation),
          this,
          &InspectorRevalidateDOMTask::OnTimer) {}

void InspectorRevalidateDOMTask::ScheduleStyleAttrRevalidationFor(
    Element* element) {
  style_attr_invalidated_elements_.insert(element);
  if (!timer_.IsActive())
    timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void InspectorRevalidateDOMTask::OnTimer(TimerBase*) {
  // The timer is stopped on m_domAgent destruction, so this method will never
  // be called after m_domAgent has been destroyed.
  HeapVector<Member<Element>> elements;
  for (auto& attribute : style_attr_invalidated_elements_)
    elements.push_back(attribute.Get());
  dom_agent_->StyleAttributeInvalidated(elements);
  style_attr_invalidated_elements_.clear();
}

void InspectorRevalidateDOMTask::Trace(Visitor* visitor) const {
  visitor->Trace(dom_agent_);
  visitor->Trace(style_attr_invalidated_elements_);
  visitor->Trace(timer_);
}

protocol::Response InspectorDOMAgent::ToResponse(
    ExceptionState& exception_state) {
  if (exception_state.HadException()) {
    String name_prefix = IsDOMExceptionCode(exception_state.Code())
                             ? DOMException::GetErrorName(
                                   exception_state.CodeAs<DOMExceptionCode>()) +
                                   " "
                             : g_empty_string;
    String msg = name_prefix + exception_state.Message();
    return protocol::Response::ServerError(msg.Utf8());
  }
  return protocol::Response::Success();
}

protocol::DOM::PseudoType InspectorDOMAgent::ProtocolPseudoElementType(
    PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdFirstLine:
      return protocol::DOM::PseudoTypeEnum::FirstLine;
    case kPseudoIdFirstLetter:
      return protocol::DOM::PseudoTypeEnum::FirstLetter;
    case kPseudoIdBefore:
      return protocol::DOM::PseudoTypeEnum::Before;
    case kPseudoIdAfter:
      return protocol::DOM::PseudoTypeEnum::After;
    case kPseudoIdMarker:
      return protocol::DOM::PseudoTypeEnum::Marker;
    case kPseudoIdBackdrop:
      return protocol::DOM::PseudoTypeEnum::Backdrop;
    case kPseudoIdSelection:
      return protocol::DOM::PseudoTypeEnum::Selection;
    case kPseudoIdSearchText:
      return protocol::DOM::PseudoTypeEnum::SearchText;
    case kPseudoIdTargetText:
      return protocol::DOM::PseudoTypeEnum::TargetText;
    case kPseudoIdSpellingError:
      return protocol::DOM::PseudoTypeEnum::SpellingError;
    case kPseudoIdGrammarError:
      return protocol::DOM::PseudoTypeEnum::GrammarError;
    case kPseudoIdHighlight:
      return protocol::DOM::PseudoTypeEnum::Highlight;
    case kPseudoIdFirstLineInherited:
      return protocol::DOM::PseudoTypeEnum::FirstLineInherited;
    case kPseudoIdScrollbar:
      return protocol::DOM::PseudoTypeEnum::Scrollbar;
    case kPseudoIdScrollbarThumb:
      return protocol::DOM::PseudoTypeEnum::ScrollbarThumb;
    case kPseudoIdScrollbarButton:
      return protocol::DOM::PseudoTypeEnum::ScrollbarButton;
    case kPseudoIdScrollbarTrack:
      return protocol::DOM::PseudoTypeEnum::ScrollbarTrack;
    case kPseudoIdScrollbarTrackPiece:
      return protocol::DOM::PseudoTypeEnum::ScrollbarTrackPiece;
    case kPseudoIdScrollbarCorner:
      return protocol::DOM::PseudoTypeEnum::ScrollbarCorner;
    case kPseudoIdScrollMarker:
      return protocol::DOM::PseudoTypeEnum::ScrollMarker;
    case kPseudoIdScrollMarkerGroup:
    case kPseudoIdScrollMarkerGroupAfter:
    case kPseudoIdScrollMarkerGroupBefore:
      return protocol::DOM::PseudoTypeEnum::ScrollMarkerGroup;
    case kPseudoIdScrollNextButton:
      return protocol::DOM::PseudoTypeEnum::ScrollNextButton;
    case kPseudoIdScrollPrevButton:
      return protocol::DOM::PseudoTypeEnum::ScrollPrevButton;
    case kPseudoIdColumn:
      return protocol::DOM::PseudoTypeEnum::Column;
    case kPseudoIdResizer:
      return protocol::DOM::PseudoTypeEnum::Resizer;
    case kPseudoIdInputListButton:
      return protocol::DOM::PseudoTypeEnum::InputListButton;
    case kPseudoIdPlaceholder:
      return protocol::DOM::PseudoTypeEnum::Placeholder;
    case kPseudoIdFileSelectorButton:
      return protocol::DOM::PseudoTypeEnum::FileSelectorButton;
    case kPseudoIdDetailsContent:
      return protocol::DOM::PseudoTypeEnum::DetailsContent;
    case kPseudoIdSelectFallbackButton:
      return protocol::DOM::PseudoTypeEnum::SelectFallbackButton;
    case kPseudoIdSelectFallbackButtonText:
      return protocol::DOM::PseudoTypeEnum::SelectFallbackButtonText;
    case kPseudoIdPickerSelect:
      return protocol::DOM::PseudoTypeEnum::Picker;
    case kPseudoIdViewTransition:
      return protocol::DOM::PseudoTypeEnum::ViewTransition;
    case kPseudoIdViewTransitionGroup:
      return protocol::DOM::PseudoTypeEnum::ViewTransitionGroup;
    case kPseudoIdViewTransitionImagePair:
      return protocol::DOM::PseudoTypeEnum::ViewTransitionImagePair;
    case kPseudoIdViewTransitionNew:
      return protocol::DOM::PseudoTypeEnum::ViewTransitionNew;
    case kPseudoIdViewTransitionOld:
      return protocol::DOM::PseudoTypeEnum::ViewTransitionOld;
    case kPseudoIdColumnScrollMarker:
      // Not reachable, since it's an internal representation of
      // ::column::scroll-marker and won't be exposed to devtools
      NOTREACHED_NORETURN();
    case kAfterLastInternalPseudoId:
    case kPseudoIdNone:
    case kPseudoIdInvalid:
      CHECK(false);
      return "";
  }
}

InspectorDOMAgent::InspectorDOMAgent(
    v8::Isolate* isolate,
    InspectedFrames* inspected_frames,
    v8_inspector::V8InspectorSession* v8_session)
    : isolate_(isolate),
      inspected_frames_(inspected_frames),
      v8_session_(v8_session),
      document_node_to_id_map_(MakeGarbageCollected<NodeToIdMap>()),
      last_node_id_(1),
      suppress_attribute_modified_event_(false),
      enabled_(&agent_state_, /*default_value=*/false),
      include_whitespace_(&agent_state_,
                          /*default_value=*/static_cast<int32_t>(
                              InspectorDOMAgent::IncludeWhitespaceEnum::NONE)),
      capture_node_stack_traces_(&agent_state_, /*default_value=*/false) {}

InspectorDOMAgent::~InspectorDOMAgent() = default;

void InspectorDOMAgent::Restore() {
  if (enabled_.Get())
    EnableAndReset();
}

HeapVector<Member<Document>> InspectorDOMAgent::Documents() {
  HeapVector<Member<Document>> result;
  if (document_) {
    for (LocalFrame* frame : *inspected_frames_) {
      if (Document* document = frame->GetDocument())
        result.push_back(document);
    }
  }
  return result;
}

void InspectorDOMAgent::AddDOMListener(DOMListener* listener) {
  dom_listeners_.insert(listener);
}

void InspectorDOMAgent::RemoveDOMListener(DOMListener* listener) {
  dom_listeners_.erase(listener);
}

void InspectorDOMAgent::NotifyDidAddDocument(Document* document) {
  for (DOMListener* listener : dom_listeners_)
    listener->DidAddDocument(document);
}

void InspectorDOMAgent::NotifyWillRemoveDOMNode(Node* node) {
  for (DOMListener* listener : dom_listeners_)
    listener->WillRemoveDOMNode(node);
}

void InspectorDOMAgent::NotifyDidModifyDOMAttr(Element* element) {
  for (DOMListener* listener : dom_listeners_)
    listener->DidModifyDOMAttr(element);
}

void InspectorDOMAgent::SetDocument(Document* doc) {
  if (doc == document_.Get())
    return;

  DiscardFrontendBindings();
  document_ = doc;

  if (!enabled_.Get())
    return;

  // Immediately communicate 0 document or document that has finished loading.
  if (!doc || !doc->Parsing())
    GetFrontend()->documentUpdated();
}

bool InspectorDOMAgent::Enabled() const {
  return enabled_.Get();
}

InspectorDOMAgent::IncludeWhitespaceEnum InspectorDOMAgent::IncludeWhitespace()
    const {
  return static_cast<InspectorDOMAgent::IncludeWhitespaceEnum>(
      include_whitespace_.Get());
}

void InspectorDOMAgent::ReleaseDanglingNodes() {
  dangling_node_to_id_maps_.clear();
}

int InspectorDOMAgent::Bind(Node* node, NodeToIdMap* nodes_map) {
  if (!nodes_map)
    return 0;
  auto it = nodes_map->find(node);
  if (it != nodes_map->end())
    return it->value;

  int id = last_node_id_++;
  nodes_map->Set(node, id);
  id_to_node_.Set(id, node);
  id_to_nodes_map_.Set(id, nodes_map);
  return id;
}

void InspectorDOMAgent::Unbind(Node* node) {
  int id = BoundNodeId(node);
  if (!id)
    return;

  id_to_node_.erase(id);
  id_to_nodes_map_.erase(id);

  if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
    Document* content_document = frame_owner->contentDocument();
    if (content_document)
      Unbind(content_document);
  }

  if (ShadowRoot* root = node->GetShadowRoot())
    Unbind(root);

  auto* element = DynamicTo<Element>(node);
  if (element) {
    auto unbind_pseudo = [&](PseudoElement* pseudo_element) {
      Unbind(pseudo_element);
    };
    ForEachSupportedPseudo(element, unbind_pseudo);
  }

  NotifyWillRemoveDOMNode(node);
  document_node_to_id_map_->erase(node);

  bool children_requested = children_requested_.Contains(id);
  if (children_requested) {
    // Unbind subtree known to client recursively.
    children_requested_.erase(id);
    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace =
        IncludeWhitespace();
    Node* child = InnerFirstChild(node, include_whitespace);
    while (child) {
      Unbind(child);
      child = InnerNextSibling(child, include_whitespace);
    }
  }
  cached_child_count_.erase(id);
}

protocol::Response InspectorDOMAgent::AssertNode(int node_id, Node*& node) {
  node = NodeForId(node_id);
  if (!node)
    return protocol::Response::ServerError("Could not find node with given id");
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::AssertNode(
    const protocol::Maybe<int>& node_id,
    const protocol::Maybe<int>& backend_node_id,
    const protocol::Maybe<String>& object_id,
    Node*& node) {
  if (node_id.has_value()) {
    return AssertNode(node_id.value(), node);
  }

  if (backend_node_id.has_value()) {
    node = DOMNodeIds::NodeForId(backend_node_id.value());
    return !node ? protocol::Response::ServerError(
                       "No node found for given backend id")
                 : protocol::Response::Success();
  }

  if (object_id.has_value()) {
    return NodeForRemoteObjectId(object_id.value(), node);
  }

  return protocol::Response::ServerError(
      "Either nodeId, backendNodeId or objectId must be specified");
}

protocol::Response InspectorDOMAgent::AssertElement(int node_id,
                                                    Element*& element) {
  Node* node = nullptr;
  protocol::Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  element = DynamicTo<Element>(node);
  if (!element)
    return protocol::Response::ServerError("Node is not an Element");
  return protocol::Response::Success();
}

// static
ShadowRoot* InspectorDOMAgent::UserAgentShadowRoot(Node* node) {
  if (!node || !node->IsInShadowTree())
    return nullptr;

  Node* candidate = node;
  while (candidate && !IsA<ShadowRoot>(candidate))
    candidate = candidate->ParentOrShadowHostNode();
  DCHECK(candidate);
  ShadowRoot* shadow_root = To<ShadowRoot>(candidate);

  return shadow_root->IsUserAgent() ? shadow_root : nullptr;
}

protocol::Response InspectorDOMAgent::AssertEditableNode(int node_id,
                                                         Node*& node) {
  protocol::Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  if (node->IsInShadowTree()) {
    if (IsA<ShadowRoot>(node))
      return protocol::Response::ServerError("Cannot edit shadow roots");
    if (UserAgentShadowRoot(node)) {
      return protocol::Response::ServerError(
          "Cannot edit nodes from user-agent shadow trees");
    }
  }

  if (node->IsPseudoElement())
    return protocol::Response::ServerError("Cannot edit pseudo elements");
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::AssertEditableChildNode(
    Element* parent_element,
    int node_id,
    Node*& node) {
  protocol::Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;
  if (node->parentNode() != parent_element) {
    return protocol::Response::ServerError(
        "Anchor node must be child of the target element");
  }
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::AssertEditableElement(int node_id,
                                                            Element*& element) {
  protocol::Response response = AssertElement(node_id, element);
  if (!response.IsSuccess())
    return response;
  if (element->IsInShadowTree() && UserAgentShadowRoot(element)) {
    return protocol::Response::ServerError(
        "Cannot edit elements from user-agent shadow trees");
  }
  if (element->IsPseudoElement())
    return protocol::Response::ServerError("Cannot edit pseudo elements");

  return protocol::Response::Success();
}

void InspectorDOMAgent::EnableAndReset() {
  enabled_.Set(true);
  history_ = MakeGarbageCollected<InspectorHistory>();
  dom_editor_ = MakeGarbageCollected<DOMEditor>(history_.Get());
  document_ = inspected_frames_->Root()->GetDocument();
  instrumenting_agents_->AddInspectorDOMAgent(this);
}

protocol::Response InspectorDOMAgent::enable(Maybe<String> includeWhitespace) {
  if (!enabled_.Get()) {
    EnableAndReset();
    include_whitespace_.Set(static_cast<int32_t>(
        includeWhitespace.value_or(
            protocol::DOM::Enable::IncludeWhitespaceEnum::None) ==
                protocol::DOM::Enable::IncludeWhitespaceEnum::All
            ? InspectorDOMAgent::IncludeWhitespaceEnum::ALL
            : InspectorDOMAgent::IncludeWhitespaceEnum::NONE));
  }
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::disable() {
  if (!enabled_.Get())
    return protocol::Response::ServerError("DOM agent hasn't been enabled");
  include_whitespace_.Clear();
  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorDOMAgent(this);
  history_.Clear();
  dom_editor_.Clear();
  SetDocument(nullptr);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getDocument(
    Maybe<int> depth,
    Maybe<bool> pierce,
    std::unique_ptr<protocol::DOM::Node>* root) {
  // Backward compatibility. Mark agent as enabled when it requests document.
  if (!enabled_.Get())
    enable(Maybe<String>());

  if (!document_)
    return protocol::Response::ServerError("Document is not available");

  DiscardFrontendBindings();

  int sanitized_depth = depth.value_or(2);
  if (sanitized_depth == -1)
    sanitized_depth = INT_MAX;

  *root = BuildObjectForNode(document_.Get(), sanitized_depth,
                             pierce.value_or(false),
                             document_node_to_id_map_.Get());
  return protocol::Response::Success();
}

namespace {

bool NodeHasMatchingStyles(
    const HashMap<CSSPropertyID, HashSet<String>>* properties,
    Node* node) {
  if (auto* element = DynamicTo<Element>(node)) {
    auto* computed_style_info =
        MakeGarbageCollected<CSSComputedStyleDeclaration>(element, true);
    for (const auto& property : *properties) {
      const CSSValue* computed_value =
          computed_style_info->GetPropertyCSSValue(property.key);
      if (computed_value &&
          property.value.Contains(computed_value->CssText())) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

protocol::Response InspectorDOMAgent::getNodesForSubtreeByStyle(
    int node_id,
    std::unique_ptr<protocol::Array<protocol::DOM::CSSComputedStyleProperty>>
        computed_styles,
    Maybe<bool> pierce,
    std::unique_ptr<protocol::Array<int>>* node_ids) {
  if (!enabled_.Get())
    return protocol::Response::ServerError("DOM agent hasn't been enabled");

  if (!document_)
    return protocol::Response::ServerError("Document is not available");

  Node* root_node = nullptr;
  protocol::Response response = AssertNode(node_id, root_node);
  if (!response.IsSuccess())
    return response;

  HashMap<CSSPropertyID, HashSet<String>> properties;
  for (const auto& style : *computed_styles) {
    std::optional<CSSPropertyName> property_name = CSSPropertyName::From(
        document_->GetExecutionContext(), style->getName());
    if (!property_name)
      return protocol::Response::InvalidParams("Invalid CSS property name");
    auto property_id = property_name->Id();
    HashMap<CSSPropertyID, HashSet<String>>::iterator it =
        properties.find(property_id);
    if (it != properties.end())
      it->value.insert(style->getValue());
    else
      properties.Set(property_id, HashSet<String>({style->getValue()}));
  }

  HeapVector<Member<Node>> nodes;

  CollectNodes(
      root_node, INT_MAX, pierce.value_or(false), IncludeWhitespace(),
      WTF::BindRepeating(&NodeHasMatchingStyles, WTF::Unretained(&properties)),
      &nodes);

  NodeToIdMap* nodes_map = document_node_to_id_map_.Get();
  *node_ids = std::make_unique<protocol::Array<int>>();
  for (Node* node : nodes) {
    int id = PushNodePathToFrontend(node, nodes_map);
    (*node_ids)->push_back(id);
  }

  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getFlattenedDocument(
    Maybe<int> depth,
    Maybe<bool> pierce,
    std::unique_ptr<protocol::Array<protocol::DOM::Node>>* nodes) {
  if (!enabled_.Get())
    return protocol::Response::ServerError("DOM agent hasn't been enabled");

  if (!document_)
    return protocol::Response::ServerError("Document is not available");

  DiscardFrontendBindings();

  int sanitized_depth = depth.value_or(-1);
  if (sanitized_depth == -1)
    sanitized_depth = INT_MAX;

  *nodes = std::make_unique<protocol::Array<protocol::DOM::Node>>();
  (*nodes)->emplace_back(BuildObjectForNode(
      document_.Get(), sanitized_depth, pierce.value_or(false),
      document_node_to_id_map_.Get(), nodes->get()));
  return protocol::Response::Success();
}

void InspectorDOMAgent::PushChildNodesToFrontend(int node_id,
                                                 int depth,
                                                 bool pierce) {
  Node* node = NodeForId(node_id);
  if (!node || (!node->IsElementNode() && !node->IsDocumentNode() &&
                !node->IsDocumentFragment()))
    return;

  NodeToIdMap* node_map = id_to_nodes_map_.at(node_id);

  if (children_requested_.Contains(node_id)) {
    if (depth <= 1)
      return;

    depth--;

    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace =
        IncludeWhitespace();
    for (node = InnerFirstChild(node, include_whitespace); node;
         node = InnerNextSibling(node, include_whitespace)) {
      int child_node_id = node_map->at(node);
      DCHECK(child_node_id);
      PushChildNodesToFrontend(child_node_id, depth, pierce);
    }

    return;
  }

  std::unique_ptr<protocol::Array<protocol::DOM::Node>> children =
      BuildArrayForContainerChildren(node, depth, pierce, node_map, nullptr);
  GetFrontend()->setChildNodes(node_id, std::move(children));
}

void InspectorDOMAgent::DiscardFrontendBindings() {
  if (history_)
    history_->Reset();
  search_results_.clear();
  document_node_to_id_map_->clear();
  id_to_node_.clear();
  id_to_nodes_map_.clear();
  ReleaseDanglingNodes();
  children_requested_.clear();
  cached_child_count_.clear();
  if (revalidate_task_)
    revalidate_task_->Reset();
}

Node* InspectorDOMAgent::NodeForId(int id) const {
  if (!id)
    return nullptr;

  const auto it = id_to_node_.find(id);
  if (it != id_to_node_.end())
    return it->value.Get();
  return nullptr;
}

protocol::Response InspectorDOMAgent::collectClassNamesFromSubtree(
    int node_id,
    std::unique_ptr<protocol::Array<String>>* class_names) {
  HashSet<String> unique_names;
  *class_names = std::make_unique<protocol::Array<String>>();
  Node* parent_node = NodeForId(node_id);
  if (!parent_node) {
    return protocol::Response::ServerError(
        "No suitable node with given id found");
  }
  auto* parent_element = DynamicTo<Element>(parent_node);
  if (!parent_element && !parent_node->IsDocumentNode() &&
      !parent_node->IsDocumentFragment()) {
    return protocol::Response::ServerError(
        "No suitable node with given id found");
  }

  for (Node* node = parent_node; node;
       node = FlatTreeTraversal::Next(*node, parent_node)) {
    if (const auto* element = DynamicTo<Element>(node)) {
      if (!element->HasClass())
        continue;
      const SpaceSplitString& class_name_list = element->ClassNames();
      for (unsigned i = 0; i < class_name_list.size(); ++i)
        unique_names.insert(class_name_list[i]);
    }
  }
  for (const String& class_name : unique_names)
    (*class_names)->emplace_back(class_name);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::requestChildNodes(
    int node_id,
    Maybe<int> depth,
    Maybe<bool> maybe_taverse_frames) {
  int sanitized_depth = depth.value_or(1);
  if (sanitized_depth == 0 || sanitized_depth < -1) {
    return protocol::Response::ServerError(
        "Please provide a positive integer as a depth or -1 for entire "
        "subtree");
  }
  if (sanitized_depth == -1)
    sanitized_depth = INT_MAX;

  PushChildNodesToFrontend(node_id, sanitized_depth,
                           maybe_taverse_frames.value_or(false));
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::querySelector(int node_id,
                                                    const String& selectors,
                                                    int* element_id) {
  *element_id = 0;
  Node* node = nullptr;
  protocol::Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;
  auto* container_node = DynamicTo<ContainerNode>(node);
  if (!container_node)
    return protocol::Response::ServerError("Not a container node");

  DummyExceptionStateForTesting exception_state;
  Element* element =
      container_node->QuerySelector(AtomicString(selectors), exception_state);
  if (exception_state.HadException())
    return protocol::Response::ServerError("DOM Error while querying");

  if (element)
    *element_id = PushNodePathToFrontend(element);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::querySelectorAll(
    int node_id,
    const String& selectors,
    std::unique_ptr<protocol::Array<int>>* result) {
  Node* node = nullptr;
  protocol::Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;
  auto* container_node = DynamicTo<ContainerNode>(node);
  if (!container_node)
    return protocol::Response::ServerError("Not a container node");

  DummyExceptionStateForTesting exception_state;
  StaticElementList* elements = container_node->QuerySelectorAll(
      AtomicString(selectors), exception_state);
  if (exception_state.HadException())
    return protocol::Response::ServerError("DOM Error while querying");

  *result = std::make_unique<protocol::Array<int>>();

  for (unsigned i = 0; i < elements->length(); ++i)
    (*result)->emplace_back(PushNodePathToFrontend(elements->item(i)));
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getTopLayerElements(
    std::unique_ptr<protocol::Array<int>>* result) {
  if (!document_)
    return protocol::Response::ServerError("DOM agent hasn't been enabled");

  *result = std::make_unique<protocol::Array<int>>();
  for (auto document : Documents()) {
    for (auto element : document->TopLayerElements()) {
      int node_id = PushNodePathToFrontend(element);
      if (node_id)
        (*result)->emplace_back(node_id);
    }
  }

  return protocol::Response::Success();
}

int InspectorDOMAgent::PushNodePathToFrontend(Node* node_to_push,
                                              NodeToIdMap* node_map) {
  DCHECK(node_to_push);  // Invalid input
  // InspectorDOMAgent might have been resetted already. See crbug.com/450491
  if (!document_)
    return 0;
  if (!BoundNodeId(document_))
    return 0;

  // Return id in case the node is known.
  if (auto it = node_map->find(node_to_push); it != node_map->end())
    return it->value;

  Node* node = node_to_push;
  HeapVector<Member<Node>> path;

  while (true) {
    Node* parent = InnerParentNode(node);
    if (!parent)
      return 0;
    path.push_back(parent);
    if (node_map->Contains(parent))
      break;
    node = parent;
  }

  for (int i = path.size() - 1; i >= 0; --i) {
    if (auto it = node_map->find(path.at(i).Get()); it != node_map->end()) {
      int node_id = it->value;
      DCHECK(node_id);
      PushChildNodesToFrontend(node_id);
    }
  }
  auto it = node_map->find(node_to_push);
  return it != node_map->end() ? it->value : 0;
}

int InspectorDOMAgent::PushNodePathToFrontend(Node* node_to_push) {
  if (!document_)
    return 0;

  int node_id =
      PushNodePathToFrontend(node_to_push, document_node_to_id_map_.Get());
  if (node_id)
    return node_id;

  Node* node = node_to_push;
  while (Node* parent = InnerParentNode(node))
    node = parent;

  // Node being pushed is detached -> push subtree root.
  NodeToIdMap* new_map = MakeGarbageCollected<NodeToIdMap>();
  NodeToIdMap* dangling_map = new_map;
  dangling_node_to_id_maps_.push_back(new_map);
  auto children = std::make_unique<protocol::Array<protocol::DOM::Node>>();
  children->emplace_back(BuildObjectForNode(node, 0, false, dangling_map));
  GetFrontend()->setChildNodes(0, std::move(children));

  return PushNodePathToFrontend(node_to_push, dangling_map);
}

int InspectorDOMAgent::BoundNodeId(Node* node) const {
  auto it = document_node_to_id_map_->find(node);
  return it != document_node_to_id_map_->end() ? it->value : 0;
}

protocol::Response InspectorDOMAgent::setAttributeValue(int element_id,
                                                        const String& name,
                                                        const String& value) {
  Element* element = nullptr;
  protocol::Response response = AssertEditableElement(element_id, element);
  if (!response.IsSuccess())
    return response;
  return dom_editor_->SetAttribute(element, name, value);
}

protocol::Response InspectorDOMAgent::setAttributesAsText(int element_id,
                                                          const String& text,
                                                          Maybe<String> name) {
  Element* element = nullptr;
  protocol::Response response = AssertEditableElement(element_id, element);
  if (!response.IsSuccess())
    return response;

  bool is_html_document = IsA<HTMLDocument>(element->GetDocument());

  auto getContextElement = [](Element* element,
                              bool is_html_document) -> Element* {
    // Not all elements can represent the context (e.g. <iframe>). Use
    // the owner <svg> element if there is any, falling back to <body>,
    // falling back to nullptr (in the case of non-SVG XML documents).
    if (auto* svg_element = DynamicTo<SVGElement>(element)) {
      SVGSVGElement* owner = svg_element->ownerSVGElement();
      if (owner)
        return owner;
    }

    if (is_html_document)
      return element->GetDocument().body();

    return nullptr;
  };

  Element* contextElement = getContextElement(element, is_html_document);

  auto getParsedElement = [](Element* element, Element* contextElement,
                             const String& text, bool is_html_document) {
    String markup = element->IsSVGElement()
                        ? "<svg " + text + "></svg>"
                        : element->IsMathMLElement()
                              ? "<math " + text + "></math>"
                              : "<span " + text + "></span>";
    DocumentFragment* fragment =
        element->GetDocument().createDocumentFragment();
    if (is_html_document && contextElement)
      fragment->ParseHTML(markup, contextElement, kAllowScriptingContent);
    else
      fragment->ParseXML(markup, contextElement, kAllowScriptingContent);
    return DynamicTo<Element>(fragment->firstChild());
  };

  Element* parsed_element =
      getParsedElement(element, contextElement, text, is_html_document);
  if (!parsed_element) {
    return protocol::Response::ServerError(
        "Could not parse value as attributes");
  }

  bool should_ignore_case = is_html_document && element->IsHTMLElement();
  String case_adjusted_name = should_ignore_case
                                  ? name.value_or("").DeprecatedLower()
                                  : name.value_or("");

  AttributeCollection attributes = parsed_element->Attributes();
  if (attributes.IsEmpty() && name.has_value()) {
    return dom_editor_->RemoveAttribute(element, case_adjusted_name);
  }

  bool found_original_attribute = false;
  for (auto& attribute : attributes) {
    // Add attribute pair
    String attribute_name = attribute.GetName().ToString();
    if (should_ignore_case)
      attribute_name = attribute_name.DeprecatedLower();
    found_original_attribute |=
        name.has_value() && attribute_name == case_adjusted_name;
    response =
        dom_editor_->SetAttribute(element, attribute_name, attribute.Value());
    if (!response.IsSuccess())
      return response;
  }

  if (!found_original_attribute && name.has_value() &&
      name.value().LengthWithStrippedWhiteSpace() > 0) {
    return dom_editor_->RemoveAttribute(element, case_adjusted_name);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::removeAttribute(int element_id,
                                                      const String& name) {
  Element* element = nullptr;
  protocol::Response response = AssertEditableElement(element_id, element);
  if (!response.IsSuccess())
    return response;

  return dom_editor_->RemoveAttribute(element, name);
}

protocol::Response InspectorDOMAgent::removeNode(int node_id) {
  Node* node = nullptr;
  protocol::Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  ContainerNode* parent_node = node->parentNode();
  if (!parent_node)
    return protocol::Response::ServerError("Cannot remove detached node");

  return dom_editor_->RemoveChild(parent_node, node);
}

protocol::Response InspectorDOMAgent::setNodeName(int node_id,
                                                  const String& tag_name,
                                                  int* new_id) {
  *new_id = 0;

  Element* old_element = nullptr;
  protocol::Response response = AssertElement(node_id, old_element);
  if (!response.IsSuccess())
    return response;

  DummyExceptionStateForTesting exception_state;
  Element* new_elem = old_element->GetDocument().CreateElementForBinding(
      AtomicString(tag_name), exception_state);
  if (exception_state.HadException())
    return ToResponse(exception_state);

  // Copy over the original node's attributes.
  new_elem->CloneAttributesFrom(*old_element);

  // Copy over the original node's children.
  for (Node* child = old_element->firstChild(); child;
       child = old_element->firstChild()) {
    response = dom_editor_->InsertBefore(new_elem, child, nullptr);
    if (!response.IsSuccess())
      return response;
  }

  // Replace the old node with the new node
  ContainerNode* parent = old_element->parentNode();
  response =
      dom_editor_->InsertBefore(parent, new_elem, old_element->nextSibling());
  if (!response.IsSuccess())
    return response;
  response = dom_editor_->RemoveChild(parent, old_element);
  if (!response.IsSuccess())
    return response;

  *new_id = PushNodePathToFrontend(new_elem);
  if (children_requested_.Contains(node_id))
    PushChildNodesToFrontend(*new_id);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getOuterHTML(Maybe<int> node_id,
                                                   Maybe<int> backend_node_id,
                                                   Maybe<String> object_id,
                                                   WTF::String* outer_html) {
  Node* node = nullptr;
  protocol::Response response =
      AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;

  *outer_html = CreateMarkup(node);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::setOuterHTML(int node_id,
                                                   const String& outer_html) {
  if (!node_id) {
    DCHECK(document_);
    DOMPatchSupport dom_patch_support(dom_editor_.Get(), *document_.Get());
    dom_patch_support.PatchDocument(outer_html);
    return protocol::Response::Success();
  }

  Node* node = nullptr;
  protocol::Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  Document* document = DynamicTo<Document>(node);
  if (!document) {
    document = node->ownerDocument();
  }
  if (!document ||
      (!IsA<HTMLDocument>(document) && !IsA<XMLDocument>(document)))
    return protocol::Response::ServerError("Not an HTML/XML document");

  Node* new_node = nullptr;
  response = dom_editor_->SetOuterHTML(node, outer_html, &new_node);
  if (!response.IsSuccess())
    return response;

  if (!new_node) {
    // The only child node has been deleted.
    return protocol::Response::Success();
  }

  int new_id = PushNodePathToFrontend(new_node);

  bool children_requested = children_requested_.Contains(node_id);
  if (children_requested)
    PushChildNodesToFrontend(new_id);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::setNodeValue(int node_id,
                                                   const String& value) {
  Node* node = nullptr;
  protocol::Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  if (node->getNodeType() != Node::kTextNode)
    return protocol::Response::ServerError("Can only set value of text nodes");

  return dom_editor_->SetNodeValue(node, value);
}

static Node* NextNodeWithShadowDOMInMind(const Node& current,
                                         const Node* stay_within,
                                         bool include_user_agent_shadow_dom) {
  // At first traverse the subtree.

  if (ShadowRoot* shadow_root = current.GetShadowRoot()) {
    if (!shadow_root->IsUserAgent() || include_user_agent_shadow_dom)
      return shadow_root;
  }
  if (current.hasChildren())
    return current.firstChild();

  // Then traverse siblings of the node itself and its ancestors.
  const Node* node = &current;
  do {
    if (node == stay_within)
      return nullptr;
    auto* shadow_root = DynamicTo<ShadowRoot>(node);
    if (shadow_root) {
      Element& host = shadow_root->host();
      if (host.HasChildren())
        return host.firstChild();
    }
    if (node->nextSibling())
      return node->nextSibling();
    node = shadow_root ? &shadow_root->host() : node->parentNode();
  } while (node);

  return nullptr;
}

protocol::Response InspectorDOMAgent::performSearch(
    const String& whitespace_trimmed_query,
    Maybe<bool> optional_include_user_agent_shadow_dom,
    String* search_id,
    int* result_count) {
  if (!enabled_.Get())
    return protocol::Response::ServerError("DOM agent is not enabled");

  // FIXME: Few things are missing here:
  // 1) Search works with node granularity - number of matches within node is
  //    not calculated.
  // 2) There is no need to push all search results to the front-end at a time,
  //    pushing next / previous result is sufficient.

  bool include_user_agent_shadow_dom =
      optional_include_user_agent_shadow_dom.value_or(false);

  unsigned query_length = whitespace_trimmed_query.length();
  bool start_tag_found = !whitespace_trimmed_query.find('<');
  bool start_closing_tag_found = !whitespace_trimmed_query.Find("</");
  bool end_tag_found =
      whitespace_trimmed_query.ReverseFind('>') + 1 == query_length;
  bool start_quote_found = !whitespace_trimmed_query.find('"');
  bool end_quote_found =
      whitespace_trimmed_query.ReverseFind('"') + 1 == query_length;
  bool exact_attribute_match = start_quote_found && end_quote_found;

  String tag_name_query = whitespace_trimmed_query;
  String attribute_query = whitespace_trimmed_query;
  if (start_closing_tag_found)
    tag_name_query = tag_name_query.Right(tag_name_query.length() - 2);
  else if (start_tag_found)
    tag_name_query = tag_name_query.Right(tag_name_query.length() - 1);
  if (end_tag_found)
    tag_name_query = tag_name_query.Left(tag_name_query.length() - 1);
  if (start_quote_found)
    attribute_query = attribute_query.Right(attribute_query.length() - 1);
  if (end_quote_found)
    attribute_query = attribute_query.Left(attribute_query.length() - 1);

  HeapVector<Member<Document>> docs = Documents();
  HeapLinkedHashSet<Member<Node>> result_collector;

  // Selector evaluation
  for (Document* document : docs) {
    DummyExceptionStateForTesting exception_state;
    StaticElementList* element_list = document->QuerySelectorAll(
        AtomicString(whitespace_trimmed_query), exception_state);
    if (exception_state.HadException() || !element_list) {
      continue;
    }

    unsigned size = element_list->length();
    for (unsigned i = 0; i < size; ++i) {
      result_collector.insert(element_list->item(i));
    }
  }

  for (Document* document : docs) {
    Node* document_element = document->documentElement();
    Node* node = document_element;
    if (!node)
      continue;

    // Manual plain text search.
    for (; node; node = NextNodeWithShadowDOMInMind(
                     *node, document_element, include_user_agent_shadow_dom)) {
      switch (node->getNodeType()) {
        case Node::kTextNode:
        case Node::kCommentNode:
        case Node::kCdataSectionNode: {
          String text = node->nodeValue();
          if (text.FindIgnoringCase(whitespace_trimmed_query) != kNotFound)
            result_collector.insert(node);
          break;
        }
        case Node::kElementNode: {
          if ((!start_tag_found && !end_tag_found &&
               (node->nodeName().FindIgnoringCase(tag_name_query) !=
                kNotFound)) ||
              (start_tag_found && end_tag_found &&
               DeprecatedEqualIgnoringCase(node->nodeName(), tag_name_query)) ||
              (start_tag_found && !end_tag_found &&
               node->nodeName().StartsWithIgnoringCase(tag_name_query)) ||
              (!start_tag_found && end_tag_found &&
               node->nodeName().EndsWithIgnoringCase(tag_name_query))) {
            result_collector.insert(node);
            break;
          }
          // Go through all attributes and serialize them.
          const auto* element = To<Element>(node);
          AttributeCollection attributes = element->Attributes();
          for (auto& attribute : attributes) {
            // Add attribute pair
            if (attribute.LocalName().FindIgnoringCase(whitespace_trimmed_query,
                                                       0) != kNotFound) {
              result_collector.insert(node);
              break;
            }
            size_t found_position =
                attribute.Value().FindIgnoringCase(attribute_query, 0);
            if (found_position != kNotFound) {
              if (!exact_attribute_match ||
                  (!found_position &&
                   attribute.Value().length() == attribute_query.length())) {
                result_collector.insert(node);
                break;
              }
            }
          }
          break;
        }
        default:
          break;
      }
    }
  }

  // XPath evaluation
  for (Document* document : docs) {
    DCHECK(document);
    DummyExceptionStateForTesting exception_state;
    XPathResult* result = DocumentXPathEvaluator::evaluate(
        *document, whitespace_trimmed_query, document, nullptr,
        XPathResult::kOrderedNodeSnapshotType, ScriptValue(), exception_state);
    if (exception_state.HadException() || !result)
      continue;

    wtf_size_t size = result->snapshotLength(exception_state);
    for (wtf_size_t i = 0; !exception_state.HadException() && i < size; ++i) {
      Node* node = result->snapshotItem(i, exception_state);
      if (exception_state.HadException())
        break;

      if (node->getNodeType() == Node::kAttributeNode)
        node = To<Attr>(node)->ownerElement();
      result_collector.insert(node);
    }
  }

  *search_id = IdentifiersFactory::CreateIdentifier();
  HeapVector<Member<Node>>* results_it =
      search_results_
          .insert(*search_id, MakeGarbageCollected<HeapVector<Member<Node>>>())
          .stored_value->value;

  for (auto& result : result_collector)
    results_it->push_back(result);

  *result_count = results_it->size();
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getSearchResults(
    const String& search_id,
    int from_index,
    int to_index,
    std::unique_ptr<protocol::Array<int>>* node_ids) {
  SearchResults::iterator it = search_results_.find(search_id);
  if (it == search_results_.end()) {
    return protocol::Response::ServerError(
        "No search session with given id found");
  }

  int size = it->value->size();
  if (from_index < 0 || to_index > size || from_index >= to_index)
    return protocol::Response::ServerError("Invalid search result range");

  *node_ids = std::make_unique<protocol::Array<int>>();
  for (int i = from_index; i < to_index; ++i)
    (*node_ids)->emplace_back(PushNodePathToFrontend((*it->value)[i].Get()));
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::discardSearchResults(
    const String& search_id) {
  search_results_.erase(search_id);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::NodeForRemoteObjectId(
    const String& object_id,
    Node*& node) {
  v8::HandleScope handles(isolate_);
  v8::Local<v8::Value> value;
  v8::Local<v8::Context> context;
  std::unique_ptr<v8_inspector::StringBuffer> error;
  if (!v8_session_->unwrapObject(&error, ToV8InspectorStringView(object_id),
                                 &value, &context, nullptr)) {
    return protocol::Response::ServerError(
        ToCoreString(std::move(error)).Utf8());
  }
  node = V8Node::ToWrappable(isolate_, value);
  if (!node) {
    return protocol::Response::ServerError(
        "Object id doesn't reference a Node");
  }
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::copyTo(int node_id,
                                             int target_element_id,
                                             Maybe<int> anchor_node_id,
                                             int* new_node_id) {
  Node* node = nullptr;
  protocol::Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  Element* target_element = nullptr;
  response = AssertEditableElement(target_element_id, target_element);
  if (!response.IsSuccess())
    return response;

  Node* anchor_node = nullptr;
  if (anchor_node_id.has_value() && anchor_node_id.value()) {
    response = AssertEditableChildNode(target_element, anchor_node_id.value(),
                                       anchor_node);
    if (!response.IsSuccess())
      return response;
  }

  // The clone is deep by default.
  Node* cloned_node = node->cloneNode(true);
  if (!cloned_node)
    return protocol::Response::ServerError("Failed to clone node");
  response =
      dom_editor_->InsertBefore(target_element, cloned_node, anchor_node);
  if (!response.IsSuccess())
    return response;

  *new_node_id = PushNodePathToFrontend(cloned_node);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::moveTo(int node_id,
                                             int target_element_id,
                                             Maybe<int> anchor_node_id,
                                             int* new_node_id) {
  Node* node = nullptr;
  protocol::Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  Element* target_element = nullptr;
  response = AssertEditableElement(target_element_id, target_element);
  if (!response.IsSuccess())
    return response;

  Node* current = target_element;
  while (current) {
    if (current == node) {
      return protocol::Response::ServerError(
          "Unable to move node into self or descendant");
    }
    current = current->parentNode();
  }

  Node* anchor_node = nullptr;
  if (anchor_node_id.has_value() && anchor_node_id.value()) {
    response = AssertEditableChildNode(target_element, anchor_node_id.value(),
                                       anchor_node);
    if (!response.IsSuccess())
      return response;
  }

  response = dom_editor_->InsertBefore(target_element, node, anchor_node);
  if (!response.IsSuccess())
    return response;

  *new_node_id = PushNodePathToFrontend(node);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::undo() {
  if (!enabled_.Get())
    return protocol::Response::ServerError("DOM agent is not enabled");
  DummyExceptionStateForTesting exception_state;
  history_->Undo(exception_state);
  return InspectorDOMAgent::ToResponse(exception_state);
}

protocol::Response InspectorDOMAgent::redo() {
  if (!enabled_.Get())
    return protocol::Response::ServerError("DOM agent is not enabled");
  DummyExceptionStateForTesting exception_state;
  history_->Redo(exception_state);
  return InspectorDOMAgent::ToResponse(exception_state);
}

protocol::Response InspectorDOMAgent::markUndoableState() {
  if (!enabled_.Get())
    return protocol::Response::ServerError("DOM agent is not enabled");
  history_->MarkUndoableState();
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::focus(Maybe<int> node_id,
                                            Maybe<int> backend_node_id,
                                            Maybe<String> object_id) {
  Node* node = nullptr;
  protocol::Response response =
      AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return protocol::Response::ServerError("Node is not an Element");
  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kInspector);
  if (!element->IsFocusable())
    return protocol::Response::ServerError("Element is not focusable");
  element->Focus(FocusParams(FocusTrigger::kUserGesture));
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::setFileInputFiles(
    std::unique_ptr<protocol::Array<String>> files,
    Maybe<int> node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id) {
  Node* node = nullptr;
  protocol::Response response =
      AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;

  auto* html_input_element = DynamicTo<HTMLInputElement>(node);
  if (!html_input_element ||
      html_input_element->FormControlType() != FormControlType::kInputFile) {
    return protocol::Response::ServerError("Node is not a file input element");
  }

  Vector<String> paths;
  for (const String& file : *files)
    paths.push_back(file);
  To<HTMLInputElement>(node)->SetFilesFromPaths(paths);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::setNodeStackTracesEnabled(bool enable) {
  capture_node_stack_traces_.Set(enable);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getNodeStackTraces(
    int node_id,
    protocol::Maybe<v8_inspector::protocol::Runtime::API::StackTrace>*
        creation) {
  Node* node = nullptr;
  protocol::Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  auto it = node_to_creation_source_location_map_.find(node);
  if (it != node_to_creation_source_location_map_.end()) {
    SourceLocation& source_location = it->value->GetSourceLocation();
    *creation = source_location.BuildInspectorObject();
  }
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getBoxModel(
    Maybe<int> node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id,
    std::unique_ptr<protocol::DOM::BoxModel>* model) {
  Node* node = nullptr;
  protocol::Response response =
      AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;

  bool result = InspectorHighlight::GetBoxModel(node, model, true);
  if (!result)
    return protocol::Response::ServerError("Could not compute box model.");
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getContentQuads(
    Maybe<int> node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id,
    std::unique_ptr<protocol::Array<protocol::Array<double>>>* quads) {
  Node* node = nullptr;
  protocol::Response response =
      AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;
  bool result = InspectorHighlight::GetContentQuads(node, quads);
  if (!result)
    return protocol::Response::ServerError("Could not compute content quads.");
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getNodeForLocation(
    int x,
    int y,
    Maybe<bool> optional_include_user_agent_shadow_dom,
    Maybe<bool> optional_ignore_pointer_events_none,
    int* backend_node_id,
    String* frame_id,
    Maybe<int>* node_id) {
  bool include_user_agent_shadow_dom =
      optional_include_user_agent_shadow_dom.value_or(false);
  Document* document = inspected_frames_->Root()->GetDocument();
  PhysicalOffset document_point(
      LayoutUnit(x * inspected_frames_->Root()->LayoutZoomFactor()),
      LayoutUnit(y * inspected_frames_->Root()->LayoutZoomFactor()));
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kMove | HitTestRequest::kReadOnly |
      HitTestRequest::kAllowChildFrameContent;
  if (optional_ignore_pointer_events_none.value_or(false)) {
    hit_type |= HitTestRequest::kIgnorePointerEventsNone;
  }
  HitTestRequest request(hit_type);
  HitTestLocation location(document->View()->DocumentToFrame(document_point));
  HitTestResult result(request, location);
  document->GetFrame()->ContentLayoutObject()->HitTest(location, result);
  if (!include_user_agent_shadow_dom)
    result.SetToShadowHostIfInUAShadowRoot();
  Node* node = result.InnerPossiblyPseudoNode();
  while (node && node->getNodeType() == Node::kTextNode)
    node = node->parentNode();
  if (!node)
    return protocol::Response::ServerError("No node found at given location");
  *backend_node_id = IdentifiersFactory::IntIdForNode(node);
  LocalFrame* frame = node->GetDocument().GetFrame();
  *frame_id = IdentifiersFactory::FrameId(frame);
  if (enabled_.Get() && document_ &&
      document_node_to_id_map_->Contains(document_)) {
    *node_id = PushNodePathToFrontend(node);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::resolveNode(
    protocol::Maybe<int> node_id,
    protocol::Maybe<int> backend_node_id,
    protocol::Maybe<String> object_group,
    protocol::Maybe<int> execution_context_id,
    std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*
        result) {
  String object_group_name = object_group.value_or("");
  Node* node = nullptr;

  if (node_id.has_value() == backend_node_id.has_value()) {
    return protocol::Response::ServerError(
        "Either nodeId or backendNodeId must be specified.");
  }

  if (node_id.has_value()) {
    node = NodeForId(node_id.value());
  } else {
    node = DOMNodeIds::NodeForId(backend_node_id.value());
  }

  if (!node)
    return protocol::Response::ServerError("No node with given id found");
  *result = ResolveNode(v8_session_, node, object_group_name,
                        std::move(execution_context_id));
  if (!*result) {
    return protocol::Response::ServerError(
        "Node with given id does not belong to the document");
  }
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getAttributes(
    int node_id,
    std::unique_ptr<protocol::Array<String>>* result) {
  Element* element = nullptr;
  protocol::Response response = AssertElement(node_id, element);
  if (!response.IsSuccess())
    return response;

  *result = BuildArrayForElementAttributes(element);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::requestNode(const String& object_id,
                                                  int* node_id) {
  Node* node = nullptr;
  protocol::Response response = NodeForRemoteObjectId(object_id, node);
  if (!response.IsSuccess())
    return response;
  *node_id = PushNodePathToFrontend(node);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getContainerForNode(
    int node_id,
    protocol::Maybe<String> container_name,
    protocol::Maybe<protocol::DOM::PhysicalAxes> physical_axes,
    protocol::Maybe<protocol::DOM::LogicalAxes> logical_axes,
    Maybe<int>* container_node_id) {
  Element* element = nullptr;
  protocol::Response response = AssertElement(node_id, element);
  if (!response.IsSuccess())
    return response;

  PhysicalAxes physical = kPhysicalAxesNone;
  // TODO(crbug.com/1378237): Need to keep the broken behavior of querying the
  // inline-axis by default to avoid even worse behavior before devtools-
  // frontend catches up. Change value here to kLogicalAxesNone.
  LogicalAxes logical = kLogicalAxesInline;

  if (physical_axes.has_value()) {
    if (physical_axes.value() == protocol::DOM::PhysicalAxesEnum::Horizontal) {
      physical = kPhysicalAxesHorizontal;
    } else if (physical_axes.value() ==
               protocol::DOM::PhysicalAxesEnum::Vertical) {
      physical = kPhysicalAxesVertical;
    } else if (physical_axes.value() == protocol::DOM::PhysicalAxesEnum::Both) {
      physical = kPhysicalAxesBoth;
    }
  }
  if (logical_axes.has_value()) {
    if (logical_axes.value() == protocol::DOM::LogicalAxesEnum::Inline) {
      logical = kLogicalAxesInline;
    } else if (logical_axes.value() == protocol::DOM::LogicalAxesEnum::Block) {
      logical = kLogicalAxesBlock;
    } else if (logical_axes.value() == protocol::DOM::LogicalAxesEnum::Both) {
      logical = kLogicalAxesBoth;
    }
  }

  element->GetDocument().UpdateStyleAndLayoutTreeForElement(
      element, DocumentUpdateReason::kInspector);
  StyleResolver& style_resolver = element->GetDocument().GetStyleResolver();
  // Container rule origin no longer known at this point, match name from all
  // scopes.
  Element* container = style_resolver.FindContainerForElement(
      element,
      ContainerSelector(AtomicString(container_name.value_or(g_null_atom)),
                        physical, logical),
      nullptr /* selector_tree_scope */);
  if (container)
    *container_node_id = PushNodePathToFrontend(container);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getQueryingDescendantsForContainer(
    int node_id,
    std::unique_ptr<protocol::Array<int>>* node_ids) {
  Element* container = nullptr;
  protocol::Response response = AssertElement(node_id, container);
  if (!response.IsSuccess())
    return response;

  *node_ids = std::make_unique<protocol::Array<int>>();
  NodeToIdMap* nodes_map = document_node_to_id_map_.Get();
  for (Element* descendant : GetContainerQueryingDescendants(container)) {
    int id = PushNodePathToFrontend(descendant, nodes_map);
    (*node_ids)->push_back(id);
  }

  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getElementByRelation(
    int node_id,
    const String& relation,
    int* related_element_id) {
  *related_element_id = 0;
  Node* node = nullptr;
  protocol::Response response = AssertNode(node_id, node);
  if (!response.IsSuccess()) {
    return response;
  }

  Element* element = nullptr;
  if (relation == protocol::DOM::GetElementByRelation::RelationEnum::PopoverTarget) {
      if (auto* invoker = DynamicTo<HTMLFormControlElement>(node)) {
        element = invoker->popoverTargetElement().popover;
      }
  }

  if (element) {
    *related_element_id = PushNodePathToFrontend(element);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getAnchorElement(
    int node_id,
    protocol::Maybe<String> anchor_specifier,
    int* anchor_element_id) {
  *anchor_element_id = 0;
  Node* node = nullptr;
  protocol::Response response = AssertNode(node_id, node);
  if (!response.IsSuccess()) {
    return response;
  }

  const LayoutObject* querying_object = node->GetLayoutObject();
  if (!querying_object) {
    return protocol::Response::ServerError(
        "No layout object for node, perhaps orphan or hidden node");
  }

  const auto* box = DynamicTo<LayoutBox>(querying_object);
  if (!box || !box->Container()) {
    return protocol::Response::ServerError(
        "The box or the container of the box does not exist");
  }

  const LayoutObject* target_object;
  if (anchor_specifier.has_value()) {
    target_object = box->FindTargetAnchor(*MakeGarbageCollected<ScopedCSSName>(
        AtomicString(anchor_specifier.value()),
        &querying_object->GetDocument()));
  } else {
    const ComputedStyle& style = box->StyleRef();
    target_object = style.PositionAnchor()
                        ? box->FindTargetAnchor(*style.PositionAnchor())
                        : box->AcceptableImplicitAnchor();
  }

  if (target_object) {
    Element* element = DynamicTo<Element>(target_object->GetNode());
    if (element) {
      *anchor_element_id = PushNodePathToFrontend(element);
    }
  }
  return protocol::Response::Success();
}

// static
const HeapVector<Member<Element>>
InspectorDOMAgent::GetContainerQueryingDescendants(Element* container) {
  // This won't work for edge cases with display locking
  // (https://crbug.com/1235306).
  container->GetDocument().UpdateStyleAndLayoutTreeForSubtree(
      container, DocumentUpdateReason::kInspector);

  HeapVector<Member<Element>> querying_descendants;
  for (Element& element : ElementTraversal::DescendantsOf(*container)) {
    if (ContainerQueriedByElement(container, &element))
      querying_descendants.push_back(element);
  }

  return querying_descendants;
}

// static
bool InspectorDOMAgent::ContainerQueriedByElement(Element* container,
                                                  Element* element) {
  const ComputedStyle* style = element->GetComputedStyle();
  if (!style || !style->DependsOnContainerQueries()) {
    return false;
  }

  StyleResolver& style_resolver = element->GetDocument().GetStyleResolver();
  RuleIndexList* matched_rules =
      style_resolver.CssRulesForElement(element, StyleResolver::kAllCSSRules);
  if (!matched_rules) {
    return false;
  }
  for (auto it = matched_rules->rbegin(); it != matched_rules->rend(); ++it) {
    CSSRule* parent_rule = it->first;
    while (parent_rule) {
      auto* container_rule = DynamicTo<CSSContainerRule>(parent_rule);
      if (container_rule) {
        // Container rule origin no longer known at this point, match name from
        // all scopes.
        if (container == style_resolver.FindContainerForElement(
                             element, container_rule->Selector(),
                             nullptr /* selector_tree_scope */)) {
          return true;
        }
      }

      parent_rule = parent_rule->parentRule();
    }
  }

  return false;
}

// static
String InspectorDOMAgent::DocumentURLString(Document* document) {
  if (!document || document->Url().IsNull())
    return "";
  return document->Url().GetString();
}

// static
String InspectorDOMAgent::DocumentBaseURLString(Document* document) {
  return document->BaseURL().GetString();
}

// static
protocol::DOM::ShadowRootType InspectorDOMAgent::GetShadowRootType(
    ShadowRoot* shadow_root) {
  switch (shadow_root->GetMode()) {
    case ShadowRootMode::kUserAgent:
      return protocol::DOM::ShadowRootTypeEnum::UserAgent;
    case ShadowRootMode::kOpen:
      return protocol::DOM::ShadowRootTypeEnum::Open;
    case ShadowRootMode::kClosed:
      return protocol::DOM::ShadowRootTypeEnum::Closed;
  }
  NOTREACHED_IN_MIGRATION();
  return protocol::DOM::ShadowRootTypeEnum::UserAgent;
}

// static
protocol::DOM::CompatibilityMode
InspectorDOMAgent::GetDocumentCompatibilityMode(Document* document) {
  switch (document->GetCompatibilityMode()) {
    case Document::CompatibilityMode::kQuirksMode:
      return protocol::DOM::CompatibilityModeEnum::QuirksMode;
    case Document::CompatibilityMode::kLimitedQuirksMode:
      return protocol::DOM::CompatibilityModeEnum::LimitedQuirksMode;
    case Document::CompatibilityMode::kNoQuirksMode:
      return protocol::DOM::CompatibilityModeEnum::NoQuirksMode;
  }
  NOTREACHED_IN_MIGRATION();
  return protocol::DOM::CompatibilityModeEnum::NoQuirksMode;
}

std::unique_ptr<protocol::DOM::Node> InspectorDOMAgent::BuildObjectForNode(
    Node* node,
    int depth,
    bool pierce,
    NodeToIdMap* nodes_map,
    protocol::Array<protocol::DOM::Node>* flatten_result) {
  // If no `nodes_map` is provided, do the best effort to provide a node id,
  // but do not create one if it's not there, since absence of the map implies
  // we're not pushing the node to the front-end at the moment.
  const int id = nodes_map ? Bind(node, nodes_map) : BoundNodeId(node);
  String local_name;
  String node_value;

  switch (node->getNodeType()) {
    case Node::kTextNode:
    case Node::kCommentNode:
    case Node::kCdataSectionNode:
      node_value = node->nodeValue();
      if (node_value.length() > kMaxTextSize)
        node_value = node_value.Left(kMaxTextSize) + kEllipsisUChar;
      break;
    case Node::kAttributeNode:
      local_name = To<Attr>(node)->localName();
      break;
    case Node::kElementNode:
      local_name = To<Element>(node)->localName();
      break;
    default:
      break;
  }

  std::unique_ptr<protocol::DOM::Node> value =
      protocol::DOM::Node::create()
          .setNodeId(id)
          .setBackendNodeId(IdentifiersFactory::IntIdForNode(node))
          .setNodeType(static_cast<int>(node->getNodeType()))
          .setNodeName(node->nodeName())
          .setLocalName(local_name)
          .setNodeValue(node_value)
          .build();

  if (node->IsSVGElement())
    value->setIsSVG(true);

  bool force_push_children = false;
  if (auto* element = DynamicTo<Element>(node)) {
    value->setAttributes(BuildArrayForElementAttributes(element));

    if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
      if (frame_owner->ContentFrame()) {
        value->setFrameId(
            IdentifiersFactory::FrameId(frame_owner->ContentFrame()));
      }
      if (Document* doc = frame_owner->contentDocument()) {
        value->setContentDocument(BuildObjectForNode(
            doc, pierce ? depth : 0, pierce, nodes_map, flatten_result));
      }
    }

    if (node->parentNode() && node->parentNode()->IsDocumentNode()) {
      LocalFrame* frame = node->GetDocument().GetFrame();
      if (frame)
        value->setFrameId(IdentifiersFactory::FrameId(frame));
    }

    if (ShadowRoot* root = element->GetShadowRoot()) {
      auto shadow_roots =
          std::make_unique<protocol::Array<protocol::DOM::Node>>();
      shadow_roots->emplace_back(BuildObjectForNode(
          root, pierce ? depth : 0, pierce, nodes_map, flatten_result));
      value->setShadowRoots(std::move(shadow_roots));
      force_push_children = true;
    }

    if (IsA<HTMLLinkElement>(*element)) {
      force_push_children = true;
    }

    if (auto* template_element = DynamicTo<HTMLTemplateElement>(*element)) {
      if (DocumentFragment* content = template_element->content()) {
        value->setTemplateContent(
            BuildObjectForNode(content, 0, pierce, nodes_map, flatten_result));
        force_push_children = true;
      }
    }

    if (element->IsPseudoElement()) {
      value->setPseudoType(
          ProtocolPseudoElementType(element->GetPseudoIdForStyling()));
      if (auto tag = To<PseudoElement>(element)->view_transition_name())
        value->setPseudoIdentifier(tag);
    } else {
      if (!element->ownerDocument()->xmlVersion().empty())
        value->setXmlVersion(element->ownerDocument()->xmlVersion());
      if (auto* slot = element->AssignedSlotWithoutRecalc())
        value->setAssignedSlot(BuildBackendNode(slot));
    }
    std::unique_ptr<protocol::Array<protocol::DOM::Node>> pseudo_elements =
        BuildArrayForPseudoElements(element, nodes_map);
    if (pseudo_elements) {
      value->setPseudoElements(std::move(pseudo_elements));
      force_push_children = true;
    }

    if (auto* slot = DynamicTo<HTMLSlotElement>(*element)) {
      if (node->IsInShadowTree()) {
        value->setDistributedNodes(BuildDistributedNodesForSlot(slot));
        force_push_children = true;
      }
    }
  } else if (auto* document = DynamicTo<Document>(node)) {
    value->setDocumentURL(DocumentURLString(document));
    value->setBaseURL(DocumentBaseURLString(document));
    value->setXmlVersion(document->xmlVersion());
    value->setCompatibilityMode(GetDocumentCompatibilityMode(document));
  } else if (auto* doc_type = DynamicTo<DocumentType>(node)) {
    value->setPublicId(doc_type->publicId());
    value->setSystemId(doc_type->systemId());
  } else if (node->IsAttributeNode()) {
    auto* attribute = To<Attr>(node);
    value->setName(attribute->name());
    value->setValue(attribute->value());
  } else if (auto* shadow_root = DynamicTo<ShadowRoot>(node)) {
    value->setShadowRootType(GetShadowRootType(shadow_root));
  }

  if (node->IsContainerNode()) {
    int node_count = InnerChildNodeCount(node, IncludeWhitespace());
    value->setChildNodeCount(node_count);
    if (nodes_map == document_node_to_id_map_)
      cached_child_count_.Set(id, node_count);
    if (nodes_map && force_push_children && !depth)
      depth = 1;
    std::unique_ptr<protocol::Array<protocol::DOM::Node>> children =
        BuildArrayForContainerChildren(node, depth, pierce, nodes_map,
                                       flatten_result);
    if (!children->empty() ||
        depth)  // Push children along with shadow in any case.
      value->setChildren(std::move(children));
  }
  if (isNodeScrollable(node)) {
    value->setIsScrollable(true);
  }
  return value;
}

std::unique_ptr<protocol::Array<String>>
InspectorDOMAgent::BuildArrayForElementAttributes(Element* element) {
  auto attributes_value = std::make_unique<protocol::Array<String>>();
  // Go through all attributes and serialize them.
  for (const blink::Attribute& attribute : element->Attributes()) {
    // Add attribute pair
    attributes_value->emplace_back(attribute.GetName().ToString());
    attributes_value->emplace_back(attribute.Value());
  }
  return attributes_value;
}

std::unique_ptr<protocol::Array<protocol::DOM::Node>>
InspectorDOMAgent::BuildArrayForContainerChildren(
    Node* container,
    int depth,
    bool pierce,
    NodeToIdMap* nodes_map,
    protocol::Array<protocol::DOM::Node>* flatten_result) {
  auto children = std::make_unique<protocol::Array<protocol::DOM::Node>>();
  if (depth == 0) {
    if (!nodes_map)
      return children;
    // Special-case the only text child - pretend that container's children have
    // been requested.
    Node* first_child = container->firstChild();
    if (first_child && first_child->getNodeType() == Node::kTextNode &&
        !first_child->nextSibling()) {
      std::unique_ptr<protocol::DOM::Node> child_node =
          BuildObjectForNode(first_child, 0, pierce, nodes_map, flatten_result);
      child_node->setParentId(Bind(container, nodes_map));
      if (flatten_result) {
        flatten_result->emplace_back(std::move(child_node));
      } else {
        children->emplace_back(std::move(child_node));
      }
      children_requested_.insert(Bind(container, nodes_map));
    }
    return children;
  }

  InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace =
      IncludeWhitespace();
  Node* child = InnerFirstChild(container, include_whitespace);
  depth--;
  if (nodes_map)
    children_requested_.insert(Bind(container, nodes_map));

  while (child) {
    std::unique_ptr<protocol::DOM::Node> child_node =
        BuildObjectForNode(child, depth, pierce, nodes_map, flatten_result);
    child_node->setParentId(Bind(container, nodes_map));
    if (flatten_result) {
      flatten_result->emplace_back(std::move(child_node));
    } else {
      children->emplace_back(std::move(child_node));
    }
    if (nodes_map)
      children_requested_.insert(Bind(container, nodes_map));
    child = InnerNextSibling(child, include_whitespace);
  }
  return children;
}

std::unique_ptr<protocol::Array<protocol::DOM::Node>>
InspectorDOMAgent::BuildArrayForPseudoElements(Element* element,
                                               NodeToIdMap* nodes_map) {
  protocol::Array<protocol::DOM::Node> pseudo_elements;
  auto add_pseudo = [&](PseudoElement* pseudo_element) {
    pseudo_elements.emplace_back(
        BuildObjectForNode(pseudo_element, 0, false, nodes_map));
  };
  ForEachSupportedPseudo(element, add_pseudo);

  if (pseudo_elements.empty())
    return nullptr;
  return std::make_unique<protocol::Array<protocol::DOM::Node>>(
      std::move(pseudo_elements));
}

std::unique_ptr<protocol::DOM::BackendNode> InspectorDOMAgent::BuildBackendNode(
    Node* slot_element) {
  return protocol::DOM::BackendNode::create()
      .setNodeType(slot_element->getNodeType())
      .setNodeName(slot_element->nodeName())
      .setBackendNodeId(IdentifiersFactory::IntIdForNode(slot_element))
      .build();
}

std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
InspectorDOMAgent::BuildDistributedNodesForSlot(HTMLSlotElement* slot_element) {
  // TODO(hayato): In Shadow DOM v1, the concept of distributed nodes should
  // not be used anymore. DistributedNodes should be replaced with
  // AssignedNodes() when IncrementalShadowDOM becomes stable and Shadow DOM v0
  // is removed.
  auto distributed_nodes =
      std::make_unique<protocol::Array<protocol::DOM::BackendNode>>();
  for (auto& node : slot_element->AssignedNodes()) {
    if (ShouldSkipNode(node, IncludeWhitespace()))
      continue;
    distributed_nodes->emplace_back(BuildBackendNode(node));
  }
  return distributed_nodes;
}

// static
Node* InspectorDOMAgent::InnerFirstChild(
    Node* node,
    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace) {
  node = node->firstChild();
  while (ShouldSkipNode(node, include_whitespace))
    node = node->nextSibling();
  return node;
}

// static
Node* InspectorDOMAgent::InnerNextSibling(
    Node* node,
    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace) {
  do {
    node = node->nextSibling();
  } while (ShouldSkipNode(node, include_whitespace));
  return node;
}

// static
Node* InspectorDOMAgent::InnerPreviousSibling(
    Node* node,
    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace) {
  do {
    node = node->previousSibling();
  } while (ShouldSkipNode(node, include_whitespace));
  return node;
}

// static
unsigned InspectorDOMAgent::InnerChildNodeCount(
    Node* node,
    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace) {
  unsigned count = 0;
  Node* child = InnerFirstChild(node, include_whitespace);
  while (child) {
    count++;
    child = InnerNextSibling(child, include_whitespace);
  }
  return count;
}

// static
Node* InspectorDOMAgent::InnerParentNode(Node* node) {
  if (auto* document = DynamicTo<Document>(node)) {
    return document->LocalOwner();
  }
  return node->ParentOrShadowHostNode();
}

// static
bool InspectorDOMAgent::ShouldSkipNode(
    Node* node,
    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace) {
  if (include_whitespace == InspectorDOMAgent::IncludeWhitespaceEnum::ALL)
    return false;

  bool is_whitespace = node && node->getNodeType() == Node::kTextNode &&
                       node->nodeValue().LengthWithStrippedWhiteSpace() == 0;

  return is_whitespace;
}

// static
void InspectorDOMAgent::CollectNodes(
    Node* node,
    int depth,
    bool pierce,
    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace,
    base::RepeatingCallback<bool(Node*)> filter,
    HeapVector<Member<Node>>* result) {
  if (filter && filter.Run(node))
    result->push_back(node);
  if (--depth <= 0)
    return;

  auto* element = DynamicTo<Element>(node);
  if (pierce && element) {
    if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
      if (frame_owner->ContentFrame() &&
          frame_owner->ContentFrame()->IsLocalFrame()) {
        if (Document* doc = frame_owner->contentDocument())
          CollectNodes(doc, depth, pierce, include_whitespace, filter, result);
      }
    }

    ShadowRoot* root = element->GetShadowRoot();
    if (pierce && root)
      CollectNodes(root, depth, pierce, include_whitespace, filter, result);
  }

  for (Node* child = InnerFirstChild(node, include_whitespace); child;
       child = InnerNextSibling(child, include_whitespace)) {
    CollectNodes(child, depth, pierce, include_whitespace, filter, result);
  }
}

void InspectorDOMAgent::DomContentLoadedEventFired(LocalFrame* frame) {
  if (frame != inspected_frames_->Root())
    return;

  // Re-push document once it is loaded.
  DiscardFrontendBindings();
  if (enabled_.Get())
    GetFrontend()->documentUpdated();
}

void InspectorDOMAgent::InvalidateFrameOwnerElement(
    HTMLFrameOwnerElement* frame_owner) {
  if (!frame_owner)
    return;

  int frame_owner_id = BoundNodeId(frame_owner);
  if (!frame_owner_id)
    return;

  // Re-add frame owner element together with its new children.
  int parent_id = BoundNodeId(InnerParentNode(frame_owner));
  GetFrontend()->childNodeRemoved(parent_id, frame_owner_id);
  Unbind(frame_owner);

  std::unique_ptr<protocol::DOM::Node> value =
      BuildObjectForNode(frame_owner, 0, false, document_node_to_id_map_.Get());
  Node* previous_sibling =
      InnerPreviousSibling(frame_owner, IncludeWhitespace());
  int prev_id = previous_sibling ? BoundNodeId(previous_sibling) : 0;
  GetFrontend()->childNodeInserted(parent_id, prev_id, std::move(value));
}

void InspectorDOMAgent::DidCommitLoad(LocalFrame*, DocumentLoader* loader) {
  Document* document = loader->GetFrame()->GetDocument();
  NotifyDidAddDocument(document);

  LocalFrame* inspected_frame = inspected_frames_->Root();
  if (loader->GetFrame() != inspected_frame) {
    InvalidateFrameOwnerElement(
        loader->GetFrame()->GetDocument()->LocalOwner());
    return;
  }

  SetDocument(inspected_frame->GetDocument());
}

void InspectorDOMAgent::DidRestoreFromBackForwardCache(LocalFrame* frame) {
  if (!enabled_.Get())
    return;
  DCHECK_EQ(frame, inspected_frames_->Root());
  Document* document = frame->GetDocument();
  DCHECK_EQ(document_, document);
  // We don't load a new document for BFCache navigations, so |document_|
  // doesn't actually update (the agent is initialized with the restored main
  // document), but the frontend doesn't know this yet, and we need to notify
  // it.
  GetFrontend()->documentUpdated();
}

void InspectorDOMAgent::DidInsertDOMNode(Node* node) {
  InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace =
      IncludeWhitespace();
  if (ShouldSkipNode(node, include_whitespace))
    return;

  // We could be attaching existing subtree. Forget the bindings.
  Unbind(node);

  ContainerNode* parent = node->parentNode();
  if (!parent)
    return;
  // Return if parent is not mapped yet.
  int parent_id = BoundNodeId(parent);
  if (!parent_id)
    return;

  if (!children_requested_.Contains(parent_id)) {
    // No children are mapped yet -> only notify on changes of child count.
    auto it = cached_child_count_.find(parent_id);
    int count = (it != cached_child_count_.end() ? it->value : 0) + 1;
    cached_child_count_.Set(parent_id, count);
    GetFrontend()->childNodeCountUpdated(parent_id, count);
  } else {
    // Children have been requested -> return value of a new child.
    Node* prev_sibling = InnerPreviousSibling(node, include_whitespace);
    int prev_id = prev_sibling ? BoundNodeId(prev_sibling) : 0;
    std::unique_ptr<protocol::DOM::Node> value =
        BuildObjectForNode(node, 0, false, document_node_to_id_map_.Get());
    GetFrontend()->childNodeInserted(parent_id, prev_id, std::move(value));
  }
}

void InspectorDOMAgent::WillRemoveDOMNode(Node* node) {
  if (ShouldSkipNode(node, IncludeWhitespace()))
    return;
  DOMNodeRemoved(node);
}

void InspectorDOMAgent::DOMNodeRemoved(Node* node) {
  ContainerNode* parent = node->parentNode();

  // If parent is not mapped yet -> ignore the event.
  int parent_id = BoundNodeId(parent);
  if (!parent_id)
    return;

  if (!children_requested_.Contains(parent_id)) {
    // No children are mapped yet -> only notify on changes of child count.
    int count = cached_child_count_.at(parent_id) - 1;
    cached_child_count_.Set(parent_id, count);
    GetFrontend()->childNodeCountUpdated(parent_id, count);
  } else {
    GetFrontend()->childNodeRemoved(parent_id, BoundNodeId(node));
  }
  Unbind(node);
}

void InspectorDOMAgent::WillModifyDOMAttr(Element*,
                                          const AtomicString& old_value,
                                          const AtomicString& new_value) {
  suppress_attribute_modified_event_ = (old_value == new_value);
}

void InspectorDOMAgent::DidModifyDOMAttr(Element* element,
                                         const QualifiedName& name,
                                         const AtomicString& value) {
  bool should_suppress_event = suppress_attribute_modified_event_;
  suppress_attribute_modified_event_ = false;
  if (should_suppress_event)
    return;

  int id = BoundNodeId(element);
  // If node is not mapped yet -> ignore the event.
  if (!id)
    return;

  NotifyDidModifyDOMAttr(element);

  GetFrontend()->attributeModified(id, name.ToString(), value);
}

void InspectorDOMAgent::DidRemoveDOMAttr(Element* element,
                                         const QualifiedName& name) {
  int id = BoundNodeId(element);
  // If node is not mapped yet -> ignore the event.
  if (!id)
    return;

  NotifyDidModifyDOMAttr(element);

  GetFrontend()->attributeRemoved(id, name.ToString());
}

void InspectorDOMAgent::StyleAttributeInvalidated(
    const HeapVector<Member<Element>>& elements) {
  auto node_ids = std::make_unique<protocol::Array<int>>();
  for (unsigned i = 0, size = elements.size(); i < size; ++i) {
    Element* element = elements.at(i);
    int id = BoundNodeId(element);
    // If node is not mapped yet -> ignore the event.
    if (!id)
      continue;

    NotifyDidModifyDOMAttr(element);
    node_ids->emplace_back(id);
  }
  GetFrontend()->inlineStyleInvalidated(std::move(node_ids));
}

void InspectorDOMAgent::CharacterDataModified(CharacterData* character_data) {
  int id = BoundNodeId(character_data);
  if (id && ShouldSkipNode(character_data, IncludeWhitespace())) {
    DOMNodeRemoved(character_data);
    return;
  }
  if (!id) {
    // Push text node if it is being created.
    DidInsertDOMNode(character_data);
    return;
  }
  GetFrontend()->characterDataModified(id, character_data->data());
}

InspectorRevalidateDOMTask* InspectorDOMAgent::RevalidateTask() {
  if (!revalidate_task_)
    revalidate_task_ = MakeGarbageCollected<InspectorRevalidateDOMTask>(this);
  return revalidate_task_.Get();
}

void InspectorDOMAgent::DidInvalidateStyleAttr(Node* node) {
  // If node is not mapped yet -> ignore the event.
  if (!BoundNodeId(node))
    return;
  RevalidateTask()->ScheduleStyleAttrRevalidationFor(To<Element>(node));
}

bool InspectorDOMAgent::isNodeScrollable(Node* node) {
  if (auto* box = DynamicTo<LayoutBox>(node->GetLayoutObject())) {
    return box->IsUserScrollable();
  }
  return false;
}

void InspectorDOMAgent::DidPushShadowRoot(Element* host, ShadowRoot* root) {
  if (!host->ownerDocument())
    return;

  int host_id = BoundNodeId(host);
  if (!host_id)
    return;

  PushChildNodesToFrontend(host_id, 1);
  GetFrontend()->shadowRootPushed(
      host_id,
      BuildObjectForNode(root, 0, false, document_node_to_id_map_.Get()));
}

void InspectorDOMAgent::WillPopShadowRoot(Element* host, ShadowRoot* root) {
  if (!host->ownerDocument())
    return;

  int host_id = BoundNodeId(host);
  int root_id = BoundNodeId(root);
  if (host_id && root_id)
    GetFrontend()->shadowRootPopped(host_id, root_id);
}

void InspectorDOMAgent::DidPerformSlotDistribution(
    HTMLSlotElement* slot_element) {
  int insertion_point_id = BoundNodeId(slot_element);
  if (insertion_point_id)
    GetFrontend()->distributedNodesUpdated(
        insertion_point_id, BuildDistributedNodesForSlot(slot_element));
}

void InspectorDOMAgent::FrameDocumentUpdated(LocalFrame* frame) {
  Document* document = frame->GetDocument();
  if (!document)
    return;

  if (frame != inspected_frames_->Root())
    return;

  // Only update the main frame document, nested frame document updates are not
  // required (will be handled by invalidateFrameOwnerElement()).
  SetDocument(document);
}

void InspectorDOMAgent::FrameOwnerContentUpdated(
    LocalFrame* frame,
    HTMLFrameOwnerElement* frame_owner) {
  if (!frame_owner->contentDocument()) {
    // frame_owner does not point to frame at this point, so Unbind it
    // explicitly.
    Unbind(frame->GetDocument());
  }

  // Revalidating owner can serialize empty frame owner - that's what we are
  // looking for when disconnecting.
  InvalidateFrameOwnerElement(frame_owner);
}

void InspectorDOMAgent::PseudoElementCreated(PseudoElement* pseudo_element) {
  Element* parent = pseudo_element->ParentOrShadowHostElement();
  if (!parent)
    return;
  if (!PseudoElement::IsWebExposed(pseudo_element->GetPseudoIdForStyling(),
                                   parent)) {
    return;
  }
  int parent_id = BoundNodeId(parent);
  if (!parent_id)
    return;

  PushChildNodesToFrontend(parent_id, 1);
  GetFrontend()->pseudoElementAdded(
      parent_id, BuildObjectForNode(pseudo_element, 0, false,
                                    document_node_to_id_map_.Get()));
}

void InspectorDOMAgent::TopLayerElementsChanged() {
  GetFrontend()->topLayerElementsUpdated();
}

void InspectorDOMAgent::PseudoElementDestroyed(PseudoElement* pseudo_element) {
  int pseudo_element_id = BoundNodeId(pseudo_element);
  if (!pseudo_element_id)
    return;

  // If a PseudoElement is bound, its parent element must be bound, too.
  Element* parent = pseudo_element->ParentOrShadowHostElement();
  DCHECK(parent);
  int parent_id = BoundNodeId(parent);
  // Since the pseudo element tree created for a view transition is destroyed
  // with in-order traversal, the parent node (::view-transition) are destroyed
  // before its children
  // (::view-transition-group).
  DCHECK(parent_id || IsTransitionPseudoElement(pseudo_element->GetPseudoId()));

  Unbind(pseudo_element);
  GetFrontend()->pseudoElementRemoved(parent_id, pseudo_element_id);
}

void InspectorDOMAgent::NodeCreated(Node* node) {
  if (!capture_node_stack_traces_.Get())
    return;

  std::unique_ptr<SourceLocation> creation_source_location =
      SourceLocation::CaptureWithFullStackTrace();
  if (creation_source_location) {
    node_to_creation_source_location_map_.Set(
        node, MakeGarbageCollected<InspectorSourceLocation>(
                  std::move(creation_source_location)));
  }
}

void InspectorDOMAgent::UpdateScrollableFlag(Node* node) {
  if (!node) {
    return;
  }
  int nodeId = BoundNodeId(node);
  // If node is not mapped yet -> ignore the event.
  if (!nodeId) {
    return;
  }
  GetFrontend()->scrollableFlagUpdated(nodeId, isNodeScrollable(node));
}

namespace {

ShadowRoot* ShadowRootForNode(Node* node, const String& type) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return nullptr;
  if (type == "a")
    return element->AuthorShadowRoot();
  if (type == "u")
    return element->UserAgentShadowRoot();
  return nullptr;
}

Document* DocumentForFrameOwner(Node* node) {
  if (auto* owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
    return owner->contentDocument();
  }
  return nullptr;
}

}  // namespace

Node* InspectorDOMAgent::NodeForPath(const String& path) {
  // The path is of form "1,HTML,2,BODY,1,DIV" (<index> and <nodeName>
  // interleaved).  <index> may also be "a" (author shadow root) or "u"
  // (user-agent shadow root), in which case <nodeName> MUST be
  // "#document-fragment".
  // The first component after an iframe will always be "d,#document".
  if (!document_)
    return nullptr;

  Node* node = document_.Get();
  Vector<String> path_tokens;
  path.Split(',', path_tokens);
  if (!path_tokens.size())
    return nullptr;

  InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace =
      IncludeWhitespace();
  for (wtf_size_t i = 0; i < path_tokens.size() - 1; i += 2) {
    bool success = true;
    String& index_value = path_tokens[i];
    wtf_size_t child_number = index_value.ToUInt(&success);
    Node* child;
    String child_name = path_tokens[i + 1];
    if (!success) {
      if (index_value == "d") {
        child = DocumentForFrameOwner(node);
      } else {
        child = ShadowRootForNode(node, index_value);
      }
    } else {
      if (child_number >= InnerChildNodeCount(node, include_whitespace))
        return nullptr;

      child = InnerFirstChild(node, include_whitespace);
    }
    for (wtf_size_t j = 0; child && j < child_number; ++j)
      child = InnerNextSibling(child, include_whitespace);

    if (!child || child->nodeName() != child_name)
      return nullptr;
    node = child;
  }
  return node;
}

protocol::Response InspectorDOMAgent::pushNodeByPathToFrontend(
    const String& path,
    int* node_id) {
  if (!enabled_.Get())
    return protocol::Response::ServerError("DOM agent is not enabled");
  if (Node* node = NodeForPath(path))
    *node_id = PushNodePathToFrontend(node);
  else
    return protocol::Response::ServerError("No node with given path found");
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::pushNodesByBackendIdsToFrontend(
    std::unique_ptr<protocol::Array<int>> backend_node_ids,
    std::unique_ptr<protocol::Array<int>>* result) {
  if (!document_ || !BoundNodeId(document_)) {
    return protocol::Response::ServerError(
        "Document needs to be requested first");
  }

  *result = std::make_unique<protocol::Array<int>>();
  for (int id : *backend_node_ids) {
    Node* node = DOMNodeIds::NodeForId(id);
    if (node && node->GetDocument().GetFrame() &&
        inspected_frames_->Contains(node->GetDocument().GetFrame()))
      (*result)->emplace_back(PushNodePathToFrontend(node));
    else
      (*result)->emplace_back(0);
  }
  return protocol::Response::Success();
}

class InspectableNode final
    : public v8_inspector::V8InspectorSession::Inspectable {
 public:
  explicit InspectableNode(Node* node) : node_id_(node->GetDomNodeId()) {}

  v8::Local<v8::Value> get(v8::Local<v8::Context> context) override {
    return NodeV8Value(context, DOMNodeIds::NodeForId(node_id_));
  }

 private:
  DOMNodeId node_id_;
};

protocol::Response InspectorDOMAgent::setInspectedNode(int node_id) {
  Node* node = nullptr;
  protocol::Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;
  v8_session_->addInspectedObject(std::make_unique<InspectableNode>(node));
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getRelayoutBoundary(
    int node_id,
    int* relayout_boundary_node_id) {
  Node* node = nullptr;
  protocol::Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object) {
    return protocol::Response::ServerError(
        "No layout object for node, perhaps orphan or hidden node");
  }
  while (layout_object && !layout_object->IsDocumentElement() &&
         !layout_object->IsRelayoutBoundary())
    layout_object = layout_object->Container();
  Node* result_node =
      layout_object ? layout_object->GeneratingNode() : node->ownerDocument();
  *relayout_boundary_node_id = PushNodePathToFrontend(result_node);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::describeNode(
    protocol::Maybe<int> node_id,
    protocol::Maybe<int> backend_node_id,
    protocol::Maybe<String> object_id,
    protocol::Maybe<int> depth,
    protocol::Maybe<bool> pierce,
    std::unique_ptr<protocol::DOM::Node>* result) {
  Node* node = nullptr;
  protocol::Response response =
      AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;
  if (!node)
    return protocol::Response::ServerError("Node not found");
  *result = BuildObjectForNode(node, depth.value_or(0), pierce.value_or(false),
                               nullptr, nullptr);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::scrollIntoViewIfNeeded(
    protocol::Maybe<int> node_id,
    protocol::Maybe<int> backend_node_id,
    protocol::Maybe<String> object_id,
    protocol::Maybe<protocol::DOM::Rect> rect) {
  Node* node = nullptr;
  protocol::Response response =
      AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;
  node->GetDocument().EnsurePaintLocationDataValidForNode(
      node, DocumentUpdateReason::kInspector);
  if (!node->isConnected())
    return protocol::Response::ServerError("Node is detached from document");
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object) {
    node = LayoutTreeBuilderTraversal::FirstLayoutChild(*node);
    if (node)
      layout_object = node->GetLayoutObject();
  }
  if (!layout_object) {
    return protocol::Response::ServerError(
        "Node does not have a layout object");
  }
  PhysicalRect rect_to_scroll =
      PhysicalRect::EnclosingRect(layout_object->AbsoluteBoundingBoxRectF());
  if (rect.has_value()) {
    rect_to_scroll.SetX(rect_to_scroll.X() + LayoutUnit(rect.value().getX()));
    rect_to_scroll.SetY(rect_to_scroll.Y() + LayoutUnit(rect.value().getY()));
    rect_to_scroll.SetWidth(LayoutUnit(rect.value().getWidth()));
    rect_to_scroll.SetHeight(LayoutUnit(rect.value().getHeight()));
  }
  scroll_into_view_util::ScrollRectToVisible(
      *layout_object, rect_to_scroll,
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::CenterIfNeeded(), ScrollAlignment::CenterIfNeeded(),
          mojom::blink::ScrollType::kProgrammatic,
          true /* make_visible_in_visual_viewport */,
          mojom::blink::ScrollBehavior::kInstant,
          true /* is_for_scroll_sequence */));
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getFrameOwner(
    const String& frame_id,
    int* backend_node_id,
    protocol::Maybe<int>* node_id) {
  Frame* found_frame = nullptr;
  for (Frame* frame = inspected_frames_->Root(); frame;
       frame = frame->Tree().TraverseNext(inspected_frames_->Root())) {
    if (IdentifiersFactory::FrameId(frame) == frame_id) {
      found_frame = frame;
      break;
    }

    if (IsA<LocalFrame>(frame)) {
      if (auto* fenced_frames = DocumentFencedFrames::Get(
              *To<LocalFrame>(frame)->GetDocument())) {
        for (HTMLFencedFrameElement* ff : fenced_frames->GetFencedFrames()) {
          Frame* ff_frame = ff->ContentFrame();
          if (ff_frame && IdentifiersFactory::FrameId(ff_frame) == frame_id) {
            found_frame = ff_frame;
            break;
          }
        }
      }
    }
  }

  if (!found_frame) {
    return protocol::Response::ServerError(
        "Frame with the given id was not found.");
  }
  auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(found_frame->Owner());
  if (!frame_owner) {
    return protocol::Response::ServerError(
        "Frame with the given id does not belong to the target.");
  }

  *backend_node_id = IdentifiersFactory::IntIdForNode(frame_owner);

  if (enabled_.Get() && document_ && BoundNodeId(document_)) {
    *node_id = PushNodePathToFrontend(frame_owner);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getFileInfo(const String& object_id,
                                                  String* path) {
  v8::HandleScope handles(isolate_);
  v8::Local<v8::Value> value;
  v8::Local<v8::Context> context;
  std::unique_ptr<v8_inspector::StringBuffer> error;
  if (!v8_session_->unwrapObject(&error, ToV8InspectorStringView(object_id),
                                 &value, &context, nullptr)) {
    return protocol::Response::ServerError(
        ToCoreString(std::move(error)).Utf8());
  }

  File* file = V8File::ToWrappable(isolate_, value);
  if (!file) {
    return protocol::Response::ServerError(
        "Object id doesn't reference a File");
  }

  *path = file->GetPath();
  return protocol::Response::Success();
}

protocol::Response InspectorDOMAgent::getDetachedDomNodes(
    std::unique_ptr<protocol::Array<protocol::DOM::DetachedElementInfo>>*
        detached_nodes) {
  *detached_nodes =
      std::make_unique<protocol::Array<protocol::DOM::DetachedElementInfo>>();
  v8::HandleScope handles(isolate_);
  std::map<DOMNodeId, size_t> seen_ids;

  for (v8::Local<v8::Value> data :
       isolate_->GetHeapProfiler()->GetDetachedJSWrapperObjects()) {
    Node* node = V8Node::ToWrappable(isolate_, data);
    if (!node) {
      continue;
    }

    // It's possible to obtain nodes that come from a different document / page
    // / frame. We want to ensure that the nodes we get are not from an
    // inspected frame. This works around a crash in the front end when nodes
    // are created in the inspector overlay.
    Document& document = node->GetDocument();
    if (!document.GetFrame() ||
        !inspected_frames_->Contains(document.GetFrame())) {
      continue;
    }

    Node* parent = node;
    // Obtain Top Most Node
    while (parent->parentNode()) {
      parent = parent->parentNode();
    }

    // It is possible to get multiple child nodes from V8 that are in the same
    // detached tree. In this case, we can see the top level node multiple
    // times. We don't want to return the same tree more than once, so we record
    // the ID and skip to avoid duplicate returns. We do want to return the ID
    // of the retained object `node`.
    blink::DOMNodeId parent_id = parent->GetDomNodeId();
    if (seen_ids.contains(parent_id)) {
      size_t parent_index = seen_ids[parent_id];
      (**detached_nodes)[parent_index]->getRetainedNodeIds()->emplace_back(
          node->GetDomNodeId());
      continue;
    }
    // Remember where the top-level node resides in the detached_nodes array
    seen_ids[parent_id] = (*detached_nodes)->size();

    auto children = std::make_unique<protocol::Array<blink::DOMNodeId>>();
    children->emplace_back(node->GetDomNodeId());
    std::unique_ptr<protocol::DOM::DetachedElementInfo> value =
        protocol::DOM::DetachedElementInfo::create()
            .setTreeNode(BuildObjectForNode(
                parent, -1, true, document_node_to_id_map_.Get(), nullptr))
            .setRetainedNodeIds(std::move(children))
            .build();

    (*detached_nodes)->emplace_back(std::move(value));
  }
  return protocol::Response::Success();
}

void InspectorDOMAgent::Trace(Visitor* visitor) const {
  visitor->Trace(dom_listeners_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(document_node_to_id_map_);
  visitor->Trace(dangling_node_to_id_maps_);
  visitor->Trace(id_to_node_);
  visitor->Trace(id_to_nodes_map_);
  visitor->Trace(document_);
  visitor->Trace(revalidate_task_);
  visitor->Trace(search_results_);
  visitor->Trace(history_);
  visitor->Trace(dom_editor_);
  visitor->Trace(node_to_creation_source_location_map_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
