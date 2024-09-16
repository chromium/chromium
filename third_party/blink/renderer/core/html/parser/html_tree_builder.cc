/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011, 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/html/parser/html_tree_builder.h"

#include <memory>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_stack_item.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"

namespace blink {

using HTMLTag = html_names::HTMLTag;

namespace {

inline bool IsHTMLSpaceOrReplacementCharacter(UChar character) {
  return IsHTMLSpace<UChar>(character) || character == kReplacementCharacter;
}
}  // namespace

static TextPosition UninitializedPositionValue1() {
  return TextPosition(OrdinalNumber::FromOneBasedInt(-1),
                      OrdinalNumber::First());
}

static inline bool IsAllWhitespace(const StringView& string_view) {
  return string_view.IsAllSpecialCharacters<IsHTMLSpace<UChar>>();
}

static inline bool IsAllWhitespaceOrReplacementCharacters(
    const StringView& string_view) {
  return string_view
      .IsAllSpecialCharacters<IsHTMLSpaceOrReplacementCharacter>();
}

// The following macros are used in switch statements for some common types.
// They are defined so that they can look like a normal case statement, e.g.:
//   case FOO_CASES:

// Disable formatting as it mangles these macros.
// clang-format off

#define CAPTION_COL_OR_COLGROUP_CASES \
  HTMLTag::kCaption: \
  case HTMLTag::kCol: \
  case HTMLTag::kColgroup

#define NUMBERED_HEADER_CASES \
  HTMLTag::kH1: \
  case HTMLTag::kH2: \
  case HTMLTag::kH3: \
  case HTMLTag::kH4: \
  case HTMLTag::kH5: \
  case HTMLTag::kH6

#define TABLE_BODY_CONTEXT_CASES \
  HTMLTag::kTbody: \
  case HTMLTag::kTfoot: \
  case HTMLTag::kThead

#define TABLE_CELL_CONTEXT_CASES \
  HTMLTag::kTh: \
  case HTMLTag::kTd

// clang-format on

static bool IsTableBodyContextTag(HTMLTag tag) {
  switch (tag) {
    case TABLE_BODY_CONTEXT_CASES:
      return true;
    default:
      return false;
  }
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#formatting
class HTMLTreeBuilder::CharacterTokenBuffer {
 public:
  explicit CharacterTokenBuffer(AtomicHTMLToken* token)
      : characters_(token->Characters()),
        current_(0),
        end_(token->Characters().length()) {
    DCHECK(!IsEmpty());
  }

  CharacterTokenBuffer(const CharacterTokenBuffer&) = delete;
  CharacterTokenBuffer& operator=(const CharacterTokenBuffer&) = delete;

  ~CharacterTokenBuffer() { DCHECK(IsEmpty()); }

  bool IsEmpty() const { return current_ == end_; }

  void SkipAtMostOneLeadingNewline() {
    DCHECK(!IsEmpty());
    if (characters_[current_] == '\n') {
      ++current_;
    }
  }

  void SkipLeadingWhitespace() { SkipLeading<IsHTMLSpace<UChar>>(); }

  struct TakeLeadingWhitespaceResult {
    StringView string;
    WhitespaceMode whitespace_mode;
  };

  TakeLeadingWhitespaceResult TakeLeadingWhitespace() {
    DCHECK(!IsEmpty());
    const unsigned start = current_;
    WhitespaceMode whitespace_mode = WhitespaceMode::kNewlineThenWhitespace;

    // First, check the first character to identify whether the string looks
    // common (i.e. "\n<space>*").
    const UChar first = characters_[current_];
    if (!IsHTMLSpace(first)) {
      return {StringView(characters_, start, 0),
              WhitespaceMode::kNotAllWhitespace};
    }
    if (first != '\n') {
      whitespace_mode = WhitespaceMode::kAllWhitespace;
    }

    // Then, check the rest.
    ++current_;
    for (; current_ != end_; ++current_) {
      const UChar ch = characters_[current_];
      if (ch == ' ') [[likely]] {
        continue;
      } else if (IsHTMLSpecialWhitespace(ch)) {
        whitespace_mode = WhitespaceMode::kAllWhitespace;
      } else {
        break;
      }
    }

    return {StringView(characters_, start, current_ - start), whitespace_mode};
  }

  void SkipLeadingNonWhitespace() { SkipLeading<IsNotHTMLSpace<UChar>>(); }

  void SkipRemaining() { current_ = end_; }

  StringView TakeRemaining() {
    DCHECK(!IsEmpty());
    unsigned start = current_;
    current_ = end_;
    return StringView(characters_, start, end_ - start);
  }

  void GiveRemainingTo(StringBuilder& recipient) {
    WTF::VisitCharacters(characters_, [&](auto chars) {
      recipient.Append(chars.data() + current_, end_ - current_);
    });
    current_ = end_;
  }

  struct TakeRemainingWhitespaceResult {
    String string;
    WhitespaceMode whitespace_mode;
  };

  TakeRemainingWhitespaceResult TakeRemainingWhitespace() {
    DCHECK(!IsEmpty());
    const unsigned start = current_;
    current_ = end_;  // One way or another, we're taking everything!

    WhitespaceMode whitespace_mode = WhitespaceMode::kNewlineThenWhitespace;
    unsigned length = 0;
    for (unsigned i = start; i < end_; ++i) {
      const UChar ch = characters_[i];
      if (length == 0) {
        if (ch == '\n') {
          ++length;
          continue;
        }
        // Otherwise, it's a random whitespace string. Drop the mode.
        whitespace_mode = WhitespaceMode::kAllWhitespace;
      }

      if (ch == ' ') {
        ++length;
      } else if (IsHTMLSpecialWhitespace<UChar>(ch)) {
        whitespace_mode = WhitespaceMode::kAllWhitespace;
        ++length;
      }
    }
    // Returning the null string when there aren't any whitespace
    // characters is slightly cleaner semantically because we don't want
    // to insert a text node (as opposed to inserting an empty text node).
    if (!length) {
      return {String(), WhitespaceMode::kNotAllWhitespace};
    }
    if (length == start - end_) {  // It's all whitespace.
      return {String(characters_.Substring(start, start - end_)),
              whitespace_mode};
    }

    // All HTML spaces are ASCII.
    StringBuffer<LChar> result(length);
    unsigned j = 0;
    for (unsigned i = start; i < end_; ++i) {
      UChar c = characters_[i];
      if (c == ' ' || IsHTMLSpecialWhitespace(c)) {
        result[j++] = static_cast<LChar>(c);
      }
    }
    DCHECK_EQ(j, length);
    return {String::Adopt(result), whitespace_mode};
  }

 private:
  template <bool characterPredicate(UChar)>
  void SkipLeading() {
    DCHECK(!IsEmpty());
    while (characterPredicate(characters_[current_])) {
      if (++current_ == end_)
        return;
    }
  }

  String characters_;
  unsigned current_;
  unsigned end_;
};

HTMLTreeBuilder::HTMLTreeBuilder(HTMLDocumentParser* parser,
                                 Document& document,
                                 ParserContentPolicy parser_content_policy,
                                 const HTMLParserOptions& options,
                                 bool include_shadow_roots,
                                 DocumentFragment* for_fragment,
                                 Element* fragment_context_element)
    : tree_(parser->ReentryPermit(),
            document,
            parser_content_policy,
            for_fragment,
            fragment_context_element),
      insertion_mode_(kInitialMode),
      original_insertion_mode_(kInitialMode),
      should_skip_leading_newline_(false),
      include_shadow_roots_(include_shadow_roots),
      frameset_ok_(true),
      parser_(parser),
      script_to_process_start_position_(UninitializedPositionValue1()),
      options_(options) {}
HTMLTreeBuilder::HTMLTreeBuilder(HTMLDocumentParser* parser,
                                 Document& document,
                                 ParserContentPolicy parser_content_policy,
                                 const HTMLParserOptions& options,
                                 bool include_shadow_roots)
    : HTMLTreeBuilder(parser,
                      document,
                      parser_content_policy,
                      options,
                      include_shadow_roots,
                      nullptr,
                      nullptr) {}
HTMLTreeBuilder::HTMLTreeBuilder(HTMLDocumentParser* parser,
                                 DocumentFragment* fragment,
                                 Element* context_element,
                                 ParserContentPolicy parser_content_policy,
                                 const HTMLParserOptions& options,
                                 bool include_shadow_roots)
    : HTMLTreeBuilder(parser,
                      fragment->GetDocument(),
                      parser_content_policy,
                      options,
                      include_shadow_roots,
                      fragment,
                      context_element) {
  DCHECK(IsMainThread());
  fragment_context_.Init(fragment, context_element);

  // Steps 4.2-4.6 of the HTML5 Fragment Case parsing algorithm:
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#fragment-case
  // For efficiency, we skip step 4.2 ("Let root be a new html element with no
  // attributes") and instead use the DocumentFragment as a root node.
  tree_.OpenElements()->PushRootNode(MakeGarbageCollected<HTMLStackItem>(
      fragment, HTMLStackItem::kItemForDocumentFragmentNode));

  if (IsA<HTMLTemplateElement>(*context_element))
    template_insertion_modes_.push_back(kTemplateContentsMode);

  ResetInsertionModeAppropriately();
}

HTMLTreeBuilder::~HTMLTreeBuilder() = default;

void HTMLTreeBuilder::FragmentParsingContext::Init(DocumentFragment* fragment,
                                                   Element* context_element) {
  DCHECK(fragment);
  DCHECK(!fragment->HasChildren());
  fragment_ = fragment;
  context_element_stack_item_ = MakeGarbageCollected<HTMLStackItem>(
      context_element, HTMLStackItem::kItemForContextElement);
}

void HTMLTreeBuilder::FragmentParsingContext::Trace(Visitor* visitor) const {
  visitor->Trace(fragment_);
  visitor->Trace(context_element_stack_item_);
}

void HTMLTreeBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(fragment_context_);
  visitor->Trace(tree_);
  visitor->Trace(parser_);
  visitor->Trace(script_to_process_);
}

void HTMLTreeBuilder::Detach() {
#if DCHECK_IS_ON()
  // This call makes little sense in fragment mode, but for consistency
  // DocumentParser expects Detach() to always be called before it's destroyed.
  is_attached_ = false;
#endif
  // HTMLConstructionSite might be on the callstack when Detach() is called
  // otherwise we'd just call tree_.Clear() here instead.
  tree_.Detach();
}

Element* HTMLTreeBuilder::TakeScriptToProcess(
    TextPosition& script_start_position) {
  DCHECK(script_to_process_);
  DCHECK(!tree_.HasPendingTasks());
  // Unpause ourselves, callers may pause us again when processing the script.
  // The HTML5 spec is written as though scripts are executed inside the tree
  // builder.  We pause the parser to exit the tree builder, and then resume
  // before running scripts.
  script_start_position = script_to_process_start_position_;
  script_to_process_start_position_ = UninitializedPositionValue1();
  return script_to_process_.Release();
}

void HTMLTreeBuilder::ConstructTree(AtomicHTMLToken* token) {
  RUNTIME_CALL_TIMER_SCOPE(parser_->GetDocument()->GetAgent().isolate(),
                           RuntimeCallStats::CounterId::kConstructTree);
  if (ShouldProcessTokenInForeignContent(token))
    ProcessTokenInForeignContent(token);
  else
    ProcessToken(token);

  if (parser_->IsDetached())
    return;

  bool in_foreign_content = false;
  if (!tree_.IsEmpty()) {
    HTMLStackItem* adjusted_current_node = AdjustedCurrentStackItem();
    in_foreign_content =
        !adjusted_current_node->IsInHTMLNamespace() &&
        !HTMLElementStack::IsHTMLIntegrationPoint(adjusted_current_node) &&
        !HTMLElementStack::IsMathMLTextIntegrationPoint(adjusted_current_node);
  }

  parser_->tokenizer().SetForceNullCharacterReplacement(
      GetInsertionMode() == kTextMode || in_foreign_content);
  parser_->tokenizer().SetShouldAllowCDATA(in_foreign_content);
  if (RuntimeEnabledFeatures::DOMPartsAPIEnabled()) {
    parser_->tokenizer().SetShouldAllowDOMParts(tree_.InParsePartsScope());
  }

  tree_.ExecuteQueuedTasks();
  // We might be detached now.
}

void HTMLTreeBuilder::ProcessToken(AtomicHTMLToken* token) {
  if (token->GetType() == HTMLToken::kCharacter) {
    ProcessCharacter(token);
    return;
  }

  // Any non-character token needs to cause us to flush any pending text
  // immediately. NOTE: flush() can cause any queued tasks to execute, possibly
  // re-entering the parser.
  tree_.Flush();
  should_skip_leading_newline_ = false;

  switch (token->GetType()) {
    case HTMLToken::kUninitialized:
    case HTMLToken::kCharacter:
      NOTREACHED_IN_MIGRATION();
      break;
    case HTMLToken::DOCTYPE:
      ProcessDoctypeToken(token);
      break;
    case HTMLToken::kStartTag:
      ProcessStartTag(token);
      break;
    case HTMLToken::kEndTag:
      ProcessEndTag(token);
      break;
    case HTMLToken::kComment:
      ProcessComment(token);
      break;
    case HTMLToken::kEndOfFile:
      ProcessEndOfFile(token);
      break;
    case HTMLToken::kDOMPart:
      ProcessDOMPart(token);
      break;
  }
}

void HTMLTreeBuilder::ProcessDoctypeToken(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::DOCTYPE);
  if (GetInsertionMode() == kInitialMode) {
    tree_.InsertDoctype(token);
    SetInsertionMode(kBeforeHTMLMode);
    return;
  }
  if (GetInsertionMode() == kInTableTextMode) {
    DefaultForInTableText();
    ProcessDoctypeToken(token);
    return;
  }
  ParseError(token);
}

