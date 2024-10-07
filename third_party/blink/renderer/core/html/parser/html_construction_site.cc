/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"

#include <limits>

#include "base/notreached.h"
#include "third_party/blink/renderer/core/dom/attribute_part.h"
#include "third_party/blink/renderer/core/dom/child_node_part.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_part.h"
#include "third_party/blink/renderer/core/dom/template_content_document_fragment.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/throw_on_dynamic_markup_insertion_count_incrementer.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/forms/form_associated.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_reentry_permit.h"
#include "third_party/blink/renderer/core/html/parser/html_stack_item.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html_element_factory.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/script/ignore_destructive_write_count_incrementer.h"
#include "third_party/blink/renderer/core/svg/svg_script_element.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

void HTMLConstructionSite::SetAttributes(Element* element,
                                         AtomicHTMLToken* token) {
  if (!is_scripting_content_allowed_)
    element->StripScriptingAttributes(token->Attributes());
  element->ParserSetAttributes(token->Attributes());
  if (token->HasDuplicateAttribute()) {
    // UseCounter is not free, and only the first call matters. Only call to it
    // if necessary.
    if (!reported_duplicate_attribute_) {
      reported_duplicate_attribute_ = true;
      UseCounter::Count(element->GetDocument(),
                        WebFeature::kDuplicatedAttribute);
    }
    element->SetHasDuplicateAttributes();
  }
}

static bool HasImpliedEndTag(const HTMLStackItem* item) {
  switch (item->GetHTMLTag()) {
    case html_names::HTMLTag::kDd:
    case html_names::HTMLTag::kDt:
    case html_names::HTMLTag::kLi:
    case html_names::HTMLTag::kOption:
    case html_names::HTMLTag::kOptgroup:
    case html_names::HTMLTag::kP:
    case html_names::HTMLTag::kRb:
    case html_names::HTMLTag::kRp:
    case html_names::HTMLTag::kRt:
    case html_names::HTMLTag::kRTC:
      return item->IsHTMLNamespace();
    default:
      return false;
  }
}

static bool ShouldUseLengthLimit(const ContainerNode& node) {
  if (auto* html_element = DynamicTo<HTMLElement>(&node)) {
    return !html_element->HasTagName(html_names::kScriptTag) &&
           !html_element->HasTagName(html_names::kStyleTag);
  }
  return !IsA<SVGScriptElement>(node);
}

static unsigned NextTextBreakPositionForContainer(
    const ContainerNode& node,
    unsigned current_position,
    unsigned string_length,
    std::optional<unsigned>& length_limit) {
  if (string_length < Text::kDefaultLengthLimit)
    return string_length;
  if (!length_limit) {
    length_limit = ShouldUseLengthLimit(node)
                       ? Text::kDefaultLengthLimit
                       : std::numeric_limits<unsigned>::max();
  }
  return std::min(current_position + *length_limit, string_length);
}

static inline WhitespaceMode RecomputeWhiteSpaceMode(
    const StringView& string_view) {
  DCHECK(!string_view.empty());
  if (string_view[0] != '\n') {
    return string_view.IsAllSpecialCharacters<IsHTMLSpace<UChar>>()
               ? WhitespaceMode::kAllWhitespace
               : WhitespaceMode::kNotAllWhitespace;
  }

  auto check_whitespace = [](auto* buffer, size_t length) {
    WhitespaceMode result = WhitespaceMode::kNewlineThenWhitespace;
    for (size_t i = 1; i < length; ++i) {
      if (buffer[i] == ' ') [[likely]] {
        continue;
      } else if (IsHTMLSpecialWhitespace(buffer[i])) {
        result = WhitespaceMode::kAllWhitespace;
      } else {
        return WhitespaceMode::kNotAllWhitespace;
      }
    }
    return result;
  };

  if (string_view.Is8Bit()) {
    return check_whitespace(string_view.Characters8(), string_view.length());
  } else {
    return check_whitespace(string_view.Characters16(), string_view.length());
  }
}

enum class RecomputeMode {
  kDontRecompute,
  kRecomputeIfNeeded,
};

// Strings composed entirely of whitespace are likely to be repeated. Turn them
// into AtomicString so we share a single string for each.
static String CheckWhitespaceAndConvertToString(const StringView& string,
                                                WhitespaceMode whitespace_mode,
                                                RecomputeMode recompute_mode) {
  switch (whitespace_mode) {
    case WhitespaceMode::kNewlineThenWhitespace:
      DCHECK(WTF::NewlineThenWhitespaceStringsTable::IsNewlineThenWhitespaces(
          string));
      if (string.length() <
          WTF::NewlineThenWhitespaceStringsTable::kTableSize) {
        return WTF::NewlineThenWhitespaceStringsTable::GetStringForLength(
            string.length());
      }
      [[fallthrough]];
    case WhitespaceMode::kAllWhitespace:
      return string.ToAtomicString().GetString();
    case WhitespaceMode::kNotAllWhitespace:
      // Other strings are pretty random and unlikely to repeat.
      return string.ToString();
    case WhitespaceMode::kWhitespaceUnknown:
      DCHECK_EQ(RecomputeMode::kRecomputeIfNeeded, recompute_mode);
      return CheckWhitespaceAndConvertToString(string,
                                               RecomputeWhiteSpaceMode(string),
                                               RecomputeMode::kDontRecompute);
  }
}

static String TryCanonicalizeString(const StringView& string,
                                    WhitespaceMode mode) {
  return CheckWhitespaceAndConvertToString(string, mode,
                                           RecomputeMode::kRecomputeIfNeeded);
}

static inline void Insert(HTMLConstructionSiteTask& task) {
  // https://html.spec.whatwg.org/multipage/parsing.html#appropriate-place-for-inserting-a-node
  // 3. If the adjusted insertion location is inside a template element, let it
  // instead be inside the template element's template contents, after its last
  // child (if any).
  if (auto* template_element = DynamicTo<HTMLTemplateElement>(*task.parent)) {
    task.parent = template_element->TemplateContentOrDeclarativeShadowRoot();
    // If the Document was detached in the middle of parsing, The template
    // element won't be able to initialize its contents, so bail out.
    if (!task.parent)
      return;
  }

  // https://html.spec.whatwg.org/C/#insert-a-foreign-element
  // 3.1, (3) Push (pop) an element queue
  CEReactionsScope reactions;
  if (task.next_child)
    task.parent->ParserInsertBefore(task.child.Get(), *task.next_child);
  else
    task.parent->ParserAppendChild(task.child.Get());
}

static inline void ExecuteInsertTask(HTMLConstructionSiteTask& task) {
  DCHECK_EQ(task.operation, HTMLConstructionSiteTask::kInsert);

  Insert(task);
  if (auto* child = DynamicTo<Element>(task.child.Get())) {
    child->BeginParsingChildren();
    if (task.self_closing)
      child->FinishParsingChildren();
  }
}

static inline unsigned TextFitsInContainer(const ContainerNode& node,
                                           unsigned length) {
  // Common case is all text fits in the default text limit. Only lookup length
  // limit when necessary as it is costly.
  return length < Text::kDefaultLengthLimit || !ShouldUseLengthLimit(node);
}

