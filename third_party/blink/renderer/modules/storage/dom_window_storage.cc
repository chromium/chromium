// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/dom_window_storage.h"

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
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

void DOMWindowStorage::Trace(Visitor* visitor) const {
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
  StorageArea* storage = GetOrCreateSessionStorage(exception_state, {});
  if (!storage)
    return nullptr;

  LocalDOMWindow* window = GetSupplementable();
  if (window->GetSecurityOrigin()->IsLocal())
    UseCounter::Count(window, WebFeature::kFileAccessedSessionStorage);

  if (!storage->CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return nullptr;
  }
  return storage;
}

StorageArea* DOMWindowStorage::localStorage(
    ExceptionState& exception_state) const {
  StorageArea* storage = GetOrCreateLocalStorage(exception_state, {});
  if (!storage)
    return nullptr;

  LocalDOMWindow* window = GetSupplementable();
  if (window->GetSecurityOrigin()->IsLocal())
    UseCounter::Count(window, WebFeature::kFileAccessedLocalStorage);

  if (!storage->CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return nullptr;
  }
  return storage;
}

void DOMWindowStorage::InitSessionStorage(
    mojo::PendingRemote<mojom::blink::StorageArea> storage_area) const {
  // It's safe to ignore exceptions here since this is just an optimization to
  // avoid requesting the storage area later.
  GetOrCreateSessionStorage(IGNORE_EXCEPTION_FOR_TESTING,
                            std::move(storage_area));
}

void DOMWindowStorage::InitLocalStorage(
    mojo::PendingRemote<mojom::blink::StorageArea> storage_area) const {
  // It's safe to ignore exceptions here since this is just an optimization to
  // avoid requesting the storage area later.
  GetOrCreateLocalStorage(IGNORE_EXCEPTION_FOR_TESTING,
                          std::move(storage_area));
}

StorageArea* DOMWindowStorage::GetOrCreateSessionStorage(
    ExceptionState& exception_state,
    mojo::PendingRemote<mojom::blink::StorageArea> storage_area_for_init)
    const {
  LocalDOMWindow* window = GetSupplementable();
  if (!window->GetFrame())
    return nullptr;

  if (!window->GetSecurityOrigin()->CanAccessSessionStorage()) {
    if (window->IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin))
      exception_state.ThrowSecurityError(StorageArea::kAccessSandboxedMessage);
    else if (window->Url().ProtocolIs("data"))
      exception_state.ThrowSecurityError(StorageArea::kAccessDataMessage);
    else
      exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return nullptr;
  }

  if (window->GetFrame()->Client()->IsDomStorageDisabled()) {
    return nullptr;
  }

  if (session_storage_)
    return session_storage_.Get();

  StorageNamespace* storage_namespace =
      StorageNamespace::From(window->GetFrame()->GetPage());
  if (!storage_namespace)
    return nullptr;
  scoped_refptr<CachedStorageArea> cached_storage_area;
  if (window->document()->IsPrerendering()) {
    cached_storage_area = storage_namespace->CreateCachedAreaForPrerender(
        window, std::move(storage_area_for_init));
  } else {
    cached_storage_area = storage_namespace->GetCachedArea(
        window, std::move(storage_area_for_init));
  }
  session_storage_ =
      StorageArea::Create(window, std::move(cached_storage_area),
                          StorageArea::StorageType::kSessionStorage);

  return session_storage_.Get();
}

StorageArea* DOMWindowStorage::GetOrCreateLocalStorage(
    ExceptionState& exception_state,
    mojo::PendingRemote<mojom::blink::StorageArea> storage_area_for_init)
    const {
  LocalDOMWindow* window = GetSupplementable();
  if (!window->GetFrame())
    return nullptr;

  if (!window->GetSecurityOrigin()->CanAccessLocalStorage()) {
    if (window->IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin))
      exception_state.ThrowSecurityError(StorageArea::kAccessSandboxedMessage);
    else if (window->Url().ProtocolIs("data"))
      exception_state.ThrowSecurityError(StorageArea::kAccessDataMessage);
    else
      exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return nullptr;
  }

  if (!window->GetFrame()->GetSettings()->GetLocalStorageEnabled()) {
    return nullptr;
  }

  if (window->GetFrame()->Client()->IsDomStorageDisabled()) {
    return nullptr;
  }

  if (local_storage_)
    return local_storage_.Get();

  auto storage_area = StorageController::GetInstance()->GetLocalStorageArea(
      window, std::move(storage_area_for_init));
  local_storage_ = StorageArea::Create(window, std::move(storage_area),
                                       StorageArea::StorageType::kLocalStorage);
  return local_storage_.Get();
}

}  // namespace blink
