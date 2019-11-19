/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_INPUT_METHOD_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_INPUT_METHOD_CONTROLLER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/blink/public/platform/web_text_input_info.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_shutdown_observer.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/ime/ime_text_span.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Editor;
class EditContext;
class LocalFrame;
class Range;
enum class TypingContinuation;

class CORE_EXPORT InputMethodController final
    : public GarbageCollected<InputMethodController>,
      public DocumentShutdownObserver {
  USING_GARBAGE_COLLECTED_MIXIN(InputMethodController);

 public:
  enum ConfirmCompositionBehavior {
    kDoNotKeepSelection,
    kKeepSelection,
  };

  explicit InputMethodController(LocalFrame&);
  virtual ~InputMethodController();
  void Trace(Visitor*) override;

  // international text input composition
  bool HasComposition() const;
  void SetComposition(const String& text,
                      const Vector<ImeTextSpan>& ime_text_spans,
                      int selection_start,
                      int selection_end);
  void SetCompositionFromExistingText(const Vector<ImeTextSpan>& ime_text_spans,
                                      unsigned composition_start,
                                      unsigned composition_end);

  // Deletes ongoing composing text if any, inserts specified text, and
  // changes the selection according to relativeCaretPosition, which is
  // relative to the end of the inserting text.
  bool CommitText(const String& text,
                  const Vector<ImeTextSpan>& ime_text_spans,
                  int relative_caret_position);

  // Replaces the text in the specified range without changing the selection.
  bool ReplaceText(const String&, PlainTextRange);

  // Inserts ongoing composing text; changes the selection to the end of
  // the inserting text if DoNotKeepSelection, or holds the selection if
  // KeepSelection.
  bool FinishComposingText(ConfirmCompositionBehavior);

  // Deletes the existing composition text.
  void CancelComposition();

  EphemeralRange CompositionEphemeralRange() const;

  void Clear();
  void DidAttachDocument(Document*);

  PlainTextRange GetSelectionOffsets() const;
  // Returns true if setting selection to specified offsets, otherwise false.
  bool SetEditableSelectionOffsets(const PlainTextRange&);
  void ExtendSelectionAndDelete(int before, int after);
  PlainTextRange CreateRangeForSelection(int start,
                                         int end,
                                         size_t text_length) const;
  void DeleteSurroundingText(int before, int after);
  void DeleteSurroundingTextInCodePoints(int before, int after);
  WebTextInputInfo TextInputInfo() const;
  // For finding NEXT/PREVIOUS everytime during frame update is a costly
  // operation, so making it specific whenever needed by splitting from
  // TextInputFlags()
  int ComputeWebTextInputNextPreviousFlags() const;
  WebTextInputType TextInputType() const;

  // Call this when we will change focus.
  void WillChangeFocus();

  // Returns the |EditContext| that is currently active
  EditContext* GetActiveEditContext() const { return active_edit_context_; }
  void SetActiveEditContext(EditContext* edit_context) {
    active_edit_context_ = edit_context;
  }

 private:
  friend class InputMethodControllerTest;

  Document& GetDocument() const;
  bool IsAvailable() const;

  Member<LocalFrame> frame_;
  Member<Range> composition_range_;
  Member<EditContext> active_edit_context_;
  bool has_composition_;

  Editor& GetEditor() const;
  LocalFrame& GetFrame() const {
    DCHECK(frame_);
    return *frame_;
  }

  String ComposingText() const;
  void SelectComposition() const;

  EphemeralRange EphemeralRangeForOffsets(const PlainTextRange&) const;

  // Returns true if selection offsets were successfully set.
  bool SetSelectionOffsets(const PlainTextRange&);

  void AddImeTextSpans(const Vector<ImeTextSpan>& ime_text_spans,
                       ContainerNode* base_element,
                       unsigned offset_in_plain_chars);

  bool InsertText(const String&);
  bool InsertTextAndMoveCaret(const String&,
                              int relative_caret_position,
                              const Vector<ImeTextSpan>& ime_text_spans);

  // Inserts the given text string in the place of the existing composition.
  // Returns true if did replace.
  bool ReplaceComposition(const String& text) WARN_UNUSED_RESULT;
  // Inserts the given text string in the place of the existing composition
  // and moves caret. Returns true if did replace and moved caret successfully.
  bool ReplaceCompositionAndMoveCaret(
      const String&,
      int relative_caret_position,
      const Vector<ImeTextSpan>& ime_text_spans);

  // Returns false if the frame was destroyed, true otherwise.
  bool DeleteSelection() WARN_UNUSED_RESULT;

  // Returns true if moved caret successfully.
  bool MoveCaret(int new_caret_position);

  // Returns false if the frame is destroyed, true otherwise.
  bool DispatchCompositionStartEvent(const String& text) WARN_UNUSED_RESULT;

  PlainTextRange CreateSelectionRangeForSetComposition(
      int selection_start,
      int selection_end,
      size_t text_length) const;
  int TextInputFlags() const;
  ui::TextInputAction InputActionOfFocusedElement() const;
  WebTextInputMode InputModeOfFocusedElement() const;

  // Implements |DocumentShutdownObserver|.
  void ContextDestroyed(Document*) final;

  enum class TypingContinuation;

  // Returns true if setting selection to specified offsets, otherwise false.
  bool SetEditableSelectionOffsets(const PlainTextRange&, TypingContinuation);

  // Returns true if selection offsets were successfully set.
  bool SetSelectionOffsets(const PlainTextRange&, TypingContinuation);

  // There are few cases we need to remove suggestion markers which are also in
  // composing range. (SuggestionSpan with FLAG_AUTO_CORRECTION and
  // Spanned#SPAN_COMPOSING)
  //   1) FinishComposingText()
  //   2) CommitText()
  //   3) SetComposingText() (SetComposition())
  void RemoveSuggestionMarkerInCompositionRange();

  FRIEND_TEST_ALL_PREFIXES(InputMethodControllerTest,
                           InputModeOfFocusedElement);

  DISALLOW_COPY_AND_ASSIGN(InputMethodController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_INPUT_METHOD_CONTROLLER_H_