static inline void ExecuteInsertTextTask(HTMLConstructionSiteTask& task) {
  DCHECK_EQ(task.operation, HTMLConstructionSiteTask::kInsertText);

  // Merge text nodes into previous ones if possible:
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#insert-a-character
  auto* new_text = To<Text>(task.child.Get());
  Node* previous_child = task.next_child ? task.next_child->previousSibling()
                                         : task.parent->lastChild();
  if (auto* previous_text = DynamicTo<Text>(previous_child)) {
    if (TextFitsInContainer(*task.parent,
                            previous_text->length() + new_text->length())) {
      previous_text->ParserAppendData(new_text->data());
      return;
    }
  }

  Insert(task);
}

static inline void ExecuteReparentTask(HTMLConstructionSiteTask& task) {
  DCHECK_EQ(task.operation, HTMLConstructionSiteTask::kReparent);

  task.parent->ParserAppendChild(task.child);
}

static inline void ExecuteInsertAlreadyParsedChildTask(
    HTMLConstructionSiteTask& task) {
  DCHECK_EQ(task.operation,
            HTMLConstructionSiteTask::kInsertAlreadyParsedChild);

  Insert(task);
}

static inline void ExecuteTakeAllChildrenTask(HTMLConstructionSiteTask& task) {
  DCHECK_EQ(task.operation, HTMLConstructionSiteTask::kTakeAllChildren);

  task.parent->ParserTakeAllChildrenFrom(*task.OldParent());
}

void HTMLConstructionSite::ExecuteTask(HTMLConstructionSiteTask& task) {
  DCHECK(task_queue_.empty());
  if (task.operation == HTMLConstructionSiteTask::kInsert) {
    ExecuteInsertTask(task);
    if (pending_dom_parts_) {
      if (RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
        if (task.dom_parts_needed.needs_node_part) {
          // Just mark the node as having a node part.
          task.child->SetHasNodePart();
        }
      } else {
        pending_dom_parts_->ConstructDOMPartsIfNeeded(*task.child,
                                                      task.dom_parts_needed);
      }
    }
    return;
  }

  if (task.operation == HTMLConstructionSiteTask::kInsertText) {
    ExecuteInsertTextTask(task);
    if (pending_dom_parts_) {
      if (RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
        if (task.dom_parts_needed.needs_node_part) {
          // Just mark the node as having a node part.
          task.child->SetHasNodePart();
        }
      } else {
        pending_dom_parts_->ConstructDOMPartsIfNeeded(*task.child,
                                                      task.dom_parts_needed);
      }
    }
    return;
  }

  // All the cases below this point are only used by the adoption agency.
  DCHECK(!task.dom_parts_needed);

  if (task.operation == HTMLConstructionSiteTask::kInsertAlreadyParsedChild)
    return ExecuteInsertAlreadyParsedChildTask(task);

  if (task.operation == HTMLConstructionSiteTask::kReparent)
    return ExecuteReparentTask(task);

  if (task.operation == HTMLConstructionSiteTask::kTakeAllChildren)
    return ExecuteTakeAllChildrenTask(task);

  NOTREACHED_IN_MIGRATION();
}

// This is only needed for TextDocuments where we might have text nodes
// approaching the default length limit (~64k) and we don't want to break a text
// node in the middle of a combining character.
static unsigned FindBreakIndexBetween(const StringBuilder& string,
                                      unsigned current_position,
                                      unsigned proposed_break_index) {
  DCHECK_LT(current_position, proposed_break_index);
  DCHECK_LE(proposed_break_index, string.length());
  // The end of the string is always a valid break.
  if (proposed_break_index == string.length())
    return proposed_break_index;

  // Latin-1 does not have breakable boundaries. If we ever moved to a different
  // 8-bit encoding this could be wrong.
  if (string.Is8Bit())
    return proposed_break_index;

  const UChar* break_search_characters =
      string.Characters16() + current_position;
  // We need at least two characters look-ahead to account for UTF-16
  // surrogates, but can't search off the end of the buffer!
  unsigned break_search_length =
      std::min(proposed_break_index - current_position + 2,
               string.length() - current_position);
  NonSharedCharacterBreakIterator it(break_search_characters,
                                     break_search_length);

  if (it.IsBreak(proposed_break_index - current_position))
    return proposed_break_index;

  int adjusted_break_index_in_substring =
      it.Preceding(proposed_break_index - current_position);
  if (adjusted_break_index_in_substring > 0)
    return current_position + adjusted_break_index_in_substring;
  // We failed to find a breakable point, let the caller figure out what to do.
  return 0;
}

void HTMLConstructionSite::FlushPendingText() {
  if (pending_text_.IsEmpty())
    return;

  // Splitting text nodes into smaller chunks contradicts HTML5 spec, but is
  // necessary for performance, see:
  // https://bugs.webkit.org/show_bug.cgi?id=55898

  // Lazily determine the line limit as it's non-trivial, and in the typical
  // case not necessary. Note that this is faster than using a ternary operator
  // to determine limit.
  std::optional<unsigned> length_limit;

  unsigned current_position = 0;
  const StringBuilder& string = pending_text_.string_builder;
  while (current_position < string.length()) {
    unsigned proposed_break_index = NextTextBreakPositionForContainer(
        *pending_text_.parent, current_position, string.length(), length_limit);
    unsigned break_index =
        FindBreakIndexBetween(string, current_position, proposed_break_index);
    DCHECK_LE(break_index, string.length());
    if (!break_index) {
      // FindBreakIndexBetween returns 0 if it cannot find a breakpoint. In this
      // case, just keep the entire string.
      break_index = string.length();
    }
    unsigned substring_view_length = break_index - current_position;
    StringView substring_view;
    if (!current_position && substring_view_length >= string.length())
        [[likely]] {
      substring_view = string;
    } else {
      substring_view = string.SubstringView(current_position,
                                            break_index - current_position);
    }
    String substring =
        TryCanonicalizeString(substring_view, pending_text_.whitespace_mode);

    DCHECK_GT(break_index, current_position);
    DCHECK_EQ(break_index - current_position, substring.length());
    HTMLConstructionSiteTask task(HTMLConstructionSiteTask::kInsertText);
    task.parent = pending_text_.parent;
    task.next_child = pending_text_.next_child;
    task.child = Text::Create(task.parent->GetDocument(), std::move(substring));
    QueueTask(task, false);
    DCHECK_EQ(To<Text>(task.child.Get())->length(),
              break_index - current_position);
    current_position = break_index;
  }
  pending_text_.Discard();
}

void HTMLConstructionSite::QueueTask(const HTMLConstructionSiteTask& task,
                                     bool flush_pending_text) {
  if (flush_pending_text)
    FlushPendingText();
  task_queue_.push_back(task);
}

