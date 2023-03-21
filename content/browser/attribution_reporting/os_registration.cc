// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/os_registration.h"

#include <utility>

#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

OsRegistration::OsRegistration(
    GURL registration_url,
    url::Origin top_level_origin,
    absl::optional<AttributionInputEvent> input_event)
    : registration_url(std::move(registration_url)),
      top_level_origin(std::move(top_level_origin)),
      input_event(std::move(input_event)) {}

OsRegistration::~OsRegistration() = default;

OsRegistration::OsRegistration(const OsRegistration&) = default;

OsRegistration& OsRegistration::operator=(const OsRegistration&) = default;

OsRegistration::OsRegistration(OsRegistration&&) = default;

OsRegistration& OsRegistration::operator=(OsRegistration&&) = default;

attribution_reporting::mojom::OsRegistrationType OsRegistration::GetType()
    const {
  return input_event.has_value()
             ? attribution_reporting::mojom::OsRegistrationType::kSource
             : attribution_reporting::mojom::OsRegistrationType::kTrigger;
}

}  // namespace content
