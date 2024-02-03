/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Igalia S.L.
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

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/clipboard_commands.h"

#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_utilities.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/clipboard/paste_mode.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/events/clipboard_event.h"
#include "third_party/blink/renderer/core/events/text_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

namespace blink {

namespace {

// This class holds some state relevant to current clipboard event dispatch. It
// helps `ClipboardCommands` to know whether a given `ExecutionContext` is
// currently handling a copy/paste command.
class ExecutionContextClipboardEventState
    : public GarbageCollected<ExecutionContextClipboardEventState>,
      public Supplement<ExecutionContext> {
 public:
  static constexpr char kSupplementName[] =
      "ExecutionContextClipboardEventState";

  static ExecutionContextClipboardEventState& From(
      ExecutionContext& execution_context) {
    {
      ExecutionContextClipboardEventState* supplement =
          Supplement<ExecutionContext>::From<
              ExecutionContextClipboardEventState>(execution_context);
      if (!supplement) {
        supplement = MakeGarbageCollected<ExecutionContextClipboardEventState>(
            execution_context);
        ProvideTo(execution_context, supplement);
      }
      return *supplement;
    }
  }

  ExecutionContextClipboardEventState(ExecutionContext& execution_context)
      : Supplement<ExecutionContext>(execution_context) {}
  virtual ~ExecutionContextClipboardEventState() = default;

  struct State {
    const AtomicString* event_type = nullptr;
    std::optional<EditorCommandSource> source;
  };

  base::AutoReset<State> SetState(const AtomicString& event_type,
                                  EditorCommandSource source) {
    State new_state;
    new_state.event_type = &event_type;
    new_state.source = source;
    return base::AutoReset<State>(&state_, new_state);
  }

  const State& GetState() const { return state_; }

 private:
  State state_;
};

}  // namespace

bool ClipboardCommands::CanReadClipboard(LocalFrame& frame,
                                         EditorCommandSource source) {
  if (source == EditorCommandSource::kMenuOrKeyBinding)
    return true;
  Settings* const settings = frame.GetSettings();
  if (settings && settings->GetJavaScriptCanAccessClipboard() &&
      settings->GetDOMPasteAllowed()) {
    return true;
  }
  return frame.GetContentSettingsClient() &&
         frame.GetContentSettingsClient()->AllowReadFromClipboard();
}

bool ClipboardCommands::CanWriteClipboard(LocalFrame& frame,
                                          EditorCommandSource source) {
  if (source == EditorCommandSource::kMenuOrKeyBinding)
    return true;
  Settings* const settings = frame.GetSettings();
  if ((settings && settings->GetJavaScriptCanAccessClipboard()) ||
      LocalFrame::HasTransientUserActivation(&frame)) {
    return true;
  }
  return frame.GetContentSettingsClient() &&
         frame.GetContentSettingsClient()->AllowWriteToClipboard();
}

bool ClipboardCommands::IsExecutingCutOrCopy(ExecutionContext& context) {
  const ExecutionContextClipboardEventState::State& event_state =
      ExecutionContextClipboardEventState::From(context).GetState();
  return (event_state.event_type == &event_type_names::kCopy ||
          event_state.event_type == &event_type_names::kCut) &&
         event_state.source == EditorCommandSource::kMenuOrKeyBinding;
}

bool ClipboardCommands::IsExecutingPaste(ExecutionContext& context) {
  const ExecutionContextClipboardEventState::State& event_state =
      ExecutionContextClipboardEventState::From(context).GetState();
  return event_state.event_type == &event_type_names::kPaste &&
         event_state.source == EditorCommandSource::kMenuOrKeyBinding;
}

bool ClipboardCommands::CanSmartReplaceInClipboard(LocalFrame& frame) {
  return frame.GetEditor().SmartInsertDeleteEnabled() &&
         frame.GetSystemClipboard()->IsFormatAvailable(
             blink::mojom::ClipboardFormat::kSmartPaste);
}

Element* ClipboardCommands::FindEventTargetForClipboardEvent(
    LocalFrame& frame,
    EditorCommandSource source) {
  // https://www.w3.org/TR/clipboard-apis/#fire-a-clipboard-event says:
  //  "Set target to be the element that contains the start of the selection in
  //   document order, or the body element if there is no selection or cursor."
  // We treat hidden selections as "no selection or cursor".
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      frame.Selection().IsHidden())
    return frame.Selection().GetDocument().body();

  return FindEventTargetFrom(
      frame, frame.Selection().ComputeVisibleSelectionInDOMTree());
}

