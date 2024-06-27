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
#include "components/attribution_reporting/data_host.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"

namespace content {

struct AttributionInputEvent;
class RenderFrameHost;
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

  AttributionInputEvent GetMostRecentNavigationInputEvent() const;

#if BUILDFLAG(IS_ANDROID)
  AttributionInputEventTrackerAndroid* input_event_tracker() {
    return input_event_tracker_android_.get();
  }
#endif

 private:
  friend class AttributionHostTestPeer;
  friend class WebContentsUserData<AttributionHost>;

  // blink::mojom::AttributionHost:
  void NotifyNavigationWithBackgroundRegistrationsWillStart(
      const blink::AttributionSrcToken& attribution_src_token,
      uint32_t expected_registrations) override;
  void RegisterDataHost(
      mojo::PendingReceiver<attribution_reporting::mojom::DataHost>,
      attribution_reporting::mojom::RegistrationEligibility,
      bool is_for_background_requests) override;
  void RegisterNavigationDataHost(
      mojo::PendingReceiver<attribution_reporting::mojom::DataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token) override;

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void NotifyNavigationRegistrationData(NavigationHandle* navigation_handle);

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