void HTMLConstructionSite::AttachLater(ContainerNode* parent,
                                       Node* child,
                                       const DOMPartsNeeded& dom_parts_needed,
                                       bool self_closing) {
  auto* element = DynamicTo<Element>(child);
  DCHECK(is_scripting_content_allowed_ || !element ||
         !element->IsScriptElement());
  DCHECK(PluginContentIsAllowed(parser_content_policy_) ||
         !IsA<HTMLPlugInElement>(child));

  HTMLConstructionSiteTask task(HTMLConstructionSiteTask::kInsert);
  task.parent = parent;
  task.child = child;
  task.self_closing = self_closing;
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled() || !dom_parts_needed);
  task.dom_parts_needed = dom_parts_needed;

  if (ShouldFosterParent()) {
    FosterParent(task.child);
    return;
  }

  // Add as a sibling of the parent if we have reached the maximum depth
  // allowed.
  if (open_elements_.StackDepth() > kMaximumHTMLParserDOMTreeDepth &&
      task.parent->parentNode()) {
    UseCounter::Count(OwnerDocumentForCurrentNode(),
                      WebFeature::kMaximumHTMLParserDOMTreeDepthHit);
    task.parent = task.parent->parentNode();
  }

  DCHECK(task.parent);
  QueueTask(task, true);
}

void HTMLConstructionSite::ExecuteQueuedTasks() {
  // This has no affect on pendingText, and we may have pendingText remaining
  // after executing all other queued tasks.
  const size_t size = task_queue_.size();
  if (!size)
    return;

  // Fast path for when |size| is 1, which is the common case
  if (size == 1) {
    HTMLConstructionSiteTask task = task_queue_.front();
    task_queue_.pop_back();
    ExecuteTask(task);
    return;
  }

  // Copy the task queue into a local variable in case executeTask re-enters the
  // parser.
  TaskQueue queue;
  queue.swap(task_queue_);

  for (auto& task : queue)
    ExecuteTask(task);

  // We might be detached now.
}

HTMLConstructionSite::HTMLConstructionSite(
    HTMLParserReentryPermit* reentry_permit,
    Document& document,
    ParserContentPolicy parser_content_policy,
    DocumentFragment* fragment,
    Element* context_element)
    : reentry_permit_(reentry_permit),
      document_(&document),
      attachment_root_(fragment ? fragment
                                : static_cast<ContainerNode*>(&document)),
      pending_dom_parts_(
          RuntimeEnabledFeatures::DOMPartsAPIEnabled()
              ? MakeGarbageCollected<PendingDOMParts>(attachment_root_)
              : nullptr),
      parser_content_policy_(parser_content_policy),
      is_scripting_content_allowed_(
          ScriptingContentIsAllowed(parser_content_policy)),
      is_parsing_fragment_(fragment),
      redirect_attach_to_foster_parent_(false),
      in_quirks_mode_(document.InQuirksMode()) {
  DCHECK(document_->IsHTMLDocument() || document_->IsXHTMLDocument() ||
         is_parsing_fragment_);

  DCHECK_EQ(!fragment, !context_element);
  if (fragment) {
    DCHECK_EQ(document_, &fragment->GetDocument());
    DCHECK_EQ(in_quirks_mode_, fragment->GetDocument().InQuirksMode());
    if (!context_element->GetDocument().IsTemplateDocument()) {
      form_ = Traversal<HTMLFormElement>::FirstAncestorOrSelf(*context_element);
    }
  }
}

HTMLConstructionSite::~HTMLConstructionSite() {
  // Depending on why we're being destroyed it might be OK to forget queued
  // tasks, but currently we don't expect to.
  DCHECK(task_queue_.empty());
  // Currently we assume that text will never be the last token in the document
  // and that we'll always queue some additional task to cause it to flush.
  DCHECK(pending_text_.IsEmpty());
}

void HTMLConstructionSite::Trace(Visitor* visitor) const {
  visitor->Trace(reentry_permit_);
  visitor->Trace(document_);
  visitor->Trace(attachment_root_);
  visitor->Trace(head_);
  visitor->Trace(form_);
  visitor->Trace(open_elements_);
  visitor->Trace(active_formatting_elements_);
  visitor->Trace(task_queue_);
  visitor->Trace(pending_text_);
  visitor->Trace(pending_dom_parts_);
}

void HTMLConstructionSite::Detach() {
  // FIXME: We'd like to ASSERT here that we're canceling and not just
  // discarding text that really should have made it into the DOM earlier, but
  // there doesn't seem to be a nice way to do that.
  pending_text_.Discard();
  document_ = nullptr;
  attachment_root_ = nullptr;
}

HTMLFormElement* HTMLConstructionSite::TakeForm() {
  return form_.Release();
}

void HTMLConstructionSite::InsertHTMLHtmlStartTagBeforeHTML(
    AtomicHTMLToken* token) {
  DCHECK(document_);
  HTMLHtmlElement* element;
  if (const auto* is_attribute = token->GetAttributeItem(html_names::kIsAttr)) {
    element = To<HTMLHtmlElement>(document_->CreateElement(
        html_names::kHTMLTag, GetCreateElementFlags(), is_attribute->Value()));
  } else {
    element = MakeGarbageCollected<HTMLHtmlElement>(*document_);
  }
  SetAttributes(element, token);
  AttachLater(attachment_root_, element, token->GetDOMPartsNeeded());
  open_elements_.PushHTMLHtmlElement(HTMLStackItem::Create(element, token));

  ExecuteQueuedTasks();
  element->InsertedByParser();
}

void HTMLConstructionSite::MergeAttributesFromTokenIntoElement(
    AtomicHTMLToken* token,
    Element* element) {
  if (token->Attributes().empty())
    return;

  for (const auto& token_attribute : token->Attributes()) {
    if (element->AttributesWithoutUpdate().FindIndex(
            token_attribute.GetName()) == kNotFound)
      element->setAttribute(token_attribute.GetName(), token_attribute.Value());
  }

  element->HideNonce();
}

void HTMLConstructionSite::InsertHTMLHtmlStartTagInBody(
    AtomicHTMLToken* token) {
  // Fragments do not have a root HTML element, so any additional HTML elements
  // encountered during fragment parsing should be ignored.
  if (is_parsing_fragment_)
    return;

  MergeAttributesFromTokenIntoElement(token, open_elements_.HtmlElement());
}

void HTMLConstructionSite::InsertHTMLBodyStartTagInBody(
    AtomicHTMLToken* token) {
  MergeAttributesFromTokenIntoElement(token, open_elements_.BodyElement());
}

void HTMLConstructionSite::SetDefaultCompatibilityMode() {
  if (is_parsing_fragment_)
    return;
  SetCompatibilityMode(Document::kQuirksMode);
}

void HTMLConstructionSite::SetCompatibilityMode(
    Document::CompatibilityMode mode) {
  in_quirks_mode_ = (mode == Document::kQuirksMode);
  document_->SetCompatibilityMode(mode);
}

