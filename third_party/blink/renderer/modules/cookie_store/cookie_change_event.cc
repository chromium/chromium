// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_store/cookie_change_event.h"

#include <utility>

#include "services/network/public/mojom/cookie_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cookie_change_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cookie_list_item.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CookieChangeEvent::~CookieChangeEvent() = default;

const AtomicString& CookieChangeEvent::InterfaceName() const {
  return event_interface_names::kCookieChangeEvent;
}

void CookieChangeEvent::Trace(Visitor* visitor) const {
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

String ToCookieListItemSameSite(net::CookieSameSite same_site) {
  switch (same_site) {
    case net::CookieSameSite::STRICT_MODE:
      return "strict";
    case net::CookieSameSite::LAX_MODE:
      return "lax";
    case net::CookieSameSite::NO_RESTRICTION:
      return "none";
    case net::CookieSameSite::UNSPECIFIED:
      return String();
  }

  NOTREACHED_IN_MIGRATION();
}

String ToCookieListItemEffectiveSameSite(
    network::mojom::CookieEffectiveSameSite effective_same_site) {
  switch (effective_same_site) {
    case network::mojom::CookieEffectiveSameSite::kStrictMode:
      return "strict";
    case network::mojom::CookieEffectiveSameSite::kLaxMode:
    case network::mojom::CookieEffectiveSameSite::kLaxModeAllowUnsafe:
      return "lax";
    case network::mojom::CookieEffectiveSameSite::kNoRestriction:
      return "none";
    case network::mojom::CookieEffectiveSameSite::kUndefined:
      return String();
  }
}

}  // namespace

// static
CookieListItem* CookieChangeEvent::ToCookieListItem(
    const net::CanonicalCookie& canonical_cookie,
    const network::mojom::blink::CookieEffectiveSameSite& effective_same_site,
    bool is_deleted) {
  CookieListItem* list_item = CookieListItem::Create();

  list_item->setName(String::FromUTF8(canonical_cookie.Name()));
  list_item->setPath(String::FromUTF8(canonical_cookie.Path()));

  list_item->setSecure(canonical_cookie.SecureAttribute());
  // Use effective same site if available, otherwise use same site.
  auto&& same_site = ToCookieListItemEffectiveSameSite(effective_same_site);
  if (same_site.IsNull())
    same_site = ToCookieListItemSameSite(canonical_cookie.SameSite());
  if (!same_site.IsNull())
    list_item->setSameSite(same_site);

  // The domain of host-only cookies is the host name, without a dot (.) prefix.
  String cookie_domain = String::FromUTF8(canonical_cookie.Domain());
  if (cookie_domain.StartsWith(".")) {
    list_item->setDomain(cookie_domain.Substring(1));
  } else {
    list_item->setDomain(String());
  }

  if (!is_deleted) {
    list_item->setValue(String::FromUTF8(canonical_cookie.Value()));
    if (canonical_cookie.ExpiryDate().is_null()) {
      list_item->setExpires(std::nullopt);
    } else {
      list_item->setExpires(
          ConvertTimeToDOMHighResTimeStamp(canonical_cookie.ExpiryDate()));
    }
  }

  list_item->setPartitioned(canonical_cookie.IsPartitioned());

  return list_item;
}

// static
void CookieChangeEvent::ToEventInfo(
    const network::mojom::blink::CookieChangeInfoPtr& change_info,
    HeapVector<Member<CookieListItem>>& changed,
    HeapVector<Member<CookieListItem>>& deleted) {
  switch (change_info->cause) {
    case ::network::mojom::CookieChangeCause::INSERTED: {
      CookieListItem* cookie = ToCookieListItem(
          change_info->cookie, change_info->access_result->effective_same_site,
          false /* is_deleted */);
      changed.push_back(cookie);
      break;
    }
    case ::network::mojom::CookieChangeCause::EXPLICIT:
    case ::network::mojom::CookieChangeCause::UNKNOWN_DELETION:
    case ::network::mojom::CookieChangeCause::EXPIRED:
    case ::network::mojom::CookieChangeCause::EVICTED:
    case ::network::mojom::CookieChangeCause::EXPIRED_OVERWRITE: {
      CookieListItem* cookie = ToCookieListItem(
          change_info->cookie, change_info->access_result->effective_same_site,
          true /* is_deleted */);
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
