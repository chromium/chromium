/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008, 2009, 2010, 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/serializers/serialization.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/cdata_section.h"
#include "third_party/blink/renderer/core/dom/child_list_mutation_scope.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/editing_strategy.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/serializers/markup_accumulator.h"
#include "third_party/blink/renderer/core/editing/serializers/styled_markup_serializer.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_quote_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser_fastpath.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class AttributeChange {
  DISALLOW_NEW();

 public:
  AttributeChange() : name_(QualifiedName::Null()) {}

  AttributeChange(Element* element,
                  const QualifiedName& name,
                  const String& value)
      : element_(element), name_(name), value_(value) {}

  void Apply() { element_->setAttribute(name_, AtomicString(value_)); }

  void Trace(Visitor* visitor) const { visitor->Trace(element_); }

 private:
  Member<Element> element_;
  QualifiedName name_;
  String value_;
};

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::AttributeChange)

namespace blink {

namespace {

class EmptyLocalFrameClientWithFailingLoaderFactory final
    : public EmptyLocalFrameClient {
 public:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    // TODO(crbug.com/1413912): CreateSanitizedFragmentFromMarkupWithContext may
    // call this method for data: URL resources. But ResourceLoader::Start()
    // don't need to call GetURLLoaderFactory() for data: URL because
    // ResourceLoader handles the data: URL resource load without the returned
    // SharedURLLoaderFactory.
    // Note: Non-data: URL resource can't be loaded because the CORS check in
    // BaseFetchContext::CanRequestInternal fails for non-data: URL resources.
    return base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
        WTF::BindOnce(
            [](const network::ResourceRequest& resource_request,
               mojo::PendingReceiver<network::mojom::URLLoader> receiver,
               mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
              NOTREACHED_IN_MIGRATION();
            }));
  }
};

void LogFastPathParserTotalTime(base::TimeDelta parse_time) {
  // The time needed to parse is typically < 1ms (even at the 99%).
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Blink.HTMLFastPathParser.TotalParseTime2", parse_time,
      base::Microseconds(1), base::Milliseconds(10), 100);
}

}  // namespace

static void CompleteURLs(DocumentFragment& fragment, const String& base_url) {
  HeapVector<AttributeChange> changes;

  KURL parsed_base_url(base_url);

  for (Element& element : ElementTraversal::DescendantsOf(fragment)) {
    AttributeCollection attributes = element.Attributes();
    // AttributeCollection::iterator end = attributes.end();
    for (const auto& attribute : attributes) {
      if (element.IsURLAttribute(attribute) && !attribute.Value().empty())
        changes.push_back(AttributeChange(
            &element, attribute.GetName(),
            KURL(parsed_base_url, attribute.Value()).GetString()));
    }
  }

  for (auto& change : changes)
    change.Apply();
}

static bool IsHTMLBlockElement(const Node* node) {
  DCHECK(node);
  return IsA<HTMLTableCellElement>(*node) ||
         IsNonTableCellHTMLBlockElement(node);
}

static HTMLElement* AncestorToRetainStructureAndAppearanceForBlock(
    Element* common_ancestor_block) {
  if (!common_ancestor_block)
    return nullptr;

  if (common_ancestor_block->HasTagName(html_names::kTbodyTag) ||
      IsA<HTMLTableRowElement>(*common_ancestor_block))
    return Traversal<HTMLTableElement>::FirstAncestor(*common_ancestor_block);

  if (IsNonTableCellHTMLBlockElement(common_ancestor_block))
    return To<HTMLElement>(common_ancestor_block);

  return nullptr;
}

static inline HTMLElement* AncestorToRetainStructureAndAppearance(
    Node* common_ancestor) {
  return AncestorToRetainStructureAndAppearanceForBlock(
      EnclosingBlock(common_ancestor));
}

static inline HTMLElement*
AncestorToRetainStructureAndAppearanceWithNoLayoutObject(
    const Node& common_ancestor) {
  auto* common_ancestor_block = To<HTMLElement>(EnclosingNodeOfType(
      FirstPositionInOrBeforeNode(common_ancestor), IsHTMLBlockElement));
  return AncestorToRetainStructureAndAppearanceForBlock(common_ancestor_block);
}

