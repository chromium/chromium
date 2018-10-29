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

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/cdata_section.h"
#include "third_party/blink/renderer/core/dom/child_list_mutation_scope.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/context_features.h"
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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_quote_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using namespace HTMLNames;

class AttributeChange {
  DISALLOW_NEW();

 public:
  AttributeChange() : name_(g_null_atom, g_null_atom, g_null_atom) {}

  AttributeChange(Element* element,
                  const QualifiedName& name,
                  const String& value)
      : element_(element), name_(name), value_(value) {}

  void Apply() { element_->setAttribute(name_, AtomicString(value_)); }

  void Trace(blink::Visitor* visitor) { visitor->Trace(element_); }

 private:
  Member<Element> element_;
  QualifiedName name_;
  String value_;
};

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::AttributeChange);

namespace blink {

static void CompleteURLs(DocumentFragment& fragment, const String& base_url) {
  HeapVector<AttributeChange> changes;

  KURL parsed_base_url(base_url);

  for (Element& element : ElementTraversal::DescendantsOf(fragment)) {
    AttributeCollection attributes = element.Attributes();
    // AttributeCollection::iterator end = attributes.end();
    for (const auto& attribute : attributes) {
      if (element.IsURLAttribute(attribute) && !attribute.Value().IsEmpty())
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
  return IsHTMLTableCellElement(*node) || IsNonTableCellHTMLBlockElement(node);
}

static HTMLElement* AncestorToRetainStructureAndAppearanceForBlock(
    Element* common_ancestor_block) {
  if (!common_ancestor_block)
    return nullptr;

  if (common_ancestor_block->HasTagName(tbodyTag) ||
      IsHTMLTableRowElement(*common_ancestor_block))
    return Traversal<HTMLTableElement>::FirstAncestor(*common_ancestor_block);

  if (IsNonTableCellHTMLBlockElement(common_ancestor_block))
    return ToHTMLElement(common_ancestor_block);

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
  HTMLElement* common_ancestor_block = ToHTMLElement(EnclosingNodeOfType(
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
  if (!value->IsIdentifierValue())
    return false;
  return ToCSSIdentifierValue(value)->GetValueID() == CSSValueNone;
}

template <typename Strategy>
static HTMLElement* HighestAncestorToWrapMarkup(
    const PositionTemplate<Strategy>& start_position,
    const PositionTemplate<Strategy>& end_position,
    EAnnotateForInterchange should_annotate,
    Node* constraining_ancestor) {
  Node* first_node = start_position.NodeAsRangeFirstNode();
  // For compatibility reason, we use container node of start and end
  // positions rather than first node and last node in selection.
  Node* common_ancestor =
      Strategy::CommonAncestor(*start_position.ComputeContainerNode(),
                               *end_position.ComputeContainerNode());
  DCHECK(common_ancestor);
  HTMLElement* special_common_ancestor = nullptr;
  if (should_annotate == kAnnotateForInterchange) {
    // Include ancestors that aren't completely inside the range but are
    // required to retain the structure and appearance of the copied markup.
    special_common_ancestor =
        AncestorToRetainStructureAndAppearance(common_ancestor);
    if (first_node) {
      const Position& first_node_position =
          FirstPositionInOrBeforeNode(*first_node);
      if (Node* parent_list_node =
              EnclosingNodeOfType(first_node_position, IsListItem)) {
        EphemeralRangeTemplate<Strategy> markup_range =
            EphemeralRangeTemplate<Strategy>(start_position, end_position);
        EphemeralRangeTemplate<Strategy> node_range =
            NormalizeRange(EphemeralRangeTemplate<Strategy>::RangeOfContents(
                *parent_list_node));
        if (node_range == markup_range) {
          ContainerNode* ancestor = parent_list_node->parentNode();
          while (ancestor && !IsHTMLListElement(ancestor))
            ancestor = ancestor->parentNode();
          special_common_ancestor = ToHTMLElement(ancestor);
        }
      }

      // Retain the Mail quote level by including all ancestor mail block
      // quotes.
      if (HTMLQuoteElement* highest_mail_blockquote =
              ToHTMLQuoteElement(HighestEnclosingNodeOfType(
                  first_node_position, IsMailHTMLBlockquoteElement,
                  kCanCrossEditingBoundary))) {
        special_common_ancestor = highest_mail_blockquote;
      }
    }
  }

  Node* check_ancestor =
      special_common_ancestor ? special_common_ancestor : common_ancestor;
  if (check_ancestor->GetLayoutObject()) {
    HTMLElement* new_special_common_ancestor =
        ToHTMLElement(HighestEnclosingNodeOfType(
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
        ToHTMLSpanElement(Strategy::Parent(*common_ancestor));
  }
  if (!special_common_ancestor && IsTabHTMLSpanElement(common_ancestor))
    special_common_ancestor = ToHTMLSpanElement(common_ancestor);

  if (HTMLAnchorElement* enclosing_anchor =
          ToHTMLAnchorElement(EnclosingElementWithTag(
              Position::FirstPositionInNode(special_common_ancestor
                                                ? *special_common_ancestor
                                                : *common_ancestor),
              aTag)))
    special_common_ancestor = enclosing_anchor;

  return special_common_ancestor;
}

template <typename Strategy>
class CreateMarkupAlgorithm {
 public:
  static String CreateMarkup(
      const PositionTemplate<Strategy>& start_position,
      const PositionTemplate<Strategy>& end_position,
      EAnnotateForInterchange should_annotate = kDoNotAnnotateForInterchange,
      ConvertBlocksToInlines = ConvertBlocksToInlines::kNotConvert,
      EAbsoluteURLs should_resolve_urls = kDoNotResolveURLs,
      Node* constraining_ancestor = nullptr);
};

// FIXME: Shouldn't we omit style info when annotate ==
// DoNotAnnotateForInterchange?
// FIXME: At least, annotation and style info should probably not be included in
// range.markupString()
template <typename Strategy>
String CreateMarkupAlgorithm<Strategy>::CreateMarkup(
    const PositionTemplate<Strategy>& start_position,
    const PositionTemplate<Strategy>& end_position,
    EAnnotateForInterchange should_annotate,
    ConvertBlocksToInlines convert_blocks_to_inlines,
    EAbsoluteURLs should_resolve_urls,
    Node* constraining_ancestor) {
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
      start_position, end_position, should_annotate, constraining_ancestor);
  StyledMarkupSerializer<Strategy> serializer(
      should_resolve_urls, should_annotate, start_position, end_position,
      special_common_ancestor, convert_blocks_to_inlines);
  return serializer.CreateMarkup();
}

String CreateMarkup(const Position& start_position,
                    const Position& end_position,
                    EAnnotateForInterchange should_annotate,
                    ConvertBlocksToInlines convert_blocks_to_inlines,
                    EAbsoluteURLs should_resolve_urls,
                    Node* constraining_ancestor) {
  return CreateMarkupAlgorithm<EditingStrategy>::CreateMarkup(
      start_position, end_position, should_annotate, convert_blocks_to_inlines,
      should_resolve_urls, constraining_ancestor);
}

String CreateMarkup(const PositionInFlatTree& start_position,
                    const PositionInFlatTree& end_position,
                    EAnnotateForInterchange should_annotate,
                    ConvertBlocksToInlines convert_blocks_to_inlines,
                    EAbsoluteURLs should_resolve_urls,
                    Node* constraining_ancestor) {
  return CreateMarkupAlgorithm<EditingInFlatTreeStrategy>::CreateMarkup(
      start_position, end_position, should_annotate, convert_blocks_to_inlines,
      should_resolve_urls, constraining_ancestor);
}

DocumentFragment* CreateFragmentFromMarkup(
    Document& document,
    const String& markup,
    const String& base_url,
    ParserContentPolicy parser_content_policy) {
  // We use a fake body element here to trick the HTML parser to using the
  // InBody insertion mode.
  HTMLBodyElement* fake_body = HTMLBodyElement::Create(document);
  DocumentFragment* fragment = DocumentFragment::Create(document);

  fragment->ParseHTML(markup, fake_body, parser_content_policy);

  if (!base_url.IsEmpty() && base_url != BlankURL() &&
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
    if (node.getNodeType() == Node::kCommentNode &&
        ToComment(node).data() == kFragmentMarkerTag) {
      if (!node_before_context) {
        node_before_context = &ToComment(node);
      } else {
        node_after_context = &ToComment(node);
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

  Document* tagged_document = Document::Create(DocumentInit::Create());
  tagged_document->SetContextFeatures(document.GetContextFeatures());

  Element* root = Element::Create(QualifiedName::Null(), tagged_document);
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
    fragment->ParserTakeAllChildrenFrom(ToContainerNode(common_ancestor));

  TrimFragment(fragment, node_before_context, node_after_context);

  return fragment;
}

String CreateMarkup(const Node* node,
                    EChildrenOnly children_only,
                    EAbsoluteURLs should_resolve_urls) {
  if (!node)
    return "";

  MarkupAccumulator accumulator(should_resolve_urls);
  return SerializeNodes<EditingStrategy>(accumulator, const_cast<Node&>(*node),
                                         children_only);
}

static void FillContainerFromString(ContainerNode* paragraph,
                                    const String& string) {
  Document& document = paragraph->GetDocument();

  if (string.IsEmpty()) {
    paragraph->AppendChild(HTMLBRElement::Create(document));
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
    if (!s.IsEmpty()) {
      if (!tab_text.IsEmpty()) {
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
    else if (!tab_text.IsEmpty())
      paragraph->AppendChild(
          CreateTabSpanElement(document, tab_text.ToString()));

    first = false;
  }
}

bool IsPlainTextMarkup(Node* node) {
  DCHECK(node);
  if (!IsHTMLDivElement(*node))
    return false;

  HTMLDivElement& element = ToHTMLDivElement(*node);
  if (!element.hasAttributes())
    return false;

  if (element.HasOneChild())
    return element.firstChild()->IsTextNode() ||
           element.firstChild()->hasChildren();

  return element.HasChildCount(2) &&
         IsTabHTMLSpanElementTextNode(element.firstChild()->firstChild()) &&
         element.lastChild()->IsTextNode();
}

static bool ShouldPreserveNewline(const EphemeralRange& range) {
  if (Node* node = range.StartPosition().NodeAsRangeFirstNode()) {
    if (LayoutObject* layout_object = node->GetLayoutObject())
      return layout_object->Style()->PreserveNewline();
  }

  if (Node* node = range.StartPosition().AnchorNode()) {
    if (LayoutObject* layout_object = node->GetLayoutObject())
      return layout_object->Style()->PreserveNewline();
  }

  return false;
}

DocumentFragment* CreateFragmentFromText(const EphemeralRange& context,
                                         const String& text) {
  if (context.IsNull())
    return nullptr;

  Document& document = context.GetDocument();
  DocumentFragment* fragment = document.createDocumentFragment();

  if (text.IsEmpty())
    return fragment;

  String string = text;
  string.Replace("\r\n", "\n");
  string.Replace('\r', '\n');

  if (!IsRichlyEditablePosition(context.StartPosition()) ||
      ShouldPreserveNewline(context)) {
    fragment->AppendChild(document.createTextNode(string));
    if (string.EndsWith('\n')) {
      HTMLBRElement* element = HTMLBRElement::Create(document);
      element->setAttribute(classAttr, AppleInterchangeNewline);
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
      block && !IsHTMLBodyElement(*block) && !IsHTMLHtmlElement(*block) &&
      block != RootEditableElementOf(context.StartPosition());

  Vector<String> list;
  string.Split('\n', true, list);  // true gets us empty strings in the list
  wtf_size_t num_lines = list.size();
  for (wtf_size_t i = 0; i < num_lines; ++i) {
    const String& s = list[i];

    Element* element = nullptr;
    if (s.IsEmpty() && i + 1 == num_lines) {
      // For last line, use the "magic BR" rather than a P.
      element = HTMLBRElement::Create(document);
      element->setAttribute(classAttr, AppleInterchangeNewline);
    } else {
      if (use_clones_of_enclosing_block)
        element = block->CloneWithoutChildren();
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
    const char* method,
    ExceptionState& exception_state) {
  DCHECK(context_element);
  Document& document =
      IsHTMLTemplateElement(*context_element)
          ? context_element->GetDocument().EnsureTemplateDocument()
          : context_element->GetDocument();
  DocumentFragment* fragment = DocumentFragment::Create(document);

  if (document.IsHTMLDocument()) {
    fragment->ParseHTML(markup, context_element, parser_content_policy);
    return fragment;
  }

  bool was_valid =
      fragment->ParseXML(markup, context_element, parser_content_policy);
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

  if (source_mime_type == "text/html") {
    // As far as I can tell, there isn't a spec for how transformToFragment is
    // supposed to work. Based on the documentation I can find, it looks like we
    // want to start parsing the fragment in the InBody insertion mode.
    // Unfortunately, that's an implementation detail of the parser. We achieve
    // that effect here by passing in a fake body element as context for the
    // fragment.
    HTMLBodyElement* fake_body = HTMLBodyElement::Create(output_doc);
    fragment->ParseHTML(source_string, fake_body);
  } else if (source_mime_type == "text/plain") {
    fragment->ParserAppendChild(Text::Create(output_doc, source_string));
  } else {
    bool successful_parse = fragment->ParseXML(source_string, nullptr);
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
      markup, element, parser_content_policy, "createContextualFragment",
      exception_state);
  if (!fragment)
    return nullptr;

  // We need to pop <html> and <body> elements and remove <head> to
  // accommodate folks passing complete HTML documents to make the
  // child of an element.

  Node* next_node = nullptr;
  for (Node* node = fragment->firstChild(); node; node = next_node) {
    next_node = node->nextSibling();
    if (IsHTMLHtmlElement(*node) || IsHTMLHeadElement(*node) ||
        IsHTMLBodyElement(*node)) {
      HTMLElement* element = ToHTMLElement(node);
      if (Node* first_child = element->firstChild())
        next_node = first_child;
      RemoveElementPreservingChildren(fragment, element);
    }
  }
  return fragment;
}

void ReplaceChildrenWithFragment(ContainerNode* container,
                                 DocumentFragment* fragment,
                                 ExceptionState& exception_state) {
  RUNTIME_CALL_TIMER_SCOPE(
      V8PerIsolateData::MainThreadIsolate(),
      RuntimeCallStats::CounterId::kReplaceChildrenWithFragment);
  DCHECK(container);
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

  // FIXME: This is wrong if containerNode->firstChild() has more than one ref!
  // Example:
  // <div>foo</div>
  // <script>
  // var oldText = div.firstChild;
  // console.log(oldText.data); // foo
  // div.innerText = "bar";
  // console.log(oldText.data); // bar!?!
  // </script>
  // I believe this is an intentional benchmark cheat from years ago.
  // We should re-visit if we actually want this still.
  if (container_node->HasOneTextChild()) {
    ToText(container_node->firstChild())->setData(text);
    return;
  }

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
  Node* next = text_node->nextSibling();
  if (!next || !next->IsTextNode())
    return;

  Text* text_next = ToText(next);
  text_node->appendData(text_next->data());
  if (text_next->parentNode())  // Might have been removed by mutation event.
    text_next->remove(exception_state);
}

template class CORE_TEMPLATE_EXPORT CreateMarkupAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    CreateMarkupAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink
