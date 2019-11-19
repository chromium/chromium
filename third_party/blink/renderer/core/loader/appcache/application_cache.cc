/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#include "third_party/blink/renderer/core/loader/appcache/application_cache.h"

#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/hosts_using_features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

ApplicationCache::ApplicationCache(LocalFrame* frame) : DOMWindowClient(frame) {
  ApplicationCacheHostForFrame* cache_host = GetApplicationCacheHost();
  if (cache_host)
    cache_host->SetApplicationCache(this);
}

void ApplicationCache::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  DOMWindowClient::Trace(visitor);
}

ApplicationCacheHostForFrame* ApplicationCache::GetApplicationCacheHost()
    const {
  if (!GetFrame() || !GetFrame()->Loader().GetDocumentLoader())
    return nullptr;
  return GetFrame()->Loader().GetDocumentLoader()->GetApplicationCacheHost();
}

uint16_t ApplicationCache::status() const {
  // Application Cache status numeric values are specified in the HTML5 spec.
  static_assert(static_cast<uint16_t>(
                    mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED) == 0,
                "");
  static_assert(
      static_cast<uint16_t>(mojom::AppCacheStatus::APPCACHE_STATUS_IDLE) == 1,
      "");
  static_assert(static_cast<uint16_t>(
                    mojom::AppCacheStatus::APPCACHE_STATUS_CHECKING) == 2,
                "");
  static_assert(static_cast<uint16_t>(
                    mojom::AppCacheStatus::APPCACHE_STATUS_DOWNLOADING) == 3,
                "");
  static_assert(static_cast<uint16_t>(
                    mojom::AppCacheStatus::APPCACHE_STATUS_UPDATE_READY) == 4,
                "");
  static_assert(static_cast<uint16_t>(
                    mojom::AppCacheStatus::APPCACHE_STATUS_OBSOLETE) == 5,
                "");

  RecordAPIUseType();
  ApplicationCacheHostForFrame* cache_host = GetApplicationCacheHost();
  if (!cache_host) {
    return static_cast<uint16_t>(
        mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED);
  }
  return static_cast<uint16_t>(cache_host->GetStatus());
}

void ApplicationCache::update(ExceptionState& exception_state) {
  RecordAPIUseType();
  ApplicationCacheHostForFrame* cache_host = GetApplicationCacheHost();
  if (!cache_host || !cache_host->Update()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "there is no application cache to update.");
  }
}

void ApplicationCache::swapCache(ExceptionState& exception_state) {
  RecordAPIUseType();
  ApplicationCacheHostForFrame* cache_host = GetApplicationCacheHost();
  if (!cache_host || !cache_host->SwapCache()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "there is no newer application cache to swap to.");
  }
}

void ApplicationCache::abort() {
  ApplicationCacheHostForFrame* cache_host = GetApplicationCacheHost();
  if (cache_host)
    cache_host->Abort();
}

const AtomicString& ApplicationCache::InterfaceName() const {
  return event_target_names::kApplicationCache;
}

ExecutionContext* ApplicationCache::GetExecutionContext() const {
  return GetFrame() ? GetFrame()->GetDocument() : nullptr;
}

const AtomicString& ApplicationCache::ToEventType(mojom::AppCacheEventID id) {
  switch (id) {
    case mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT:
      return event_type_names::kChecking;
    case mojom::AppCacheEventID::APPCACHE_ERROR_EVENT:
      return event_type_names::kError;
    case mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT:
      return event_type_names::kNoupdate;
    case mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT:
      return event_type_names::kDownloading;
    case mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT:
      return event_type_names::kProgress;
    case mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT:
      return event_type_names::kUpdateready;
    case mojom::AppCacheEventID::APPCACHE_CACHED_EVENT:
      return event_type_names::kCached;
    case mojom::AppCacheEventID::APPCACHE_OBSOLETE_EVENT:
      return event_type_names::kObsolete;
  }
  NOTREACHED();
  return event_type_names::kError;
}

void ApplicationCache::RecordAPIUseType() const {
  if (!GetFrame())
    return;

  Document* document = GetFrame()->GetDocument();

  if (!document)
    return;

  if (document->IsSecureContext()) {
    Deprecation::CountDeprecation(document,
                                  WebFeature::kApplicationCacheAPISecureOrigin);
  } else {
    Deprecation::CountDeprecation(
        document, WebFeature::kApplicationCacheAPIInsecureOrigin);
    HostsUsingFeatures::CountAnyWorld(
        *document,
        HostsUsingFeatures::Feature::kApplicationCacheAPIInsecureHost);
  }
}

}  // namespace blink