// Returns true if Editor should continue with default processing.
bool ClipboardCommands::DispatchClipboardEvent(LocalFrame& frame,
                                               const AtomicString& event_type,
                                               DataTransferAccessPolicy policy,
                                               EditorCommandSource source,
                                               PasteMode paste_mode) {
  Element* const target = FindEventTargetForClipboardEvent(frame, source);
  if (!target)
    return true;

  SystemClipboard* system_clipboard = frame.GetSystemClipboard();
  DataTransfer* const data_transfer = DataTransfer::Create(
      DataTransfer::kCopyAndPaste, policy,
      policy == DataTransferAccessPolicy::kWritable
          ? DataObject::Create()
          : DataObject::CreateFromClipboard(target->GetExecutionContext(),
                                            system_clipboard, paste_mode));

  bool no_default_processing = false;
  {
    base::AutoReset<ExecutionContextClipboardEventState::State> reset =
        ExecutionContextClipboardEventState::From(
            *target->GetExecutionContext())
            .SetState(event_type, source);
    Event* const evt = ClipboardEvent::Create(event_type, data_transfer);
    target->DispatchEvent(*evt);
    no_default_processing = evt->defaultPrevented();
  }
  if (no_default_processing && policy == DataTransferAccessPolicy::kWritable) {
    frame.GetSystemClipboard()->WriteDataObject(data_transfer->GetDataObject());
    frame.GetSystemClipboard()->CommitWrite();
  }

  // Invalidate clipboard here for security.
  data_transfer->SetAccessPolicy(DataTransferAccessPolicy::kNumb);
  return !no_default_processing;
}

bool ClipboardCommands::DispatchCopyOrCutEvent(LocalFrame& frame,
                                               EditorCommandSource source,
                                               const AtomicString& event_type) {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  if (IsInPasswordField(
          frame.Selection().ComputeVisibleSelectionInDOMTree().Start()))
    return true;

  return DispatchClipboardEvent(frame, event_type,
                                DataTransferAccessPolicy::kWritable, source,
                                PasteMode::kAllMimeTypes);
}

bool ClipboardCommands::DispatchPasteEvent(LocalFrame& frame,
                                           PasteMode paste_mode,
                                           EditorCommandSource source) {
  return DispatchClipboardEvent(frame, event_type_names::kPaste,
                                DataTransferAccessPolicy::kReadable, source,
                                paste_mode);
}

// WinIE uses onbeforecut and onbeforepaste to enables the cut and paste menu
// items. They also send onbeforecopy, apparently for symmetry, but it doesn't
// affect the menu items. We need to use onbeforecopy as a real menu enabler
// because we allow elements that are not normally selectable to implement
// copy/paste (like divs, or a document body).

bool ClipboardCommands::EnabledCopy(LocalFrame& frame,
                                    Event*,
                                    EditorCommandSource source) {
  if (!CanWriteClipboard(frame, source))
    return false;
  return !DispatchCopyOrCutEvent(frame, source,
                                 event_type_names::kBeforecopy) ||
         frame.GetEditor().CanCopy();
}

bool ClipboardCommands::EnabledCut(LocalFrame& frame,
                                   Event*,
                                   EditorCommandSource source) {
  if (!CanWriteClipboard(frame, source))
    return false;
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;
  return !DispatchCopyOrCutEvent(frame, source, event_type_names::kBeforecut) ||
         frame.GetEditor().CanCut();
}

bool ClipboardCommands::EnabledPaste(LocalFrame& frame,
                                     Event*,
                                     EditorCommandSource source) {
  if (!CanReadClipboard(frame, source))
    return false;
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;
  return frame.GetEditor().CanPaste();
}

static SystemClipboard::SmartReplaceOption GetSmartReplaceOption(
    const LocalFrame& frame) {
  if (frame.GetEditor().SmartInsertDeleteEnabled() &&
      frame.Selection().Granularity() == TextGranularity::kWord)
    return SystemClipboard::kCanSmartReplace;
  return SystemClipboard::kCannotSmartReplace;
}

void ClipboardCommands::WriteSelectionToClipboard(LocalFrame& frame) {
  const KURL& url = frame.GetDocument()->Url();
  const String html = frame.Selection().SelectedHTMLForClipboard();
  String plain_text = frame.SelectedTextForClipboard();
  frame.GetSystemClipboard()->WriteHTML(html, url,
                                        GetSmartReplaceOption(frame));
  ReplaceNBSPWithSpace(plain_text);
  frame.GetSystemClipboard()->WritePlainText(plain_text,
                                             GetSmartReplaceOption(frame));
  frame.GetSystemClipboard()->CommitWrite();
}

