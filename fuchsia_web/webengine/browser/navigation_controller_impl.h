// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>

#include "base/memory/weak_ptr.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia_web/webengine/web_engine_export.h"

namespace content {
class NavigationEntry;
class NavigationHandle;
class WebContents;
}  // namespace content

// Implementation of fuchsia.web.NavigationController for content::WebContents.
class NavigationControllerImpl final
    : public fuchsia::web::NavigationController,
      public content::WebContentsObserver,
      public favicon::FaviconDriverObserver {
 public:
  NavigationControllerImpl(content::WebContents* web_contents,
                           void* parent_for_trace_flow);

  NavigationControllerImpl(const NavigationControllerImpl&) = delete;
  NavigationControllerImpl& operator=(const NavigationControllerImpl&) = delete;

  ~NavigationControllerImpl() override;

  void AddBinding(
      fidl::InterfaceRequest<fuchsia::web::NavigationController> controller);

  void SetEventListener(
      fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener,
      fuchsia::web::NavigationEventListenerFlags flags);

 private:
  // Returns a NavigationState reflecting the current state of |web_contents_|'s
  // visible navigation entry, taking into account |is_main_document_loaded_|
  // and |uncommitted_load_error_| states.
  fuchsia::web::NavigationState GetVisibleNavigationState() const;

  // Processes the most recent changes to the browser's navigation state and
  // triggers the publishing of change events.
  void OnNavigationEntryChanged();

  // Sends |pending_navigation_event_| to the observer if there are any changes
  // to be reported.
  void MaybeSendNavigationEvent();

  // fuchsia::web::NavigationController implementation.
  void LoadUrl(std::string url,
               fuchsia::web::LoadUrlParams params,
               LoadUrlCallback callback) override;
  void GoBack() override;
  void GoForward() override;
  void Stop() override;
  void Reload(fuchsia::web::ReloadType type) override;

  // content::WebContentsObserver implementation.
  void TitleWasSet(content::NavigationEntry*) override;
  void PrimaryMainDocumentElementAvailable() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // favicon::FaviconDriverObserver implementation.
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

  const raw_ptr<void> parent_for_trace_flow_;
  content::WebContents* const web_contents_;

  // NavigationController client bindings.
  fidl::BindingSet<fuchsia::web::NavigationController> controller_bindings_;

  // Fields used to dispatch events to the NavigationEventListener.
  fuchsia::web::NavigationEventListenerPtr navigation_listener_;
  fuchsia::web::NavigationState previous_navigation_state_;
  fuchsia::web::NavigationState pending_navigation_event_;
  bool waiting_for_navigation_event_ack_ = false;

  // True once the main document finishes loading and there are no outstanding
  // navigations.
  bool is_main_document_loaded_ = false;
  content::NavigationHandle* active_navigation_ = nullptr;

  // True if navigation failed due to an error during page load.
  bool uncommitted_load_error_ = false;

  // Set to true  when NavigationEventListenerFlags::FAVICON flag
  // was passed to the last SetEventListener() call, i.e. favicon reporting is
  // enabled.
  bool send_favicon_ = false;

  base::WeakPtrFactory<NavigationControllerImpl> weak_factory_;
};

// Exposed to allow unit-testing of NavigationState differencing.
WEB_ENGINE_EXPORT void DiffNavigationEntriesForTest(
    const fuchsia::web::NavigationState& old_entry,
    const fuchsia::web::NavigationState& new_entry,
    fuchsia::web::NavigationState* difference);

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
