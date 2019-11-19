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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_CONSTRUCTION_SITE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_CONSTRUCTION_SITE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/parser_content_policy.h"
#include "third_party/blink/renderer/core/html/parser/html_element_stack.h"
#include "third_party/blink/renderer/core/html/parser/html_formatting_element_list.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct HTMLConstructionSiteTask {
  DISALLOW_NEW();

 public:
  enum Operation {
    kInsert,
    kInsertText,                // Handles possible merging of text nodes.
    kInsertAlreadyParsedChild,  // Insert w/o calling begin/end parsing.
    kReparent,
    kTakeAllChildren,
  };

  explicit HTMLConstructionSiteTask(Operation op)
      : operation(op), self_closing(false) {}

  void Trace(Visitor* visitor) {
    visitor->Trace(parent);
    visitor->Trace(next_child);
    visitor->Trace(child);
  }

  ContainerNode* OldParent() {
    // It's sort of ugly, but we store the |oldParent| in the |child| field of
    // the task so that we don't bloat the HTMLConstructionSiteTask object in
    // the common case of the Insert operation.
    return To<ContainerNode>(child.Get());
  }

  Operation operation;
  Member<ContainerNode> parent;
  Member<Node> next_child;
  Member<Node> child;
  bool self_closing;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::HTMLConstructionSiteTask)

namespace blink {

// Note: These are intentionally ordered so that when we concatonate strings and
// whitespaces the resulting whitespace is ws = min(ws1, ws2).
enum WhitespaceMode {
  kWhitespaceUnknown,
  kNotAllWhitespace,
  kAllWhitespace,
};

enum FlushMode {
  // Flush pending text. Flush queued tasks.
  kFlushAlways,

  // Flush pending text if node has length limit. Flush queued tasks.
  kFlushIfAtTextLimit,
};

class AtomicHTMLToken;
class CustomElementDefinition;
class Document;
class Element;
class HTMLFormElement;
class HTMLParserReentryPermit;

class HTMLConstructionSite final {
  DISALLOW_NEW();

 public:
  HTMLConstructionSite(HTMLParserReentryPermit*,
                       Document&,
                       ParserContentPolicy);
  ~HTMLConstructionSite();
  void Trace(Visitor*);

  void InitFragmentParsing(DocumentFragment*, Element* context_element);

  void Detach();

  // executeQueuedTasks empties the queue but does not flush pending text.
  // NOTE: Possible reentrancy via JavaScript execution.
  void ExecuteQueuedTasks();

  // flushPendingText turns pending text into queued Text insertions, but does
  // not execute them.
  void FlushPendingText(FlushMode);

  // Called before every token in HTMLTreeBuilder::processToken, thus inlined:
  void Flush(FlushMode mode) {
    if (!HasPendingTasks())
      return;
    FlushPendingText(mode);
    // NOTE: Possible reentrancy via JavaScript execution.
    ExecuteQueuedTasks();
    DCHECK(mode == kFlushIfAtTextLimit || !HasPendingTasks());
  }

  bool HasPendingTasks() {
    return !pending_text_.IsEmpty() || !task_queue_.IsEmpty();
  }

  void SetDefaultCompatibilityMode();
  void ProcessEndOfFile();
  void FinishedParsing();

  void InsertDoctype(AtomicHTMLToken*);
  void InsertComment(AtomicHTMLToken*);
  void InsertCommentOnDocument(AtomicHTMLToken*);
  void InsertCommentOnHTMLHtmlElement(AtomicHTMLToken*);
  void InsertHTMLElement(AtomicHTMLToken*);
  void InsertSelfClosingHTMLElementDestroyingToken(AtomicHTMLToken*);
  void InsertFormattingElement(AtomicHTMLToken*);
  void InsertHTMLHeadElement(AtomicHTMLToken*);
  void InsertHTMLBodyElement(AtomicHTMLToken*);
  void InsertHTMLFormElement(AtomicHTMLToken*, bool is_demoted = false);
  void InsertScriptElement(AtomicHTMLToken*);
  void InsertTextNode(const StringView&, WhitespaceMode = kWhitespaceUnknown);
  void InsertForeignElement(AtomicHTMLToken*,
                            const AtomicString& namespace_uri);

  void InsertHTMLHtmlStartTagBeforeHTML(AtomicHTMLToken*);
  void InsertHTMLHtmlStartTagInBody(AtomicHTMLToken*);
  void InsertHTMLBodyStartTagInBody(AtomicHTMLToken*);

  void Reparent(HTMLElementStack::ElementRecord* new_parent,
                HTMLElementStack::ElementRecord* child);
  void Reparent(HTMLElementStack::ElementRecord* new_parent,
                HTMLStackItem* child);
  // insertAlreadyParsedChild assumes that |child| has already been parsed
  // (i.e., we're just moving it around in the tree rather than parsing it for
  // the first time). That means this function doesn't call beginParsingChildren
  // / finishParsingChildren.
  void InsertAlreadyParsedChild(HTMLStackItem* new_parent,
                                HTMLElementStack::ElementRecord* child);
  void TakeAllChildren(HTMLStackItem* new_parent,
                       HTMLElementStack::ElementRecord* old_parent);

  HTMLStackItem* CreateElementFromSavedToken(HTMLStackItem*);

  bool ShouldFosterParent() const;
  void FosterParent(Node*);

  bool IndexOfFirstUnopenFormattingElement(
      unsigned& first_unopen_element_index) const;
  void ReconstructTheActiveFormattingElements();

  void GenerateImpliedEndTags();
  void GenerateImpliedEndTagsWithExclusion(const AtomicString& tag_name);

