// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_ID_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_ID_H_

#include <stdint.h>
#include <string>

#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// The Background Fetch registration id corresponds to the information required
// to uniquely identify a Background Fetch registration in scope of a profile.
class CONTENT_EXPORT BackgroundFetchRegistrationId {
 public:
  // Constructs a null ID.
  BackgroundFetchRegistrationId();

  // See corresponding getters for descriptions of |developer_id| and
  // |unique_id|.
  BackgroundFetchRegistrationId(int64_t service_worker_registration_id,
                                const url::Origin& origin,
                                const std::string& developer_id,
                                const std::string& unique_id);

  // Copyable and movable.
  BackgroundFetchRegistrationId(const BackgroundFetchRegistrationId& other);
  BackgroundFetchRegistrationId(BackgroundFetchRegistrationId&& other);
  BackgroundFetchRegistrationId& operator=(
      const BackgroundFetchRegistrationId& other);
  BackgroundFetchRegistrationId& operator=(
      BackgroundFetchRegistrationId&& other);

  ~BackgroundFetchRegistrationId();

  // Return whether the |other| registration id are identical or different.
  bool operator==(const BackgroundFetchRegistrationId& other) const;
  bool operator!=(const BackgroundFetchRegistrationId& other) const;

  // Enables this type to be used in an std::map and std::set.
  bool operator<(const BackgroundFetchRegistrationId& other) const;

  // Returns whether this registration id refers to valid data.
  bool is_null() const;

  int64_t service_worker_registration_id() const {
    return service_worker_registration_id_;
  }
  const url::Origin& origin() const { return origin_; }

  // The IDL 'id' attribute provided by the website.
  //
  // Should not be used as a primary key in any data structures, since not only
  // are they per-ServiceWorkerRegistration, but there can exist two or more
  // different Background Fetch registrations at once with the same
  // |developer_id|. For example, if JavaScript holds a reference to a
  // BackgroundFetchRegistration object after that registration is
  // completed/failed/aborted and then the website creates a new registration
  // with the same |developer_id|, the original BackgroundFetchRegistration
  // object is still valid and must continue to refer to the old data.
  const std::string& developer_id() const { return developer_id_; }

  // The opaque globally unique ID for the Background Fetch registration.
  //
  // Values are never re-used. Internal only, never exposed to JavaScript.
  // Always use this instead of |developer_id| (APIs that receive a
  // |developer_id| from the website must look up the associated |unique_id| as
  // soon as possible).
  const std::string& unique_id() const { return unique_id_; }

 private:
  int64_t service_worker_registration_id_;
  url::Origin origin_;
  std::string developer_id_;
  std::string unique_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_ID_H_
