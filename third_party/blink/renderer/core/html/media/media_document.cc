/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/media/media_document.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/raw_data_document_parser.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_source_element.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

class MediaDocumentParser : public RawDataDocumentParser {
 public:
  explicit MediaDocumentParser(Document* document)
      : RawDataDocumentParser(document) {}

 private:
  void AppendBytes(base::span<const uint8_t>) override {}
  void Finish() override;

  void CreateDocumentStructure();

  bool did_build_document_structure_ = false;
};

void MediaDocumentParser::CreateDocumentStructure() {
  // TODO(dgozman): DocumentLoader might call Finish on a stopped parser.
  // See also comments for DocumentParser::{Detach,StopParsing}.
  if (IsStopped())
    return;
  if (did_build_document_structure_)
    return;
  did_build_document_structure_ = true;

  DCHECK(GetDocument());
  GetDocument()->SetOverrideSiteForCookiesForCSPMedia(true);
  auto* root_element = MakeGarbageCollected<HTMLHtmlElement>(*GetDocument());
  GetDocument()->AppendChild(root_element);
  root_element->InsertedByParser();

  if (IsDetached())
    return;  // runScriptsAtDocumentElementAvailable can detach the frame.

  auto* head = MakeGarbageCollected<HTMLHeadElement>(*GetDocument());
  auto* meta = MakeGarbageCollected<HTMLMetaElement>(*GetDocument(),
                                                     CreateElementFlags());
  meta->setAttribute(html_names::kNameAttr, AtomicString("viewport"));
  meta->setAttribute(html_names::kContentAttr,
                     AtomicString("width=device-width"));
  head->AppendChild(meta);

  auto* media = MakeGarbageCollected<HTMLVideoElement>(*GetDocument());
  media->setAttribute(html_names::kControlsAttr, g_empty_atom);
  media->setAttribute(html_names::kAutoplayAttr, g_empty_atom);
  media->setAttribute(html_names::kNameAttr, AtomicString("media"));

  auto* source = MakeGarbageCollected<HTMLSourceElement>(*GetDocument());
  source->setAttribute(html_names::kSrcAttr,
                       AtomicString(GetDocument()->Url()));

  if (DocumentLoader* loader = GetDocument()->Loader())
    source->setType(loader->MimeType());

  media->AppendChild(source);

  auto* body = MakeGarbageCollected<HTMLBodyElement>(*GetDocument());

  GetDocument()->WillInsertBody();

  body->AppendChild(media);
  root_element->AppendChild(head);
  if (IsDetached())
    return;  // DOM insertion events can detach the frame.
  root_element->AppendChild(body);
}

void MediaDocumentParser::Finish() {
  CreateDocumentStructure();
  RawDataDocumentParser::Finish();
}

MediaDocument::MediaDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, {DocumentClass::kMedia}) {
  SetCompatibilityMode(kNoQuirksMode);
  LockCompatibilityMode();

  // Set the autoplay policy to kNoUserGestureRequired.
  if (GetSettings() && IsInOutermostMainFrame()) {
    GetSettings()->SetAutoplayPolicy(
        AutoplayPolicy::Type::kNoUserGestureRequired);
  }
}

DocumentParser* MediaDocument::CreateParser() {
  return MakeGarbageCollected<MediaDocumentParser>(this);
}

void MediaDocument::DefaultEventHandler(Event& event) {
  Node* target_node = event.target()->ToNode();
  if (!target_node)
    return;

  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  if (event.type() == event_type_names::kKeydown && keyboard_event) {
    HTMLVideoElement* video =
        Traversal<HTMLVideoElement>::FirstWithin(*target_node);
    if (!video)
      return;

    if (keyboard_event->key() == " " ||
        keyboard_event->keyCode() == VKEY_MEDIA_PLAY_PAUSE) {
      // space or media key (play/pause)
      video->TogglePlayState();
      event.SetDefaultHandled();
      return;
    }
    // Route the keyboard events directly to the media element
    video->DispatchEvent(event);
    return;
  }
}

}  // namespace blink
