// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_OBSERVER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_OBSERVER_H_

#include <memory>

#include "base/optional.h"

class SkBitmap;

namespace content {

struct BackgroundFetchOptions;
struct BackgroundFetchRegistration;
class BackgroundFetchRegistrationId;

// Observer interface for objects that would like to be notified about changes
// committed to storage through the Background Fetch data manager. All methods
// will be invoked on the IO thread.
class BackgroundFetchDataManagerObserver {
 public:
  // Called when the Background Fetch |registration| has been created.
  virtual void OnRegistrationCreated(
      const BackgroundFetchRegistrationId& registration_id,
      const BackgroundFetchRegistration& registration,
      const BackgroundFetchOptions& options,
      const SkBitmap& icon,
      int num_requests,
      bool start_paused) = 0;

  // Called when the UI options for the Background Fetch |registration_id| have
  // been updated in the data store.
  virtual void OnUpdatedUI(const BackgroundFetchRegistrationId& registration_id,
                           const base::Optional<std::string>& title,
                           const base::Optional<SkBitmap>& icon) = 0;

  // Called if corrupted data is found in the Service Worker database.
  virtual void OnServiceWorkerDatabaseCorrupted(
      int64_t service_worker_registration_id) = 0;

  // Called if the origin is out of quota during the fetch.
  virtual void OnQuotaExceeded(
      const BackgroundFetchRegistrationId& registration_id) = 0;

  // Called if a database task encountered a storage error in the context of a
  // fetch workflow, such as preparing a request or storing a response.
  virtual void OnFetchStorageError(
      const BackgroundFetchRegistrationId& registration_id) = 0;

  virtual ~BackgroundFetchDataManagerObserver() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_OBSERVER_H_