void HTMLConstructionSite::SetCompatibilityModeFromDoctype(
    html_names::HTMLTag tag,
    const String& public_id,
    const String& system_id) {
  // There are three possible compatibility modes:
  // Quirks - quirks mode emulates WinIE and NS4. CSS parsing is also relaxed in
  // this mode, e.g., unit types can be omitted from numbers.
  // Limited Quirks - This mode is identical to no-quirks mode except for its
  // treatment of line-height in the inline box model.
  // No Quirks - no quirks apply. Web pages will obey the specifications to the
  // letter.

  DCHECK(document_->IsHTMLDocument() || document_->IsXHTMLDocument());

  // Check for Quirks Mode.
  if (tag != html_names::HTMLTag::kHTML ||
      public_id.StartsWithIgnoringASCIICase(
          "+//Silmaril//dtd html Pro v0r11 19970101//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//AdvaSoft Ltd//DTD HTML 3.0 asWedit + extensions//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//AS//DTD HTML 3.0 asWedit + extensions//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//IETF//DTD HTML 2.0 Level 1//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//IETF//DTD HTML 2.0 Level 2//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//IETF//DTD HTML 2.0 Strict Level 1//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//IETF//DTD HTML 2.0 Strict Level 2//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML 2.0 Strict//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML 2.0//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML 2.1E//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML 3.0//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML 3.2 Final//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML 3.2//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML 3//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML Level 0//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML Level 1//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML Level 2//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML Level 3//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//IETF//DTD HTML Strict Level 0//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//IETF//DTD HTML Strict Level 1//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//IETF//DTD HTML Strict Level 2//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//IETF//DTD HTML Strict Level 3//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML Strict//") ||
      public_id.StartsWithIgnoringASCIICase("-//IETF//DTD HTML//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Metrius//DTD Metrius Presentational//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Microsoft//DTD Internet Explorer 2.0 HTML Strict//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Microsoft//DTD Internet Explorer 2.0 HTML//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Microsoft//DTD Internet Explorer 2.0 Tables//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Microsoft//DTD Internet Explorer 3.0 HTML Strict//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Microsoft//DTD Internet Explorer 3.0 HTML//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Microsoft//DTD Internet Explorer 3.0 Tables//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Netscape Comm. Corp.//DTD HTML//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Netscape Comm. Corp.//DTD Strict HTML//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//O'Reilly and Associates//DTD HTML 2.0//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//O'Reilly and Associates//DTD HTML Extended 1.0//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//O'Reilly and Associates//DTD HTML Extended Relaxed 1.0//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//SoftQuad Software//DTD HoTMetaL PRO "
          "6.0::19990601::extensions to HTML 4.0//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//SoftQuad//DTD HoTMetaL PRO "
          "4.0::19971010::extensions to HTML 4.0//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Spyglass//DTD HTML 2.0 Extended//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//SQ//DTD HTML 2.0 HoTMetaL + extensions//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Sun Microsystems Corp.//DTD HotJava HTML//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//Sun Microsystems Corp.//DTD HotJava Strict HTML//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//W3C//DTD HTML 3 1995-03-24//") ||
      public_id.StartsWithIgnoringASCIICase("-//W3C//DTD HTML 3.2 Draft//") ||
      public_id.StartsWithIgnoringASCIICase("-//W3C//DTD HTML 3.2 Final//") ||
      public_id.StartsWithIgnoringASCIICase("-//W3C//DTD HTML 3.2//") ||
      public_id.StartsWithIgnoringASCIICase("-//W3C//DTD HTML 3.2S Draft//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//W3C//DTD HTML 4.0 Frameset//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//W3C//DTD HTML 4.0 Transitional//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//W3C//DTD HTML Experimental 19960712//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//W3C//DTD HTML Experimental 970421//") ||
      public_id.StartsWithIgnoringASCIICase("-//W3C//DTD W3 HTML//") ||
      public_id.StartsWithIgnoringASCIICase("-//W3O//DTD W3 HTML 3.0//") ||
      EqualIgnoringASCIICase(public_id,
                             "-//W3O//DTD W3 HTML Strict 3.0//EN//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//WebTechs//DTD Mozilla HTML 2.0//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//WebTechs//DTD Mozilla HTML//") ||
      EqualIgnoringASCIICase(public_id, "-/W3C/DTD HTML 4.0 Transitional/EN") ||
      EqualIgnoringASCIICase(public_id, "HTML") ||
      EqualIgnoringASCIICase(
          system_id,
          "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd") ||
      (system_id.empty() && public_id.StartsWithIgnoringASCIICase(
                                "-//W3C//DTD HTML 4.01 Frameset//")) ||
      (system_id.empty() && public_id.StartsWithIgnoringASCIICase(
                                "-//W3C//DTD HTML 4.01 Transitional//"))) {
    SetCompatibilityMode(Document::kQuirksMode);
    return;
  }

  // Check for Limited Quirks Mode.
  if (public_id.StartsWithIgnoringASCIICase(
          "-//W3C//DTD XHTML 1.0 Frameset//") ||
      public_id.StartsWithIgnoringASCIICase(
          "-//W3C//DTD XHTML 1.0 Transitional//") ||
      (!system_id.empty() && public_id.StartsWithIgnoringASCIICase(
                                 "-//W3C//DTD HTML 4.01 Frameset//")) ||
      (!system_id.empty() && public_id.StartsWithIgnoringASCIICase(
                                 "-//W3C//DTD HTML 4.01 Transitional//"))) {
    SetCompatibilityMode(Document::kLimitedQuirksMode);
    return;
  }

  // Otherwise we are No Quirks Mode.
  SetCompatibilityMode(Document::kNoQuirksMode);
}

void HTMLConstructionSite::ProcessEndOfFile() {
  DCHECK(CurrentNode());
  Flush();
  OpenElements()->PopAll();
}

void HTMLConstructionSite::FinishedParsing() {
  // We shouldn't have any queued tasks but we might have pending text which we
  // need to promote to tasks and execute.
  DCHECK(task_queue_.empty());
  Flush();
  document_->FinishedParsing();
}

void HTMLConstructionSite::InsertDoctype(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::DOCTYPE);

  const String& public_id =
      StringImpl::Create8BitIfPossible(token->PublicIdentifier());
  const String& system_id =
      StringImpl::Create8BitIfPossible(token->SystemIdentifier());
  auto* doctype = MakeGarbageCollected<DocumentType>(
      document_, token->GetName(), public_id, system_id);
  AttachLater(attachment_root_, doctype);

  // DOCTYPE nodes are only processed when parsing fragments w/o
  // contextElements, which never occurs.  However, if we ever chose to support
  // such, this code is subtly wrong, because context-less fragments can
  // determine their own quirks mode, and thus change parsing rules (like <p>
  // inside <table>).  For now we ASSERT that we never hit this code in a
  // fragment, as changing the owning document's compatibility mode would be
  // wrong.
  DCHECK(!is_parsing_fragment_);
  if (is_parsing_fragment_)
    return;

  if (token->ForceQuirks())
    SetCompatibilityMode(Document::kQuirksMode);
  else {
    SetCompatibilityModeFromDoctype(token->GetHTMLTag(), public_id, system_id);
  }
}

void HTMLConstructionSite::InsertComment(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kComment);
  auto comment = token->Comment();
  Comment& comment_node =
      *Comment::Create(OwnerDocumentForCurrentNode(), comment);
  AttachLater(CurrentNode(), &comment_node);
}

