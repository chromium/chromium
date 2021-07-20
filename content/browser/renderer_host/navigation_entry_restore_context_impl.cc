// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"

#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "url/gurl.h"

namespace content {

// static
std::unique_ptr<NavigationEntryRestoreContext>
NavigationEntryRestoreContext::Create() {
  return std::make_unique<NavigationEntryRestoreContextImpl>();
}

NavigationEntryRestoreContextImpl::NavigationEntryRestoreContextImpl() =
    default;
NavigationEntryRestoreContextImpl::~NavigationEntryRestoreContextImpl() =
    default;

void NavigationEntryRestoreContextImpl::AddFrameNavigationEntry(
    FrameNavigationEntry* entry) {
  // Do not track FrameNavigationEntries for the default ISN of 0, since this
  // value can be used for any arbitrary document.
  if (entry->item_sequence_number() == 0)
    return;
  Key key(entry->item_sequence_number(), entry->frame_unique_name());
  DCHECK(entries_.find(key) == entries_.end());
  entries_.emplace(key, entry);
}

FrameNavigationEntry*
NavigationEntryRestoreContextImpl::GetFrameNavigationEntryForItemSequenceNumber(
    int64_t item_sequence_number,
    const std::string& unique_name,
    const GURL& expected_url) {
  if (item_sequence_number == 0)
    return nullptr;
  auto it = entries_.find(Key(item_sequence_number, unique_name));
  if (it == entries_.end())
    return nullptr;
  return it->second->url() == expected_url ? it->second : nullptr;
}

}  // namespace content
