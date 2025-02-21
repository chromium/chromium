// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_COOKIE_LISTENER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_COOKIE_LISTENER_H_

#include <memory>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace content {

// Listens for changes to the cookies for a specific URL. This includes both
// host and domain cookies. Keeps track of whether the cookies have changed or
// remained the same since the object is created until StopListening() is
// called.
class CONTENT_EXPORT PrefetchCookieListener
    : public network::mojom::CookieChangeListener {
 public:
  static std::unique_ptr<PrefetchCookieListener> MakeAndRegister(
      const GURL& url,
      network::mojom::CookieManager* cookie_manager);

  explicit PrefetchCookieListener(const GURL& url);
  ~PrefetchCookieListener() override;

  PrefetchCookieListener(const PrefetchCookieListener&) = delete;
  PrefetchCookieListener& operator=(const PrefetchCookieListener&) = delete;

  // Causes the Cookie Listener to stop listening to cookie changes to |url_|.
  // After this is called the value of |have_cookies_changed_| will no longer
  // change.
  void StopListening();

  // Gets whether the cookies of |url_| have changed between the creation of the
  // object and either the first time |StopListening| is called or now (if
  // |StopListening| has never been called).
  bool HaveCookiesChanged() const { return have_cookies_changed_; }

 private:
  // network::mojom::CookieChangeListener
  void OnCookieChange(const net::CookieChangeInfo& change) override;

  bool have_cookies_changed_ = false;
  GURL url_;

  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_COOKIE_LISTENER_H_