void HTMLConstructionSite::InsertDOMPart(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kDOMPart);
  CHECK(pending_dom_parts_);
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
  DCHECK(InParsePartsScope());
  // Insert an empty comment in place of the part token.
  Comment& comment_node = *Comment::Create(OwnerDocumentForCurrentNode(), "");
  if (RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
    // Just set a bit on this comment node that it has a NodePart, and change
    // the content of the comment to kChildNodePartStartCommentData or
    // kChildNodePartEndCommentData, as appropriate.
    comment_node.SetHasNodePart();
    switch (token->DOMPartType()) {
      case DOMPartTokenType::kChildNodePartStart:
        comment_node.setData(kChildNodePartStartCommentData);
        break;
      case DOMPartTokenType::kChildNodePartEnd:
        comment_node.setData(kChildNodePartEndCommentData);
        break;
    }
  } else {
    switch (token->DOMPartType()) {
      case DOMPartTokenType::kChildNodePartStart:
        pending_dom_parts_->AddChildNodePartStart(comment_node,
                                                  token->DOMPartMetadata());
        break;
      case DOMPartTokenType::kChildNodePartEnd:
        pending_dom_parts_->AddChildNodePartEnd(comment_node);
        break;
    }
  }
  AttachLater(CurrentNode(), &comment_node);
}

void HTMLConstructionSite::InsertCommentOnDocument(AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kComment);
  DCHECK(document_);
  AttachLater(attachment_root_, Comment::Create(*document_, token->Comment()));
}

void HTMLConstructionSite::InsertCommentOnHTMLHtmlElement(
    AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kComment);
  ContainerNode* parent = open_elements_.RootNode();
  AttachLater(parent, Comment::Create(parent->GetDocument(), token->Comment()));
}

void HTMLConstructionSite::InsertHTMLHeadElement(AtomicHTMLToken* token) {
  DCHECK(!ShouldFosterParent());
  head_ = HTMLStackItem::Create(
      CreateElement(token, html_names::xhtmlNamespaceURI), token);
  AttachLater(CurrentNode(), head_->GetElement(), token->GetDOMPartsNeeded());
  open_elements_.PushHTMLHeadElement(head_);
}

void HTMLConstructionSite::InsertHTMLBodyElement(AtomicHTMLToken* token) {
  DCHECK(!ShouldFosterParent());
  Element* body = CreateElement(token, html_names::xhtmlNamespaceURI);
  AttachLater(CurrentNode(), body, token->GetDOMPartsNeeded());
  open_elements_.PushHTMLBodyElement(HTMLStackItem::Create(body, token));
  if (document_)
    document_->WillInsertBody();
}

void HTMLConstructionSite::InsertHTMLFormElement(AtomicHTMLToken* token,
                                                 bool is_demoted) {
  auto* form_element =
      To<HTMLFormElement>(CreateElement(token, html_names::xhtmlNamespaceURI));
  if (!OpenElements()->HasTemplateInHTMLScope())
    form_ = form_element;
  if (is_demoted) {
    UseCounter::Count(OwnerDocumentForCurrentNode(),
                      WebFeature::kDemotedFormElement);
  }
  AttachLater(CurrentNode(), form_element, token->GetDOMPartsNeeded());
  open_elements_.Push(HTMLStackItem::Create(form_element, token));
}

void HTMLConstructionSite::InsertHTMLTemplateElement(
    AtomicHTMLToken* token,
    String declarative_shadow_root_mode) {
  // Regardless of whether a declarative shadow root is being attached, the
  // template element is always created. If the template is a valid declarative
  // Shadow Root (has a valid attribute value and parent element), then the
  // template is only added to the stack of open elements, but is not attached
  // to the DOM tree.
  auto* template_element = To<HTMLTemplateElement>(
      CreateElement(token, html_names::xhtmlNamespaceURI));
  HTMLStackItem* template_stack_item =
      HTMLStackItem::Create(template_element, token);
  bool should_attach_template = true;
  if (!declarative_shadow_root_mode.IsNull() &&
      IsA<Element>(open_elements_.TopStackItem()->GetNode())) {
    auto focus_delegation = template_stack_item->GetAttributeItem(
                                html_names::kShadowrootdelegatesfocusAttr)
                                ? FocusDelegation::kDelegateFocus
                                : FocusDelegation::kNone;
    // TODO(crbug.com/1063157): Add an attribute for imperative slot
    // assignment.
    auto slot_assignment_mode = SlotAssignmentMode::kNamed;
    bool serializable =
        template_stack_item->GetAttributeItem(
            html_names::kShadowrootserializableAttr);
    bool clonable = template_stack_item->GetAttributeItem(
        html_names::kShadowrootclonableAttr);
    const auto* reference_target_attr =
        RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled()
            ? template_stack_item->GetAttributeItem(
                  html_names::kShadowrootreferencetargetAttr)
            : nullptr;
    const auto& reference_target =
        reference_target_attr ? reference_target_attr->Value() : g_null_atom;
    HTMLStackItem* shadow_host_stack_item = open_elements_.TopStackItem();
    Element* host = shadow_host_stack_item->GetElement();

    bool success = host->AttachDeclarativeShadowRoot(
        *template_element, declarative_shadow_root_mode, focus_delegation,
        slot_assignment_mode, serializable, clonable, reference_target);
    // If the shadow root attachment fails, e.g. if the host element isn't a
    // valid shadow host, then we leave should_attach_template true, so that
    // a "normal" template element gets attached to the DOM tree.
    if (success) {
      DCHECK(host->AuthorShadowRoot());
      UseCounter::Count(host->GetDocument(),
                        WebFeature::kStreamingDeclarativeShadowDOM);
      should_attach_template = false;
      template_element->SetDeclarativeShadowRoot(*host->AuthorShadowRoot());
    }
  }
  if (should_attach_template) {
    // Attach a normal template element.
    AttachLater(CurrentNode(), template_element, token->GetDOMPartsNeeded());
    DocumentFragment* template_content = template_element->content();
    if (pending_dom_parts_ && template_content &&
        !RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
      pending_dom_parts_->PushPartRoot(&template_content->getPartRoot());
    }
  }
  open_elements_.Push(template_stack_item);
}

void HTMLConstructionSite::InsertHTMLElement(AtomicHTMLToken* token) {
  Element* element = CreateElement(token, html_names::xhtmlNamespaceURI);
  AttachLater(CurrentNode(), element, token->GetDOMPartsNeeded());
  open_elements_.Push(HTMLStackItem::Create(element, token));
}

void HTMLConstructionSite::InsertSelfClosingHTMLElementDestroyingToken(
    AtomicHTMLToken* token) {
  DCHECK_EQ(token->GetType(), HTMLToken::kStartTag);
  // Normally HTMLElementStack is responsible for calling finishParsingChildren,
  // but self-closing elements are never in the element stack so the stack
  // doesn't get a chance to tell them that we're done parsing their children.
  AttachLater(CurrentNode(),
              CreateElement(token, html_names::xhtmlNamespaceURI),
              token->GetDOMPartsNeeded(), /*self_closing*/ true);
  // FIXME: Do we want to acknowledge the token's self-closing flag?
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#acknowledge-self-closing-flag
}

void HTMLConstructionSite::InsertFormattingElement(AtomicHTMLToken* token) {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#the-stack-of-open-elements
  // Possible active formatting elements include:
  // a, b, big, code, em, font, i, nobr, s, small, strike, strong, tt, and u.
  InsertHTMLElement(token);
  active_formatting_elements_.Append(CurrentStackItem());
}

