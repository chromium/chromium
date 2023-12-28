// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_AUTOMATIC_BEACON_INFO_H_
#define CONTENT_BROWSER_FENCED_FRAME_AUTOMATIC_BEACON_INFO_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"

namespace content {

struct CONTENT_EXPORT AutomaticBeaconInfo {
  AutomaticBeaconInfo();

  AutomaticBeaconInfo(
      const std::string& data,
      const std::vector<blink::FencedFrame::ReportingDestination>& destinations,
      bool once,
      bool cross_origin_exposed);

  AutomaticBeaconInfo(const AutomaticBeaconInfo&);
  AutomaticBeaconInfo(AutomaticBeaconInfo&&);

  AutomaticBeaconInfo& operator=(const AutomaticBeaconInfo&);
  AutomaticBeaconInfo& operator=(AutomaticBeaconInfo&&);

  ~AutomaticBeaconInfo();

  std::string data;
  std::vector<blink::FencedFrame::ReportingDestination> destinations;
  // Indicates whether the automatic beacon will only be sent out for one event,
  // or if it will be sent out every time an event occurs.
  bool once;
  // Indicates whether this data can be used for automatic beacons that
  // originate from a document that is cross-origin to the fenced frame config's
  // mapped URL.
  bool cross_origin_exposed;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_AUTOMATIC_BEACON_INFO_H_