void HTMLTreeBuilder::ProcessFakeStartTag(HTMLTag tag,
                                          const Vector<Attribute>& attributes) {
  // FIXME: We'll need a fancier conversion than just "localName" for SVG/MathML
  // tags.
  AtomicHTMLToken fake_token(HTMLToken::kStartTag, tag, attributes);
  ProcessStartTag(&fake_token);
}

void HTMLTreeBuilder::ProcessFakeEndTag(HTMLTag tag) {
  AtomicHTMLToken fake_token(HTMLToken::kEndTag, tag);
  ProcessEndTag(&fake_token);
}

void HTMLTreeBuilder::ProcessFakeEndTag(const HTMLStackItem& stack_item) {
  AtomicHTMLToken fake_token(HTMLToken::kEndTag, stack_item.GetTokenName());
  ProcessEndTag(&fake_token);
}

void HTMLTreeBuilder::ProcessFakePEndTagIfPInButtonScope() {
  if (!tree_.OpenElements()->InButtonScope(HTMLTag::kP))
    return;
  AtomicHTMLToken end_p(HTMLToken::kEndTag, HTMLTag::kP);
  ProcessEndTag(&end_p);
}

namespace {

bool IsLi(const HTMLStackItem* item) {
  return item->MatchesHTMLTag(HTMLTag::kLi);
}

bool IsDdOrDt(const HTMLStackItem* item) {
  return item->MatchesHTMLTag(HTMLTag::kDd) ||
         item->MatchesHTMLTag(HTMLTag::kDt);
}

}  // namespace

template <bool shouldClose(const HTMLStackItem*)>
void HTMLTreeBuilder::ProcessCloseWhenNestedTag(AtomicHTMLToken* token) {
  frameset_ok_ = false;
  HTMLStackItem* item = tree_.OpenElements()->TopStackItem();
  while (true) {
    if (shouldClose(item)) {
      DCHECK(item->IsElementNode());
      ProcessFakeEndTag(*item);
      break;
    }
    if (item->IsSpecialNode() && !item->MatchesHTMLTag(HTMLTag::kAddress) &&
        !item->MatchesHTMLTag(HTMLTag::kDiv) &&
        !item->MatchesHTMLTag(HTMLTag::kP))
      break;
    item = item->NextItemInStack();
  }
  ProcessFakePEndTagIfPInButtonScope();
  tree_.InsertHTMLElement(token);
}

namespace {
typedef HashMap<AtomicString, QualifiedName> PrefixedNameToQualifiedNameMap;

template <typename TableQualifiedName>
void MapLoweredLocalNameToName(PrefixedNameToQualifiedNameMap* map,
                               const TableQualifiedName* const* names,
                               size_t length) {
  for (size_t i = 0; i < length; ++i) {
    const QualifiedName& name = *names[i];
    const AtomicString& local_name = name.LocalName();
    AtomicString lowered_local_name = local_name.LowerASCII();
    if (lowered_local_name != local_name)
      map->insert(lowered_local_name, name);
  }
}

void AddManualLocalName(PrefixedNameToQualifiedNameMap* map, const char* name) {
  const QualifiedName item{AtomicString(name)};
  const blink::QualifiedName* const names = &item;
  MapLoweredLocalNameToName<QualifiedName>(map, &names, 1);
}

// "Any other start tag" bullet in
// https://html.spec.whatwg.org/C/#parsing-main-inforeign
void AdjustSVGTagNameCase(AtomicHTMLToken* token) {
  static PrefixedNameToQualifiedNameMap* case_map = nullptr;
  if (!case_map) {
    case_map = new PrefixedNameToQualifiedNameMap;
    std::unique_ptr<const SVGQualifiedName*[]> svg_tags = svg_names::GetTags();
    MapLoweredLocalNameToName(case_map, svg_tags.get(), svg_names::kTagsCount);
    // These tags aren't implemented by Chromium, so they don't exist in
    // svg_tag_names.json5.
    AddManualLocalName(case_map, "altGlyph");
    AddManualLocalName(case_map, "altGlyphDef");
    AddManualLocalName(case_map, "altGlyphItem");
    AddManualLocalName(case_map, "glyphRef");
  }

  const auto it = case_map->find(token->GetName());
  if (it != case_map->end()) {
    DCHECK(!it->value.LocalName().IsNull());
    token->SetTokenName(HTMLTokenName::FromLocalName(it->value.LocalName()));
  }
}

template <std::unique_ptr<const QualifiedName* []> getAttrs(),
          unsigned length,
          bool forSVG>
void AdjustAttributes(AtomicHTMLToken* token) {
  static PrefixedNameToQualifiedNameMap* case_map = nullptr;
  if (!case_map) {
    case_map = new PrefixedNameToQualifiedNameMap;
    std::unique_ptr<const QualifiedName*[]> attrs = getAttrs();
    MapLoweredLocalNameToName(case_map, attrs.get(), length);
    if (forSVG) {
      // This attribute isn't implemented by Chromium, so it doesn't exist in
      // svg_attribute_names.json5.
      AddManualLocalName(case_map, "viewTarget");
    }
  }

  for (auto& token_attribute : token->Attributes()) {
    const auto it = case_map->find(token_attribute.LocalName());
    if (it != case_map->end()) {
      DCHECK(!it->value.LocalName().IsNull());
      token_attribute.ParserSetName(it->value);
    }
  }
}

// https://html.spec.whatwg.org/C/#adjust-svg-attributes
void AdjustSVGAttributes(AtomicHTMLToken* token) {
  AdjustAttributes<svg_names::GetAttrs, svg_names::kAttrsCount,
                   /*forSVG*/ true>(token);
}

// https://html.spec.whatwg.org/C/#adjust-mathml-attributes
void AdjustMathMLAttributes(AtomicHTMLToken* token) {
  AdjustAttributes<mathml_names::GetAttrs, mathml_names::kAttrsCount,
                   /*forSVG*/ false>(token);
}

void AddNamesWithPrefix(PrefixedNameToQualifiedNameMap* map,
                        const AtomicString& prefix,
                        const QualifiedName* const* names,
                        size_t length) {
  for (size_t i = 0; i < length; ++i) {
    const QualifiedName* name = names[i];
    const AtomicString& local_name = name->LocalName();
    AtomicString prefix_colon_local_name = prefix + ':' + local_name;
    QualifiedName name_with_prefix(prefix, local_name, name->NamespaceURI());
    map->insert(prefix_colon_local_name, name_with_prefix);
  }
}

void AdjustForeignAttributes(AtomicHTMLToken* token) {
  static PrefixedNameToQualifiedNameMap* map = nullptr;
  if (!map) {
    map = new PrefixedNameToQualifiedNameMap;

    std::unique_ptr<const QualifiedName*[]> attrs = xlink_names::GetAttrs();
    AddNamesWithPrefix(map, g_xlink_atom, attrs.get(),
                       xlink_names::kAttrsCount);

    std::unique_ptr<const QualifiedName*[]> xml_attrs = xml_names::GetAttrs();
    AddNamesWithPrefix(map, g_xml_atom, xml_attrs.get(),
                       xml_names::kAttrsCount);

    map->insert(WTF::g_xmlns_atom, xmlns_names::kXmlnsAttr);
    map->insert(
        AtomicString("xmlns:xlink"),
        QualifiedName(g_xmlns_atom, g_xlink_atom, xmlns_names::kNamespaceURI));
  }

  for (unsigned i = 0; i < token->Attributes().size(); ++i) {
    Attribute& token_attribute = token->Attributes().at(i);
    const auto it = map->find(token_attribute.LocalName());
    if (it != map->end()) {
      DCHECK(!it->value.LocalName().IsNull());
      token_attribute.ParserSetName(it->value);
    }
  }
}

}  // namespace