bool ClipboardCommands::PasteSupported(LocalFrame* frame) {
  const Settings* const settings = frame->GetSettings();
  if (settings && settings->GetJavaScriptCanAccessClipboard() &&
      settings->GetDOMPasteAllowed()) {
    return true;
  }
  return frame->GetContentSettingsClient() &&
         frame->GetContentSettingsClient()->AllowReadFromClipboard();
}

bool ClipboardCommands::ExecuteCopy(LocalFrame& frame,
                                    Event*,
                                    EditorCommandSource source,
                                    const String&) {
  if (!DispatchCopyOrCutEvent(frame, source, event_type_names::kCopy))
    return true;
  if (!frame.GetEditor().CanCopy())
    return true;

  Document* const document = frame.GetDocument();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // A 'copy' event handler might have dirtied the layout so we need to update
  // before we obtain the selection.
  document->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  if (HTMLImageElement* image_element =
          ImageElementFromImageDocument(document)) {
    // In an image document, normally there isn't anything to select, and we
    // only want to copy the image itself.
    if (frame.Selection().ComputeVisibleSelectionInDOMTree().IsNone()) {
      WriteImageNodeToClipboard(*frame.GetSystemClipboard(), *image_element,
                                document->title());
      return true;
    }

    // Scripts may insert other contents into an image document. Falls through
    // when they are selected.
  }

  // Since copy is a read-only operation it succeeds anytime a selection
  // is *visible*. In contrast to cut or paste, the selection does not
  // need to be focused - being visible is enough.
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      frame.Selection().IsHidden())
    return true;

  if (EnclosingTextControl(
          frame.Selection().ComputeVisibleSelectionInDOMTree().Start())) {
    frame.GetSystemClipboard()->WritePlainText(frame.SelectedTextForClipboard(),
                                               GetSmartReplaceOption(frame));
    frame.GetSystemClipboard()->CommitWrite();
    return true;
  }
  WriteSelectionToClipboard(frame);
  return true;
}

bool ClipboardCommands::CanDeleteRange(const EphemeralRange& range) {
  if (range.IsCollapsed())
    return false;

  const Node& start_container = *range.StartPosition().ComputeContainerNode();
  const Node& end_container = *range.EndPosition().ComputeContainerNode();

  return IsEditable(start_container) && IsEditable(end_container);
}

static DeleteMode ConvertSmartReplaceOptionToDeleteMode(
    SystemClipboard::SmartReplaceOption smart_replace_option) {
  if (smart_replace_option == SystemClipboard::kCanSmartReplace)
    return DeleteMode::kSmart;
  DCHECK_EQ(smart_replace_option, SystemClipboard::kCannotSmartReplace);
  return DeleteMode::kSimple;
}

bool ClipboardCommands::ExecuteCut(LocalFrame& frame,
                                   Event*,
                                   EditorCommandSource source,
                                   const String&) {
  // document.execCommand("cut") is a no-op in EditContext
  if (source == EditorCommandSource::kDOM &&
      frame.GetInputMethodController().GetActiveEditContext()) {
    return true;
  }

  if (!DispatchCopyOrCutEvent(frame, source, event_type_names::kCut))
    return true;
  if (!frame.GetEditor().CanCut())
    return true;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // A 'cut' event handler might have dirtied the layout so we need to update
  // before we obtain the selection.
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return true;

  if (!CanDeleteRange(frame.GetEditor().SelectedRange()))
    return true;
  if (EnclosingTextControl(
          frame.Selection().ComputeVisibleSelectionInDOMTree().Start())) {
    const String plain_text = frame.SelectedTextForClipboard();
    frame.GetSystemClipboard()->WritePlainText(plain_text,
                                               GetSmartReplaceOption(frame));
    frame.GetSystemClipboard()->CommitWrite();
  } else {
    WriteSelectionToClipboard(frame);
  }

  if (source == EditorCommandSource::kMenuOrKeyBinding) {
    if (DispatchBeforeInputDataTransfer(
            FindEventTargetForClipboardEvent(frame, source),
            InputEvent::InputType::kDeleteByCut,
            nullptr) != DispatchEventResult::kNotCanceled)
      return true;
    // 'beforeinput' event handler may destroy target frame.
    if (frame.GetDocument()->GetFrame() != frame)
      return true;

    // No DOM mutation if EditContext is active.
    if (frame.GetInputMethodController().GetActiveEditContext())
      return true;
  }

  frame.GetEditor().DeleteSelectionWithSmartDelete(
      ConvertSmartReplaceOptionToDeleteMode(GetSmartReplaceOption(frame)),
      InputEvent::InputType::kDeleteByCut);

  return true;
}

