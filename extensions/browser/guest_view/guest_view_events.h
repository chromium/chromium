// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_EVENTS_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_EVENTS_H_

#include <string>

#include "extensions/browser/extension_event_histogram_value.h"

namespace extensions {
namespace guest_view_events {

// Returns the events::HistogramValue for the |event_name| guest view event.
// This knows about all events for all guest view types, whether web view,
// extension options, the guest view base class, etc.
events::HistogramValue GetEventHistogramValue(const std::string& event_name);

}  // namespace guest_view_events
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_EVENTS_H_