bool PropertyMissingOrEqualToNone(CSSPropertyValueSet* style,
                                  CSSPropertyID property_id) {
  if (!style)
    return false;
  const CSSValue* value = style->GetPropertyCSSValue(property_id);
  if (!value)
    return true;
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return false;
  return identifier_value->GetValueID() == CSSValueID::kNone;
}

template <typename Strategy>
static HTMLElement* HighestAncestorToWrapMarkup(
    const PositionTemplate<Strategy>& start_position,
    const PositionTemplate<Strategy>& end_position,
    const CreateMarkupOptions& options) {
  Node* first_node = start_position.NodeAsRangeFirstNode();
  // For compatibility reason, we use container node of start and end
  // positions rather than first node and last node in selection.
  Node* common_ancestor =
      Strategy::CommonAncestor(*start_position.ComputeContainerNode(),
                               *end_position.ComputeContainerNode());
  DCHECK(common_ancestor);
  HTMLElement* special_common_ancestor = nullptr;
  if (options.ShouldAnnotateForInterchange()) {
    // Include ancestors that aren't completely inside the range but are
    // required to retain the structure and appearance of the copied markup.
    special_common_ancestor =
        AncestorToRetainStructureAndAppearance(common_ancestor);
    if (first_node) {
      const Position& first_node_position =
          FirstPositionInOrBeforeNode(*first_node);
      if (Node* parent_list_node =
              EnclosingNodeOfType(first_node_position, IsListItem)) {
        if (AreSameRanges(parent_list_node, start_position, end_position)) {
          ContainerNode* ancestor = parent_list_node->parentNode();
          while (ancestor && !IsHTMLListElement(ancestor))
            ancestor = ancestor->parentNode();
          special_common_ancestor = To<HTMLElement>(ancestor);
        }
      }

      // Retain the Mail quote level by including all ancestor mail block
      // quotes.
      if (auto* highest_mail_blockquote =
              To<HTMLQuoteElement>(HighestEnclosingNodeOfType(
                  first_node_position, IsMailHTMLBlockquoteElement,
                  kCanCrossEditingBoundary))) {
        special_common_ancestor = highest_mail_blockquote;
      }
    }
  }

  Node* check_ancestor =
      special_common_ancestor ? special_common_ancestor : common_ancestor;
  if (check_ancestor->GetLayoutObject()) {
    // We want to constrain the ancestor to the enclosing block.
    // Ex: <b><p></p></b> is an ill-formed html and we don't want to return <b>
    // as the ancestor because paragraph element is the enclosing block of the
    // start and end positions provided to this API.
    // TODO(editing-dev): Make |HighestEnclosingNodeOfType| take const pointer
    // to remove the |const_cast| below.
    Node* constraining_ancestor =
        options.ConstrainingAncestor()
            ? const_cast<Node*>(options.ConstrainingAncestor())
            : EnclosingBlock(check_ancestor);
    auto* new_special_common_ancestor =
        To<HTMLElement>(HighestEnclosingNodeOfType(
            Position::FirstPositionInNode(*check_ancestor),
            &IsPresentationalHTMLElement, kCanCrossEditingBoundary,
            constraining_ancestor));
    if (new_special_common_ancestor)
      special_common_ancestor = new_special_common_ancestor;
  }

  // If a single tab is selected, commonAncestor will be a text node inside a
  // tab span. If two or more tabs are selected, commonAncestor will be the tab
  // span. In either case, if there is a specialCommonAncestor already, it will
  // necessarily be above any tab span that needs to be included.
  if (!special_common_ancestor &&
      IsTabHTMLSpanElementTextNode(common_ancestor)) {
    special_common_ancestor =
        To<HTMLSpanElement>(Strategy::Parent(*common_ancestor));
  }
  if (!special_common_ancestor && IsTabHTMLSpanElement(common_ancestor))
    special_common_ancestor = To<HTMLSpanElement>(common_ancestor);

  if (auto* enclosing_anchor = To<HTMLAnchorElement>(EnclosingElementWithTag(
          Position::FirstPositionInNode(special_common_ancestor
                                            ? *special_common_ancestor
                                            : *common_ancestor),
          html_names::kATag)))
    special_common_ancestor = enclosing_anchor;

  return special_common_ancestor;
}

template <typename Strategy>
class CreateMarkupAlgorithm {
 public:
  static String CreateMarkup(
      const PositionTemplate<Strategy>& start_position,
      const PositionTemplate<Strategy>& end_position,
      const CreateMarkupOptions& options = CreateMarkupOptions());
};

