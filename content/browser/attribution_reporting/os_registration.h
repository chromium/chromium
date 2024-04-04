// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_

#include <optional>
#include <vector>

#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "url/origin.h"

namespace attribution_reporting {
enum class Registrar;
struct OsRegistrationItem;
}  // namespace attribution_reporting

namespace content {

struct CONTENT_EXPORT OsRegistration {
  std::vector<attribution_reporting::OsRegistrationItem> registration_items;
  url::Origin top_level_origin;
  // If `std::nullopt`, represents an OS trigger. Otherwise, represents an OS
  // source.
  std::optional<AttributionInputEvent> input_event;
  bool is_within_fenced_frame;
  GlobalRenderFrameHostId render_frame_id;
  attribution_reporting::Registrar registrar;

  OsRegistration(std::vector<attribution_reporting::OsRegistrationItem>,
                 url::Origin top_level_origin,
                 std::optional<AttributionInputEvent> input_event,
                 bool is_within_fenced_frame,
                 GlobalRenderFrameHostId render_frame_id,
                 attribution_reporting::Registrar);

  ~OsRegistration();

  OsRegistration(const OsRegistration&);
  OsRegistration& operator=(const OsRegistration&);

  OsRegistration(OsRegistration&&);
  OsRegistration& operator=(OsRegistration&&);

  attribution_reporting::mojom::RegistrationType GetType() const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_
