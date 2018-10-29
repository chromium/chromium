// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_NOTIFIER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_NOTIFIER_H_

#include <stdint.h>
#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace content {

// Tracks the live BackgroundFetchRegistration objects across the renderer
// processes and provides the functionality to notify them of progress updates.
class CONTENT_EXPORT BackgroundFetchRegistrationNotifier {
 public:
  BackgroundFetchRegistrationNotifier();
  ~BackgroundFetchRegistrationNotifier();

  // Registers the |observer| to be notified when fetches for the registration
  // identified by the |unique_id| progress.
  void AddObserver(
      const std::string& unique_id,
      blink::mojom::BackgroundFetchRegistrationObserverPtr observer);

  // Notifies any registered observers for the |registration| of the progress.
  // This will cause JavaScript events to fire.
  // Completed fetches must also call Notify with the final state.
  void Notify(const BackgroundFetchRegistration& registration);

  // Notifies any registered observers for the registration identifier by
  // |unique_id| that the records for the fetch are no longer available.
  void NotifyRecordsUnavailable(const std::string& unique_id);

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
                blink::mojom::BackgroundFetchRegistrationObserverPtr>
      observers_;

  base::WeakPtrFactory<BackgroundFetchRegistrationNotifier> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchRegistrationNotifier);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_NOTIFIER_H_