// FIXME: Shouldn't we omit style info when annotate ==
// DoNotAnnotateForInterchange?
// FIXME: At least, annotation and style info should probably not be included in
// range.markupString()
template <typename Strategy>
String CreateMarkupAlgorithm<Strategy>::CreateMarkup(
    const PositionTemplate<Strategy>& start_position,
    const PositionTemplate<Strategy>& end_position,
    const CreateMarkupOptions& options) {
  if (start_position.IsNull() || end_position.IsNull())
    return g_empty_string;

  CHECK_LE(start_position.CompareTo(end_position), 0);

  bool collapsed = start_position == end_position;
  if (collapsed)
    return g_empty_string;
  Node* common_ancestor =
      Strategy::CommonAncestor(*start_position.ComputeContainerNode(),
                               *end_position.ComputeContainerNode());
  if (!common_ancestor)
    return g_empty_string;

  Document* document = start_position.GetDocument();

  DCHECK(!document->NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document->Lifecycle());

  HTMLElement* special_common_ancestor = HighestAncestorToWrapMarkup<Strategy>(
      start_position, end_position, options);
  StyledMarkupSerializer<Strategy> serializer(start_position, end_position,
                                              special_common_ancestor, options);
  return serializer.CreateMarkup();
}

String CreateMarkup(const Position& start_position,
                    const Position& end_position,
                    const CreateMarkupOptions& options) {
  return CreateMarkupAlgorithm<EditingStrategy>::CreateMarkup(
      start_position, end_position, options);
}

String CreateMarkup(const PositionInFlatTree& start_position,
                    const PositionInFlatTree& end_position,
                    const CreateMarkupOptions& options) {
  return CreateMarkupAlgorithm<EditingInFlatTreeStrategy>::CreateMarkup(
      start_position, end_position, options);
}

DocumentFragment* CreateFragmentFromMarkup(
    Document& document,
    const String& markup,
    const String& base_url,
    ParserContentPolicy parser_content_policy) {
  // We use a fake body element here to trick the HTML parser to using the
  // InBody insertion mode.
  auto* fake_body = MakeGarbageCollected<HTMLBodyElement>(document);
  DocumentFragment* fragment = DocumentFragment::Create(document);

  fragment->ParseHTML(markup, fake_body, parser_content_policy);

  if (!base_url.empty() && base_url != BlankURL() &&
      base_url != document.BaseURL())
    CompleteURLs(*fragment, base_url);

  return fragment;
}

static const char kFragmentMarkerTag[] = "webkit-fragment-marker";

static bool FindNodesSurroundingContext(DocumentFragment* fragment,
                                        Comment*& node_before_context,
                                        Comment*& node_after_context) {
  if (!fragment->firstChild())
    return false;
  for (Node& node : NodeTraversal::StartsAt(*fragment->firstChild())) {
    auto* comment_node = DynamicTo<Comment>(node);
    if (comment_node && comment_node->data() == kFragmentMarkerTag) {
      if (!node_before_context) {
        node_before_context = comment_node;
      } else {
        node_after_context = comment_node;
        return true;
      }
    }
  }
  return false;
}

static void TrimFragment(DocumentFragment* fragment,
                         Comment* node_before_context,
                         Comment* node_after_context) {
  Node* next = nullptr;
  for (Node* node = fragment->firstChild(); node; node = next) {
    if (node_before_context->IsDescendantOf(node)) {
      next = NodeTraversal::Next(*node);
      continue;
    }
    next = NodeTraversal::NextSkippingChildren(*node);
    DCHECK(!node->contains(node_after_context))
        << node << " " << node_after_context;
    node->parentNode()->RemoveChild(node, ASSERT_NO_EXCEPTION);
    if (node_before_context == node)
      break;
  }

  DCHECK(node_after_context->parentNode()) << node_after_context;
  for (Node* node = node_after_context; node; node = next) {
    next = NodeTraversal::NextSkippingChildren(*node);
    node->parentNode()->RemoveChild(node, ASSERT_NO_EXCEPTION);
  }
}

