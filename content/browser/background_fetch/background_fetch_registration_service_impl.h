// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_SERVICE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {

class BackgroundFetchRegistrationServiceImpl
    : public blink::mojom::BackgroundFetchRegistrationService {
 public:
  static mojo::PendingRemote<blink::mojom::BackgroundFetchRegistrationService>
  CreateInterfaceInfo(
      BackgroundFetchRegistrationId registration_id,
      base::WeakPtr<BackgroundFetchContext> background_fetch_context);

  // blink::mojom::BackgroundFetchRegistrationService implementation.
  void MatchRequests(blink::mojom::FetchAPIRequestPtr request_to_match,
                     blink::mojom::CacheQueryOptionsPtr cache_query_options,
                     bool match_all,
                     MatchRequestsCallback callback) override;
  void UpdateUI(const std::optional<std::string>& title,
                const SkBitmap& icon,
                UpdateUICallback callback) override;
  void Abort(AbortCallback callback) override;
  void AddRegistrationObserver(
      mojo::PendingRemote<blink::mojom::BackgroundFetchRegistrationObserver>
          observer) override;

  BackgroundFetchRegistrationServiceImpl(
      const BackgroundFetchRegistrationServiceImpl&) = delete;
  BackgroundFetchRegistrationServiceImpl& operator=(
      const BackgroundFetchRegistrationServiceImpl&) = delete;

  ~BackgroundFetchRegistrationServiceImpl() override;

 private:
  BackgroundFetchRegistrationServiceImpl(
      BackgroundFetchRegistrationId registration_id,
      base::WeakPtr<BackgroundFetchContext> background_fetch_context);

  [[nodiscard]] bool ValidateTitle(const std::string& title);

  BackgroundFetchRegistrationId registration_id_;
  base::WeakPtr<BackgroundFetchContext> background_fetch_context_;
  mojo::Receiver<blink::mojom::BackgroundFetchRegistrationService> receiver_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_SERVICE_IMPL_H_