void HTMLTreeBuilder::ProcessStartTagForInBody(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kStartTag);
  switch (token->GetHTMLTag()) {
    case HTMLTag::kHTML:
      ProcessHtmlStartTagForInBody(token);
      break;
    case HTMLTag::kBase:
    case HTMLTag::kBasefont:
    case HTMLTag::kBgsound:
    case HTMLTag::kCommand:
    case HTMLTag::kLink:
    case HTMLTag::kMeta:
    case HTMLTag::kNoframes:
    case HTMLTag::kScript:
    case HTMLTag::kStyle:
    case HTMLTag::kTitle:
    case HTMLTag::kTemplate: {
      bool did_process = ProcessStartTagForInHead(token);
      DCHECK(did_process);
      break;
    }
    case HTMLTag::kBody:
      ParseError(token);
      if (!tree_.OpenElements()->SecondElementIsHTMLBodyElement() ||
          tree_.OpenElements()->HasOnlyOneElement() ||
          tree_.OpenElements()->HasTemplateInHTMLScope()) {
        DCHECK(IsParsingFragmentOrTemplateContents());
        break;
      }
      frameset_ok_ = false;
      tree_.InsertHTMLBodyStartTagInBody(token);
      break;
    case HTMLTag::kFrameset:
      ParseError(token);
      if (!tree_.OpenElements()->SecondElementIsHTMLBodyElement() ||
          tree_.OpenElements()->HasOnlyOneElement()) {
        DCHECK(IsParsingFragmentOrTemplateContents());
        break;
      }
      if (!frameset_ok_)
        break;
      tree_.OpenElements()->BodyElement()->remove(ASSERT_NO_EXCEPTION);
      tree_.OpenElements()->PopUntil(tree_.OpenElements()->BodyElement());
      tree_.OpenElements()->PopHTMLBodyElement();

      // Note: in the fragment case the root is a DocumentFragment instead of
      // a proper html element which is a quirk in Blink's implementation.
      DCHECK(!IsParsingTemplateContents());
      DCHECK(!IsParsingFragment() ||
             To<DocumentFragment>(tree_.OpenElements()->TopNode()));
      DCHECK(IsParsingFragment() || tree_.OpenElements()->Top() ==
                                        tree_.OpenElements()->HtmlElement());
      tree_.InsertHTMLElement(token);
      SetInsertionMode(kInFramesetMode);
      break;
    case HTMLTag::kAddress:
    case HTMLTag::kArticle:
    case HTMLTag::kAside:
    case HTMLTag::kBlockquote:
    case HTMLTag::kCenter:
    case HTMLTag::kDetails:
    case HTMLTag::kDialog:
    case HTMLTag::kDir:
    case HTMLTag::kDiv:
    case HTMLTag::kDl:
    case HTMLTag::kFieldset:
    case HTMLTag::kFigcaption:
    case HTMLTag::kFigure:
    case HTMLTag::kFooter:
    case HTMLTag::kHeader:
    case HTMLTag::kHgroup:
    case HTMLTag::kMain:
    case HTMLTag::kMenu:
    case HTMLTag::kNav:
    case HTMLTag::kOl:
    case HTMLTag::kP:
    case HTMLTag::kSearch:
    case HTMLTag::kSection:
    case HTMLTag::kSummary:
    case HTMLTag::kUl:
      // https://html.spec.whatwg.org/multipage/parsing.html#:~:text=A%20start%20tag%20whose%20tag%20name%20is%20one%20of%3A%20%22address%22%2C
      ProcessFakePEndTagIfPInButtonScope();
      tree_.InsertHTMLElement(token);
      break;
    case HTMLTag::kLi:
      ProcessCloseWhenNestedTag<IsLi>(token);
      break;
    case HTMLTag::kInput: {
      // Per spec https://html.spec.whatwg.org/C/#parsing-main-inbody,
      // section "A start tag whose tag name is "input""

      Attribute* type_attribute =
          token->GetAttributeItem(html_names::kTypeAttr);
      bool disable_frameset =
          !type_attribute ||
          !EqualIgnoringASCIICase(type_attribute->Value(), "hidden");

      tree_.ReconstructTheActiveFormattingElements();
      tree_.InsertSelfClosingHTMLElementDestroyingToken(token);

      if (disable_frameset)
        frameset_ok_ = false;
      break;
    }
    case HTMLTag::kButton:
      if (tree_.OpenElements()->InScope(HTMLTag::kButton)) {
        ParseError(token);
        ProcessFakeEndTag(HTMLTag::kButton);
        ProcessStartTag(token);  // FIXME: Could we just fall through here?
        break;
      }
      tree_.ReconstructTheActiveFormattingElements();
      tree_.InsertHTMLElement(token);
      frameset_ok_ = false;
      break;
    case NUMBERED_HEADER_CASES:
      ProcessFakePEndTagIfPInButtonScope();
      if (tree_.CurrentStackItem()->IsNumberedHeaderElement()) {
        ParseError(token);
        tree_.OpenElements()->Pop();
      }
      tree_.InsertHTMLElement(token);
      break;
    case HTMLTag::kListing:
    case HTMLTag::kPre:
      ProcessFakePEndTagIfPInButtonScope();
      tree_.InsertHTMLElement(token);
      should_skip_leading_newline_ = true;
      frameset_ok_ = false;
      break;
    case HTMLTag::kForm:
      if (tree_.IsFormElementPointerNonNull() && !IsParsingTemplateContents()) {
        ParseError(token);
        UseCounter::Count(tree_.CurrentNode()->GetDocument(),
                          WebFeature::kHTMLParseErrorNestedForm);
        break;
      }
      ProcessFakePEndTagIfPInButtonScope();
      tree_.InsertHTMLFormElement(token);
      break;
    case HTMLTag::kDd:
    case HTMLTag::kDt:
      ProcessCloseWhenNestedTag<IsDdOrDt>(token);
      break;
    case HTMLTag::kPlaintext:
      ProcessFakePEndTagIfPInButtonScope();
      tree_.InsertHTMLElement(token);
      parser_->tokenizer().SetState(HTMLTokenizer::kPLAINTEXTState);
      break;
    case HTMLTag::kA: {
      Element* active_a_tag =
          tree_.ActiveFormattingElements()->ClosestElementInScopeWithName(
              token->GetName());
      if (active_a_tag) {
        ParseError(token);
        ProcessFakeEndTag(HTMLTag::kA);
        tree_.ActiveFormattingElements()->Remove(active_a_tag);
        if (tree_.OpenElements()->Contains(active_a_tag))
          tree_.OpenElements()->Remove(active_a_tag);
      }
      tree_.ReconstructTheActiveFormattingElements();
      tree_.InsertFormattingElement(token);
      break;
    }
    case HTMLTag::kB:
    case HTMLTag::kBig:
    case HTMLTag::kCode:
    case HTMLTag::kEm:
    case HTMLTag::kFont:
    case HTMLTag::kI:
    case HTMLTag::kS:
    case HTMLTag::kSmall:
    case HTMLTag::kStrike:
    case HTMLTag::kStrong:
    case HTMLTag::kTt:
    case HTMLTag::kU:
      tree_.ReconstructTheActiveFormattingElements();
      tree_.InsertFormattingElement(token);
      break;
    case HTMLTag::kNobr:
      tree_.ReconstructTheActiveFormattingElements();
      if (tree_.OpenElements()->InScope(HTMLTag::kNobr)) {
        ParseError(token);
        ProcessFakeEndTag(HTMLTag::kNobr);
        tree_.ReconstructTheActiveFormattingElements();
      }
      tree_.InsertFormattingElement(token);
      break;
    case HTMLTag::kApplet:
    case HTMLTag::kObject:
      if (!PluginContentIsAllowed(tree_.GetParserContentPolicy()))
        break;
      [[fallthrough]];
    case HTMLTag::kMarquee:
      tree_.ReconstructTheActiveFormattingElements();
      tree_.InsertHTMLElement(token);
      tree_.ActiveFormattingElements()->AppendMarker();
      frameset_ok_ = false;
      break;
    case HTMLTag::kTable:
      if (!tree_.InQuirksMode() &&
          tree_.OpenElements()->InButtonScope(HTMLTag::kP))
        ProcessFakeEndTag(HTMLTag::kP);
      tree_.InsertHTMLElement(token);
      frameset_ok_ = false;
      SetInsertionMode(kInTableMode);
      break;
    case HTMLTag::kImage:
      ParseError(token);
      // Apparently we're not supposed to ask.
      token->SetTokenName(HTMLTokenName(HTMLTag::kImg));
      [[fallthrough]];
    case HTMLTag::kArea:  // Includes kImgTag, thus the
    case HTMLTag::kBr:    // fallthrough.
    case HTMLTag::kEmbed:
    case HTMLTag::kImg:
    case HTMLTag::kKeygen:
    case HTMLTag::kWbr:
      if (token->GetHTMLTag() == HTMLTag::kEmbed &&
          !PluginContentIsAllowed(tree_.GetParserContentPolicy())) {
        break;
      }
      tree_.ReconstructTheActiveFormattingElements();
      tree_.InsertSelfClosingHTMLElementDestroyingToken(token);
      frameset_ok_ = false;
      break;
    case HTMLTag::kParam:
    case HTMLTag::kSource:
    case HTMLTag::kTrack:
      tree_.InsertSelfClosingHTMLElementDestroyingToken(token);
      break;
    case HTMLTag::kHr:
      ProcessFakePEndTagIfPInButtonScope();
      tree_.InsertSelfClosingHTMLElementDestroyingToken(token);
      frameset_ok_ = false;
      break;
    case HTMLTag::kTextarea:
      tree_.InsertHTMLElement(token);
      should_skip_leading_newline_ = true;
      parser_->tokenizer().SetState(HTMLTokenizer::kRCDATAState);
      original_insertion_mode_ = insertion_mode_;
      frameset_ok_ = false;
      SetInsertionMode(kTextMode);
      break;
    case HTMLTag::kXmp:
      ProcessFakePEndTagIfPInButtonScope();
      tree_.ReconstructTheActiveFormattingElements();
      frameset_ok_ = false;
      ProcessGenericRawTextStartTag(token);
      break;
    case HTMLTag::kIFrame:
      frameset_ok_ = false;
      ProcessGenericRawTextStartTag(token);
      break;
    case HTMLTag::kNoembed:
      ProcessGenericRawTextStartTag(token);
      break;
    case HTMLTag::kNoscript:
      if (options_.scripting_flag) {
        ProcessGenericRawTextStartTag(token);
      } else {
        tree_.ReconstructTheActiveFormattingElements();
        tree_.InsertHTMLElement(token);
      }
      break;
    case HTMLTag::kSelect:
      if (RuntimeEnabledFeatures::SelectParserRelaxationEnabled() &&
          tree_.OpenElements()->InScope(HTMLTag::kSelect)) {
        tree_.OpenElements()->TopNode()->AddConsoleMessage(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "A <select> tag was parsed within another <select> tag and was converted into </select><select>. Please add the missing </select> end tag.");
        // Don't allow nested <select>s. This is the exact same logic as
        // <button>s.
        ParseError(token);
        ProcessFakeEndTag(HTMLTag::kSelect);
        ProcessStartTag(token);
        break;
      }
      tree_.ReconstructTheActiveFormattingElements();
      tree_.InsertHTMLElement(token);
      frameset_ok_ = false;
      // When SelectParserRelaxation is enabled, we don't want to enter
      // InSelectMode or InSelectInTableMode.
      if (!RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
        if (GetInsertionMode() == kInTableMode ||
            GetInsertionMode() == kInCaptionMode ||
            GetInsertionMode() == kInColumnGroupMode ||
            GetInsertionMode() == kInTableBodyMode ||
            GetInsertionMode() == kInRowMode ||
            GetInsertionMode() == kInCellMode) {
          SetInsertionMode(kInSelectInTableMode);
        } else {
          SetInsertionMode(kInSelectMode);
        }
      }
      break;
    case HTMLTag::kOptgroup:
    case HTMLTag::kOption:
      if (tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kOption)) {
        AtomicHTMLToken end_option(HTMLToken::kEndTag, HTMLTag::kOption);
        ProcessEndTag(&end_option);
      }
      tree_.ReconstructTheActiveFormattingElements();
      tree_.InsertHTMLElement(token);
      break;
    case HTMLTag::kRb:
    case HTMLTag::kRTC:
      if (tree_.OpenElements()->InScope(HTMLTag::kRuby)) {
        tree_.GenerateImpliedEndTags();
        if (!tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kRuby))
          ParseError(token);
      }
      tree_.InsertHTMLElement(token);
      break;
    case HTMLTag::kRt:
    case HTMLTag::kRp:
      if (tree_.OpenElements()->InScope(HTMLTag::kRuby)) {
        tree_.GenerateImpliedEndTagsWithExclusion(HTMLTokenName(HTMLTag::kRTC));
        if (!tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kRuby) &&
            !tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kRTC))
          ParseError(token);
      }
      tree_.InsertHTMLElement(token);
      break;
    case HTMLTag::kCaption:
    case HTMLTag::kCol:
    case HTMLTag::kColgroup:
    case HTMLTag::kFrame:
    case HTMLTag::kHead:
    case HTMLTag::kTbody:
    case HTMLTag::kTfoot:
    case HTMLTag::kThead:
    case HTMLTag::kTh:
    case HTMLTag::kTd:
    case HTMLTag::kTr:
      ParseError(token);
      break;
    case HTMLTag::kPermissionOrUnknown:
      if (RuntimeEnabledFeatures::PermissionElementEnabled(
              tree_.OwnerDocumentForCurrentNode().GetExecutionContext())) {
        tree_.ReconstructTheActiveFormattingElements();
        tree_.InsertSelfClosingHTMLElementDestroyingToken(token);
        frameset_ok_ = false;
        break;
      }
      [[fallthrough]];
    default:
      if (token->GetName() == mathml_names::kMathTag.LocalName()) {
        tree_.ReconstructTheActiveFormattingElements();
        AdjustMathMLAttributes(token);
        AdjustForeignAttributes(token);
        tree_.InsertForeignElement(token, mathml_names::kNamespaceURI);
      } else if (token->GetName() == svg_names::kSVGTag.LocalName()) {
        tree_.ReconstructTheActiveFormattingElements();
        AdjustSVGAttributes(token);
        AdjustForeignAttributes(token);
        tree_.InsertForeignElement(token, svg_names::kNamespaceURI);
      } else {
        tree_.ReconstructTheActiveFormattingElements();
        // Flush before creating custom elements. NOTE: Flush() can cause any
        // queued tasks to execute, possibly re-entering the parser.
        tree_.Flush();
        tree_.InsertHTMLElement(token);
      }
      break;
  }
}

namespace {
String DeclarativeShadowRootModeFromToken(AtomicHTMLToken* token,
                                          const Document& document,
                                          bool include_shadow_roots) {
  Attribute* mode_attribute =
      token->GetAttributeItem(html_names::kShadowrootmodeAttr);
  if (!mode_attribute) {
    return String();
  }
  if (!include_shadow_roots) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "Found declarative shadowrootmode attribute on a template, but "
        "declarative Shadow DOM is not being parsed. Use setHTMLUnsafe() "
        "or parseHTMLUnsafe() instead."));
    return String();
  }
  return mode_attribute->Value();
}
}  // namespace

void HTMLTreeBuilder::ProcessTemplateStartTag(AtomicHTMLToken* token) {
  tree_.ActiveFormattingElements()->AppendMarker();
  tree_.InsertHTMLTemplateElement(
      token,
      DeclarativeShadowRootModeFromToken(
          token, tree_.OwnerDocumentForCurrentNode(), include_shadow_roots_));
  frameset_ok_ = false;
  template_insertion_modes_.push_back(kTemplateContentsMode);
  SetInsertionMode(kTemplateContentsMode);
}

bool HTMLTreeBuilder::ProcessTemplateEndTag(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetName(), html_names::kTemplateTag.LocalName());
  if (!tree_.OpenElements()->HasTemplateInHTMLScope()) {
    DCHECK(template_insertion_modes_.empty() ||
           (template_insertion_modes_.size() == 1 &&
            IsA<HTMLTemplateElement>(fragment_context_.ContextElement())));
    ParseError(token);
    return false;
  }
  tree_.GenerateImpliedEndTags();
  if (!tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kTemplate))
    ParseError(token);
  tree_.OpenElements()->PopUntil(HTMLTag::kTemplate);
  HTMLStackItem* template_stack_item = tree_.OpenElements()->TopStackItem();
  tree_.OpenElements()->Pop();
  tree_.ActiveFormattingElements()->ClearToLastMarker();
  template_insertion_modes_.pop_back();
  ResetInsertionModeAppropriately();
  if (template_stack_item) {
    DCHECK(template_stack_item->IsElementNode());
    HTMLTemplateElement* template_element =
        DynamicTo<HTMLTemplateElement>(template_stack_item->GetElement());
    if (DocumentFragment* template_content = template_element->getContent()) {
      tree_.FinishedTemplateElement(template_content);
    }
  }
  return true;
}

bool HTMLTreeBuilder::ProcessEndOfFileForInTemplateContents(
    AtomicHTMLToken* token) {
  AtomicHTMLToken end_template(HTMLToken::kEndTag, HTMLTag::kTemplate);
  if (!ProcessTemplateEndTag(&end_template))
    return false;

  ProcessEndOfFile(token);
  return true;
}

bool HTMLTreeBuilder::ProcessColgroupEndTagForInColumnGroup() {
  if (tree_.CurrentIsRootNode() ||
      IsA<HTMLTemplateElement>(*tree_.CurrentNode())) {
    DCHECK(IsParsingFragmentOrTemplateContents());
    // FIXME: parse error
    return false;
  }
  tree_.OpenElements()->Pop();
  SetInsertionMode(kInTableMode);
  return true;
}

// http://www.whatwg.org/specs/web-apps/current-work/#adjusted-current-node
HTMLStackItem* HTMLTreeBuilder::AdjustedCurrentStackItem() const {
  DCHECK(!tree_.IsEmpty());
  if (IsParsingFragment() && tree_.OpenElements()->HasOnlyOneElement())
    return fragment_context_.ContextElementStackItem();

  return tree_.CurrentStackItem();
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#close-the-cell
void HTMLTreeBuilder::CloseTheCell() {
  DCHECK_EQ(GetInsertionMode(), kInCellMode);
  if (tree_.OpenElements()->InTableScope(HTMLTag::kTd)) {
    DCHECK(!tree_.OpenElements()->InTableScope(HTMLTag::kTh));
    ProcessFakeEndTag(HTMLTag::kTd);
    return;
  }
  DCHECK(tree_.OpenElements()->InTableScope(HTMLTag::kTh));
  ProcessFakeEndTag(HTMLTag::kTh);
  DCHECK_EQ(GetInsertionMode(), kInRowMode);
}

void HTMLTreeBuilder::ProcessStartTagForInTable(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kStartTag);
  switch (token->GetHTMLTag()) {
    case HTMLTag::kCaption:
      tree_.OpenElements()->PopUntilTableScopeMarker();
      tree_.ActiveFormattingElements()->AppendMarker();
      tree_.InsertHTMLElement(token);
      SetInsertionMode(kInCaptionMode);
      return;
    case HTMLTag::kColgroup:
      tree_.OpenElements()->PopUntilTableScopeMarker();
      tree_.InsertHTMLElement(token);
      SetInsertionMode(kInColumnGroupMode);
      return;
    case HTMLTag::kCol:
      ProcessFakeStartTag(HTMLTag::kColgroup);
      DCHECK(kInColumnGroupMode);
      ProcessStartTag(token);
      return;
    case HTMLTag::kTbody:
    case HTMLTag::kTfoot:
    case HTMLTag::kThead:
      tree_.OpenElements()->PopUntilTableScopeMarker();
      tree_.InsertHTMLElement(token);
      SetInsertionMode(kInTableBodyMode);
      return;
    case HTMLTag::kTd:
    case HTMLTag::kTh:
    case HTMLTag::kTr:
      ProcessFakeStartTag(HTMLTag::kTbody);
      DCHECK_EQ(GetInsertionMode(), kInTableBodyMode);
      ProcessStartTag(token);
      return;
    case HTMLTag::kTable:
      ParseError(token);
      if (!ProcessTableEndTagForInTable()) {
        DCHECK(IsParsingFragmentOrTemplateContents());
        return;
      }
      ProcessStartTag(token);
      return;
    case HTMLTag::kStyle:
    case HTMLTag::kScript:
      ProcessStartTagForInHead(token);
      return;
    case HTMLTag::kInput: {
      Attribute* type_attribute =
          token->GetAttributeItem(html_names::kTypeAttr);
      if (type_attribute &&
          EqualIgnoringASCIICase(type_attribute->Value(), "hidden")) {
        ParseError(token);
        tree_.InsertSelfClosingHTMLElementDestroyingToken(token);
        return;
      }
      // break to hit "anything else" case.
      break;
    }
    case HTMLTag::kForm:
      ParseError(token);
      if (tree_.IsFormElementPointerNonNull() && !IsParsingTemplateContents())
        return;
      tree_.InsertHTMLFormElement(token, true);
      tree_.OpenElements()->Pop();
      return;
    case HTMLTag::kTemplate:
      ProcessTemplateStartTag(token);
      return;
    default:
      break;
  }
  ParseError(token);
  HTMLConstructionSite::RedirectToFosterParentGuard redirecter(tree_);
  ProcessStartTagForInBody(token);
}