void ClipboardCommands::PasteAsFragment(LocalFrame& frame,
                                        DocumentFragment* pasting_fragment,
                                        bool smart_replace,
                                        bool match_style,
                                        EditorCommandSource source) {
  Element* const target = FindEventTargetForClipboardEvent(frame, source);
  if (!target)
    return;
  target->DispatchEvent(*TextEvent::CreateForFragmentPaste(
      frame.DomWindow(), pasting_fragment, smart_replace, match_style));
}

void ClipboardCommands::PasteAsPlainTextFromClipboard(
    LocalFrame& frame,
    EditorCommandSource source) {
  Element* const target = FindEventTargetForClipboardEvent(frame, source);
  if (!target)
    return;
  target->DispatchEvent(*TextEvent::CreateForPlainTextPaste(
      frame.DomWindow(), frame.GetSystemClipboard()->ReadPlainText(),
      CanSmartReplaceInClipboard(frame)));
}

ClipboardCommands::FragmentAndPlainText
ClipboardCommands::GetFragmentFromClipboard(LocalFrame& frame) {
  DocumentFragment* fragment = nullptr;
  if (frame.GetSystemClipboard()->IsFormatAvailable(
          blink::mojom::ClipboardFormat::kHtml)) {
    unsigned fragment_start = 0;
    unsigned fragment_end = 0;
    KURL url;
    const String markup =
        frame.GetSystemClipboard()->ReadHTML(url, fragment_start, fragment_end);
    fragment = CreateStrictlyProcessedFragmentFromMarkupWithContext(
        *frame.GetDocument(), markup, fragment_start, fragment_end, url);
  }
  if (fragment)
    return std::make_pair(fragment, false);

  if (const String markup = frame.GetSystemClipboard()->ReadImageAsImageMarkup(
          mojom::blink::ClipboardBuffer::kStandard)) {
    fragment = CreateFragmentFromMarkup(*frame.GetDocument(), markup,
                                        /* base_url */ "",
                                        kDisallowScriptingAndPluginContent);
    DCHECK(fragment);
    return std::make_pair(fragment, false);
  }

  const String text = frame.GetSystemClipboard()->ReadPlainText();
  if (text.empty())
    return std::make_pair(fragment, false);

  // TODO(editing-dev): Use of UpdateStyleAndLayout
  // needs to be audited. See http://crbug.com/590369 for more details.
  // |SelectedRange| requires clean layout for visible selection
  // normalization.
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  fragment = CreateFragmentFromText(frame.GetEditor().SelectedRange(), text);
  return std::make_pair(fragment, true);
}

void ClipboardCommands::PasteFromClipboard(LocalFrame& frame,
                                           EditorCommandSource source) {
  const ClipboardCommands::FragmentAndPlainText fragment_and_plain_text =
      GetFragmentFromClipboard(frame);

  if (!fragment_and_plain_text.first)
    return;
  PasteAsFragment(frame, fragment_and_plain_text.first,
                  CanSmartReplaceInClipboard(frame),
                  fragment_and_plain_text.second, source);
}