  bool InQuirksMode();

  bool IsEmpty() const { return !open_elements_.StackDepth(); }
  HTMLElementStack::ElementRecord* CurrentElementRecord() const {
    return open_elements_.TopRecord();
  }
  Element* CurrentElement() const { return open_elements_.Top(); }
  ContainerNode* CurrentNode() const { return open_elements_.TopNode(); }
  HTMLStackItem* CurrentStackItem() const {
    return open_elements_.TopStackItem();
  }
  HTMLStackItem* OneBelowTop() const { return open_elements_.OneBelowTop(); }
  Document& OwnerDocumentForCurrentNode();
  HTMLElementStack* OpenElements() const { return &open_elements_; }
  HTMLFormattingElementList* ActiveFormattingElements() const {
    return &active_formatting_elements_;
  }
  bool CurrentIsRootNode() {
    return open_elements_.TopNode() == open_elements_.RootNode();
  }

  Element* Head() const { return head_->GetElement(); }
  HTMLStackItem* HeadStackItem() const { return head_.Get(); }

  bool IsFormElementPointerNonNull() const { return form_; }
  HTMLFormElement* TakeForm();

  ParserContentPolicy GetParserContentPolicy() {
    return parser_content_policy_;
  }

  class RedirectToFosterParentGuard {
    STACK_ALLOCATED();
    DISALLOW_COPY_AND_ASSIGN(RedirectToFosterParentGuard);

   public:
    RedirectToFosterParentGuard(HTMLConstructionSite& tree)
        : tree_(tree),
          was_redirecting_before_(tree.redirect_attach_to_foster_parent_) {
      tree_.redirect_attach_to_foster_parent_ = true;
    }

    ~RedirectToFosterParentGuard() {
      tree_.redirect_attach_to_foster_parent_ = was_redirecting_before_;
    }

   private:
    HTMLConstructionSite& tree_;
    bool was_redirecting_before_;
  };

 private:
  // In the common case, this queue will have only one task because most tokens
  // produce only one DOM mutation.
  typedef HeapVector<HTMLConstructionSiteTask, 1> TaskQueue;

  void SetCompatibilityMode(Document::CompatibilityMode);
  void SetCompatibilityModeFromDoctype(const String& name,
                                       const String& public_id,
                                       const String& system_id);

  void AttachLater(ContainerNode* parent,
                   Node* child,
                   bool self_closing = false);

  void FindFosterSite(HTMLConstructionSiteTask&);

  CreateElementFlags GetCreateElementFlags() const;
  Element* CreateElement(AtomicHTMLToken*, const AtomicString& namespace_uri);

  void MergeAttributesFromTokenIntoElement(AtomicHTMLToken*, Element*);

  void ExecuteTask(HTMLConstructionSiteTask&);
  void QueueTask(const HTMLConstructionSiteTask&);

  CustomElementDefinition* LookUpCustomElementDefinition(
      Document&,
      const QualifiedName&,
      const AtomicString& is);

  HTMLParserReentryPermit* reentry_permit_;
  Member<Document> document_;

  // This is the root ContainerNode to which the parser attaches all newly
  // constructed nodes. It points to a DocumentFragment when parsing fragments
  // and a Document in all other cases.
  Member<ContainerNode> attachment_root_;

  // https://html.spec.whatwg.org/C/#head-element-pointer
  Member<HTMLStackItem> head_;
  // https://html.spec.whatwg.org/C/#form-element-pointer
  Member<HTMLFormElement> form_;
  mutable HTMLElementStack open_elements_;
  mutable HTMLFormattingElementList active_formatting_elements_;

  TaskQueue task_queue_;

  class PendingText final {
    DISALLOW_NEW();

   public:
    PendingText() : whitespace_mode(kWhitespaceUnknown) {}

    void Append(ContainerNode* new_parent,
                Node* new_next_child,
                const StringView& new_string,
                WhitespaceMode new_whitespace_mode) {
      DCHECK(!parent || parent == new_parent);
      parent = new_parent;
      DCHECK(!next_child || next_child == new_next_child);
      next_child = new_next_child;
      string_builder.Append(new_string);
      whitespace_mode = std::min(whitespace_mode, new_whitespace_mode);
    }

    void Swap(PendingText& other) {
      std::swap(whitespace_mode, other.whitespace_mode);
      parent.Swap(other.parent);
      next_child.Swap(other.next_child);
      string_builder.Swap(other.string_builder);
    }

    void Discard() {
      PendingText discarded_text;
      Swap(discarded_text);
    }

    bool IsEmpty() {
      // When the stringbuilder is empty, the parent and whitespace should also
      // be "empty".
      DCHECK_EQ(string_builder.IsEmpty(), !parent);
      DCHECK(!string_builder.IsEmpty() || !next_child);
      DCHECK(!string_builder.IsEmpty() ||
             (whitespace_mode == kWhitespaceUnknown));
      return string_builder.IsEmpty();
    }

    void Trace(Visitor*);

    Member<ContainerNode> parent;
    Member<Node> next_child;
    StringBuilder string_builder;
    WhitespaceMode whitespace_mode;
  };

  PendingText pending_text_;

  ParserContentPolicy parser_content_policy_;
  bool is_parsing_fragment_;

  // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-intable
  // In the "in table" insertion mode, we sometimes get into a state where
  // "whenever a node would be inserted into the current node, it must instead
  // be foster parented."  This flag tracks whether we're in that state.
  bool redirect_attach_to_foster_parent_;

  bool in_quirks_mode_;

  DISALLOW_COPY_AND_ASSIGN(HTMLConstructionSite);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_CONSTRUCTION_SITE_H_
