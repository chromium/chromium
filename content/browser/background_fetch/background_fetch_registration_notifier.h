// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_NOTIFIER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_NOTIFIER_H_

#include <stdint.h>
#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {

// Tracks the live BackgroundFetchRegistration objects across the
// renderer processes and provides the functionality to notify them of progress
// updates.
class CONTENT_EXPORT BackgroundFetchRegistrationNotifier {
 public:
  BackgroundFetchRegistrationNotifier();

  BackgroundFetchRegistrationNotifier(
      const BackgroundFetchRegistrationNotifier&) = delete;
  BackgroundFetchRegistrationNotifier& operator=(
      const BackgroundFetchRegistrationNotifier&) = delete;

  ~BackgroundFetchRegistrationNotifier();

  // Registers the |observer| to be notified when fetches for the registration
  // identified by the |unique_id| progress.
  void AddObserver(
      const std::string& unique_id,
      mojo::PendingRemote<blink::mojom::BackgroundFetchRegistrationObserver>
          observer);

  // Notifies any registered observers for the |registration_data| of the
  // progress. This will cause JavaScript events to fire. Completed fetches must
  // also call Notify with the final state.
  void Notify(
      const std::string& unique_id,
      const blink::mojom::BackgroundFetchRegistrationData& registration_data);

  // Notifies any registered observers for the registration identified by
  // |unique_id| that the records for the fetch are no longer available.
  void NotifyRecordsUnavailable(const std::string& unique_id);

  // Notifies any registered observers for the registration identified by
  // |unique_id| that the |request| has completed. |response| points to the
  // completed response, if any.
  void NotifyRequestCompleted(const std::string& unique_id,
                              blink::mojom::FetchAPIRequestPtr request,
                              blink::mojom::FetchAPIResponsePtr response);

  // Add |url| to the list of |observed_urls_|. Once this is done, the
  // |observers_| start getting updates about any requests with this URL.
  void AddObservedUrl(const std::string& unique_id, const GURL& url);

  base::WeakPtr<BackgroundFetchRegistrationNotifier> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Called when the connection with the |observer| for the registration
  // identified by the |unique_id| goes away.
  void OnConnectionError(
      const std::string& unique_id,
      blink::mojom::BackgroundFetchRegistrationObserver* observer);

  // Storage of observers keyed by the |unique_id| of a registration.
  std::multimap<std::string,
                mojo::Remote<blink::mojom::BackgroundFetchRegistrationObserver>>
      observers_;

  // URLs the observers care about, indexed by the unique_id of the observer.
  std::map<std::string, std::set<GURL>> observed_urls_;

  base::WeakPtrFactory<BackgroundFetchRegistrationNotifier> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_NOTIFIER_H_