DocumentFragment* CreateFragmentFromMarkupWithContext(
    Document& document,
    const String& markup,
    unsigned fragment_start,
    unsigned fragment_end,
    const String& base_url,
    ParserContentPolicy parser_content_policy) {
  // FIXME: Need to handle the case where the markup already contains these
  // markers.

  StringBuilder tagged_markup;
  tagged_markup.Append(markup.Left(fragment_start));
  MarkupFormatter::AppendComment(tagged_markup, kFragmentMarkerTag);
  tagged_markup.Append(
      markup.Substring(fragment_start, fragment_end - fragment_start));
  MarkupFormatter::AppendComment(tagged_markup, kFragmentMarkerTag);
  tagged_markup.Append(markup.Substring(fragment_end));

  DocumentFragment* tagged_fragment = CreateFragmentFromMarkup(
      document, tagged_markup.ToString(), base_url, parser_content_policy);

  Comment* node_before_context = nullptr;
  Comment* node_after_context = nullptr;
  if (!FindNodesSurroundingContext(tagged_fragment, node_before_context,
                                   node_after_context))
    return nullptr;

  auto* tagged_document = MakeGarbageCollected<Document>(
      DocumentInit::Create()
          .WithExecutionContext(document.GetExecutionContext())
          .WithAgent(document.GetAgent()));

  auto* root =
      MakeGarbageCollected<Element>(QualifiedName::Null(), tagged_document);
  root->AppendChild(tagged_fragment);
  tagged_document->AppendChild(root);

  const EphemeralRange range(
      Position::AfterNode(*node_before_context).ParentAnchoredEquivalent(),
      Position::BeforeNode(*node_after_context).ParentAnchoredEquivalent());

  DCHECK(range.CommonAncestorContainer());
  Node& common_ancestor = *range.CommonAncestorContainer();
  HTMLElement* special_common_ancestor =
      AncestorToRetainStructureAndAppearanceWithNoLayoutObject(common_ancestor);

  // When there's a special common ancestor outside of the fragment, we must
  // include it as well to preserve the structure and appearance of the
  // fragment. For example, if the fragment contains TD, we need to include the
  // enclosing TABLE tag as well.
  DocumentFragment* fragment = DocumentFragment::Create(document);
  if (special_common_ancestor)
    fragment->AppendChild(special_common_ancestor);
  else
    fragment->ParserTakeAllChildrenFrom(To<ContainerNode>(common_ancestor));

  TrimFragment(fragment, node_before_context, node_after_context);

  return fragment;
}

String CreateMarkup(const Node* node,
                    ChildrenOnly children_only,
                    AbsoluteURLs should_resolve_urls,
                    const ShadowRootInclusion& shadow_root_inclusion) {
  if (!node)
    return "";

  MarkupAccumulator accumulator(should_resolve_urls,
                                IsA<HTMLDocument>(node->GetDocument())
                                    ? SerializationType::kHTML
                                    : SerializationType::kXML,
                                shadow_root_inclusion);
  return accumulator.SerializeNodes<EditingStrategy>(*node, children_only);
}

static void FillContainerFromString(ContainerNode* paragraph,
                                    const String& string) {
  Document& document = paragraph->GetDocument();

  if (string.empty()) {
    paragraph->AppendChild(MakeGarbageCollected<HTMLBRElement>(document));
    return;
  }

  DCHECK_EQ(string.find('\n'), kNotFound) << string;

  Vector<String> tab_list;
  string.Split('\t', true, tab_list);
  StringBuilder tab_text;
  bool first = true;
  wtf_size_t num_entries = tab_list.size();
  for (wtf_size_t i = 0; i < num_entries; ++i) {
    const String& s = tab_list[i];

    // append the non-tab textual part
    if (!s.empty()) {
      if (!tab_text.empty()) {
        paragraph->AppendChild(
            CreateTabSpanElement(document, tab_text.ToString()));
        tab_text.Clear();
      }
      Text* text_node = document.createTextNode(
          StringWithRebalancedWhitespace(s, first, i + 1 == num_entries));
      paragraph->AppendChild(text_node);
    }

    // there is a tab after every entry, except the last entry
    // (if the last character is a tab, the list gets an extra empty entry)
    if (i + 1 != num_entries)
      tab_text.Append('\t');
    else if (!tab_text.empty())
      paragraph->AppendChild(
          CreateTabSpanElement(document, tab_text.ToString()));

    first = false;
  }
}

