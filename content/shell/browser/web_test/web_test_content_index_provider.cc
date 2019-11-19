// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_content_index_provider.h"

namespace content {

WebTestContentIndexProvider::WebTestContentIndexProvider()
    : icon_sizes_({{96, 96}}) {}

WebTestContentIndexProvider::~WebTestContentIndexProvider() = default;

std::vector<gfx::Size> WebTestContentIndexProvider::GetIconSizes(
    blink::mojom::ContentCategory category) {
  return icon_sizes_;
}

void WebTestContentIndexProvider::OnContentAdded(ContentIndexEntry entry) {
  entries_[entry.description->id] = {
      entry.service_worker_registration_id,
      url::Origin::Create(entry.launch_url.GetOrigin())};
}

void WebTestContentIndexProvider::OnContentDeleted(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& description_id) {
  entries_.erase(description_id);
}

std::pair<int64_t, url::Origin>
WebTestContentIndexProvider::GetRegistrationDataFromId(const std::string& id) {
  if (!entries_.count(id))
    return {-1, url::Origin()};
  return entries_[id];
}

}  // namespace content