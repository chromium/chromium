// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_store/cookie_change_event.h"

#include <utility>

#include "services/network/public/mojom/cookie_manager.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_time_stamp.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_change_event_init.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_list_item.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/cookie/canonical_cookie.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CookieChangeEvent::~CookieChangeEvent() = default;

const AtomicString& CookieChangeEvent::InterfaceName() const {
  return event_interface_names::kCookieChangeEvent;
}

void CookieChangeEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
  visitor->Trace(changed_);
  visitor->Trace(deleted_);
}

CookieChangeEvent::CookieChangeEvent() = default;

CookieChangeEvent::CookieChangeEvent(const AtomicString& type,
                                     HeapVector<Member<CookieListItem>> changed,
                                     HeapVector<Member<CookieListItem>> deleted)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      changed_(std::move(changed)),
      deleted_(std::move(deleted)) {}

CookieChangeEvent::CookieChangeEvent(const AtomicString& type,
                                     const CookieChangeEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasChanged())
    changed_ = initializer->changed();
  if (initializer->hasDeleted())
    deleted_ = initializer->deleted();
}

namespace {

String ToCookieListItemSameSite(network::mojom::CookieSameSite same_site) {
  switch (same_site) {
    case network::mojom::CookieSameSite::STRICT_MODE:
      return "strict";
    case network::mojom::CookieSameSite::LAX_MODE:
      return "lax";
    case network::mojom::CookieSameSite::NO_RESTRICTION:
      return "unrestricted";
    case network::mojom::CookieSameSite::UNSPECIFIED:
      return "unspecified";
  }

  NOTREACHED();
}

}  // namespace

// static
CookieListItem* CookieChangeEvent::ToCookieListItem(
    const CanonicalCookie& canonical_cookie,
    bool is_deleted) {
  CookieListItem* list_item = CookieListItem::Create();

  list_item->setName(canonical_cookie.Name());
  list_item->setPath(canonical_cookie.Path());
  list_item->setSecure(canonical_cookie.IsSecure());
  list_item->setSameSite(ToCookieListItemSameSite(canonical_cookie.SameSite()));

  // The domain of host-only cookies is the host name, without a dot (.) prefix.
  String cookie_domain = canonical_cookie.Domain();
  if (cookie_domain.StartsWith("."))
    list_item->setDomain(cookie_domain.Substring(1));

  if (!is_deleted) {
    list_item->setValue(canonical_cookie.Value());
    if (!canonical_cookie.ExpiryDate().is_null()) {
      list_item->setExpires(ConvertSecondsToDOMTimeStamp(
          canonical_cookie.ExpiryDate().ToDoubleT()));
    }
  }
  return list_item;
}

// static
void CookieChangeEvent::ToEventInfo(
    const CanonicalCookie& backend_cookie,
    ::network::mojom::CookieChangeCause change_cause,
    HeapVector<Member<CookieListItem>>& changed,
    HeapVector<Member<CookieListItem>>& deleted) {
  switch (change_cause) {
    case ::network::mojom::CookieChangeCause::INSERTED: {
      CookieListItem* cookie =
          ToCookieListItem(backend_cookie, false /* is_deleted */);
      changed.push_back(cookie);
      break;
    }
    case ::network::mojom::CookieChangeCause::EXPLICIT:
    case ::network::mojom::CookieChangeCause::UNKNOWN_DELETION:
    case ::network::mojom::CookieChangeCause::EXPIRED:
    case ::network::mojom::CookieChangeCause::EVICTED:
    case ::network::mojom::CookieChangeCause::EXPIRED_OVERWRITE: {
      CookieListItem* cookie =
          ToCookieListItem(backend_cookie, true /* is_deleted */);
      deleted.push_back(cookie);
      break;
    }

    case ::network::mojom::CookieChangeCause::OVERWRITE:
      // A cookie overwrite causes an OVERWRITE (meaning the old cookie was
      // deleted) and an INSERTED.
      break;
  }
}

}  // namespace blink
