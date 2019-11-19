// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_SERVICE_IMPL_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {

class CONTENT_EXPORT BackgroundFetchRegistrationServiceImpl
    : public blink::mojom::BackgroundFetchRegistrationService {
 public:
  static mojo::PendingRemote<blink::mojom::BackgroundFetchRegistrationService>
  CreateInterfaceInfo(
      BackgroundFetchRegistrationId registration_id,
      scoped_refptr<BackgroundFetchContext> background_fetch_context);

  // blink::mojom::BackgroundFetchRegistrationService implementation.
  void MatchRequests(blink::mojom::FetchAPIRequestPtr request_to_match,
                     blink::mojom::CacheQueryOptionsPtr cache_query_options,
                     bool match_all,
                     MatchRequestsCallback callback) override;
  void UpdateUI(const base::Optional<std::string>& title,
                const SkBitmap& icon,
                UpdateUICallback callback) override;
  void Abort(AbortCallback callback) override;
  void AddRegistrationObserver(
      mojo::PendingRemote<blink::mojom::BackgroundFetchRegistrationObserver>
          observer) override;

  ~BackgroundFetchRegistrationServiceImpl() override;

 private:
  BackgroundFetchRegistrationServiceImpl(
      BackgroundFetchRegistrationId registration_id,
      scoped_refptr<BackgroundFetchContext> background_fetch_context);

  bool ValidateTitle(const std::string& title) WARN_UNUSED_RESULT;

  BackgroundFetchRegistrationId registration_id_;
  scoped_refptr<BackgroundFetchContext> background_fetch_context_;
  mojo::Receiver<blink::mojom::BackgroundFetchRegistrationService> receiver_{
      this};

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchRegistrationServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_SERVICE_IMPL_H_
