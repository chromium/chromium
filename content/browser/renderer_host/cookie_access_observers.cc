// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/cookie_access_observers.h"

#include "services/network/public/mojom/cookie_access_observer.mojom.h"

namespace content {

CookieAccessObservers::CookieAccessObservers(
    NotifyCookiesAccessedCallback callback)
    : callback_(std::move(callback)) {}

CookieAccessObservers::~CookieAccessObservers() = default;

void CookieAccessObservers::Add(
    mojo::PendingReceiver<network::mojom::CookieAccessObserver> receiver,
    CookieAccessDetails::Source source) {
  cookie_observer_set_.Add(this, std::move(receiver), source);
}

CookieAccessObservers::PendingObserversWithContext
CookieAccessObservers::TakeReceiversWithContext() {
  return cookie_observer_set_.TakeReceiversWithContext();
}

void CookieAccessObservers::OnCookiesAccessed(
    std::vector<network::mojom::CookieAccessDetailsPtr> details_vector) {
  callback_.Run(std::move(details_vector),
                cookie_observer_set_.current_context());
}

void CookieAccessObservers::Clone(
    mojo::PendingReceiver<network::mojom::CookieAccessObserver> observer) {
  cookie_observer_set_.Add(this, std::move(observer),
                           cookie_observer_set_.current_context());
}

}  // namespace content