void ClipboardCommands::Paste(LocalFrame& frame, EditorCommandSource source) {
  DCHECK(frame.GetDocument());

  // document.execCommand("paste") is a no-op in EditContext
  if (source == EditorCommandSource::kDOM &&
      frame.GetInputMethodController().GetActiveEditContext()) {
    return;
  }

  // The code below makes multiple calls to SystemClipboard methods which
  // implies multiple IPC calls to the ClipboardHost in the browaser process.
  // SystemClipboard snapshotting tells SystemClipboard to cache results from
  // the ClipboardHost so that at most one IPC is made for each type.
  ScopedSystemClipboardSnapshot snapshot(*frame.GetSystemClipboard());

  if (!DispatchPasteEvent(frame, PasteMode::kAllMimeTypes, source))
    return;
  if (!frame.GetEditor().CanPaste())
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // A 'paste' event handler might have dirtied the layout so we need to update
  // before we obtain the selection.
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return;

  ResourceFetcher* const loader = frame.GetDocument()->Fetcher();
  ResourceCacheValidationSuppressor validation_suppressor(loader);

  const PasteMode paste_mode = frame.GetEditor().CanEditRichly()
                                   ? PasteMode::kAllMimeTypes
                                   : PasteMode::kPlainTextOnly;

  if (source == EditorCommandSource::kMenuOrKeyBinding) {
    Element* const target = FindEventTargetForClipboardEvent(frame, source);

    DataTransfer* data_transfer = DataTransfer::Create(
        DataTransfer::kCopyAndPaste, DataTransferAccessPolicy::kReadable,
        DataObject::CreateFromClipboard(
            target ? target->GetExecutionContext() : nullptr,
            frame.GetSystemClipboard(), paste_mode));

    if (DispatchBeforeInputDataTransfer(
            target, InputEvent::InputType::kInsertFromPaste, data_transfer) !=
        DispatchEventResult::kNotCanceled) {
      return;
    }
    // 'beforeinput' event handler may destroy target frame.
    if (frame.GetDocument()->GetFrame() != frame)
      return;

    // No DOM mutation if EditContext is active.
    if (frame.GetInputMethodController().GetActiveEditContext())
      return;
  }

  if (paste_mode == PasteMode::kAllMimeTypes) {
    PasteFromClipboard(frame, source);
    return;
  }
  PasteAsPlainTextFromClipboard(frame, source);
}

class CORE_EXPORT PasteImageResourceObserver final
    : public GarbageCollected<PasteImageResourceObserver>,
      public ImageResourceObserver {
 public:
  PasteImageResourceObserver(LocalFrame* frame,
                             EditorCommandSource source,
                             const KURL& src)
      : frame_(frame), source_(source), src_(src) {
    DCHECK(frame);
    frame->GetEditor().AddImageResourceObserver(this);
  }

  void ImageNotifyFinished(ImageResourceContent* image_content) override {
    if (!frame_ || !frame_->GetDocument()) {
      return;
    }

    if (!image_content || !image_content->IsLoaded()) {
      return;
    }

    if (!DispatchClipboardEvent(image_content)) {
      return;
    }

    if (!frame_->GetEditor().CanPaste()) {
      return;
    }

    if (source_ == EditorCommandSource::kMenuOrKeyBinding &&
        !frame_->Selection().SelectionHasFocus()) {
      return;
    }

    if (!IsRichlyEditable(*(frame_->GetDocument()->FocusedElement()))) {
      return;
    }

    frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

    PasteAsFragment();

    frame_->GetEditor().RemoveImageResourceObserver(this);
  }

  String DebugName() const override { return "PasteImageResourceObserver"; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(frame_);
    ImageResourceObserver::Trace(visitor);
  }

 private:
  Element* FindEventTargetForClipboardEvent() const {
    if (source_ == EditorCommandSource::kMenuOrKeyBinding &&
        frame_->Selection().IsHidden()) {
      return frame_->Selection().GetDocument().body();
    }
    return FindEventTargetFrom(
        *frame_, frame_->Selection().ComputeVisibleSelectionInDOMTree());
  }

  String BuildMarkup() const {
    return "<img src=\"" + src_.GetString() +
           "\" referrerpolicy=\"no-referrer\" />";
  }

  DocumentFragment* BuildFragment() const {
    unsigned fragment_start = 0;
    unsigned fragment_end = 0;

    return CreateStrictlyProcessedFragmentFromMarkupWithContext(
        *(frame_->GetDocument()), BuildMarkup(), fragment_start, fragment_end,
        String());
  }

  // Dispatches a paste event with the image; returns true if we need to
  // continue with default process.
  bool DispatchClipboardEvent(ImageResourceContent* image_content) {
    Element* const target = FindEventTargetForClipboardEvent();

    if (!target) {
      return true;
    }

    Image* image = image_content->GetImage();

    if (!image) {
      return true;
    }

    scoped_refptr<SharedBuffer> image_buffer = image->Data();

    if (!image_buffer || !image_buffer->size()) {
      return true;
    }

    DataObject* data_object = DataObject::Create();

    data_object->AddFileSharedBuffer(
        image_buffer, /*is_accessible_from_start_frame=*/true, src_,
        image->FilenameExtension(),
        image_content->GetResponse().HttpHeaderFields().Get(
            http_names::kContentDisposition));

    DataTransfer* const data_transfer =
        DataTransfer::Create(DataTransfer::kCopyAndPaste,
                             DataTransferAccessPolicy::kReadable, data_object);

    Event* const evt =
        ClipboardEvent::Create(event_type_names::kPaste, data_transfer);

    target->DispatchEvent(*evt);

    if (!evt->defaultPrevented()) {
      return true;
    }

    return false;
  }

  void PasteAsFragment() {
    Element* const target = FindEventTargetForClipboardEvent();

    if (!target) {
      return;
    }

    target->DispatchEvent(*TextEvent::CreateForFragmentPaste(
        frame_->DomWindow(), BuildFragment(), false, false));
  }

  WeakMember<LocalFrame> frame_;
  EditorCommandSource source_;
  const KURL src_;
};

