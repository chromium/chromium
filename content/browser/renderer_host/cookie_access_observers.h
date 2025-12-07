// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COOKIE_ACCESS_OBSERVERS_H_
#define CONTENT_BROWSER_RENDERER_HOST_COOKIE_ACCESS_OBSERVERS_H_

#include "content/common/content_export.h"
#include "content/public/browser/cookie_access_details.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"

namespace content {

// A thin wrapper around ReceiverSet that inherits from
// mojom::CookieAccessObserver. Directly subclassing mojom::CookieAccessObserver
// would force classes to have a public OnCookiesAccessed() method that doesn't
// take a CookieAccessDetails::Source, and we don't want to expose that. If
// other classes call OnCookiesAccessed() they need to pass a Source.
class CONTENT_EXPORT CookieAccessObservers
    : public network::mojom::CookieAccessObserver {
 public:
  using NotifyCookiesAccessedCallback = base::RepeatingCallback<void(
      std::vector<network::mojom::CookieAccessDetailsPtr>,
      CookieAccessDetails::Source)>;

  explicit CookieAccessObservers(NotifyCookiesAccessedCallback callback);

  CookieAccessObservers(const CookieAccessObservers&) = delete;
  CookieAccessObservers& operator=(const CookieAccessObservers&) = delete;

  ~CookieAccessObservers() override;

  virtual void Add(
      mojo::PendingReceiver<network::mojom::CookieAccessObserver> receiver,
      CookieAccessDetails::Source source);

  using PendingObserversWithContext = std::vector<
      std::pair<mojo::PendingReceiver<network::mojom::CookieAccessObserver>,
                CookieAccessDetails::Source>>;
  virtual PendingObserversWithContext TakeReceiversWithContext();

  // network::mojom::CookieAccessObserver
  void OnCookiesAccessed(std::vector<network::mojom::CookieAccessDetailsPtr>
                             details_vector) override;
  void Clone(mojo::PendingReceiver<network::mojom::CookieAccessObserver>
                 observer) override;

 private:
  NotifyCookiesAccessedCallback callback_;

  mojo::ReceiverSet<network::mojom::CookieAccessObserver,
                    CookieAccessDetails::Source>
      cookie_observer_set_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COOKIE_ACCESS_OBSERVERS_H_
