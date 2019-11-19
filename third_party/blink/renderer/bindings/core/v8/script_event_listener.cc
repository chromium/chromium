/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "v8/include/v8.h"

namespace blink {

// TODO(yukiy): make this method receive |Document*| instead of |Node*|, which
// is no longer necessary.
EventListener* CreateAttributeEventListener(Node* node,
                                            const QualifiedName& name,
                                            const AtomicString& value,
                                            JSEventHandler::HandlerType type) {
  DCHECK(node);
  if (value.IsNull())
    return nullptr;

  // FIXME: Very strange: we initialize zero-based number with '1'.
  TextPosition position(OrdinalNumber::FromZeroBasedInt(1),
                        OrdinalNumber::First());
  String source_url;

  v8::Isolate* isolate = node->GetDocument().GetIsolate();

  if (LocalFrame* frame = node->GetDocument().GetFrame()) {
    ScriptController& script_controller = frame->GetScriptController();
    if (!node->GetDocument().CanExecuteScripts(kAboutToExecuteScript))
      return nullptr;
    position = script_controller.EventHandlerPosition();
    source_url = node->GetDocument().Url().GetString();
  }

  // An assumption here is that the content attributes are used only in the main
  // world or the isolated world for the content scripts, they are never used in
  // other isolated worlds nor worker/worklets.
  // In case of the content scripts, Blink runs script in the main world instead
  // of the isolated world for the content script by design.
  DOMWrapperWorld& world = DOMWrapperWorld::MainWorld();

  return MakeGarbageCollected<JSEventHandlerForContentAttribute>(
      isolate, world, name.LocalName(), value, source_url, position, type);
}

EventListener* CreateAttributeEventListener(LocalFrame* frame,
                                            const QualifiedName& name,
                                            const AtomicString& value,
                                            JSEventHandler::HandlerType type) {
  if (!frame)
    return nullptr;

  if (value.IsNull())
    return nullptr;

  if (!frame->GetDocument()->CanExecuteScripts(kAboutToExecuteScript))
    return nullptr;

  TextPosition position = frame->GetScriptController().EventHandlerPosition();
  String source_url = frame->GetDocument()->Url().GetString();

  v8::Isolate* isolate = ToIsolate(frame);

  // An assumption here is that the content attributes are used only in the main
  // world or the isolated world for the content scripts, they are never used in
  // other isolated worlds nor worker/worklets.
  // In case of the content scripts, Blink runs script in the main world instead
  // of the isolated world for the content script by design.
  DOMWrapperWorld& world = DOMWrapperWorld::MainWorld();

  return MakeGarbageCollected<JSEventHandlerForContentAttribute>(
      isolate, world, name.LocalName(), value, source_url, position, type);
}

}  // namespace blink
