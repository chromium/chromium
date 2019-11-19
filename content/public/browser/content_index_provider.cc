// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/content_index_provider.h"

namespace content {

ContentIndexEntry::ContentIndexEntry(
    int64_t service_worker_registration_id,
    blink::mojom::ContentDescriptionPtr description,
    const GURL& launch_url,
    base::Time registration_time)
    : service_worker_registration_id(service_worker_registration_id),
      description(std::move(description)),
      launch_url(launch_url),
      registration_time(registration_time) {}

ContentIndexEntry::ContentIndexEntry(ContentIndexEntry&& other) = default;

ContentIndexEntry& ContentIndexEntry::operator=(ContentIndexEntry&& other) {
  service_worker_registration_id = other.service_worker_registration_id;
  description = std::move(other.description);
  launch_url = std::move(other.launch_url);
  registration_time = other.registration_time;
  return *this;
}

ContentIndexEntry::~ContentIndexEntry() = default;

ContentIndexProvider::ContentIndexProvider() = default;

ContentIndexProvider::~ContentIndexProvider() = default;

}  // namespace content