void HTMLTreeBuilder::ProcessStartTag(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kStartTag);
  const HTMLTag tag = token->GetHTMLTag();
  switch (GetInsertionMode()) {
    case kInitialMode:
      DefaultForInitial();
      [[fallthrough]];
    case kBeforeHTMLMode:
      DCHECK_EQ(GetInsertionMode(), kBeforeHTMLMode);
      if (tag == HTMLTag::kHTML) {
        tree_.InsertHTMLHtmlStartTagBeforeHTML(token);
        SetInsertionMode(kBeforeHeadMode);
        return;
      }
      DefaultForBeforeHTML();
      [[fallthrough]];
    case kBeforeHeadMode:
      DCHECK_EQ(GetInsertionMode(), kBeforeHeadMode);
      if (tag == HTMLTag::kHTML) {
        ProcessHtmlStartTagForInBody(token);
        return;
      }
      if (tag == HTMLTag::kHead) {
        tree_.InsertHTMLHeadElement(token);
        SetInsertionMode(kInHeadMode);
        return;
      }
      DefaultForBeforeHead();
      [[fallthrough]];
    case kInHeadMode:
      DCHECK_EQ(GetInsertionMode(), kInHeadMode);
      if (ProcessStartTagForInHead(token))
        return;
      DefaultForInHead();
      [[fallthrough]];
    case kAfterHeadMode:
      DCHECK_EQ(GetInsertionMode(), kAfterHeadMode);
      switch (tag) {
        case HTMLTag::kHTML:
          ProcessHtmlStartTagForInBody(token);
          return;
        case HTMLTag::kBody:
          frameset_ok_ = false;
          tree_.InsertHTMLBodyElement(token);
          SetInsertionMode(kInBodyMode);
          return;
        case HTMLTag::kFrameset:
          tree_.InsertHTMLElement(token);
          SetInsertionMode(kInFramesetMode);
          return;
        case HTMLTag::kBase:
        case HTMLTag::kBasefont:
        case HTMLTag::kBgsound:
        case HTMLTag::kLink:
        case HTMLTag::kMeta:
        case HTMLTag::kNoframes:
        case HTMLTag::kScript:
        case HTMLTag::kStyle:
        case HTMLTag::kTemplate:
        case HTMLTag::kTitle:
          ParseError(token);
          DCHECK(tree_.Head());
          tree_.OpenElements()->PushHTMLHeadElement(tree_.HeadStackItem());
          ProcessStartTagForInHead(token);
          tree_.OpenElements()->RemoveHTMLHeadElement(tree_.Head());
          return;
        case HTMLTag::kHead:
          ParseError(token);
          return;
        default:
          break;
      }
      DefaultForAfterHead();
      [[fallthrough]];
    case kInBodyMode:
      DCHECK_EQ(GetInsertionMode(), kInBodyMode);
      ProcessStartTagForInBody(token);
      break;

    case kInTableMode:
      ProcessStartTagForInTable(token);
      break;
    case kInCaptionMode:
      switch (tag) {
        case CAPTION_COL_OR_COLGROUP_CASES:
        case TABLE_BODY_CONTEXT_CASES:
        case TABLE_CELL_CONTEXT_CASES:
        case HTMLTag::kTr:
          ParseError(token);
          if (!ProcessCaptionEndTagForInCaption()) {
            DCHECK(IsParsingFragment());
            return;
          }
          ProcessStartTag(token);
          return;
        default:
          break;
      }
      ProcessStartTagForInBody(token);
      break;
    case kInColumnGroupMode:
      switch (tag) {
        case HTMLTag::kHTML:
          ProcessHtmlStartTagForInBody(token);
          return;
        case HTMLTag::kCol:
          tree_.InsertSelfClosingHTMLElementDestroyingToken(token);
          return;
        case HTMLTag::kTemplate:
          ProcessTemplateStartTag(token);
          return;
        default:
          break;
      }
      if (!ProcessColgroupEndTagForInColumnGroup()) {
        DCHECK(IsParsingFragmentOrTemplateContents());
        return;
      }
      ProcessStartTag(token);
      break;
    case kInTableBodyMode:
      switch (tag) {
        case HTMLTag::kTr:
          // How is there ever anything to pop?
          tree_.OpenElements()->PopUntilTableBodyScopeMarker();
          tree_.InsertHTMLElement(token);
          SetInsertionMode(kInRowMode);
          return;
        case TABLE_CELL_CONTEXT_CASES:
          ParseError(token);
          ProcessFakeStartTag(HTMLTag::kTr);
          DCHECK_EQ(GetInsertionMode(), kInRowMode);
          ProcessStartTag(token);
          return;
        case CAPTION_COL_OR_COLGROUP_CASES:
        case TABLE_BODY_CONTEXT_CASES:
          // FIXME: This is slow.
          if (!tree_.OpenElements()->InTableScope(HTMLTag::kTbody) &&
              !tree_.OpenElements()->InTableScope(HTMLTag::kThead) &&
              !tree_.OpenElements()->InTableScope(HTMLTag::kTfoot)) {
            DCHECK(IsParsingFragmentOrTemplateContents());
            ParseError(token);
            return;
          }
          tree_.OpenElements()->PopUntilTableBodyScopeMarker();
          DCHECK(IsTableBodyContextTag(tree_.CurrentStackItem()->GetHTMLTag()));
          ProcessFakeEndTag(*tree_.CurrentStackItem());
          ProcessStartTag(token);
          return;
        default:
          break;
      }
      ProcessStartTagForInTable(token);
      break;
    case kInRowMode:
      switch (tag) {
        case TABLE_CELL_CONTEXT_CASES:
          tree_.OpenElements()->PopUntilTableRowScopeMarker();
          tree_.InsertHTMLElement(token);
          SetInsertionMode(kInCellMode);
          tree_.ActiveFormattingElements()->AppendMarker();
          return;
        case HTMLTag::kTr:
        case CAPTION_COL_OR_COLGROUP_CASES:
        case TABLE_BODY_CONTEXT_CASES:
          if (!ProcessTrEndTagForInRow()) {
            DCHECK(IsParsingFragmentOrTemplateContents());
            return;
          }
          DCHECK_EQ(GetInsertionMode(), kInTableBodyMode);
          ProcessStartTag(token);
          return;
        default:
          break;
      }
      ProcessStartTagForInTable(token);
      break;
    case kInCellMode:
      switch (tag) {
        case CAPTION_COL_OR_COLGROUP_CASES:
        case TABLE_CELL_CONTEXT_CASES:
        case HTMLTag::kTr:
        case TABLE_BODY_CONTEXT_CASES:
          // FIXME: This could be more efficient.
          if (!tree_.OpenElements()->InTableScope(HTMLTag::kTd) &&
              !tree_.OpenElements()->InTableScope(HTMLTag::kTh)) {
            DCHECK(IsParsingFragment());
            ParseError(token);
            return;
          }
          CloseTheCell();
          ProcessStartTag(token);
          return;
        default:
          break;
      }
      ProcessStartTagForInBody(token);
      break;
    case kAfterBodyMode:
    case kAfterAfterBodyMode:
      if (tag == HTMLTag::kHTML) {
        ProcessHtmlStartTagForInBody(token);
        return;
      }
      SetInsertionMode(kInBodyMode);
      ProcessStartTag(token);
      break;
    case kInHeadNoscriptMode:
      switch (tag) {
        case HTMLTag::kHTML:
          ProcessHtmlStartTagForInBody(token);
          return;
        case HTMLTag::kBasefont:
        case HTMLTag::kBgsound:
        case HTMLTag::kLink:
        case HTMLTag::kMeta:
        case HTMLTag::kNoframes:
        case HTMLTag::kStyle: {
          bool did_process = ProcessStartTagForInHead(token);
          DCHECK(did_process);
          return;
        }
        case HTMLTag::kNoscript:
          ParseError(token);
          return;
        default:
          break;
      }
      DefaultForInHeadNoscript();
      ProcessToken(token);
      break;
    case kInFramesetMode:
      switch (tag) {
        case HTMLTag::kHTML:
          ProcessHtmlStartTagForInBody(token);
          return;
        case HTMLTag::kFrameset:
          tree_.InsertHTMLElement(token);
          return;
        case HTMLTag::kFrame:
          tree_.InsertSelfClosingHTMLElementDestroyingToken(token);
          return;
        case HTMLTag::kNoframes:
          ProcessStartTagForInHead(token);
          return;
        default:
          break;
      }
      ParseError(token);
      break;
    case kAfterFramesetMode:
    case kAfterAfterFramesetMode:
      if (tag == HTMLTag::kHTML) {
        ProcessHtmlStartTagForInBody(token);
        return;
      }
      if (tag == HTMLTag::kNoframes) {
        ProcessStartTagForInHead(token);
        return;
      }
      ParseError(token);
      break;
    case kInSelectInTableMode:
      switch (tag) {
        case HTMLTag::kCaption:
        case HTMLTag::kTable:
        case TABLE_BODY_CONTEXT_CASES:
        case HTMLTag::kTr:
        case TABLE_CELL_CONTEXT_CASES: {
          ParseError(token);
          AtomicHTMLToken end_select(HTMLToken::kEndTag, HTMLTag::kSelect);
          ProcessEndTag(&end_select);
          ProcessStartTag(token);
          return;
        }
        default:
          break;
      }
      [[fallthrough]];
    case kInSelectMode:
      switch (tag) {
        case HTMLTag::kHTML:
          ProcessHtmlStartTagForInBody(token);
          return;
        case HTMLTag::kOption:
          if (tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kOption)) {
            AtomicHTMLToken end_option(HTMLToken::kEndTag, HTMLTag::kOption);
            ProcessEndTag(&end_option);
          }
          tree_.InsertHTMLElement(token);
          return;
        case HTMLTag::kOptgroup:
          if (tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kOption)) {
            AtomicHTMLToken end_option(HTMLToken::kEndTag, HTMLTag::kOption);
            ProcessEndTag(&end_option);
          }
          if (tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kOptgroup)) {
            AtomicHTMLToken end_optgroup(HTMLToken::kEndTag,
                                         HTMLTag::kOptgroup);
            ProcessEndTag(&end_optgroup);
          }
          tree_.InsertHTMLElement(token);
          return;
        case HTMLTag::kHr:
          if (tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kOption)) {
            AtomicHTMLToken end_option(HTMLToken::kEndTag, HTMLTag::kOption);
            ProcessEndTag(&end_option);
          }
          if (tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kOptgroup)) {
            AtomicHTMLToken end_optgroup(HTMLToken::kEndTag,
                                         HTMLTag::kOptgroup);
            ProcessEndTag(&end_optgroup);
          }
          tree_.InsertSelfClosingHTMLElementDestroyingToken(token);
          return;
        case HTMLTag::kSelect: {
        tree_.OpenElements()->TopNode()->AddConsoleMessage(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kError,
            "A <select> tag was parsed within another <select> tag and was converted into </select>. This behavior will change in a future browser version. Please add the missing </select> end tag.");
          ParseError(token);
          AtomicHTMLToken end_select(HTMLToken::kEndTag, HTMLTag::kSelect);
          ProcessEndTag(&end_select);
          return;
        }
        case HTMLTag::kInput:
          // TODO(crbug.com/1511354): Remove this UseCounter when the
          // SelectParserRelaxation/CustomizableSelect flags are removed.
          UseCounter::Count(tree_.CurrentNode()->GetDocument(),
                            WebFeature::kHTMLInputInSelect);
          [[fallthrough]];
        case HTMLTag::kKeygen:
        case HTMLTag::kTextarea: {
          if (RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
            ProcessStartTagForInBody(token);
          } else {
            ParseError(token);
            if (!tree_.OpenElements()->InSelectScope(HTMLTag::kSelect)) {
              DCHECK(IsParsingFragment());
              return;
            }
            AtomicHTMLToken end_select(HTMLToken::kEndTag, HTMLTag::kSelect);
            ProcessEndTag(&end_select);
            ProcessStartTag(token);

            tree_.OpenElements()->TopNode()->AddConsoleMessage(
                mojom::blink::ConsoleMessageSource::kJavaScript,
                mojom::blink::ConsoleMessageLevel::kWarning,
                "A " + token->GetName() +
                    " tag was parsed inside of a <select> which caused a "
                    "</select> to be inserted before this tag. "
                    "This is not valid HTML and the behavior may be changed in "
                    "future versions of chrome.");
          }
          return;
        }
        case HTMLTag::kScript: {
          bool did_process = ProcessStartTagForInHead(token);
          DCHECK(did_process);
          return;
        }
        case HTMLTag::kTemplate:
          ProcessTemplateStartTag(token);
          return;
        case HTMLTag::kButton:
          if (!RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
            // TODO(crbug.com/1511354): Remove this UseCounter when the
            // SelectParserRelaxation/CustomizableSelect flags are removed.
            UseCounter::Count(tree_.CurrentNode()->GetDocument(),
                              WebFeature::kHTMLButtonInSelect);
          }
          [[fallthrough]];
        case HTMLTag::kDatalist:
          if (tag == HTMLTag::kDatalist &&
              !RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
            // TODO(crbug.com/1511354): Remove this UseCounter when the
            // SelectParserRelaxation/CustomizableSelect flags are removed.
            UseCounter::Count(tree_.CurrentNode()->GetDocument(),
                              WebFeature::kHTMLDatalistInSelect);
          }
          [[fallthrough]];
        default:
          if (RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
            ProcessStartTagForInBody(token);
          } else {
            // TODO(crbug.com/1511354): Remove this UseCounter when the
            // SelectParserRelaxation/CustomizableSelect flags are removed.
            UseCounter::Count(tree_.CurrentNode()->GetDocument(),
                              WebFeature::kSelectParserDroppedTag);
            tree_.OpenElements()->TopNode()->AddConsoleMessage(
                mojom::blink::ConsoleMessageSource::kJavaScript,
                mojom::blink::ConsoleMessageLevel::kWarning,
                "A " + token->GetName() +
                    " tag was parsed inside of a <select> which was not "
                    "inserted into the document. This is not valid HTML and "
                    "the behavior may be changed in future versions of "
                    "chrome.");
          }
          break;
      }
      break;
    case kInTableTextMode:
      DefaultForInTableText();
      ProcessStartTag(token);
      break;
    case kTextMode:
      NOTREACHED_IN_MIGRATION();
      break;
    case kTemplateContentsMode:
      switch (tag) {
        case HTMLTag::kTemplate:
          ProcessTemplateStartTag(token);
          return;
        case HTMLTag::kLink:
        case HTMLTag::kScript:
        case HTMLTag::kStyle:
        case HTMLTag::kMeta:
          ProcessStartTagForInHead(token);
          return;
        default:
          break;
      }

      InsertionMode insertion_mode = kTemplateContentsMode;
      switch (tag) {
        case HTMLTag::kCol:
          insertion_mode = kInColumnGroupMode;
          break;
        case HTMLTag::kCaption:
        case HTMLTag::kColgroup:
        case TABLE_BODY_CONTEXT_CASES:
          insertion_mode = kInTableMode;
          break;
        case HTMLTag::kTr:
          insertion_mode = kInTableBodyMode;
          break;
        case TABLE_CELL_CONTEXT_CASES:
          insertion_mode = kInRowMode;
          break;
        default:
          insertion_mode = kInBodyMode;
          break;
      }

      DCHECK_NE(insertion_mode, kTemplateContentsMode);
      DCHECK_EQ(template_insertion_modes_.back(), kTemplateContentsMode);
      template_insertion_modes_.back() = insertion_mode;
      SetInsertionMode(insertion_mode);

      ProcessStartTag(token);
      break;
  }
}

