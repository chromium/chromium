// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/os_registration.h"

#include <utility>

#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

OsRegistration::OsRegistration(
    GURL registration_url,
    bool debug_reporting,
    url::Origin top_level_origin,
    absl::optional<AttributionInputEvent> input_event,
    bool is_within_fenced_frame,
    GlobalRenderFrameHostId render_frame_id)
    : registration_url(std::move(registration_url)),
      debug_reporting(debug_reporting),
      top_level_origin(std::move(top_level_origin)),
      input_event(std::move(input_event)),
      is_within_fenced_frame(is_within_fenced_frame),
      render_frame_id(render_frame_id) {}

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
