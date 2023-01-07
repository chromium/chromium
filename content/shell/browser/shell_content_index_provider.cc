// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_content_index_provider.h"

namespace content {

ShellContentIndexProvider::ShellContentIndexProvider()
    : icon_sizes_({{96, 96}}) {}

ShellContentIndexProvider::~ShellContentIndexProvider() = default;

std::vector<gfx::Size> ShellContentIndexProvider::GetIconSizes(
    blink::mojom::ContentCategory category) {
  return icon_sizes_;
}

void ShellContentIndexProvider::OnContentAdded(ContentIndexEntry entry) {
  entries_[entry.description->id] = {
      entry.service_worker_registration_id,
      url::Origin::Create(entry.launch_url.DeprecatedGetOriginAsURL())};
}

void ShellContentIndexProvider::OnContentDeleted(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& description_id) {
  entries_.erase(description_id);
}

std::pair<int64_t, url::Origin>
ShellContentIndexProvider::GetRegistrationDataFromId(const std::string& id) {
  if (!entries_.count(id))
    return {-1, url::Origin()};
  return entries_[id];
}

}  // namespace content