void HTMLTreeBuilder::ProcessHtmlStartTagForInBody(AtomicHTMLToken* token) {
  ParseError(token);
  if (tree_.OpenElements()->HasTemplateInHTMLScope()) {
    DCHECK(IsParsingTemplateContents());
    return;
  }
  tree_.InsertHTMLHtmlStartTagInBody(token);
}

bool HTMLTreeBuilder::ProcessBodyEndTagForInBody(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kEndTag);
  DCHECK_EQ(token->GetHTMLTag(), HTMLTag::kBody);
  if (!tree_.OpenElements()->InScope(HTMLTag::kBody)) {
    ParseError(token);
    return false;
  }
  // Emit a more specific parse error based on stack contents.
  DVLOG(1) << "Not implemented.";
  SetInsertionMode(kAfterBodyMode);
  return true;
}

void HTMLTreeBuilder::ProcessAnyOtherEndTagForInBody(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kEndTag);
  HTMLStackItem* item = tree_.OpenElements()->TopStackItem();
  while (true) {
    if (item->MatchesHTMLTag(token->GetTokenName())) {
      tree_.GenerateImpliedEndTagsWithExclusion(token->GetTokenName());
      if (!tree_.CurrentStackItem()->MatchesHTMLTag(token->GetTokenName()))
        ParseError(token);
      tree_.OpenElements()->PopUntilPopped(item->GetElement());
      return;
    }
    if (item->IsSpecialNode()) {
      ParseError(token);
      return;
    }
    item = item->NextItemInStack();
  }
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-inbody
void HTMLTreeBuilder::CallTheAdoptionAgency(AtomicHTMLToken* token) {
  // The adoption agency algorithm is N^2. We limit the number of iterations
  // to stop from hanging the whole browser. This limit is specified in the
  // adoption agency algorithm:
  // https://html.spec.whatwg.org/multipage/parsing.html#adoption-agency-algorithm
  static const int kOuterIterationLimit = 8;
  static const int kInnerIterationLimit = 3;

  // 2. If the current node is an HTML element whose tag name is subject,
  // and the current node is not in the list of active formatting elements,
  // then pop the current node off the stack of open elements and return.
  if (!tree_.IsEmpty() && tree_.CurrentStackItem()->IsElementNode() &&
      tree_.CurrentElement()->HasLocalName(token->GetName()) &&
      !tree_.ActiveFormattingElements()->Contains(tree_.CurrentElement())) {
    tree_.OpenElements()->Pop();
    return;
  }

  // 1, 2, 3 and 16 are covered by the for() loop.
  for (int i = 0; i < kOuterIterationLimit; ++i) {
    // 4.
    // ClosestElementInScopeWithName() returns null for non-html tags.
    if (!token->IsValidHTMLTag())
      return ProcessAnyOtherEndTagForInBody(token);
    Element* formatting_element =
        tree_.ActiveFormattingElements()->ClosestElementInScopeWithName(
            token->GetName());
    // 4.a
    if (!formatting_element)
      return ProcessAnyOtherEndTagForInBody(token);
    // 4.c
    if ((tree_.OpenElements()->Contains(formatting_element)) &&
        !tree_.OpenElements()->InScope(formatting_element)) {
      ParseError(token);
      // Check the stack of open elements for a more specific parse error.
      DVLOG(1) << "Not implemented.";
      return;
    }
    // 4.b
    HTMLStackItem* formatting_element_item =
        tree_.OpenElements()->Find(formatting_element);
    if (!formatting_element_item) {
      ParseError(token);
      tree_.ActiveFormattingElements()->Remove(formatting_element);
      return;
    }
    // 4.d
    if (formatting_element != tree_.CurrentElement())
      ParseError(token);
    // 5.
    HTMLStackItem* furthest_block =
        tree_.OpenElements()->FurthestBlockForFormattingElement(
            formatting_element);
    // 6.
    if (!furthest_block) {
      tree_.OpenElements()->PopUntilPopped(formatting_element);
      tree_.ActiveFormattingElements()->Remove(formatting_element);
      return;
    }
    // 7.
    DCHECK(furthest_block->IsAboveItemInStack(formatting_element_item));
    HTMLStackItem* common_ancestor = formatting_element_item->NextItemInStack();
    // 8.
    HTMLFormattingElementList::Bookmark bookmark =
        tree_.ActiveFormattingElements()->BookmarkFor(formatting_element);
    // 9.
    HTMLStackItem* node = furthest_block;
    HTMLStackItem* next_node = node->NextItemInStack();
    HTMLStackItem* last_node = furthest_block;
    // 9.1, 9.2, 9.3 and 9.11 are covered by the for() loop.
    for (int j = 0; j < kInnerIterationLimit; ++j) {
      // 9.4
      node = next_node;
      DCHECK(node);
      // Save node->next() for the next iteration in case node is deleted in
      // 9.5.
      next_node = node->NextItemInStack();
      // 9.5
      if (!tree_.ActiveFormattingElements()->Contains(node->GetElement())) {
        tree_.OpenElements()->Remove(node->GetElement());
        node = nullptr;
        continue;
      }
      // 9.6
      if (node == formatting_element_item) {
        break;
      }
      // 9.7
      HTMLStackItem* new_item = tree_.CreateElementFromSavedToken(node);

      HTMLFormattingElementList::Entry* node_entry =
          tree_.ActiveFormattingElements()->Find(node->GetElement());
      node_entry->ReplaceElement(new_item);
      tree_.OpenElements()->Replace(node, new_item);
      node = new_item;

      // 9.8
      if (last_node == furthest_block)
        bookmark.MoveToAfter(node_entry);
      // 9.9
      tree_.Reparent(node, last_node);
      // 9.10
      last_node = node;
    }
    // 10.
    tree_.InsertAlreadyParsedChild(common_ancestor, last_node);
    // 11.
    HTMLStackItem* new_item =
        tree_.CreateElementFromSavedToken(formatting_element_item);
    // 12.
    tree_.TakeAllChildren(new_item, furthest_block);
    // 13.
    tree_.Reparent(furthest_block, new_item);
    // 14.
    tree_.ActiveFormattingElements()->SwapTo(formatting_element, new_item,
                                             bookmark);
    // 15.
    tree_.OpenElements()->Remove(formatting_element);
    tree_.OpenElements()->InsertAbove(new_item, furthest_block);
  }
}

void HTMLTreeBuilder::ResetInsertionModeAppropriately() {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#reset-the-insertion-mode-appropriately
  bool last = false;
  HTMLStackItem* item = tree_.OpenElements()->TopStackItem();
  while (true) {
    if (item->GetNode() == tree_.OpenElements()->RootNode()) {
      last = true;
      if (IsParsingFragment())
        item = fragment_context_.ContextElementStackItem();
    }
    const HTMLTag tag = item->GetHTMLTag();
    if (item->IsHTMLNamespace()) {
      switch (tag) {
        case HTMLTag::kTemplate:
          return SetInsertionMode(template_insertion_modes_.back());
        case HTMLTag::kSelect:
          if (RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
            break;
          }
          if (!last) {
            while (item->GetNode() != tree_.OpenElements()->RootNode() &&
                   !item->MatchesHTMLTag(HTMLTag::kTemplate)) {
              item = item->NextItemInStack();
              if (item->MatchesHTMLTag(HTMLTag::kTable))
                return SetInsertionMode(kInSelectInTableMode);
            }
          }
          return SetInsertionMode(kInSelectMode);
        case HTMLTag::kTd:
        case HTMLTag::kTh:
          return SetInsertionMode(kInCellMode);
        case HTMLTag::kTr:
          return SetInsertionMode(kInRowMode);
        case HTMLTag::kTbody:
        case HTMLTag::kThead:
        case HTMLTag::kTfoot:
          return SetInsertionMode(kInTableBodyMode);
        case HTMLTag::kCaption:
          return SetInsertionMode(kInCaptionMode);
        case HTMLTag::kColgroup:
          return SetInsertionMode(kInColumnGroupMode);
        case HTMLTag::kTable:
          return SetInsertionMode(kInTableMode);
        case HTMLTag::kHead:
          if (!fragment_context_.Fragment() ||
              fragment_context_.ContextElement() != item->GetNode())
            return SetInsertionMode(kInHeadMode);
          return SetInsertionMode(kInBodyMode);
        case HTMLTag::kBody:
          return SetInsertionMode(kInBodyMode);
        case HTMLTag::kFrameset:
          return SetInsertionMode(kInFramesetMode);
        case HTMLTag::kHTML:
          if (tree_.HeadStackItem())
            return SetInsertionMode(kAfterHeadMode);

          DCHECK(IsParsingFragment());
          return SetInsertionMode(kBeforeHeadMode);
        default:
          break;
      }
    }
    if (last) {
      DCHECK(IsParsingFragment());
      return SetInsertionMode(kInBodyMode);
    }
    item = item->NextItemInStack();
  }
}

void HTMLTreeBuilder::ProcessEndTagForInTableBody(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kEndTag);
  const HTMLTag tag = token->GetHTMLTag();
  switch (tag) {
    case TABLE_BODY_CONTEXT_CASES:
      if (!tree_.OpenElements()->InTableScope(tag)) {
        ParseError(token);
        return;
      }
      tree_.OpenElements()->PopUntilTableBodyScopeMarker();
      tree_.OpenElements()->Pop();
      SetInsertionMode(kInTableMode);
      return;
    case HTMLTag::kTable:
      // FIXME: This is slow.
      if (!tree_.OpenElements()->InTableScope(HTMLTag::kTbody) &&
          !tree_.OpenElements()->InTableScope(HTMLTag::kThead) &&
          !tree_.OpenElements()->InTableScope(HTMLTag::kTfoot)) {
        DCHECK(IsParsingFragmentOrTemplateContents());
        ParseError(token);
        return;
      }
      tree_.OpenElements()->PopUntilTableBodyScopeMarker();
      DCHECK(IsTableBodyContextTag(tree_.CurrentStackItem()->GetHTMLTag()));
      ProcessFakeEndTag(*tree_.CurrentStackItem());
      ProcessEndTag(token);
      return;
    case HTMLTag::kBody:
    case CAPTION_COL_OR_COLGROUP_CASES:
    case HTMLTag::kHTML:
    case TABLE_CELL_CONTEXT_CASES:
    case HTMLTag::kTr:
      ParseError(token);
      return;
    default:
      break;
  }
  ProcessEndTagForInTable(token);
}

void HTMLTreeBuilder::ProcessEndTagForInRow(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kEndTag);
  const HTMLTag tag = token->GetHTMLTag();
  switch (tag) {
    case HTMLTag::kTr:
      ProcessTrEndTagForInRow();
      return;
    case HTMLTag::kTable:
      if (!ProcessTrEndTagForInRow()) {
        DCHECK(IsParsingFragmentOrTemplateContents());
        return;
      }
      DCHECK_EQ(GetInsertionMode(), kInTableBodyMode);
      ProcessEndTag(token);
      return;
    case TABLE_BODY_CONTEXT_CASES:
      if (!tree_.OpenElements()->InTableScope(tag)) {
        ParseError(token);
        return;
      }
      ProcessFakeEndTag(HTMLTag::kTr);
      DCHECK_EQ(GetInsertionMode(), kInTableBodyMode);
      ProcessEndTag(token);
      return;
    case HTMLTag::kBody:
    case CAPTION_COL_OR_COLGROUP_CASES:
    case HTMLTag::kHTML:
    case TABLE_CELL_CONTEXT_CASES:
      ParseError(token);
      return;
    default:
      break;
  }
  ProcessEndTagForInTable(token);
}

