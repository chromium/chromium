/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_scheduler.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_callback_invocation.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_lifecycle_callbacks.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_dispatcher.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_import_step.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_resolution_step.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_run_queue.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_sync_microtask_queue.h"
#include "third_party/blink/renderer/core/html/imports/html_import_child.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// FIXME: Consider moving the element's callback queue to ElementRareData.
typedef HeapHashMap<Member<Element>, Member<V0CustomElementCallbackQueue>>
    ElementCallbackQueueMap;

static ElementCallbackQueueMap& CallbackQueues() {
  DEFINE_STATIC_LOCAL(Persistent<ElementCallbackQueueMap>, map,
                      (MakeGarbageCollected<ElementCallbackQueueMap>()));
  return *map;
}

static V0CustomElementCallbackQueue& EnsureCallbackQueue(Element* element) {
  ElementCallbackQueueMap::ValueType* it =
      CallbackQueues().insert(element, nullptr).stored_value;
  if (!it->value)
    it->value = MakeGarbageCollected<V0CustomElementCallbackQueue>(element);
  return *it->value.Get();
}

// Finds or creates the callback queue for element.
static V0CustomElementCallbackQueue& ScheduleCallbackQueue(Element* element) {
  V0CustomElementCallbackQueue& callback_queue = EnsureCallbackQueue(element);
  if (callback_queue.InCreatedCallback()) {
    // Don't move it. Authors use the createdCallback like a
    // constructor. By not moving it, the createdCallback
    // completes before any other callbacks are entered for this
    // element.
    return callback_queue;
  }

  if (V0CustomElementProcessingStack::InCallbackDeliveryScope()) {
    // The processing stack is active.
    V0CustomElementProcessingStack::Instance().Enqueue(&callback_queue);
    return callback_queue;
  }

  V0CustomElementMicrotaskDispatcher::Instance().Enqueue(&callback_queue);
  return callback_queue;
}

void V0CustomElementScheduler::ScheduleCallback(
    V0CustomElementLifecycleCallbacks* callbacks,
    Element* element,
    V0CustomElementLifecycleCallbacks::CallbackType type) {
  DCHECK(type != V0CustomElementLifecycleCallbacks::kAttributeChangedCallback);

  if (!callbacks->HasCallback(type))
    return;

  V0CustomElementCallbackQueue& queue = ScheduleCallbackQueue(element);
  queue.Append(
      V0CustomElementCallbackInvocation::CreateInvocation(callbacks, type));
}

void V0CustomElementScheduler::ScheduleAttributeChangedCallback(
    V0CustomElementLifecycleCallbacks* callbacks,
    Element* element,
    const AtomicString& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  if (!callbacks->HasCallback(
          V0CustomElementLifecycleCallbacks::kAttributeChangedCallback))
    return;

  V0CustomElementCallbackQueue& queue = ScheduleCallbackQueue(element);
  queue.Append(
      V0CustomElementCallbackInvocation::CreateAttributeChangedInvocation(
          callbacks, name, old_value, new_value));
}

void V0CustomElementScheduler::ResolveOrScheduleResolution(
    V0CustomElementRegistrationContext* context,
    Element* element,
    const V0CustomElementDescriptor& descriptor) {
  if (V0CustomElementProcessingStack::InCallbackDeliveryScope()) {
    context->Resolve(element, descriptor);
    return;
  }

  Document& document = element->GetDocument();
  auto* step = MakeGarbageCollected<V0CustomElementMicrotaskResolutionStep>(
      context, element, descriptor);
  EnqueueMicrotaskStep(document, step);
}

V0CustomElementMicrotaskImportStep* V0CustomElementScheduler::ScheduleImport(
    HTMLImportChild* import) {
  DCHECK(!import->HasFinishedLoading());
  DCHECK(import->Parent());

  // Ownership of the new step is transferred to the parent
  // processing step, or the base queue.
  auto* step = MakeGarbageCollected<V0CustomElementMicrotaskImportStep>(import);
  V0CustomElementMicrotaskImportStep* raw_step = step;
  EnqueueMicrotaskStep(*(import->Parent()->GetDocument()), step,
                       import->IsSync());
  return raw_step;
}

void V0CustomElementScheduler::EnqueueMicrotaskStep(
    Document& document,
    V0CustomElementMicrotaskStep* step,
    bool import_is_sync) {
  Document& master = document.ImportsController()
                         ? *(document.ImportsController()->Master())
                         : document;
  master.CustomElementMicrotaskRunQueue()->Enqueue(document.ImportLoader(),
                                                   step, import_is_sync);
}

void V0CustomElementScheduler::CallbackDispatcherDidFinish() {
  if (V0CustomElementMicrotaskDispatcher::Instance().ElementQueueIsEmpty())
    CallbackQueues().clear();
}

void V0CustomElementScheduler::MicrotaskDispatcherDidFinish() {
  DCHECK(!V0CustomElementProcessingStack::InCallbackDeliveryScope());
  CallbackQueues().clear();
}

}  // namespace blink
