// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_BREADCRUMBS_BREADCRUMB_MANAGER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_BREADCRUMBS_BREADCRUMB_MANAGER_BROWSER_AGENT_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/breadcrumbs/core/breadcrumb_manager_browser_agent.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class Browser;
class WebStateList;

// Name of Overlay initial presentation event.
extern const char kBreadcrumbOverlay[];

// Appended to `kBreadcrumbOverlay` event if overlay was re-activated rather
// than presented for the first time (f.e. the user has switched to a tab with
// an overlay).
extern const char kBreadcrumbOverlayActivated[];

// Appended to `kBreadcrumbOverlay` event if overlay is Http Authentication.
extern const char kBreadcrumbOverlayHttpAuth[];

// Appended to `kBreadcrumbOverlay` event if overlay is generic app dialog.
extern const char kBreadcrumbOverlayAlert[];

// Appended to `kBreadcrumbOverlay` event if overlay is app launch confirmation.
extern const char kBreadcrumbOverlayAppLaunch[];

// Appended to `kBreadcrumbOverlay` event if overlay is JavaScript alert.
extern const char kBreadcrumbOverlayJsAlert[];

// Appended to `kBreadcrumbOverlay` event if overlay is JavaScript confirm.
extern const char kBreadcrumbOverlayJsConfirm[];

// Appended to `kBreadcrumbOverlay` event if overlay is JavaScript prompt.
extern const char kBreadcrumbOverlayJsPrompt[];

class BreadcrumbManagerBrowserAgent
    : public breadcrumbs::BreadcrumbManagerBrowserAgent,
      public BrowserObserver,
      public OverlayPresenterObserver,
      public BrowserUserData<BreadcrumbManagerBrowserAgent>,
      public WebStateListObserver {
 public:
  BreadcrumbManagerBrowserAgent(const BreadcrumbManagerBrowserAgent&) = delete;
  BreadcrumbManagerBrowserAgent& operator=(
      const BreadcrumbManagerBrowserAgent&) = delete;
  ~BreadcrumbManagerBrowserAgent() override;

 private:
  friend class BrowserUserData<BreadcrumbManagerBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit BreadcrumbManagerBrowserAgent(Browser* browser);

  // breadcrumbs::BreadcrumbManagerBrowserAgent:
  void PlatformLogEvent(const std::string& event) override;

  // BrowserObserver:
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WillBeginBatchOperation(WebStateList* web_state_list) override;
  void BatchOperationEnded(WebStateList* web_state_list) override;

  // OverlayPresenterObserver:
  void WillShowOverlay(OverlayPresenter* presenter,
                       OverlayRequest* request,
                       bool initial_presentation) override;
  void OverlayPresenterDestroyed(OverlayPresenter* presenter) override;

  raw_ptr<Browser> browser_ = nullptr;

  // Keeps track of WebState mutation count to avoid logging every event.
  // Created in WillBeginBatchOperation and destroyed in BatchOperationEnded.
  // Final mutation count is logged in BatchOperationEnded.
  struct BatchOperation {
    // Number of WebState objects inserted between WillBeginBatchOperation and
    // BatchOperationEnded callbacks.
    int insertion_count = 0;
    // Number of WebState objects closed between WillBeginBatchOperation and
    // BatchOperationEnded callbacks.
    int close_count = 0;
  };
  std::unique_ptr<BatchOperation> batch_operation_;

  // Observes overlays presentation.
  base::ScopedObservation<OverlayPresenter, OverlayPresenterObserver>
      overlay_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_BREADCRUMBS_BREADCRUMB_MANAGER_BROWSER_AGENT_H_