bool IsPlainTextMarkup(Node* node) {
  DCHECK(node);
  auto* element = DynamicTo<HTMLDivElement>(*node);
  if (!element)
    return false;

  if (!element->hasAttributes())
    return false;

  if (element->HasOneChild()) {
    return element->firstChild()->IsTextNode() ||
           element->firstChild()->hasChildren();
  }

  return element->HasChildCount(2) &&
         IsTabHTMLSpanElementTextNode(element->firstChild()->firstChild()) &&
         element->lastChild()->IsTextNode();
}

static bool ShouldPreserveNewline(const EphemeralRange& range) {
  if (Node* node = range.StartPosition().NodeAsRangeFirstNode()) {
    if (LayoutObject* layout_object = node->GetLayoutObject())
      return layout_object->Style()->ShouldPreserveBreaks();
  }

  if (Node* node = range.StartPosition().AnchorNode()) {
    if (LayoutObject* layout_object = node->GetLayoutObject())
      return layout_object->Style()->ShouldPreserveBreaks();
  }

  return false;
}

DocumentFragment* CreateFragmentFromText(const EphemeralRange& context,
                                         const String& text) {
  if (context.IsNull())
    return nullptr;

  Document& document = context.GetDocument();
  DocumentFragment* fragment = document.createDocumentFragment();

  if (text.empty())
    return fragment;

  String string = text;
  string.Replace("\r\n", "\n");
  string.Replace('\r', '\n');

  if (!IsRichlyEditablePosition(context.StartPosition()) ||
      ShouldPreserveNewline(context)) {
    fragment->AppendChild(document.createTextNode(string));
    if (string.EndsWith('\n')) {
      auto* element = MakeGarbageCollected<HTMLBRElement>(document);
      element->setAttribute(html_names::kClassAttr,
                            AtomicString(AppleInterchangeNewline));
      fragment->AppendChild(element);
    }
    return fragment;
  }

  // A string with no newlines gets added inline, rather than being put into a
  // paragraph.
  if (string.find('\n') == kNotFound) {
    FillContainerFromString(fragment, string);
    return fragment;
  }

  // Break string into paragraphs. Extra line breaks turn into empty paragraphs.
  Element* block =
      EnclosingBlock(context.StartPosition().NodeAsRangeFirstNode());
  bool use_clones_of_enclosing_block =
      block && !IsA<HTMLBodyElement>(block) && !IsA<HTMLHtmlElement>(block) &&
      block != RootEditableElementOf(context.StartPosition());

  Vector<String> list;
  string.Split('\n', true, list);  // true gets us empty strings in the list
  wtf_size_t num_lines = list.size();
  for (wtf_size_t i = 0; i < num_lines; ++i) {
    const String& s = list[i];

    Element* element = nullptr;
    if (s.empty() && i + 1 == num_lines) {
      // For last line, use the "magic BR" rather than a P.
      element = MakeGarbageCollected<HTMLBRElement>(document);
      element->setAttribute(html_names::kClassAttr,
                            AtomicString(AppleInterchangeNewline));
    } else {
      if (use_clones_of_enclosing_block)
        element = &block->CloneWithoutChildren();
      else
        element = CreateDefaultParagraphElement(document);
      FillContainerFromString(element, s);
    }
    fragment->AppendChild(element);
  }
  return fragment;
}

