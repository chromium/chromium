// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_ENTRY_RESTORE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_ENTRY_RESTORE_CONTEXT_H_

#include <memory>

#include "content/common/content_export.h"

namespace content {

// A NavigationEntryRestoreContext is an opaque structure passed to
// NavigationEntry::SetPageState() when restoring a vector of NavigationEntries.
// It tracks the item sequence number (ISN) associated with each session
// history item, and maintains a mapping of ISNs to session history items to
// ensure items are de-duplicated if they appear in multiple NavigationEntries.
class NavigationEntryRestoreContext {
 public:
  virtual ~NavigationEntryRestoreContext() = default;

  CONTENT_EXPORT static std::unique_ptr<NavigationEntryRestoreContext> Create();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_ENTRY_RESTORE_CONTEXT_H_
