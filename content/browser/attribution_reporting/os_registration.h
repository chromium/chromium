// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_

#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

struct CONTENT_EXPORT OsRegistration {
  GURL registration_url;
  bool debug_reporting;
  url::Origin top_level_origin;
  // If `absl::nullopt`, represents an OS trigger. Otherwise, represents an OS
  // source.
  absl::optional<AttributionInputEvent> input_event;
  bool is_within_fenced_frame;

  OsRegistration(GURL registration_url,
                 bool debug_reporting,
                 url::Origin top_level_origin,
                 absl::optional<AttributionInputEvent> input_event,
                 bool is_within_fenced_frame);

  ~OsRegistration();

  OsRegistration(const OsRegistration&);
  OsRegistration& operator=(const OsRegistration&);

  OsRegistration(OsRegistration&&);
  OsRegistration& operator=(OsRegistration&&);

  attribution_reporting::mojom::OsRegistrationType GetType() const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_