DocumentFragment* CreateFragmentForInnerOuterHTML(
    const String& markup,
    Element* context_element,
    ParserContentPolicy parser_content_policy,
    Element::ParseDeclarativeShadowRoots parse_declarative_shadows,
    Element::ForceHtml force_html,
    ExceptionState& exception_state) {
  DCHECK(context_element);
  const HTMLTemplateElement* template_element =
      DynamicTo<HTMLTemplateElement>(*context_element);
  if (template_element && !template_element->GetExecutionContext()) {
    return nullptr;
  }

  Document& document =
      IsA<HTMLTemplateElement>(*context_element)
          ? context_element->GetDocument().EnsureTemplateDocument()
          : context_element->GetDocument();
  DocumentFragment* fragment = DocumentFragment::Create(document);
  document.setAllowDeclarativeShadowRoots(
      parse_declarative_shadows ==
      Element::ParseDeclarativeShadowRoots::kParse);

  if (IsA<HTMLDocument>(document) || force_html == Element::ForceHtml::kForce) {
    bool log_tag_stats = false;
    base::ElapsedTimer parse_timer;
    HTMLFragmentParsingBehaviorSet parser_behavior;
    if (parse_declarative_shadows ==
        Element::ParseDeclarativeShadowRoots::kParse) {
      parser_behavior.Put(HTMLFragmentParsingBehavior::kIncludeShadowRoots);
    }
    const bool parsed_fast_path = TryParsingHTMLFragment(
        markup, document, *fragment, *context_element, parser_content_policy,
        parser_behavior, &log_tag_stats);
    if (parsed_fast_path) {
      LogFastPathParserTotalTime(parse_timer.Elapsed());
#if DCHECK_IS_ON()
      // As a sanity check for the fast-path, create another fragment using
      // the full parser and compare the results.
      // See https://bugs.chromium.org/p/chromium/issues/detail?id=1407201
      // for details.
      DocumentFragment* fragment2 = DocumentFragment::Create(document);
      fragment2->ParseHTML(markup, context_element, parser_content_policy);
      DCHECK_EQ(CreateMarkup(fragment), CreateMarkup(fragment2))
          << " supplied value " << markup;
      DCHECK(fragment->isEqualNode(fragment2));
#endif
      return fragment;
    }
    fragment = DocumentFragment::Create(document);
    fragment->ParseHTML(markup, context_element, parser_content_policy);
    LogFastPathParserTotalTime(parse_timer.Elapsed());
    if (log_tag_stats &&
        RuntimeEnabledFeatures::InnerHTMLParserFastpathLogFailureEnabled()) {
      LogTagsForUnsupportedTagTypeFailure(*fragment);
    }
    return fragment;
  }

  bool was_valid = fragment->ParseXML(markup, context_element,
                                      parser_content_policy, &exception_state);
  if (!was_valid) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The provided markup is invalid XML, and "
        "therefore cannot be inserted into an XML "
        "document.");
    return nullptr;
  }
  return fragment;
}

DocumentFragment* CreateFragmentForTransformToFragment(
    const String& source_string,
    const String& source_mime_type,
    Document& output_doc) {
  DocumentFragment* fragment = output_doc.createDocumentFragment();

  // The HTML spec says that we should execute scripts and set their already
  // started flag to false for transformToFragment, so we use
  // kAllowScriptingContentAndDoNotMarkAlreadyStarted in ParseHTML and ParseXML
  // below. https://html.spec.whatwg.org/multipage/scripting.html#scriptTagXSLT

  if (source_mime_type == "text/html") {
    // As far as I can tell, there isn't a spec for how transformToFragment is
    // supposed to work. Based on the documentation I can find, it looks like we
    // want to start parsing the fragment in the InBody insertion mode.
    // Unfortunately, that's an implementation detail of the parser. We achieve
    // that effect here by passing in a fake body element as context for the
    // fragment.
    auto* fake_body = MakeGarbageCollected<HTMLBodyElement>(output_doc);
    fragment->ParseHTML(source_string, fake_body,
                        kAllowScriptingContentAndDoNotMarkAlreadyStarted);
  } else if (source_mime_type == "text/plain") {
    fragment->ParserAppendChild(Text::Create(output_doc, source_string));
  } else {
    bool successful_parse =
        fragment->ParseXML(source_string, nullptr,
                           kAllowScriptingContentAndDoNotMarkAlreadyStarted);
    if (!successful_parse)
      return nullptr;
  }

  // FIXME: Do we need to mess with URLs here?

  return fragment;
}

static inline void RemoveElementPreservingChildren(DocumentFragment* fragment,
                                                   HTMLElement* element) {
  Node* next_child = nullptr;
  for (Node* child = element->firstChild(); child; child = next_child) {
    next_child = child->nextSibling();
    element->RemoveChild(child);
    fragment->InsertBefore(child, element);
  }
  fragment->RemoveChild(element);
}

DocumentFragment* CreateContextualFragment(
    const String& markup,
    Element* element,
    ParserContentPolicy parser_content_policy,
    ExceptionState& exception_state) {
  DCHECK(element);

  DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
      markup, element, parser_content_policy,
      Element::ParseDeclarativeShadowRoots::kDontParse,
      Element::ForceHtml::kDontForce, exception_state);
  if (!fragment)
    return nullptr;

  // We need to pop <html> and <body> elements and remove <head> to
  // accommodate folks passing complete HTML documents to make the
  // child of an element.

  Node* next_node = nullptr;
  for (Node* node = fragment->firstChild(); node; node = next_node) {
    next_node = node->nextSibling();
    if (IsA<HTMLHtmlElement>(node) || IsA<HTMLHeadElement>(node) ||
        IsA<HTMLBodyElement>(node)) {
      auto* child_element = To<HTMLElement>(node);
      if (Node* first_child = child_element->firstChild())
        next_node = first_child;
      RemoveElementPreservingChildren(fragment, child_element);
    }
  }
  return fragment;
}

