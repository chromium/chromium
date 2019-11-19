// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
#define FUCHSIA_ENGINE_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia/engine/web_engine_export.h"

namespace content {
class NavigationEntry;
class NavigationHandle;
class WebContents;
}  // namespace content

// Implementation of fuchsia.web.NavigationController for content::WebContents.
class NavigationControllerImpl : public fuchsia::web::NavigationController,
                                 public content::WebContentsObserver {
 public:
  explicit NavigationControllerImpl(content::WebContents* web_contents);
  ~NavigationControllerImpl() final;

  void AddBinding(
      fidl::InterfaceRequest<fuchsia::web::NavigationController> controller);

  void SetEventListener(
      fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener);

 private:
  // Processes the most recent changes to the browser's navigation state and
  // triggers the publishing of change events.
  void OnNavigationEntryChanged();

  // Sends |pending_navigation_event_| to the observer if there are any changes
  // to be reported.
  void MaybeSendNavigationEvent();

  // fuchsia::web::NavigationController implementation.
  void LoadUrl(std::string url,
               fuchsia::web::LoadUrlParams params,
               LoadUrlCallback callback) final;
  void GoBack() final;
  void GoForward() final;
  void Stop() final;
  void Reload(fuchsia::web::ReloadType type) final;
  void GetVisibleEntry(GetVisibleEntryCallback callback) final;

  // content::WebContentsObserver implementation.
  void TitleWasSet(content::NavigationEntry*) override;
  void DocumentAvailableInMainFrame() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  content::WebContents* const web_contents_;

  // NavigationController client bindings.
  fidl::BindingSet<fuchsia::web::NavigationController> controller_bindings_;

  // Fields used to dispatch events to the NavigationEventListener.
  fuchsia::web::NavigationEventListenerPtr navigation_listener_;
  fuchsia::web::NavigationState previous_navigation_state_;
  fuchsia::web::NavigationState pending_navigation_event_;
  bool waiting_for_navigation_event_ack_ = false;

  // True once the main document finishes loading.
  bool is_main_document_loaded_ = false;

  base::WeakPtrFactory<NavigationControllerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerImpl);
};

// Computes the differences from old_entry to new_entry and stores the result in
// |difference|.
WEB_ENGINE_EXPORT void DiffNavigationEntries(
    const fuchsia::web::NavigationState& old_entry,
    const fuchsia::web::NavigationState& new_entry,
    fuchsia::web::NavigationState* difference);

#endif  // FUCHSIA_ENGINE_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