void HTMLConstructionSite::InsertScriptElement(AtomicHTMLToken* token) {
  CreateElementFlags flags;
  bool should_be_parser_inserted =
      parser_content_policy_ !=
      kAllowScriptingContentAndDoNotMarkAlreadyStarted;
  flags
      // http://www.whatwg.org/specs/web-apps/current-work/multipage/scripting-1.html#already-started
      // http://html5.org/specs/dom-parsing.html#dom-range-createcontextualfragment
      // For createContextualFragment, the specifications say to mark it
      // parser-inserted and already-started and later unmark them. However, we
      // short circuit that logic to avoid the subtree traversal to find script
      // elements since scripts can never see those flags or effects thereof.
      .SetCreatedByParser(should_be_parser_inserted,
                          should_be_parser_inserted ? document_ : nullptr)
      .SetAlreadyStarted(is_parsing_fragment_ && flags.IsCreatedByParser());
  HTMLScriptElement* element = nullptr;
  if (const auto* is_attribute = token->GetAttributeItem(html_names::kIsAttr)) {
    element = To<HTMLScriptElement>(OwnerDocumentForCurrentNode().CreateElement(
        html_names::kScriptTag, flags, is_attribute->Value()));
  } else {
    element = MakeGarbageCollected<HTMLScriptElement>(
        OwnerDocumentForCurrentNode(), flags);
  }
  SetAttributes(element, token);
  if (is_scripting_content_allowed_)
    AttachLater(CurrentNode(), element, token->GetDOMPartsNeeded());
  open_elements_.Push(HTMLStackItem::Create(element, token));
}

void HTMLConstructionSite::InsertForeignElement(
    AtomicHTMLToken* token,
    const AtomicString& namespace_uri) {
  DCHECK_EQ(token->GetType(), HTMLToken::kStartTag);
  // parseError when xmlns or xmlns:xlink are wrong.
  DVLOG(1) << "Not implemented.";

  Element* element = CreateElement(token, namespace_uri);
  if (is_scripting_content_allowed_ || !element->IsScriptElement()) {
    DCHECK(!token->GetDOMPartsNeeded());
    AttachLater(CurrentNode(), element, /*dom_parts_needed*/ {},
                token->SelfClosing());
  }
  if (!token->SelfClosing()) {
    open_elements_.Push(HTMLStackItem::Create(element, token, namespace_uri));
  }
}

void HTMLConstructionSite::InsertTextNode(const StringView& string,
                                          WhitespaceMode whitespace_mode) {
  HTMLConstructionSiteTask dummy_task(HTMLConstructionSiteTask::kInsert);
  dummy_task.parent = CurrentNode();

  if (ShouldFosterParent())
    FindFosterSite(dummy_task);

  if (auto* template_element =
          DynamicTo<HTMLTemplateElement>(*dummy_task.parent)) {
    // If the Document was detached in the middle of parsing, the template
    // element won't be able to initialize its contents.
    if (auto* content =
            template_element->TemplateContentOrDeclarativeShadowRoot()) {
      dummy_task.parent = content;
    }
  }

  // Unclear when parent != case occurs. Somehow we insert text into two
  // separate nodes while processing the same Token. The nextChild !=
  // dummy.nextChild case occurs whenever foster parenting happened and we hit a
  // new text node "<table>a</table>b" In either case we have to flush the
  // pending text into the task queue before making more.
  if (!pending_text_.IsEmpty() &&
      (pending_text_.parent != dummy_task.parent ||
       pending_text_.next_child != dummy_task.next_child))
    FlushPendingText();
  pending_text_.Append(dummy_task.parent, dummy_task.next_child, string,
                       whitespace_mode);
}

void HTMLConstructionSite::Reparent(HTMLStackItem* new_parent,
                                    HTMLStackItem* child) {
  HTMLConstructionSiteTask task(HTMLConstructionSiteTask::kReparent);
  task.parent = new_parent->GetNode();
  task.child = child->GetNode();
  QueueTask(task, true);
}

void HTMLConstructionSite::InsertAlreadyParsedChild(HTMLStackItem* new_parent,
                                                    HTMLStackItem* child) {
  if (new_parent->CausesFosterParenting()) {
    FosterParent(child->GetNode());
    return;
  }

  HTMLConstructionSiteTask task(
      HTMLConstructionSiteTask::kInsertAlreadyParsedChild);
  task.parent = new_parent->GetNode();
  task.child = child->GetNode();
  QueueTask(task, true);
}

void HTMLConstructionSite::TakeAllChildren(HTMLStackItem* new_parent,
                                           HTMLStackItem* old_parent) {
  HTMLConstructionSiteTask task(HTMLConstructionSiteTask::kTakeAllChildren);
  task.parent = new_parent->GetNode();
  task.child = old_parent->GetNode();
  QueueTask(task, true);
}

CreateElementFlags HTMLConstructionSite::GetCreateElementFlags() const {
  return is_parsing_fragment_ ? CreateElementFlags::ByFragmentParser(document_)
                              : CreateElementFlags::ByParser(document_);
}

Document& HTMLConstructionSite::OwnerDocumentForCurrentNode() {
  // TODO(crbug.com/1070667): For <template> elements, many operations need to
  // be re-targeted to the .content() document of the template. This function is
  // used in those places. The spec needs to be updated to reflect this
  // behavior, and when that happens, a link to the spec should be placed here.
  if (auto* template_element = DynamicTo<HTMLTemplateElement>(*CurrentNode())) {
    // If the Document was detached in the middle of parsing, The template
    // element won't be able to initialize its contents. Fallback to the
    // current node's document in that case..
    if (auto* content =
            template_element->TemplateContentOrDeclarativeShadowRoot()) {
      return content->GetDocument();
    }
  }
  return CurrentNode()->GetDocument();
}

// "look up a custom element definition" for a token
// https://html.spec.whatwg.org/C/#look-up-a-custom-element-definition
// static
CustomElementDefinition* HTMLConstructionSite::LookUpCustomElementDefinition(
    Document& document,
    const QualifiedName& tag_name,
    const AtomicString& is) {
  // "1. If namespace is not the HTML namespace, return null."
  if (tag_name.NamespaceURI() != html_names::xhtmlNamespaceURI)
    return nullptr;

  // "2. If document does not have a browsing context, return null."
  LocalDOMWindow* window = document.domWindow();
  if (!window)
    return nullptr;

  // "3. Let registry be document's browsing context's Window's
  // CustomElementRegistry object."
  CustomElementRegistry* registry = window->MaybeCustomElements();
  if (!registry)
    return nullptr;

  const AtomicString& local_name = tag_name.LocalName();
  const AtomicString& name = !is.IsNull() ? is : local_name;
  CustomElementDescriptor descriptor(name, local_name);

  // 4.-6.
  return registry->DefinitionFor(descriptor);
}

