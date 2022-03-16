// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EVENT_FILTERING_INFO_H_
#define EXTENSIONS_COMMON_EVENT_FILTERING_INFO_H_

#include <memory>
#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace extensions {

// Extra information about an event that is used in event filtering.
//
// This is the information that is matched against criteria specified in JS
// extension event listeners. Eg:
//
// chrome.someApi.onSomeEvent.addListener(cb,
//                                        {url: [{hostSuffix: 'google.com'}],
//                                         tabId: 1});
struct EventFilteringInfo {
 public:
  EventFilteringInfo();
  EventFilteringInfo(const EventFilteringInfo& other);
  ~EventFilteringInfo();

  absl::optional<GURL> url;
  absl::optional<std::string> service_type;
  absl::optional<int> instance_id;

  // Note: window type & visible are Chrome concepts, so arguably
  // doesn't belong in the extensions module. If the number of Chrome
  // concept grows, consider a delegation model with a
  // ChromeEventFilteringInfo class.
  absl::optional<std::string> window_type;

  // By default events related to windows are filtered based on the
  // listener's extension. This parameter will be set if the listener
  // didn't set any filter on window types.
  absl::optional<bool> window_exposed_by_default;

  bool is_empty() const {
    return !url && !service_type && !instance_id && !window_type &&
           !window_exposed_by_default;
  }
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EVENT_FILTERING_INFO_H_
