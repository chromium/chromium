// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_OBSERVER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

class SkBitmap;

namespace content {

class BackgroundFetchRegistrationId;
class BackgroundFetchRequestInfo;

// Observer interface for objects that would like to be notified about changes
// committed to storage through the Background Fetch data manager. All methods
// will be invoked on the service worker core thread.
class BackgroundFetchDataManagerObserver {
 public:
  // Called when the Background Fetch registration has been created.
  virtual void OnRegistrationCreated(
      const BackgroundFetchRegistrationId& registration_id,
      const blink::mojom::BackgroundFetchRegistrationData& registration_data,
      blink::mojom::BackgroundFetchOptionsPtr options,
      const SkBitmap& icon,
      int num_requests,
      bool start_paused) = 0;

  // Called on start-up when an incomplete registration has been found.
  virtual void OnRegistrationLoadedAtStartup(
      const BackgroundFetchRegistrationId& registration_id,
      const blink::mojom::BackgroundFetchRegistrationData& registration_data,
      blink::mojom::BackgroundFetchOptionsPtr options,
      const SkBitmap& icon,
      int num_completed_requests,
      int num_requests,
      std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
          active_fetch_requests) = 0;

  // Called when a registration is being queried. Implementations should update
  // |registration_data| with in-progress information.
  virtual void OnRegistrationQueried(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchRegistrationData* registration_data) = 0;

  // Called if corrupted data is found in the Service Worker database.
  virtual void OnServiceWorkerDatabaseCorrupted(
      int64_t service_worker_registration_id) = 0;

  // Called when a request has been completed, for the registration identified
  // by |unique_id|.
  virtual void OnRequestCompleted(
      const std::string& unique_id,
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::FetchAPIResponsePtr response) = 0;

  virtual ~BackgroundFetchDataManagerObserver() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_OBSERVER_H_
