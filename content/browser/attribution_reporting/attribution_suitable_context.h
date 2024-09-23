// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_SUITABLE_CONTEXT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_SUITABLE_CONTEXT_H_

#include <stdint.h>

#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

class AttributionDataHostManager;
class RenderFrameHostImpl;

// The `AttributionSuitableContext` encapsulates the context necessary from a
// `RenderFrameHost` for a `KeepAliveAttributionRequestHelper` to be created.
class CONTENT_EXPORT AttributionSuitableContext {
 public:
  // Returns `AttributionSuitableContext` if the rfh is suitable to register
  // attribution.
  static std::optional<AttributionSuitableContext> Create(
      GlobalRenderFrameHostId initiator_frame_id);
  static std::optional<AttributionSuitableContext> Create(RenderFrameHostImpl*);

  // Allows to create a context with arbitrary properties for testing purposes.
  static AttributionSuitableContext CreateForTesting(
      attribution_reporting::SuitableOrigin context_origin,
      bool is_nested_within_fenced_frame,
      GlobalRenderFrameHostId root_render_frame_id,
      int64_t last_navigation_id,
      AttributionInputEvent last_input_event = AttributionInputEvent(),
      ContentBrowserClient::AttributionReportingOsRegistrars os_registrars =
          {ContentBrowserClient::AttributionReportingOsRegistrar::kWeb,
           ContentBrowserClient::AttributionReportingOsRegistrar::kWeb},
      AttributionDataHostManager* attribution_data_host_manager = nullptr);

  bool operator==(const AttributionSuitableContext& other) const;

  AttributionSuitableContext(const AttributionSuitableContext&);
  AttributionSuitableContext& operator=(const AttributionSuitableContext&);
  AttributionSuitableContext(AttributionSuitableContext&&);
  AttributionSuitableContext& operator=(AttributionSuitableContext&&);
  ~AttributionSuitableContext();

  const attribution_reporting::SuitableOrigin& context_origin() const {
    return context_origin_;
  }
  bool is_nested_within_fenced_frame() const {
    return is_nested_within_fenced_frame_;
  }
  GlobalRenderFrameHostId root_render_frame_id() const {
    return root_render_frame_id_;
  }
  int64_t last_navigation_id() const { return last_navigation_id_; }
  const AttributionInputEvent& last_input_event() const {
    return last_input_event_;
  }
  ContentBrowserClient::AttributionReportingOsRegistrars os_registrars() const {
    return os_registrars_;
  }

  AttributionDataHostManager* data_host_manager() const {
    return attribution_data_host_manager_.get();
  }

 private:
  AttributionSuitableContext(
      attribution_reporting::SuitableOrigin context_origin,
      bool is_nested_within_fenced_frame,
      GlobalRenderFrameHostId root_render_frame_id,
      int64_t last_navigation_id,
      AttributionInputEvent last_input_event,
      ContentBrowserClient::AttributionReportingOsRegistrars,
      base::WeakPtr<AttributionDataHostManager>);

  attribution_reporting::SuitableOrigin context_origin_;
  bool is_nested_within_fenced_frame_;
  GlobalRenderFrameHostId root_render_frame_id_;
  int64_t last_navigation_id_;
  AttributionInputEvent last_input_event_;
  ContentBrowserClient::AttributionReportingOsRegistrars os_registrars_;

  base::WeakPtr<AttributionDataHostManager> attribution_data_host_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_SUITABLE_CONTEXT_H_