void HTMLTreeBuilder::ProcessEndTagForInCell(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kEndTag);
  const HTMLTag tag = token->GetHTMLTag();
  switch (tag) {
    case TABLE_CELL_CONTEXT_CASES:
      if (!tree_.OpenElements()->InTableScope(tag)) {
        ParseError(token);
        return;
      }
      tree_.GenerateImpliedEndTags();
      if (!tree_.CurrentStackItem()->MatchesHTMLTag(tag))
        ParseError(token);
      tree_.OpenElements()->PopUntilPopped(tag);
      tree_.ActiveFormattingElements()->ClearToLastMarker();
      SetInsertionMode(kInRowMode);
      return;
    case HTMLTag::kBody:
    case CAPTION_COL_OR_COLGROUP_CASES:
    case HTMLTag::kHTML:
      ParseError(token);
      return;
    case HTMLTag::kTable:
    case HTMLTag::kTr:
    case TABLE_BODY_CONTEXT_CASES:
      if (!tree_.OpenElements()->InTableScope(tag)) {
        DCHECK(IsTableBodyContextTag(tag) ||
               tree_.OpenElements()->InTableScope(HTMLTag::kTemplate) ||
               IsParsingFragment());
        ParseError(token);
        return;
      }
      CloseTheCell();
      ProcessEndTag(token);
      return;
    default:
      break;
  }
  ProcessEndTagForInBody(token);
}

void HTMLTreeBuilder::ProcessEndTagForInBody(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kEndTag);
  const HTMLTag tag = token->GetHTMLTag();
  switch (tag) {
    case HTMLTag::kBody:
      ProcessBodyEndTagForInBody(token);
      return;
    case HTMLTag::kHTML: {
      AtomicHTMLToken end_body(HTMLToken::kEndTag, HTMLTag::kBody);
      if (ProcessBodyEndTagForInBody(&end_body))
        ProcessEndTag(token);
      return;
    }
      // https://html.spec.whatwg.org/multipage/parsing.html#:~:text=An%20end%20tag%20whose%20tag%20name%20is%20one%20of%3A%20%22address%22%2C
    case HTMLTag::kAddress:
    case HTMLTag::kArticle:
    case HTMLTag::kAside:
    case HTMLTag::kBlockquote:
    case HTMLTag::kButton:
    case HTMLTag::kCenter:
    case HTMLTag::kDetails:
    case HTMLTag::kDialog:
    case HTMLTag::kDir:
    case HTMLTag::kDiv:
    case HTMLTag::kDl:
    case HTMLTag::kFieldset:
    case HTMLTag::kFigcaption:
    case HTMLTag::kFigure:
    case HTMLTag::kFooter:
    case HTMLTag::kHeader:
    case HTMLTag::kHgroup:
    case HTMLTag::kListing:
    case HTMLTag::kMain:
    case HTMLTag::kMenu:
    case HTMLTag::kNav:
    case HTMLTag::kOl:
    case HTMLTag::kPre:
    case HTMLTag::kSearch:
    case HTMLTag::kSection:
    case HTMLTag::kSummary:
    case HTMLTag::kSelect:
    case HTMLTag::kUl:
      if (!tree_.OpenElements()->InScope(tag)) {
        ParseError(token);
        return;
      }
      tree_.GenerateImpliedEndTags();
      if (!tree_.CurrentStackItem()->MatchesHTMLTag(tag))
        ParseError(token);
      tree_.OpenElements()->PopUntilPopped(tag);
      return;
    case HTMLTag::kForm:
      if (!IsParsingTemplateContents()) {
        Element* node = tree_.TakeForm();
        if (!node || !tree_.OpenElements()->InScope(node)) {
          ParseError(token);
          return;
        }
        tree_.GenerateImpliedEndTags();
        if (tree_.CurrentElement() != node)
          ParseError(token);
        tree_.OpenElements()->Remove(node);
      }
      break;
    case HTMLTag::kP:
      if (!tree_.OpenElements()->InButtonScope(tag)) {
        ParseError(token);
        ProcessFakeStartTag(HTMLTag::kP);
        DCHECK(tree_.OpenElements()->InScope(tag));
        ProcessEndTag(token);
        return;
      }
      tree_.GenerateImpliedEndTagsWithExclusion(token->GetTokenName());
      if (!tree_.CurrentStackItem()->MatchesHTMLTag(tag))
        ParseError(token);
      tree_.OpenElements()->PopUntilPopped(tag);
      return;
    case HTMLTag::kLi:
      if (!tree_.OpenElements()->InListItemScope(tag)) {
        ParseError(token);
        return;
      }
      tree_.GenerateImpliedEndTagsWithExclusion(token->GetTokenName());
      if (!tree_.CurrentStackItem()->MatchesHTMLTag(tag))
        ParseError(token);
      tree_.OpenElements()->PopUntilPopped(tag);
      return;
    case HTMLTag::kDd:
    case HTMLTag::kDt:
      if (!tree_.OpenElements()->InScope(tag)) {
        ParseError(token);
        return;
      }
      tree_.GenerateImpliedEndTagsWithExclusion(token->GetTokenName());
      if (!tree_.CurrentStackItem()->MatchesHTMLTag(tag))
        ParseError(token);
      tree_.OpenElements()->PopUntilPopped(tag);
      return;
    case HTMLTag::kH1:
    case HTMLTag::kH2:
    case HTMLTag::kH3:
    case HTMLTag::kH4:
    case HTMLTag::kH5:
    case HTMLTag::kH6:
      if (!tree_.OpenElements()->HasNumberedHeaderElementInScope()) {
        ParseError(token);
        return;
      }
      tree_.GenerateImpliedEndTags();
      if (!tree_.CurrentStackItem()->MatchesHTMLTag(tag))
        ParseError(token);
      tree_.OpenElements()->PopUntilNumberedHeaderElementPopped();
      return;
    case HTMLTag::kA:
    case HTMLTag::kNobr:
    case HTMLTag::kB:
    case HTMLTag::kBig:
    case HTMLTag::kCode:
    case HTMLTag::kEm:
    case HTMLTag::kFont:
    case HTMLTag::kI:
    case HTMLTag::kS:
    case HTMLTag::kSmall:
    case HTMLTag::kStrike:
    case HTMLTag::kStrong:
    case HTMLTag::kTt:
    case HTMLTag::kU:
      CallTheAdoptionAgency(token);
      return;
    case HTMLTag::kApplet:
    case HTMLTag::kMarquee:
    case HTMLTag::kObject:
      if (!tree_.OpenElements()->InScope(tag)) {
        ParseError(token);
        return;
      }
      tree_.GenerateImpliedEndTags();
      if (!tree_.CurrentStackItem()->MatchesHTMLTag(tag))
        ParseError(token);
      tree_.OpenElements()->PopUntilPopped(tag);
      tree_.ActiveFormattingElements()->ClearToLastMarker();
      return;
    case HTMLTag::kBr:
      ParseError(token);
      ProcessFakeStartTag(HTMLTag::kBr);
      return;
    case HTMLTag::kTemplate:
      ProcessTemplateEndTag(token);
      return;
    default:
      break;
  }
  ProcessAnyOtherEndTagForInBody(token);
}

bool HTMLTreeBuilder::ProcessCaptionEndTagForInCaption() {
  if (!tree_.OpenElements()->InTableScope(HTMLTag::kCaption)) {
    DCHECK(IsParsingFragment());
    // FIXME: parse error
    return false;
  }
  tree_.GenerateImpliedEndTags();
  // FIXME: parse error if
  // (!tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kCaption))
  tree_.OpenElements()->PopUntilPopped(HTMLTag::kCaption);
  tree_.ActiveFormattingElements()->ClearToLastMarker();
  SetInsertionMode(kInTableMode);
  return true;
}

bool HTMLTreeBuilder::ProcessTrEndTagForInRow() {
  if (!tree_.OpenElements()->InTableScope(HTMLTag::kTr)) {
    DCHECK(IsParsingFragmentOrTemplateContents());
    // FIXME: parse error
    return false;
  }
  tree_.OpenElements()->PopUntilTableRowScopeMarker();
  DCHECK(tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kTr));
  tree_.OpenElements()->Pop();
  SetInsertionMode(kInTableBodyMode);
  return true;
}

bool HTMLTreeBuilder::ProcessTableEndTagForInTable() {
  if (!tree_.OpenElements()->InTableScope(HTMLTag::kTable)) {
    DCHECK(IsParsingFragmentOrTemplateContents());
    // FIXME: parse error.
    return false;
  }
  tree_.OpenElements()->PopUntilPopped(HTMLTag::kTable);
  ResetInsertionModeAppropriately();
  return true;
}

void HTMLTreeBuilder::ProcessEndTagForInTable(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kEndTag);
  switch (token->GetHTMLTag()) {
    case HTMLTag::kTable:
      ProcessTableEndTagForInTable();
      return;
    case HTMLTag::kBody:
    case CAPTION_COL_OR_COLGROUP_CASES:
    case HTMLTag::kHTML:
    case TABLE_BODY_CONTEXT_CASES:
    case TABLE_CELL_CONTEXT_CASES:
    case HTMLTag::kTr:
      ParseError(token);
      return;
    default:
      break;
  }
  ParseError(token);
  // Is this redirection necessary here?
  HTMLConstructionSite::RedirectToFosterParentGuard redirecter(tree_);
  ProcessEndTagForInBody(token);
}

