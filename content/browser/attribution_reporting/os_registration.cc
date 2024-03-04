// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/os_registration.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "components/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "url/origin.h"

namespace content {

OsRegistration::OsRegistration(
    std::vector<attribution_reporting::OsRegistrationItem> items,
    url::Origin top_level_origin,
    std::optional<AttributionInputEvent> input_event,
    bool is_within_fenced_frame,
    GlobalRenderFrameHostId render_frame_id,
    ContentBrowserClient::AttributionReportingOsReportTypes os_report_types)
    : registration_items(std::move(items)),
      top_level_origin(std::move(top_level_origin)),
      input_event(std::move(input_event)),
      is_within_fenced_frame(is_within_fenced_frame),
      render_frame_id(render_frame_id) {
  CHECK(!this->registration_items.empty());
  switch (GetType()) {
    case attribution_reporting::mojom::RegistrationType::kSource:
      report_type = os_report_types.source_report_type;
      break;
    case attribution_reporting::mojom::RegistrationType::kTrigger:
      report_type = os_report_types.trigger_report_type;
      break;
  }
}

OsRegistration::~OsRegistration() = default;

OsRegistration::OsRegistration(const OsRegistration&) = default;

OsRegistration& OsRegistration::operator=(const OsRegistration&) = default;

OsRegistration::OsRegistration(OsRegistration&&) = default;

OsRegistration& OsRegistration::operator=(OsRegistration&&) = default;

attribution_reporting::mojom::RegistrationType OsRegistration::GetType() const {
  return input_event.has_value()
             ? attribution_reporting::mojom::RegistrationType::kSource
             : attribution_reporting::mojom::RegistrationType::kTrigger;
}

}  // namespace content
