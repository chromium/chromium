// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENT_PAGE_TRACKER_H_
#define EXTENSIONS_BROWSER_EVENT_PAGE_TRACKER_H_

#include "base/functional/callback_forward.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// Tracks an extension's event page suspend state.
class EventPageTracker {
 public:
  // Returns true if an extension's event page is suspended,
  // or false if it is active.
  virtual bool IsEventPageSuspended(const ExtensionId& extension_id) = 0;

  // Wakes an extension's event page from a suspended state and calls
  // |callback| after it is reactivated.
  //
  // |callback| will be passed true if the extension was reactivated
  // successfully, or false if an error occurred.
  //
  // Returns true if a wake operation was scheduled successfully,
  // or false if the event page was already awake.
  // Callback will be run asynchronously if true, and never run if false.
  virtual bool WakeEventPage(const ExtensionId& extension_id,
                             base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENT_PAGE_TRACKER_H_