// "create an element for a token"
// https://html.spec.whatwg.org/C/#create-an-element-for-the-token
Element* HTMLConstructionSite::CreateElement(
    AtomicHTMLToken* token,
    const AtomicString& namespace_uri) {
  // "1. Let document be intended parent's node document."
  Document& document = OwnerDocumentForCurrentNode();

  // "2. Let local name be the tag name of the token."
  QualifiedName tag_name =
      ((token->IsValidHTMLTag() &&
        namespace_uri == html_names::xhtmlNamespaceURI)
           ? static_cast<const QualifiedName&>(
                 html_names::TagToQualifiedName(token->GetHTMLTag()))
           : QualifiedName(g_null_atom, token->GetName(), namespace_uri));
  // "3. Let is be the value of the "is" attribute in the given token ..." etc.
  const Attribute* is_attribute = token->GetAttributeItem(html_names::kIsAttr);
  const AtomicString& is = is_attribute ? is_attribute->Value() : g_null_atom;
  // "4. Let definition be the result of looking up a custom element ..." etc.
  auto* definition = LookUpCustomElementDefinition(document, tag_name, is);
  // "5. If definition is non-null and the parser was not originally created
  // for the HTML fragment parsing algorithm, then let will execute script
  // be true."
  bool will_execute_script = definition && !is_parsing_fragment_;

  Element* element;

  // This check and the steps inside are duplicated in
  // XMLDocumentParser::StartElementNs.
  if (will_execute_script) {
    // "6.1 Increment the document's throw-on-dynamic-insertion counter."
    ThrowOnDynamicMarkupInsertionCountIncrementer
        throw_on_dynamic_markup_insertions(&document);

    // "6.2 If the JavaScript execution context stack is empty,
    // then perform a microtask checkpoint."

    // TODO(dominicc): This is the way the Blink HTML parser performs
    // checkpoints, but note the spec is different--it talks about the
    // JavaScript stack, not the script nesting level.
    if (0u == reentry_permit_->ScriptNestingLevel())
      document.GetAgent().event_loop()->PerformMicrotaskCheckpoint();

    // "6.3 Push a new element queue onto the custom element
    // reactions stack."
    CEReactionsScope reactions;

    // "7. Let element be the result of creating an element given document,
    // localName, given namespace, null, and is. If will execute script is true,
    // set the synchronous custom elements flag; otherwise, leave it unset."
    // TODO(crbug.com/1080673): We clear the CreatedbyParser flag here, so that
    // elements get fully constructed. Some elements (e.g. HTMLInputElement)
    // only partially construct themselves when created by the parser, but since
    // this is a custom element, we need a fully-constructed element here.
    element = definition->CreateElement(
        document, tag_name,
        GetCreateElementFlags().SetCreatedByParser(false, nullptr));

    // "8. Append each attribute in the given token to element." We don't use
    // setAttributes here because the custom element constructor may have
    // manipulated attributes.
    for (const auto& attribute : token->Attributes())
      element->setAttribute(attribute.GetName(), attribute.Value());

    // "9. If will execute script is true, then ..." etc. The CEReactionsScope
    // and ThrowOnDynamicMarkupInsertionCountIncrementer destructors implement
    // steps 9.1-3.
  } else {
    if (definition) {
      DCHECK(GetCreateElementFlags().IsAsyncCustomElements());
      element = definition->CreateElement(document, tag_name,
                                          GetCreateElementFlags());
    } else {
      element = CustomElement::CreateUncustomizedOrUndefinedElement(
          document, tag_name, GetCreateElementFlags(), is);
    }
    // Definition for the created element does not exist here and it cannot be
    // custom, precustomized, or failed.
    DCHECK_NE(element->GetCustomElementState(), CustomElementState::kCustom);
    DCHECK_NE(element->GetCustomElementState(),
              CustomElementState::kPreCustomized);
    DCHECK_NE(element->GetCustomElementState(), CustomElementState::kFailed);

    // TODO(dominicc): Move these steps so they happen for custom
    // elements as well as built-in elements when customized built in
    // elements are implemented for resettable, listed elements.

    // 10. If element has an xmlns attribute in the XMLNS namespace
    // whose value is not exactly the same as the element's namespace,
    // that is a parse error. Similarly, if element has an xmlns:xlink
    // attribute in the XMLNS namespace whose value is not the XLink
    // Namespace, that is a parse error.

    // TODO(dominicc): Implement step 10 when the HTML parser does
    // something useful with parse errors.

    // 11. If element is a resettable element, invoke its reset
    // algorithm. (This initializes the element's value and
    // checkedness based on the element's attributes.)
    // TODO(dominicc): Implement step 11, resettable elements.

    // 12. If element is a form-associated element, and the form
    // element pointer is not null, and there is no template element
    // on the stack of open elements, ...
    auto* html_element = DynamicTo<HTMLElement>(element);
    FormAssociated* form_associated_element =
        html_element ? html_element->ToFormAssociatedOrNull() : nullptr;
    if (form_associated_element && document.GetFrame() && form_.Get()) {
      // ... and element is either not listed or doesn't have a form
      // attribute, and the intended parent is in the same tree as the
      // element pointed to by the form element pointer, associate
      // element with the form element pointed to by the form element
      // pointer, and suppress the running of the reset the form owner
      // algorithm when the parser subsequently attempts to insert the
      // element.

      // TODO(dominicc): There are many differences to the spec here;
      // some of them are observable:
      //
      // - The HTML spec tracks whether there is a template element on
      //   the stack both for manipulating the form element pointer
      //   and using it here.
      // - FormAssociated::AssociateWith implementations don't do the
      //   "same tree" check; for example
      //   HTMLImageElement::AssociateWith just checks whether the form
      //   is in *a* tree. This check should be done here consistently.
      // - ListedElement is a mixin; add IsListedElement and skip
      //   setting the form for listed attributes with form=. Instead
      //   we set attributes (step 8) out of order, after this step,
      //   to reset the form association.
      form_associated_element->AssociateWith(form_.Get());
    }
    // "8. Append each attribute in the given token to element."
    SetAttributes(element, token);
  }

  return element;
}

HTMLStackItem* HTMLConstructionSite::CreateElementFromSavedToken(
    HTMLStackItem* item) {
  Element* element;
  // NOTE: Moving from item -> token -> item copies the Attribute vector twice!
  Vector<Attribute> attributes;
  attributes.ReserveInitialCapacity(
      static_cast<wtf_size_t>(item->Attributes().size()));
  for (Attribute& attr : item->Attributes()) {
    attributes.push_back(std::move(attr));
  }
  AtomicHTMLToken fake_token(HTMLToken::kStartTag, item->GetTokenName(),
                             std::move(attributes));
  element = CreateElement(&fake_token, item->NamespaceURI());
  return HTMLStackItem::Create(element, &fake_token, item->NamespaceURI());
}

bool HTMLConstructionSite::IndexOfFirstUnopenFormattingElement(
    unsigned& first_unopen_element_index) const {
  if (active_formatting_elements_.IsEmpty())
    return false;
  unsigned index = active_formatting_elements_.size();
  do {
    --index;
    const HTMLFormattingElementList::Entry& entry =
        active_formatting_elements_.at(index);
    if (entry.IsMarker() || open_elements_.Contains(entry.GetElement())) {
      first_unopen_element_index = index + 1;
      return first_unopen_element_index < active_formatting_elements_.size();
    }
  } while (index);
  first_unopen_element_index = index;
  return true;
}

