// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_header_utils.h"

#include <utility>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/storable_source.h"

namespace content {

base::expected<StorableSource,
               attribution_reporting::mojom::SourceRegistrationError>
ParseSourceRegistration(base::Value::Dict registration,
                        base::Time source_time,
                        attribution_reporting::SuitableOrigin reporting_origin,
                        attribution_reporting::SuitableOrigin source_origin,
                        AttributionSourceType source_type,
                        bool is_within_fenced_frame) {
  base::expected<attribution_reporting::SourceRegistration,
                 attribution_reporting::mojom::SourceRegistrationError>
      reg = attribution_reporting::SourceRegistration::Parse(
          std::move(registration));
  if (!reg.has_value()) {
    return base::unexpected(reg.error());
  }

  return StorableSource(std::move(reporting_origin), std::move(*reg),
                        source_time, std::move(source_origin), source_type,
                        is_within_fenced_frame);
}

}  // namespace content