void HTMLTreeBuilder::ProcessEndTag(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kEndTag);
  const HTMLTag tag = token->GetHTMLTag();
  switch (GetInsertionMode()) {
    case kInitialMode:
      DefaultForInitial();
      [[fallthrough]];
    case kBeforeHTMLMode:
      switch (tag) {
        case HTMLTag::kHead:
        case HTMLTag::kBody:
        case HTMLTag::kHTML:
        case HTMLTag::kBr:
          break;
        default:
          ParseError(token);
          return;
      }
      DefaultForBeforeHTML();
      [[fallthrough]];
    case kBeforeHeadMode:
      switch (tag) {
        case HTMLTag::kHead:
        case HTMLTag::kBody:
        case HTMLTag::kHTML:
        case HTMLTag::kBr:
          break;
        default:
          ParseError(token);
          return;
      }
      DefaultForBeforeHead();
      [[fallthrough]];
    case kInHeadMode:
      // FIXME: This case should be broken out into processEndTagForInHead,
      // because other end tag cases now refer to it ("process the token for
      // using the rules of the "in head" insertion mode"). but because the
      // logic falls through to AfterHeadMode, that gets a little messy.
      switch (tag) {
        case HTMLTag::kTemplate:
          ProcessTemplateEndTag(token);
          return;
        case HTMLTag::kHead:
          tree_.OpenElements()->PopHTMLHeadElement();
          SetInsertionMode(kAfterHeadMode);
          return;
        case HTMLTag::kBody:
        case HTMLTag::kHTML:
        case HTMLTag::kBr:
          break;
        default:
          ParseError(token);
          return;
      }
      DefaultForInHead();
      [[fallthrough]];
    case kAfterHeadMode:
      switch (tag) {
        case HTMLTag::kBody:
        case HTMLTag::kHTML:
        case HTMLTag::kBr:
          break;
        default:
          ParseError(token);
          return;
      }
      DefaultForAfterHead();
      [[fallthrough]];
    case kInBodyMode:
      ProcessEndTagForInBody(token);
      break;
    case kInTableMode:
      ProcessEndTagForInTable(token);
      break;
    case kInCaptionMode:
      switch (tag) {
        case HTMLTag::kCaption:
          ProcessCaptionEndTagForInCaption();
          return;
        case HTMLTag::kTable:
          ParseError(token);
          if (!ProcessCaptionEndTagForInCaption()) {
            DCHECK(IsParsingFragment());
            return;
          }
          ProcessEndTag(token);
          return;
        case HTMLTag::kBody:
        case HTMLTag::kCol:
        case HTMLTag::kColgroup:
        case HTMLTag::kHTML:
        case TABLE_BODY_CONTEXT_CASES:
        case TABLE_CELL_CONTEXT_CASES:
        case HTMLTag::kTr:
          ParseError(token);
          return;
        default:
          break;
      }
      ProcessEndTagForInBody(token);
      break;
    case kInColumnGroupMode:
      switch (tag) {
        case HTMLTag::kColgroup:
          ProcessColgroupEndTagForInColumnGroup();
          return;
        case HTMLTag::kCol:
          ParseError(token);
          return;
        case HTMLTag::kTemplate:
          ProcessTemplateEndTag(token);
          return;
        default:
          break;
      }
      if (!ProcessColgroupEndTagForInColumnGroup()) {
        DCHECK(IsParsingFragmentOrTemplateContents());
        return;
      }
      ProcessEndTag(token);
      break;
    case kInRowMode:
      ProcessEndTagForInRow(token);
      break;
    case kInCellMode:
      ProcessEndTagForInCell(token);
      break;
    case kInTableBodyMode:
      ProcessEndTagForInTableBody(token);
      break;
    case kAfterBodyMode:
      if (tag == HTMLTag::kHTML) {
        if (IsParsingFragment()) {
          ParseError(token);
          return;
        }
        SetInsertionMode(kAfterAfterBodyMode);
        return;
      }
      [[fallthrough]];
    case kAfterAfterBodyMode:
      ParseError(token);
      SetInsertionMode(kInBodyMode);
      ProcessEndTag(token);
      break;
    case kInHeadNoscriptMode:
      if (tag == HTMLTag::kNoscript) {
        DCHECK(tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kNoscript));
        tree_.OpenElements()->Pop();
        DCHECK(tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kHead));
        SetInsertionMode(kInHeadMode);
        return;
      }
      if (tag != HTMLTag::kBr) {
        ParseError(token);
        return;
      }
      DefaultForInHeadNoscript();
      ProcessToken(token);
      break;
    case kTextMode:
      if (tag == HTMLTag::kScript &&
          tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kScript)) {
        // Pause ourselves so that parsing stops until the script can be
        // processed by the caller.
        if (ScriptingContentIsAllowed(tree_.GetParserContentPolicy()))
          script_to_process_ = tree_.CurrentElement();
        tree_.OpenElements()->Pop();
        SetInsertionMode(original_insertion_mode_);

        // We must set the tokenizer's state to DataState explicitly if the
        // tokenizer didn't have a chance to.
        parser_->tokenizer().SetState(HTMLTokenizer::kDataState);
        return;
      }
      tree_.OpenElements()->Pop();
      SetInsertionMode(original_insertion_mode_);
      break;
    case kInFramesetMode:
      if (tag == HTMLTag::kFrameset) {
        bool ignore_frameset_for_fragment_parsing = tree_.CurrentIsRootNode();
        ignore_frameset_for_fragment_parsing =
            ignore_frameset_for_fragment_parsing ||
            tree_.OpenElements()->HasTemplateInHTMLScope();
        if (ignore_frameset_for_fragment_parsing) {
          DCHECK(IsParsingFragmentOrTemplateContents());
          ParseError(token);
          return;
        }
        tree_.OpenElements()->Pop();
        if (!IsParsingFragment() &&
            !tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kFrameset)) {
          SetInsertionMode(kAfterFramesetMode);
        }
        return;
      }
      break;
    case kAfterFramesetMode:
      if (tag == HTMLTag::kHTML) {
        SetInsertionMode(kAfterAfterFramesetMode);
        return;
      }
      [[fallthrough]];
    case kAfterAfterFramesetMode:
      ParseError(token);
      break;
    case kInSelectInTableMode:
      switch (tag) {
        case HTMLTag::kCaption:
        case HTMLTag::kTable:
        case TABLE_BODY_CONTEXT_CASES:
        case HTMLTag::kTr:
        case TABLE_CELL_CONTEXT_CASES:
          ParseError(token);
          if (tree_.OpenElements()->InTableScope(tag)) {
            AtomicHTMLToken end_select(HTMLToken::kEndTag, HTMLTag::kSelect);
            ProcessEndTag(&end_select);
            ProcessEndTag(token);
          }
          return;
        default:
          break;
      }
      [[fallthrough]];
    case kInSelectMode:
      CHECK(!RuntimeEnabledFeatures::SelectParserRelaxationEnabled());
      switch (tag) {
        case HTMLTag::kOptgroup:
          if (tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kOption) &&
              tree_.OneBelowTop() &&
              tree_.OneBelowTop()->MatchesHTMLTag(HTMLTag::kOptgroup))
            ProcessFakeEndTag(HTMLTag::kOption);
          if (tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kOptgroup)) {
            tree_.OpenElements()->Pop();
            return;
          }
          ParseError(token);
          return;
        case HTMLTag::kOption:
          if (tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kOption)) {
            tree_.OpenElements()->Pop();
            return;
          }
          ParseError(token);
          return;
        case HTMLTag::kSelect:
          if (!tree_.OpenElements()->InSelectScope(tag)) {
            DCHECK(IsParsingFragment());
            ParseError(token);
            return;
          }
          tree_.OpenElements()->PopUntilPopped(HTMLTag::kSelect);
          ResetInsertionModeAppropriately();
          return;
        case HTMLTag::kTemplate:
          ProcessTemplateEndTag(token);
          return;
        default:
          break;
      }
      break;
    case kInTableTextMode:
      DefaultForInTableText();
      ProcessEndTag(token);
      break;
    case kTemplateContentsMode:
      if (tag == HTMLTag::kTemplate) {
        ProcessTemplateEndTag(token);
        return;
      }
      break;
  }
}

void HTMLTreeBuilder::ProcessComment(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kComment);
  if (GetInsertionMode() == kInitialMode ||
      GetInsertionMode() == kBeforeHTMLMode ||
      GetInsertionMode() == kAfterAfterBodyMode ||
      GetInsertionMode() == kAfterAfterFramesetMode) {
    tree_.InsertCommentOnDocument(token);
    return;
  }
  if (GetInsertionMode() == kAfterBodyMode) {
    tree_.InsertCommentOnHTMLHtmlElement(token);
    return;
  }
  if (GetInsertionMode() == kInTableTextMode) {
    DefaultForInTableText();
    ProcessComment(token);
    return;
  }
  tree_.InsertComment(token);
}

void HTMLTreeBuilder::ProcessDOMPart(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kDOMPart);
  DCHECK(tree_.InParsePartsScope());
  tree_.InsertDOMPart(token);
}

void HTMLTreeBuilder::ProcessCharacter(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kCharacter);
  CharacterTokenBuffer buffer(token);
  ProcessCharacterBuffer(buffer);
}

void HTMLTreeBuilder::ProcessCharacterBuffer(CharacterTokenBuffer& buffer) {
ReprocessBuffer:
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-inbody
  // Note that this logic is different than the generic \r\n collapsing
  // handled in the input stream preprocessor. This logic is here as an
  // "authoring convenience" so folks can write:
  //
  // <pre>
  // lorem ipsum
  // lorem ipsum
  // </pre>
  //
  // without getting an extra newline at the start of their <pre> element.
  if (should_skip_leading_newline_) {
    should_skip_leading_newline_ = false;
    buffer.SkipAtMostOneLeadingNewline();
    if (buffer.IsEmpty())
      return;
  }

  switch (GetInsertionMode()) {
    case kInitialMode: {
      buffer.SkipLeadingWhitespace();
      if (buffer.IsEmpty())
        return;
      DefaultForInitial();
      [[fallthrough]];
    }
    case kBeforeHTMLMode: {
      buffer.SkipLeadingWhitespace();
      if (buffer.IsEmpty())
        return;
      DefaultForBeforeHTML();
      if (parser_->IsStopped()) {
        buffer.SkipRemaining();
        return;
      }
      [[fallthrough]];
    }
    case kBeforeHeadMode: {
      buffer.SkipLeadingWhitespace();
      if (buffer.IsEmpty())
        return;
      DefaultForBeforeHead();
      [[fallthrough]];
    }
    case kInHeadMode: {
      auto leading_whitespace = buffer.TakeLeadingWhitespace();
      if (!leading_whitespace.string.empty()) {
        tree_.InsertTextNode(leading_whitespace.string,
                             leading_whitespace.whitespace_mode);
      }
      if (buffer.IsEmpty())
        return;
      DefaultForInHead();
      [[fallthrough]];
    }
    case kAfterHeadMode: {
      auto leading_whitespace = buffer.TakeLeadingWhitespace();
      if (!leading_whitespace.string.empty()) {
        tree_.InsertTextNode(leading_whitespace.string,
                             leading_whitespace.whitespace_mode);
      }
      if (buffer.IsEmpty())
        return;
      DefaultForAfterHead();
      [[fallthrough]];
    }
    case kInBodyMode:
    case kInCaptionMode:
    case kTemplateContentsMode:
    case kInCellMode: {
      ProcessCharacterBufferForInBody(buffer);
      break;
    }
    case kInTableMode:
    case kInTableBodyMode:
    case kInRowMode: {
      DCHECK(pending_table_characters_.empty());
      if (tree_.CurrentStackItem()->IsElementNode() &&
          (tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kTable) ||
           tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kTbody) ||
           tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kTfoot) ||
           tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kThead) ||
           tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kTr))) {
        original_insertion_mode_ = insertion_mode_;
        SetInsertionMode(kInTableTextMode);
        // Note that we fall through to the InTableTextMode case below.
      } else {
        HTMLConstructionSite::RedirectToFosterParentGuard redirecter(tree_);
        ProcessCharacterBufferForInBody(buffer);
        break;
      }
      [[fallthrough]];
    }
    case kInTableTextMode: {
      buffer.GiveRemainingTo(pending_table_characters_);
      break;
    }
    case kInColumnGroupMode: {
      auto leading_whitespace = buffer.TakeLeadingWhitespace();
      if (!leading_whitespace.string.empty()) {
        tree_.InsertTextNode(leading_whitespace.string,
                             leading_whitespace.whitespace_mode);
      }
      if (buffer.IsEmpty())
        return;
      if (!ProcessColgroupEndTagForInColumnGroup()) {
        DCHECK(IsParsingFragmentOrTemplateContents());
        // The spec tells us to drop these characters on the floor.
        buffer.SkipLeadingNonWhitespace();
        if (buffer.IsEmpty())
          return;
      }
      goto ReprocessBuffer;
    }
    case kAfterBodyMode:
    case kAfterAfterBodyMode: {
      // FIXME: parse error
      auto leading_whitespace = buffer.TakeLeadingWhitespace();
      if (!leading_whitespace.string.empty()) {
        InsertionMode mode = GetInsertionMode();
        SetInsertionMode(kInBodyMode);
        tree_.InsertTextNode(leading_whitespace.string,
                             leading_whitespace.whitespace_mode);
        SetInsertionMode(mode);
      }
      if (buffer.IsEmpty())
        return;
      SetInsertionMode(kInBodyMode);
      goto ReprocessBuffer;
    }
    case kTextMode: {
      tree_.InsertTextNode(buffer.TakeRemaining());
      break;
    }
    case kInHeadNoscriptMode: {
      auto leading_whitespace = buffer.TakeLeadingWhitespace();
      if (!leading_whitespace.string.empty()) {
        tree_.InsertTextNode(leading_whitespace.string,
                             leading_whitespace.whitespace_mode);
      }
      if (buffer.IsEmpty()) {
        return;
      }
      DefaultForInHeadNoscript();
      goto ReprocessBuffer;
    }
    case kInFramesetMode:
    case kAfterFramesetMode: {
      auto leading_whitespace = buffer.TakeRemainingWhitespace();
      if (!leading_whitespace.string.empty()) {
        tree_.InsertTextNode(leading_whitespace.string,
                             leading_whitespace.whitespace_mode);
      }
      // FIXME: We should generate a parse error if we skipped over any
      // non-whitespace characters.
      break;
    }
    case kInSelectInTableMode:
    case kInSelectMode: {
      tree_.InsertTextNode(buffer.TakeRemaining());
      break;
    }
    case kAfterAfterFramesetMode: {
      auto leading_whitespace = buffer.TakeRemainingWhitespace();
      if (!leading_whitespace.string.empty()) {
        tree_.ReconstructTheActiveFormattingElements();
        tree_.InsertTextNode(leading_whitespace.string,
                             leading_whitespace.whitespace_mode);
      }
      // FIXME: We should generate a parse error if we skipped over any
      // non-whitespace characters.
      break;
    }
  }
}

void HTMLTreeBuilder::ProcessCharacterBufferForInBody(
    CharacterTokenBuffer& buffer) {
  tree_.ReconstructTheActiveFormattingElements();
  StringView characters = buffer.TakeRemaining();
  tree_.InsertTextNode(characters);
  if (frameset_ok_ && !IsAllWhitespaceOrReplacementCharacters(characters))
    frameset_ok_ = false;
}

void HTMLTreeBuilder::ProcessEndOfFile(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kEndOfFile);
  switch (GetInsertionMode()) {
    case kInitialMode:
      DefaultForInitial();
      [[fallthrough]];
    case kBeforeHTMLMode:
      DefaultForBeforeHTML();
      [[fallthrough]];
    case kBeforeHeadMode:
      DefaultForBeforeHead();
      [[fallthrough]];
    case kInHeadMode:
      DefaultForInHead();
      [[fallthrough]];
    case kAfterHeadMode:
      DefaultForAfterHead();
      [[fallthrough]];
    case kInBodyMode:
    case kInCellMode:
    case kInCaptionMode:
    case kInRowMode:
      // Emit parse error based on what elements are still open.
      DVLOG(1) << "Not implemented.";
      if (!template_insertion_modes_.empty() &&
          ProcessEndOfFileForInTemplateContents(token))
        return;
      break;
    case kAfterBodyMode:
    case kAfterAfterBodyMode:
      break;
    case kInHeadNoscriptMode:
      DefaultForInHeadNoscript();
      ProcessEndOfFile(token);
      return;
    case kAfterFramesetMode:
    case kAfterAfterFramesetMode:
      break;
    case kInColumnGroupMode:
      if (tree_.CurrentIsRootNode()) {
        DCHECK(IsParsingFragment());
        return;  // FIXME: Should we break here instead of returning?
      }
      DCHECK(tree_.CurrentNode()->HasTagName(html_names::kColgroupTag) ||
             IsA<HTMLTemplateElement>(tree_.CurrentNode()));
      ProcessColgroupEndTagForInColumnGroup();
      [[fallthrough]];
    case kInFramesetMode:
    case kInTableMode:
    case kInTableBodyMode:
    case kInSelectInTableMode:
    case kInSelectMode:
      if (tree_.CurrentNode() != tree_.OpenElements()->RootNode())
        ParseError(token);
      if (!template_insertion_modes_.empty() &&
          ProcessEndOfFileForInTemplateContents(token))
        return;
      break;
    case kInTableTextMode:
      DefaultForInTableText();
      ProcessEndOfFile(token);
      return;
    case kTextMode: {
      ParseError(token);
      if (tree_.CurrentStackItem()->MatchesHTMLTag(HTMLTag::kScript)) {
        // Mark the script element as "already started".
        DVLOG(1) << "Not implemented.";
      }
      Element* el = tree_.OpenElements()->Top();
      if (IsA<HTMLTextAreaElement>(el))
        To<HTMLFormControlElement>(el)->SetBlocksFormSubmission(true);
      tree_.OpenElements()->Pop();
      DCHECK_NE(original_insertion_mode_, kTextMode);
      SetInsertionMode(original_insertion_mode_);
      ProcessEndOfFile(token);
      return;
    }
    case kTemplateContentsMode:
      if (ProcessEndOfFileForInTemplateContents(token))
        return;
      break;
  }
  tree_.ProcessEndOfFile();
}

