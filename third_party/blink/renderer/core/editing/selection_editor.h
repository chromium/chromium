/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SELECTION_EDITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SELECTION_EDITOR_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_result.h"
#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"

namespace blink {

// TODO(yosin): We will rename |SelectionEditor| to appropriate name since
// it is no longer have a changing selection functionality, it was moved to
// |SelectionModifier| class.
class SelectionEditor final : public GarbageCollected<SelectionEditor>,
                              public SynchronousMutationObserver {
  USING_GARBAGE_COLLECTED_MIXIN(SelectionEditor);

 public:
  explicit SelectionEditor(LocalFrame&);
  virtual ~SelectionEditor();
  void Dispose();

  SelectionInDOMTree GetSelectionInDOMTree() const;

  VisibleSelection ComputeVisibleSelectionInDOMTree() const;
  VisibleSelectionInFlatTree ComputeVisibleSelectionInFlatTree() const;
  void SetSelectionAndEndTyping(const SelectionInDOMTree&);

  void DidAttachDocument(Document*);

  // There functions are exposed for |FrameSelection|.
  void CacheRangeOfDocument(Range*);
  Range* DocumentCachedRange() const;
  void ClearDocumentCachedRange();

  void Trace(Visitor*) override;

 private:
  Document& GetDocument() const;
  LocalFrame* GetFrame() const { return frame_.Get(); }

  void AssertSelectionValid() const;
  void ClearVisibleSelection();
  void MarkCacheDirty();
  bool ShouldAlwaysUseDirectionalSelection() const;

  // VisibleSelection cache related
  bool NeedsUpdateVisibleSelection() const;
  bool NeedsUpdateVisibleSelectionInFlatTree() const;
  void UpdateCachedVisibleSelectionIfNeeded() const;
  void UpdateCachedVisibleSelectionInFlatTreeIfNeeded() const;

  void DidFinishTextChange(const Position& base, const Position& extent);
  void DidFinishDOMMutation();

  // Implementation of |SynchronousMutationObsderver| member functions.
  void ContextDestroyed(Document*) final;
  void DidChangeChildren(const ContainerNode&) final;
  void DidMergeTextNodes(const Text& merged_node,
                         const NodeWithIndex& node_to_be_removed_with_index,
                         unsigned old_length) final;
  void DidSplitTextNode(const Text&) final;
  void DidUpdateCharacterData(CharacterData*,
                              unsigned offset,
                              unsigned old_length,
                              unsigned new_length) final;
  void NodeChildrenWillBeRemoved(ContainerNode&) final;
  void NodeWillBeRemoved(Node&) final;

  Member<LocalFrame> frame_;

  SelectionInDOMTree selection_;

  // If document is root, document.getSelection().addRange(range) is cached on
  // this.
  Member<Range> cached_range_;

  mutable VisibleSelection cached_visible_selection_in_dom_tree_;
  mutable VisibleSelectionInFlatTree cached_visible_selection_in_flat_tree_;
  mutable uint64_t style_version_for_dom_tree_ = static_cast<uint64_t>(-1);
  mutable uint64_t style_version_for_flat_tree_ = static_cast<uint64_t>(-1);
  mutable bool cached_visible_selection_in_dom_tree_is_dirty_ = false;
  mutable bool cached_visible_selection_in_flat_tree_is_dirty_ = false;

  DISALLOW_COPY_AND_ASSIGN(SelectionEditor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SELECTION_EDITOR_H_
