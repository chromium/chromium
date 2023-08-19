// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_HOST_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-forward.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"

namespace attribution_reporting {
class SuitableOrigin;
}  // namespace attribution_reporting

namespace content {

struct AttributionInputEvent;
class RenderFrameHost;
class RenderFrameHostImpl;
class WebContents;

#if BUILDFLAG(IS_ANDROID)
class AttributionInputEventTrackerAndroid;
#endif

// Class responsible for listening to conversion events originating from blink,
// and verifying that they are valid. Owned by the WebContents. Lifetime is
// bound to lifetime of the WebContents.
class CONTENT_EXPORT AttributionHost
    : public WebContentsObserver,
      public WebContentsUserData<AttributionHost>,
      public blink::mojom::AttributionHost {
 public:
  explicit AttributionHost(WebContents* web_contents);
  AttributionHost(const AttributionHost&) = delete;
  AttributionHost& operator=(const AttributionHost&) = delete;
  AttributionHost(AttributionHost&&) = delete;
  AttributionHost& operator=(AttributionHost&&) = delete;
  ~AttributionHost() override;

  static void BindReceiver(
      mojo::PendingAssociatedReceiver<blink::mojom::AttributionHost> receiver,
      RenderFrameHost* rfh);

#if BUILDFLAG(IS_ANDROID)
  AttributionInputEventTrackerAndroid* input_event_tracker() {
    return input_event_tracker_android_.get();
  }
#endif

  // This should be called when the fenced frame reporting beacon was initiated
  // for reportEvent or for an automatic beacon. It may be cached and sent
  // later. This should be called before the navigation committed for a
  // navigation beacon.
  // This function should only be invoked if Attribution Reporting API is
  // enabled on the page.
  // `navigation_id` will be set if this beacon is being sent as the result of a
  // top navigation initiated by a fenced frame. This is used to track
  // attributions that occur on a navigated page after the current page has been
  // unloaded. Otherwise `absl::nullopt`.
  // Returns whether fenced frame reporting beacons can support Attribution
  // Reporting API.
  bool NotifyFencedFrameReportingBeaconStarted(
      BeaconId beacon_id,
      absl::optional<int64_t> navigation_id,
      RenderFrameHostImpl* initiator_frame_host,
      std::string devtools_request_id);

 private:
  friend class AttributionHostTestPeer;
  friend class WebContentsUserData<AttributionHost>;

  // blink::mojom::AttributionHost:
  void RegisterDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost>,
      attribution_reporting::mojom::RegistrationEligibility) override;
  void RegisterNavigationDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token) override;

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void NotifyNavigationRegistrationData(NavigationHandle* navigation_handle,
                                        bool is_final_response);

  // Returns the top frame origin corresponding to the current target frame.
  // Returns `absl::nullopt` and reports a bad message if the top frame origin
  // is not potentially trustworthy or the current target frame is not a secure
  // context.
  absl::optional<attribution_reporting::SuitableOrigin>
  TopFrameOriginForSecureContext();

  AttributionInputEvent GetMostRecentNavigationInputEvent() const;

  // Keeps track of navigations for which we can register sources (i.e. All
  // conditions were met in `DidStartNavigation` and
  // `DataHostManager::NotifyNavigationRegistrationStarted` was called). This
  // avoids making useless calls or checks when processing responses in
  // `DidRedirectNavigation` and `DidFinishNavigation` for navigations for which
  // we can't register sources.
  base::flat_set<int64_t> ongoing_registration_eligible_navigations_;

  RenderFrameHostReceiverSet<blink::mojom::AttributionHost> receivers_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<AttributionInputEventTrackerAndroid>
      input_event_tracker_android_;
#endif

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_HOST_H_
