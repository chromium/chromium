// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/dom_window_storage.h"

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/storage/storage_area.h"
#include "third_party/blink/renderer/modules/storage/storage_controller.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

DOMWindowStorage::DOMWindowStorage(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void DOMWindowStorage::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_storage_);
  visitor->Trace(local_storage_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
const char DOMWindowStorage::kSupplementName[] = "DOMWindowStorage";

// static
DOMWindowStorage& DOMWindowStorage::From(LocalDOMWindow& window) {
  DOMWindowStorage* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowStorage>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowStorage>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

// static
StorageArea* DOMWindowStorage::sessionStorage(LocalDOMWindow& window,
                                              ExceptionState& exception_state) {
  return From(window).sessionStorage(exception_state);
}

// static
StorageArea* DOMWindowStorage::localStorage(LocalDOMWindow& window,
                                            ExceptionState& exception_state) {
  return From(window).localStorage(exception_state);
}

StorageArea* DOMWindowStorage::sessionStorage(
    ExceptionState& exception_state) const {
  if (!GetSupplementable()->GetFrame())
    return nullptr;

  Document* document = GetSupplementable()->GetFrame()->GetDocument();
  DCHECK(document);
  String access_denied_message = "Access is denied for this document.";
  if (!document->GetSecurityOrigin()->CanAccessSessionStorage()) {
    if (document->IsSandboxed(WebSandboxFlags::kOrigin))
      exception_state.ThrowSecurityError(
          "The document is sandboxed and lacks the 'allow-same-origin' flag.");
    else if (document->Url().ProtocolIs("data"))
      exception_state.ThrowSecurityError(
          "Storage is disabled inside 'data:' URLs.");
    else
      exception_state.ThrowSecurityError(access_denied_message);
    return nullptr;
  }

  if (document->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(document, WebFeature::kFileAccessedSessionStorage);
  }

  if (session_storage_) {
    if (!session_storage_->CanAccessStorage()) {
      exception_state.ThrowSecurityError(access_denied_message);
      return nullptr;
    }
    return session_storage_;
  }

  Page* page = document->GetPage();
  if (!page)
    return nullptr;

  StorageNamespace* storage_namespace = StorageNamespace::From(page);
  if (!storage_namespace)
    return nullptr;
  auto storage_area =
      storage_namespace->GetCachedArea(document->GetSecurityOrigin());
  session_storage_ =
      StorageArea::Create(document->GetFrame(), std::move(storage_area),
                          StorageArea::StorageType::kSessionStorage);

  if (!session_storage_->CanAccessStorage()) {
    exception_state.ThrowSecurityError(access_denied_message);
    return nullptr;
  }
  return session_storage_;
}

StorageArea* DOMWindowStorage::localStorage(
    ExceptionState& exception_state) const {
  if (!GetSupplementable()->GetFrame())
    return nullptr;

  Document* document = GetSupplementable()->GetFrame()->GetDocument();
  DCHECK(document);
  String access_denied_message = "Access is denied for this document.";
  if (!document->GetSecurityOrigin()->CanAccessLocalStorage()) {
    if (document->IsSandboxed(WebSandboxFlags::kOrigin))
      exception_state.ThrowSecurityError(
          "The document is sandboxed and lacks the 'allow-same-origin' flag.");
    else if (document->Url().ProtocolIs("data"))
      exception_state.ThrowSecurityError(
          "Storage is disabled inside 'data:' URLs.");
    else
      exception_state.ThrowSecurityError(access_denied_message);
    return nullptr;
  }

  if (document->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(document, WebFeature::kFileAccessedLocalStorage);
  }

  if (local_storage_) {
    if (!local_storage_->CanAccessStorage()) {
      exception_state.ThrowSecurityError(access_denied_message);
      return nullptr;
    }
    return local_storage_;
  }
  // FIXME: Seems this check should be much higher?
  Page* page = document->GetPage();
  if (!page || !page->GetSettings().GetLocalStorageEnabled())
    return nullptr;
  auto storage_area = StorageController::GetInstance()->GetLocalStorageArea(
      document->GetSecurityOrigin());
  local_storage_ =
      StorageArea::Create(document->GetFrame(), std::move(storage_area),
                          StorageArea::StorageType::kLocalStorage);

  if (!local_storage_->CanAccessStorage()) {
    exception_state.ThrowSecurityError(access_denied_message);
    return nullptr;
  }
  return local_storage_;
}

}  // namespace blink