void ReplaceChildrenWithFragment(ContainerNode* container,
                                 DocumentFragment* fragment,
                                 ExceptionState& exception_state) {
  DCHECK(container);
  RUNTIME_CALL_TIMER_SCOPE(
      container->GetDocument().GetAgent().isolate(),
      RuntimeCallStats::CounterId::kReplaceChildrenWithFragment);
  ContainerNode* container_node(container);

  ChildListMutationScope mutation(*container_node);

  if (!fragment->firstChild()) {
    container_node->RemoveChildren();
    return;
  }

  // FIXME: No need to replace the child it is a text node and its contents are
  // already == text.
  if (container_node->HasOneChild()) {
    container_node->ReplaceChild(fragment, container_node->firstChild(),
                                 exception_state);
    return;
  }

  container_node->RemoveChildren();
  container_node->AppendChild(fragment, exception_state);
}

void ReplaceChildrenWithText(ContainerNode* container,
                             const String& text,
                             ExceptionState& exception_state) {
  DCHECK(container);
  ContainerNode* container_node(container);

  ChildListMutationScope mutation(*container_node);

  // NOTE: This method currently always creates a text node, even if that text
  // node will be empty.
  Text* text_node = Text::Create(container_node->GetDocument(), text);

  // FIXME: No need to replace the child it is a text node and its contents are
  // already == text.
  if (container_node->HasOneChild()) {
    container_node->ReplaceChild(text_node, container_node->firstChild(),
                                 exception_state);
    return;
  }

  container_node->RemoveChildren();
  container_node->AppendChild(text_node, exception_state);
}

void MergeWithNextTextNode(Text* text_node, ExceptionState& exception_state) {
  DCHECK(text_node);
  auto* text_next = DynamicTo<Text>(text_node->nextSibling());
  if (!text_next)
    return;

  text_node->appendData(text_next->data());
  if (text_next->parentNode())  // Might have been removed by mutation event.
    text_next->remove(exception_state);
}

static Document* CreateStagingDocumentForMarkupSanitization(
    AgentGroupScheduler& agent_group_scheduler) {
  Page* page = Page::CreateNonOrdinary(GetStaticEmptyChromeClientInstance(),
                                       agent_group_scheduler,
                                       /*color_provider_colors=*/nullptr);

  page->GetSettings().SetScriptEnabled(false);
  page->GetSettings().SetPluginsEnabled(false);
  page->GetSettings().SetAcceleratedCompositingEnabled(false);
  page->GetSettings().SetParserScriptingFlagPolicy(
      ParserScriptingFlagPolicy::kEnabled);

  auto* client =
      MakeGarbageCollected<EmptyLocalFrameClientWithFailingLoaderFactory>();
  LocalFrame* frame = MakeGarbageCollected<LocalFrame>(
      client, *page,
      nullptr,  // FrameOwner*
      nullptr,  // Frame* parent
      nullptr,  // Frame* previous_sibling
      FrameInsertType::kInsertInConstructor, blink::LocalFrameToken(),
      nullptr,            // WindowAgentFactory*
      nullptr,            // InterfaceRegistry*
      mojo::NullRemote()  // BrowserInterfaceBroker
  );
  // Don't leak the actual viewport size to unsanitized markup
  LocalFrameView* frame_view =
      MakeGarbageCollected<LocalFrameView>(*frame, gfx::Size(800, 600));
  frame->SetView(frame_view);
  // TODO(https://crbug.com/1355751) Initialize `storage_key`.
  frame->Init(/*opener=*/nullptr, DocumentToken(), /*policy_container=*/nullptr,
              StorageKey(), /*document_ukm_source_id=*/ukm::kInvalidSourceId,
              /*creator_base_url=*/KURL());

  Document* document = frame->GetDocument();
  DCHECK(document);
  DCHECK(IsA<HTMLDocument>(document));
  DCHECK(document->body());

  document->SetIsForMarkupSanitization(true);

  return document;
}