void HTMLConstructionSite::ReconstructTheActiveFormattingElements() {
  unsigned first_unopen_element_index;
  if (!IndexOfFirstUnopenFormattingElement(first_unopen_element_index))
    return;

  unsigned unopen_entry_index = first_unopen_element_index;
  DCHECK_LT(unopen_entry_index, active_formatting_elements_.size());
  for (; unopen_entry_index < active_formatting_elements_.size();
       ++unopen_entry_index) {
    HTMLFormattingElementList::Entry& unopened_entry =
        active_formatting_elements_.at(unopen_entry_index);
    HTMLStackItem* reconstructed =
        CreateElementFromSavedToken(unopened_entry.StackItem());
    AttachLater(CurrentNode(), reconstructed->GetNode());
    open_elements_.Push(reconstructed);
    unopened_entry.ReplaceElement(reconstructed);
  }
}

void HTMLConstructionSite::GenerateImpliedEndTagsWithExclusion(
    const HTMLTokenName& name) {
  while (HasImpliedEndTag(CurrentStackItem()) &&
         !CurrentStackItem()->MatchesHTMLTag(name))
    open_elements_.Pop();
}

void HTMLConstructionSite::GenerateImpliedEndTags() {
  while (HasImpliedEndTag(CurrentStackItem()))
    open_elements_.Pop();
}

bool HTMLConstructionSite::InQuirksMode() {
  return in_quirks_mode_;
}

// Adjusts |task| to match the "adjusted insertion location" determined by the
// foster parenting algorithm, laid out as the substeps of step 2 of
// https://html.spec.whatwg.org/C/#appropriate-place-for-inserting-a-node
void HTMLConstructionSite::FindFosterSite(HTMLConstructionSiteTask& task) {
  // 2.1
  HTMLStackItem* last_template =
      open_elements_.Topmost(html_names::HTMLTag::kTemplate);

  // 2.2
  HTMLStackItem* last_table =
      open_elements_.Topmost(html_names::HTMLTag::kTable);

  // 2.3
  if (last_template &&
      (!last_table || last_template->IsAboveItemInStack(last_table))) {
    task.parent = last_template->GetElement();
    return;
  }

  // 2.4
  if (!last_table) {
    // Fragment case
    task.parent = open_elements_.RootNode();  // DocumentFragment
    return;
  }

  // 2.5
  if (ContainerNode* parent = last_table->GetElement()->parentNode()) {
    task.parent = parent;
    task.next_child = last_table->GetElement();
    return;
  }

  // 2.6, 2.7
  task.parent = last_table->NextItemInStack()->GetElement();
}

bool HTMLConstructionSite::ShouldFosterParent() const {
  return redirect_attach_to_foster_parent_ &&
         CurrentStackItem()->IsElementNode() &&
         CurrentStackItem()->CausesFosterParenting();
}

void HTMLConstructionSite::FosterParent(Node* node) {
  HTMLConstructionSiteTask task(HTMLConstructionSiteTask::kInsert);
  FindFosterSite(task);
  task.child = node;
  DCHECK(task.parent);
  QueueTask(task, true);
}

void HTMLConstructionSite::FinishedTemplateElement(
    DocumentFragment* content_fragment) {
  if (!pending_dom_parts_) {
    return;
  }
  if (!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
    PartRoot* last_root = pending_dom_parts_->PopPartRoot();
    CHECK_EQ(&content_fragment->getPartRoot(), last_root);
  }
}

HTMLConstructionSite::PendingDOMParts::PendingDOMParts(
    ContainerNode* attachment_root) {
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
  if (Document* document = DynamicTo<Document>(attachment_root)) {
    part_root_stack_.push_back(&document->getPartRoot());
  } else {
    DocumentFragment* fragment = DynamicTo<DocumentFragment>(attachment_root);
    CHECK(fragment) << "Attachment root should be Document or DocumentFragment";
    part_root_stack_.push_back(&fragment->getPartRoot());
  }
}

void HTMLConstructionSite::PendingDOMParts::AddChildNodePartStart(
    Node& previous_sibling,
    Vector<String> metadata) {
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  // Note that this ChildNodePart is constructed with both `previous_sibling`
  // and `next_sibling` pointing to the same node, `previous_sibling`. That's
  // because at this point we will move on to parse the children of this
  // ChildNodePart, and at that point, we'll need a constructed PartRoot for
  // those to attach to. So we build this currently-invalid ChildNodePart, and
  // then update its `next_sibling` later when we find it, rendering it (and
  // any dependant Parts) valid.
  ChildNodePart* new_part = MakeGarbageCollected<ChildNodePart>(
      *CurrentPartRoot(), previous_sibling, previous_sibling,
      std::move(metadata));
  part_root_stack_.push_back(new_part);
}

void HTMLConstructionSite::PendingDOMParts::AddChildNodePartEnd(
    Node& next_sibling) {
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  PartRoot* current_part_root = CurrentPartRoot();
  if (current_part_root->IsDocumentPartRoot()) {
    // Mismatched opening/closing child parts.
    return;
  }
  ChildNodePart* last_child_node_part =
      static_cast<ChildNodePart*>(current_part_root);
  last_child_node_part->setNextSibling(next_sibling);
  part_root_stack_.pop_back();
}

void HTMLConstructionSite::PendingDOMParts::ConstructDOMPartsIfNeeded(
    Node& last_node,
    const DOMPartsNeeded& dom_parts_needed) {
  if (!dom_parts_needed) {
    return;
  }
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  DCHECK(pending_node_part_metadata_.empty());
  // For now, there's no syntax for metadata, so just use empty.
  Vector<String> metadata;
  if (dom_parts_needed.needs_node_part) {
    MakeGarbageCollected<NodePart>(*CurrentPartRoot(), last_node, metadata);
  }
  if (!dom_parts_needed.needs_attribute_parts.empty()) {
    Element& element = To<Element>(last_node);
    for (auto attribute_name : dom_parts_needed.needs_attribute_parts) {
      MakeGarbageCollected<AttributePart>(*CurrentPartRoot(), element,
                                          attribute_name, metadata);
    }
  }
}

PartRoot* HTMLConstructionSite::PendingDOMParts::CurrentPartRoot() const {
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  CHECK(!part_root_stack_.empty());
  return part_root_stack_.back().Get();
}

void HTMLConstructionSite::PendingDOMParts::PushPartRoot(PartRoot* root) {
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  return part_root_stack_.push_back(root);
}

PartRoot* HTMLConstructionSite::PendingDOMParts::PopPartRoot() {
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  CHECK(!part_root_stack_.empty());
  PartRoot* popped = part_root_stack_.back();
  part_root_stack_.pop_back();
  return popped;
}

void HTMLConstructionSite::PendingText::Trace(Visitor* visitor) const {
  visitor->Trace(parent);
  visitor->Trace(next_child);
}

void HTMLConstructionSite::PendingDOMParts::Trace(Visitor* visitor) const {
  visitor->Trace(part_root_stack_);
}

}  // namespace blink