void HTMLTreeBuilder::DefaultForInitial() {
  DVLOG(1) << "Not implemented.";
  tree_.SetDefaultCompatibilityMode();
  // FIXME: parse error
  SetInsertionMode(kBeforeHTMLMode);
}

void HTMLTreeBuilder::DefaultForBeforeHTML() {
  AtomicHTMLToken start_html(HTMLToken::kStartTag, HTMLTag::kHTML);
  tree_.InsertHTMLHtmlStartTagBeforeHTML(&start_html);
  SetInsertionMode(kBeforeHeadMode);
}

void HTMLTreeBuilder::DefaultForBeforeHead() {
  AtomicHTMLToken start_head(HTMLToken::kStartTag, HTMLTag::kHead);
  ProcessStartTag(&start_head);
}

void HTMLTreeBuilder::DefaultForInHead() {
  AtomicHTMLToken end_head(HTMLToken::kEndTag, HTMLTag::kHead);
  ProcessEndTag(&end_head);
}

void HTMLTreeBuilder::DefaultForInHeadNoscript() {
  AtomicHTMLToken end_noscript(HTMLToken::kEndTag, HTMLTag::kNoscript);
  ProcessEndTag(&end_noscript);
}

void HTMLTreeBuilder::DefaultForAfterHead() {
  AtomicHTMLToken start_body(HTMLToken::kStartTag, HTMLTag::kBody);
  ProcessStartTag(&start_body);
  frameset_ok_ = true;
}

void HTMLTreeBuilder::DefaultForInTableText() {
  String characters = pending_table_characters_.ToString();
  pending_table_characters_.Clear();
  if (!IsAllWhitespace(characters)) {
    // FIXME: parse error
    HTMLConstructionSite::RedirectToFosterParentGuard redirecter(tree_);
    tree_.ReconstructTheActiveFormattingElements();
    tree_.InsertTextNode(characters, WhitespaceMode::kNotAllWhitespace);
    frameset_ok_ = false;
    SetInsertionMode(original_insertion_mode_);
    return;
  }
  tree_.InsertTextNode(characters);
  SetInsertionMode(original_insertion_mode_);
}

bool HTMLTreeBuilder::ProcessStartTagForInHead(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kStartTag);
  switch (token->GetHTMLTag()) {
    case HTMLTag::kHTML:
      ProcessHtmlStartTagForInBody(token);
      return true;
    case HTMLTag::kBase:
    case HTMLTag::kBasefont:
    case HTMLTag::kBgsound:
    case HTMLTag::kCommand:
    case HTMLTag::kLink:
    case HTMLTag::kMeta:
      tree_.InsertSelfClosingHTMLElementDestroyingToken(token);
      // Note: The custom processing for the <meta> tag is done in
      // HTMLMetaElement::process().
      return true;
    case HTMLTag::kTitle:
      ProcessGenericRCDATAStartTag(token);
      return true;
    case HTMLTag::kNoscript:
      if (options_.scripting_flag) {
        ProcessGenericRawTextStartTag(token);
        return true;
      }
      tree_.InsertHTMLElement(token);
      SetInsertionMode(kInHeadNoscriptMode);
      return true;
    case HTMLTag::kNoframes:
    case HTMLTag::kStyle:
      ProcessGenericRawTextStartTag(token);
      return true;
    case HTMLTag::kScript:
      ProcessScriptStartTag(token);
      return true;
    case HTMLTag::kTemplate:
      ProcessTemplateStartTag(token);
      return true;
    case HTMLTag::kHead:
      ParseError(token);
      return true;
    default:
      return false;
  }
}

void HTMLTreeBuilder::ProcessGenericRCDATAStartTag(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kStartTag);
  tree_.InsertHTMLElement(token);
  parser_->tokenizer().SetState(HTMLTokenizer::kRCDATAState);
  original_insertion_mode_ = insertion_mode_;
  SetInsertionMode(kTextMode);
}

void HTMLTreeBuilder::ProcessGenericRawTextStartTag(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kStartTag);
  tree_.InsertHTMLElement(token);
  parser_->tokenizer().SetState(HTMLTokenizer::kRAWTEXTState);
  original_insertion_mode_ = insertion_mode_;
  SetInsertionMode(kTextMode);
}

void HTMLTreeBuilder::ProcessScriptStartTag(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kStartTag);
  tree_.InsertScriptElement(token);
  parser_->tokenizer().SetState(HTMLTokenizer::kScriptDataState);
  original_insertion_mode_ = insertion_mode_;

  TextPosition position = parser_->GetTextPosition();

  script_to_process_start_position_ = position;

  SetInsertionMode(kTextMode);
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#tree-construction
bool HTMLTreeBuilder::ShouldProcessTokenInForeignContent(
    AtomicHTMLToken* token) {
  if (tree_.IsEmpty())
    return false;
  HTMLStackItem* adjusted_current_node = AdjustedCurrentStackItem();

  if (adjusted_current_node->IsInHTMLNamespace())
    return false;
  if (HTMLElementStack::IsMathMLTextIntegrationPoint(adjusted_current_node)) {
    if (token->GetType() == HTMLToken::kStartTag &&
        token->GetName() != mathml_names::kMglyphTag &&
        token->GetName() != mathml_names::kMalignmarkTag)
      return false;
    if (token->GetType() == HTMLToken::kCharacter)
      return false;
  }
  if (adjusted_current_node->HasTagName(mathml_names::kAnnotationXmlTag) &&
      token->GetType() == HTMLToken::kStartTag &&
      token->GetName() == svg_names::kSVGTag)
    return false;
  if (HTMLElementStack::IsHTMLIntegrationPoint(adjusted_current_node)) {
    if (token->GetType() == HTMLToken::kStartTag)
      return false;
    if (token->GetType() == HTMLToken::kCharacter)
      return false;
  }
  if (token->GetType() == HTMLToken::kEndOfFile)
    return false;
  return true;
}

void HTMLTreeBuilder::ProcessTokenInForeignContent(AtomicHTMLToken* token) {
  if (token->GetType() == HTMLToken::kCharacter) {
    const String& characters = token->Characters();
    tree_.InsertTextNode(characters);
    if (frameset_ok_ && !IsAllWhitespaceOrReplacementCharacters(characters))
      frameset_ok_ = false;
    return;
  }

  tree_.Flush();
  HTMLStackItem* adjusted_current_node = AdjustedCurrentStackItem();

  switch (token->GetType()) {
    case HTMLToken::kUninitialized:
      NOTREACHED_IN_MIGRATION();
      break;
    case HTMLToken::DOCTYPE:
    // TODO(crbug.com/1453291) This needs to be expanded to properly handle
    // foreign content (e.g. <svg>) inside an element with `parseparts`.
    case HTMLToken::kDOMPart:
      ParseError(token);
      break;
    case HTMLToken::kStartTag: {
      const HTMLTag tag = token->GetHTMLTag();
      switch (tag) {
        case HTMLTag::kFont:
          if (!token->GetAttributeItem(html_names::kColorAttr) &&
              !token->GetAttributeItem(html_names::kFaceAttr) &&
              !token->GetAttributeItem(html_names::kSizeAttr)) {
            break;
          }
          [[fallthrough]];
        case HTMLTag::kB:
        case HTMLTag::kBig:
        case HTMLTag::kBlockquote:
        case HTMLTag::kBody:
        case HTMLTag::kBr:
        case HTMLTag::kCenter:
        case HTMLTag::kCode:
        case HTMLTag::kDd:
        case HTMLTag::kDiv:
        case HTMLTag::kDl:
        case HTMLTag::kDt:
        case HTMLTag::kEm:
        case HTMLTag::kEmbed:
        case NUMBERED_HEADER_CASES:
        case HTMLTag::kHead:
        case HTMLTag::kHr:
        case HTMLTag::kI:
        case HTMLTag::kImg:
        case HTMLTag::kLi:
        case HTMLTag::kListing:
        case HTMLTag::kMenu:
        case HTMLTag::kMeta:
        case HTMLTag::kNobr:
        case HTMLTag::kOl:
        case HTMLTag::kP:
        case HTMLTag::kPre:
        case HTMLTag::kRuby:
        case HTMLTag::kS:
        case HTMLTag::kSmall:
        case HTMLTag::kSpan:
        case HTMLTag::kStrong:
        case HTMLTag::kStrike:
        case HTMLTag::kSub:
        case HTMLTag::kSup:
        case HTMLTag::kTable:
        case HTMLTag::kTt:
        case HTMLTag::kU:
        case HTMLTag::kUl:
        case HTMLTag::kVar:
          ParseError(token);
          tree_.OpenElements()->PopUntilForeignContentScopeMarker();
          ProcessStartTag(token);
          return;
        case HTMLTag::kScript:
          script_to_process_start_position_ = parser_->GetTextPosition();
          break;
        default:
          break;
      }
      const AtomicString& current_namespace =
          adjusted_current_node->NamespaceURI();
      if (current_namespace == mathml_names::kNamespaceURI)
        AdjustMathMLAttributes(token);
      if (current_namespace == svg_names::kNamespaceURI) {
        AdjustSVGTagNameCase(token);
        AdjustSVGAttributes(token);
      }
      AdjustForeignAttributes(token);

      if (tag == HTMLTag::kScript && token->SelfClosing() &&
          current_namespace == svg_names::kNamespaceURI) {
        token->SetSelfClosingToFalse();
        tree_.InsertForeignElement(token, current_namespace);
        AtomicHTMLToken fake_token(HTMLToken::kEndTag, HTMLTag::kScript);
        ProcessTokenInForeignContent(&fake_token);
        return;
      }

      tree_.InsertForeignElement(token, current_namespace);
      break;
    }
    case HTMLToken::kEndTag: {
      if (adjusted_current_node->NamespaceURI() == svg_names::kNamespaceURI)
        AdjustSVGTagNameCase(token);

      if (token->GetName() == svg_names::kScriptTag &&
          tree_.CurrentStackItem()->HasTagName(svg_names::kScriptTag)) {
        if (ScriptingContentIsAllowed(tree_.GetParserContentPolicy()))
          script_to_process_ = tree_.CurrentElement();
        tree_.OpenElements()->Pop();
        return;
      }
      const HTMLTag tag = token->GetHTMLTag();
      if (tag == HTMLTag::kBr || tag == HTMLTag::kP) {
        ParseError(token);
        tree_.OpenElements()->PopUntilForeignContentScopeMarker();
        ProcessEndTag(token);
        return;
      }
      if (!tree_.CurrentStackItem()->IsInHTMLNamespace()) {
        // FIXME: This code just wants an Element* iterator, instead of an
        // HTMLStackItem*
        HTMLStackItem* item = tree_.OpenElements()->TopStackItem();
        if (!item->HasLocalName(token->GetName())) {
          ParseError(token);
        }
        while (true) {
          if (item->HasLocalName(token->GetName())) {
            tree_.OpenElements()->PopUntilPopped(item->GetElement());
            return;
          }
          item = item->NextItemInStack();

          if (item->IsInHTMLNamespace()) {
            break;
          }
        }
      }
      // Otherwise, process the token according to the rules given in the
      // section corresponding to the current insertion mode in HTML content.
      ProcessEndTag(token);
      break;
    }
    case HTMLToken::kComment:
      tree_.InsertComment(token);
      break;
    case HTMLToken::kCharacter:
    case HTMLToken::kEndOfFile:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void HTMLTreeBuilder::Finished() {
  if (IsParsingFragment())
    return;

  DCHECK(template_insertion_modes_.empty());
#if DCHECK_IS_ON()
  DCHECK(is_attached_);
#endif
  // Warning, this may detach the parser. Do not do anything else after this.
  tree_.FinishedParsing();
}

void HTMLTreeBuilder::ParseError(AtomicHTMLToken*) {}

#ifndef NDEBUG
const char* HTMLTreeBuilder::ToString(HTMLTreeBuilder::InsertionMode mode) {
  switch (mode) {
#define DEFINE_STRINGIFY(mode) \
  case mode:                   \
    return #mode;
    DEFINE_STRINGIFY(kInitialMode)
    DEFINE_STRINGIFY(kBeforeHTMLMode)
    DEFINE_STRINGIFY(kBeforeHeadMode)
    DEFINE_STRINGIFY(kInHeadMode)
    DEFINE_STRINGIFY(kInHeadNoscriptMode)
    DEFINE_STRINGIFY(kAfterHeadMode)
    DEFINE_STRINGIFY(kTemplateContentsMode)
    DEFINE_STRINGIFY(kInBodyMode)
    DEFINE_STRINGIFY(kTextMode)
    DEFINE_STRINGIFY(kInTableMode)
    DEFINE_STRINGIFY(kInTableTextMode)
    DEFINE_STRINGIFY(kInCaptionMode)
    DEFINE_STRINGIFY(kInColumnGroupMode)
    DEFINE_STRINGIFY(kInTableBodyMode)
    DEFINE_STRINGIFY(kInRowMode)
    DEFINE_STRINGIFY(kInCellMode)
    DEFINE_STRINGIFY(kInSelectMode)
    DEFINE_STRINGIFY(kInSelectInTableMode)
    DEFINE_STRINGIFY(kAfterBodyMode)
    DEFINE_STRINGIFY(kInFramesetMode)
    DEFINE_STRINGIFY(kAfterFramesetMode)
    DEFINE_STRINGIFY(kAfterAfterBodyMode)
    DEFINE_STRINGIFY(kAfterAfterFramesetMode)
#undef DEFINE_STRINGIFY
  }
  return "<unknown>";
}
#endif

}  // namespace blink