static bool ContainsStyleElements(const DocumentFragment& fragment) {
  for (const Node& node : NodeTraversal::DescendantsOf(fragment)) {
    if (IsA<HTMLStyleElement>(node) || IsA<SVGStyleElement>(node))
      return true;
  }
  return false;
}

// Returns true if any svg <use> element is removed.
static bool StripSVGUseNonLocalHrefs(Node& node) {
  if (auto* use = DynamicTo<SVGUseElement>(node)) {
    SVGURLReferenceResolver resolver(use->HrefString(), use->GetDocument());
    if (!resolver.IsLocal() || resolver.AbsoluteUrl().ProtocolIsData()) {
      node.remove();
    }
    return true;
  }
  bool stripped = false;
  for (Node* child = node.firstChild(); child;) {
    Node* next = child->nextSibling();
    if (StripSVGUseNonLocalHrefs(*child)) {
      stripped = true;
    }
    child = next;
  }
  return stripped;
}

namespace {

constexpr unsigned kMaxSanitizationIterations = 16;

}  // namespace

String CreateStrictlyProcessedMarkupWithContext(
    Document& document,
    const String& raw_markup,
    unsigned fragment_start,
    unsigned fragment_end,
    const String& base_url,
    ChildrenOnly children_only,
    AbsoluteURLs should_resolve_urls,
    const ShadowRootInclusion& shadow_root_inclusion) {
  if (raw_markup.empty())
    return String();

  Document* staging_document = CreateStagingDocumentForMarkupSanitization(
      *document.GetFrame()->GetFrameScheduler()->GetAgentGroupScheduler());

  // Iterate on parsing, sanitization and serialization until the markup is
  // stable, or if we have exceeded the maximum allowed number of iterations.
  String last_markup;
  String markup = raw_markup;
  for (unsigned iteration = 0;
       iteration < kMaxSanitizationIterations && last_markup != markup;
       ++iteration) {
    last_markup = markup;

    DocumentFragment* fragment = CreateFragmentFromMarkupWithContext(
        *staging_document, last_markup, fragment_start, fragment_end, KURL(),
        kDisallowScriptingAndPluginContent);
    if (!fragment) {
      staging_document->GetPage()->WillBeDestroyed();
      return String();
    }

    bool needs_sanitization = false;
    if (ContainsStyleElements(*fragment))
      needs_sanitization = true;
    if (StripSVGUseNonLocalHrefs(*fragment)) {
      needs_sanitization = true;
    }

    if (!needs_sanitization) {
      markup = CreateMarkup(fragment);
    } else {
      Element* body = staging_document->body();
      staging_document->body()->appendChild(fragment);
      staging_document->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

      // This sanitizes stylesheets in the markup into element inline styles
      markup = CreateMarkup(Position::FirstPositionInNode(*body),
                            Position::LastPositionInNode(*body),
                            CreateMarkupOptions::Builder()
                                .SetShouldAnnotateForInterchange(true)
                                .SetIsForMarkupSanitization(true)
                                .Build());

      body->RemoveChildren();
    }

    fragment_start = 0;
    fragment_end = markup.length();
  }

  String final_markup;
  // Sanitization succeeds only if the markup can stabilize.
  if (last_markup == markup) {
    DocumentFragment* final_fragment =
        CreateFragmentFromMarkup(*staging_document, markup, base_url,
                                 kDisallowScriptingAndPluginContent);
    final_markup = CreateMarkup(final_fragment, children_only,
                                should_resolve_urls, shadow_root_inclusion);
  }
  staging_document->GetPage()->WillBeDestroyed();
  return final_markup;
}

DocumentFragment* CreateStrictlyProcessedFragmentFromMarkupWithContext(
    Document& document,
    const String& raw_markup,
    unsigned fragment_start,
    unsigned fragment_end,
    const String& base_url) {
  String sanitized_markup = CreateStrictlyProcessedMarkupWithContext(
      document, raw_markup, fragment_start, fragment_end, KURL());
  if (sanitized_markup.IsNull())
    return nullptr;
  return CreateFragmentFromMarkup(document, sanitized_markup, base_url,
                                  kDisallowScriptingAndPluginContent);
}

template class CORE_TEMPLATE_EXPORT CreateMarkupAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    CreateMarkupAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink
