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

#include "third_party/blink/renderer/modules/storage/storage_area.h"

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/storage/dom_window_storage.h"
#include "third_party/blink/renderer/modules/storage/inspector_dom_storage_agent.h"
#include "third_party/blink/renderer/modules/storage/storage_controller.h"
#include "third_party/blink/renderer/modules/storage/storage_event.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// static
const char StorageArea::kAccessDataMessage[] =
    "Storage is disabled inside 'data:' URLs.";

// static
const char StorageArea::kAccessDeniedMessage[] =
    "Access is denied for this document.";

// static
const char StorageArea::kAccessSandboxedMessage[] =
    "The document is sandboxed and lacks the 'allow-same-origin' flag.";

StorageArea* StorageArea::Create(LocalDOMWindow* window,
                                 scoped_refptr<CachedStorageArea> storage_area,
                                 StorageType storage_type) {
  return MakeGarbageCollected<StorageArea>(window, std::move(storage_area),
                                           storage_type,
                                           /* should_enqueue_events */ true);
}

StorageArea* StorageArea::CreateForInspectorAgent(
    LocalDOMWindow* window,
    scoped_refptr<CachedStorageArea> storage_area,
    StorageType storage_type) {
  return MakeGarbageCollected<StorageArea>(window, std::move(storage_area),
                                           storage_type,
                                           /* should_enqueue_events */ false);
}

StorageArea::StorageArea(LocalDOMWindow* window,
                         scoped_refptr<CachedStorageArea> storage_area,
                         StorageType storage_type,
                         bool should_enqueue_events)
    : ExecutionContextClient(window),
      cached_area_(std::move(storage_area)),
      storage_type_(storage_type),
      should_enqueue_events_(should_enqueue_events) {
  DCHECK(window);
  DCHECK(cached_area_);
  cached_area_->RegisterSource(this);
  if (cached_area_->is_session_storage_for_prerendering()) {
    DomWindow()->document()->AddWillDispatchPrerenderingchangeCallback(
        WTF::BindOnce(&StorageArea::OnDocumentActivatedForPrerendering,
                      WrapWeakPersistent(this)));
  }
}

unsigned StorageArea::length(ExceptionState& exception_state) const {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return 0;
  }
  return cached_area_->GetLength();
}

String StorageArea::key(unsigned index, ExceptionState& exception_state) const {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return String();
  }
  return cached_area_->GetKey(index);
}

String StorageArea::getItem(const String& key,
                            ExceptionState& exception_state) const {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return String();
  }
  return cached_area_->GetItem(key);
}

NamedPropertySetterResult StorageArea::setItem(
    const String& key,
    const String& value,
    ExceptionState& exception_state) {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return NamedPropertySetterResult::kIntercepted;
  }
  if (!cached_area_->SetItem(key, value, this)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kQuotaExceededError,
        "Setting the value of '" + key + "' exceeded the quota.");
    return NamedPropertySetterResult::kIntercepted;
  }
  return NamedPropertySetterResult::kIntercepted;
}

NamedPropertyDeleterResult StorageArea::removeItem(
    const String& key,
    ExceptionState& exception_state) {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return NamedPropertyDeleterResult::kDidNotDelete;
  }
  cached_area_->RemoveItem(key, this);
  return NamedPropertyDeleterResult::kDeleted;
}

void StorageArea::clear(ExceptionState& exception_state) {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return;
  }
  cached_area_->Clear(this);
}

bool StorageArea::Contains(const String& key,
                           ExceptionState& exception_state) const {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return false;
  }
  return !cached_area_->GetItem(key).IsNull();
}

void StorageArea::NamedPropertyEnumerator(Vector<String>& names,
                                          ExceptionState& exception_state) {
  unsigned length = this->length(exception_state);
  if (exception_state.HadException())
    return;
  names.resize(length);
  for (unsigned i = 0; i < length; ++i) {
    String key = this->key(i, exception_state);
    if (exception_state.HadException())
      return;
    DCHECK(!key.IsNull());
    String val = getItem(key, exception_state);
    if (exception_state.HadException())
      return;
    names[i] = key;
  }
}

bool StorageArea::NamedPropertyQuery(const AtomicString& name,
                                     ExceptionState& exception_state) {
  if (name == "length")
    return false;
  bool found = Contains(name, exception_state);
  return found && !exception_state.HadException();
}

void StorageArea::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

bool StorageArea::CanAccessStorage() const {
  if (!DomWindow())
    return false;

  if (did_check_can_access_storage_)
    return can_access_storage_cached_result_;
  can_access_storage_cached_result_ = StorageController::CanAccessStorageArea(
      DomWindow()->GetFrame(), storage_type_);
  did_check_can_access_storage_ = true;
  return can_access_storage_cached_result_;
}

KURL StorageArea::GetPageUrl() const {
  return DomWindow() ? DomWindow()->Url() : KURL();
}

bool StorageArea::EnqueueStorageEvent(const String& key,
                                      const String& old_value,
                                      const String& new_value,
                                      const String& url) {
  if (!should_enqueue_events_)
    return true;
  if (!DomWindow())
    return false;
  DomWindow()->EnqueueWindowEvent(
      *StorageEvent::Create(event_type_names::kStorage, key, old_value,
                            new_value, url, this),
      TaskType::kDOMManipulation);
  return true;
}

blink::WebScopedVirtualTimePauser StorageArea::CreateWebScopedVirtualTimePauser(
    const char* name,
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  if (!DomWindow())
    return blink::WebScopedVirtualTimePauser();
  return DomWindow()
      ->GetFrame()
      ->GetFrameScheduler()
      ->CreateWebScopedVirtualTimePauser(name, duration);
}

LocalDOMWindow* StorageArea::GetDOMWindow() {
  return DomWindow();
}

void StorageArea::OnDocumentActivatedForPrerendering() {
  StorageNamespace* storage_namespace =
      StorageNamespace::From(DomWindow()->GetFrame()->GetPage());
  if (!storage_namespace)
    return;

  // Swap out the session storage state used within prerendering, and replace it
  // with the normal session storage state. For more details:
  // https://docs.google.com/document/d/1I5Hr8I20-C1GBr4tAXdm0U8a1RDUKHt4n7WcH4fxiSE/edit?usp=sharing
  cached_area_ = storage_namespace->GetCachedArea(DomWindow());
  cached_area_->RegisterSource(this);
}

}  // namespace blink