void ClipboardCommands::PasteFromImageURL(LocalFrame& frame,
                                          EditorCommandSource source,
                                          const String src) {
  DCHECK(frame.GetDocument());

  Element* const target = FindEventTargetForClipboardEvent(frame, source);
  if (!target) {
    return;
  }

  ResourceRequest resource_request(src);
  resource_request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);

  ResourceLoaderOptions resource_loader_options(
      target->GetExecutionContext()->GetCurrentWorld());

  FetchParameters fetch_params(std::move(resource_request),
                               resource_loader_options);

  if (!fetch_params.Url().IsValid()) {
    return;
  }

  // Apply CORS checks (and use CredentialsMode::kOmit as a safer default) to
  // ensure the image content are safe to be exposed. The CORS checks are
  // expected to always pass given the expected URLs/use cases of this command.
  fetch_params.SetCrossOriginAccessControl(
      target->GetExecutionContext()->GetSecurityOrigin(),
      network::mojom::CredentialsMode::kOmit);

  ImageResourceContent* image_content = ImageResourceContent::Fetch(
      fetch_params, target->GetDocument().Fetcher());

  image_content->AddObserver(MakeGarbageCollected<PasteImageResourceObserver>(
      &frame, source, fetch_params.Url()));
}

bool ClipboardCommands::ExecutePaste(LocalFrame& frame,
                                     Event*,
                                     EditorCommandSource source,
                                     const String&) {
  Paste(frame, source);
  return true;
}

bool ClipboardCommands::ExecutePasteGlobalSelection(LocalFrame& frame,
                                                    Event*,
                                                    EditorCommandSource source,
                                                    const String&) {
  if (!frame.GetEditor().Behavior().SupportsGlobalSelection())
    return false;
  DCHECK_EQ(source, EditorCommandSource::kMenuOrKeyBinding);

  const bool old_selection_mode = frame.GetSystemClipboard()->IsSelectionMode();
  frame.GetSystemClipboard()->SetSelectionMode(true);
  Paste(frame, source);
  frame.GetSystemClipboard()->SetSelectionMode(old_selection_mode);
  return true;
}

bool ClipboardCommands::ExecutePasteAndMatchStyle(LocalFrame& frame,
                                                  Event*,
                                                  EditorCommandSource source,
                                                  const String&) {
  if (!DispatchPasteEvent(frame, PasteMode::kPlainTextOnly, source))
    return false;
  if (!frame.GetEditor().CanPaste())
    return false;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // A 'paste' event handler might have dirtied the layout so we need to update
  // before we obtain the selection.
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  if (source == EditorCommandSource::kMenuOrKeyBinding) {
    if (!frame.Selection().SelectionHasFocus())
      return false;

    Element* const target = FindEventTargetForClipboardEvent(frame, source);

    DataTransfer* data_transfer = DataTransfer::Create(
        DataTransfer::kCopyAndPaste, DataTransferAccessPolicy::kReadable,
        DataObject::CreateFromClipboard(
            target ? target->GetExecutionContext() : nullptr,
            frame.GetSystemClipboard(), PasteMode::kPlainTextOnly));
    if (DispatchBeforeInputDataTransfer(
            target, InputEvent::InputType::kInsertFromPaste, data_transfer) !=
        DispatchEventResult::kNotCanceled) {
      return true;
    }
    // 'beforeinput' event handler may destroy target frame.
    if (frame.GetDocument()->GetFrame() != frame)
      return true;

    // No DOM mutation if EditContext is active.
    if (frame.GetInputMethodController().GetActiveEditContext())
      return true;
  }

  PasteAsPlainTextFromClipboard(frame, source);
  return true;
}

bool ClipboardCommands::ExecutePasteFromImageURL(LocalFrame& frame,
                                                 Event*,
                                                 EditorCommandSource source,
                                                 const String& src) {
  PasteFromImageURL(frame, source, src);
  return true;
}

}  // namespace blink
