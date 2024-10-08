// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_DELEGATE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_DELEGATE_BROWSER_AGENT_H_

#include <CoreFoundation/CoreFoundation.h>

#import "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/dialogs/ui_bundled/overlay_java_script_dialog_presenter.h"
#include "ios/chrome/browser/shared/model/browser/browser_observer.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#include "ios/web/public/navigation/form_warning_type.h"
#include "ios/web/public/web_state_delegate.h"
#include "ios/web/public/web_state_observer.h"

@class ContextMenuConfigurationProvider;
@protocol CRWResponderInputView;
class TabInsertionBrowserAgent;
@protocol WebStateContainerViewProvider;
class GURL;

// A browser agent that acts as the delegate for all webstates in its browser.
// This object handles assigning itself as the delegate to webstates as they are
// added to the browser's WebStateList (and unassigning them when they leave).
// This browser agent must be created after TabInsertionBrowserAgent.
class WebStateDelegateBrowserAgent
    : public BrowserObserver,
      public BrowserUserData<WebStateDelegateBrowserAgent>,
      public WebStateListObserver,
      public web::WebStateObserver,
      public web::WebStateDelegate {
 public:
  ~WebStateDelegateBrowserAgent() override;

  // Not copyable or assignable.
  WebStateDelegateBrowserAgent(const WebStateDelegateBrowserAgent&) = delete;
  WebStateDelegateBrowserAgent& operator=(const WebStateDelegateBrowserAgent&) =
      delete;

  // Sets the UI providers to be used for WebStateDelegate tasks that require
  // them.
  // If providers are added, factor these params into a Params struct.
  void SetUIProviders(
      ContextMenuConfigurationProvider* context_menu_provider,
      id<CRWResponderInputView> input_view_provider,
      id<WebStateContainerViewProvider> container_view_provider);

  // Clears the UI providers.
  void ClearUIProviders();

 private:
  friend class BrowserUserData<WebStateDelegateBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  WebStateDelegateBrowserAgent(Browser* browser,
                               TabInsertionBrowserAgent* tab_insertion_agent);

  // WebStateListObserver::
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // BrowserObserver::
  void BrowserDestroyed(Browser* browser) override;

  // web::WebStateObserver:
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // web::WebStateDelegate:
  web::WebState* CreateNewWebState(web::WebState* source,
                                   const GURL& url,
                                   const GURL& opener_url,
                                   bool initiated_by_user) override;
  void CloseWebState(web::WebState* source) override;
  web::WebState* OpenURLFromWebState(
      web::WebState* source,
      const web::WebState::OpenURLParams& params) override;
  void ShowRepostFormWarningDialog(
      web::WebState* source,
      web::FormWarningType warning_type,
      base::OnceCallback<void(bool)> callback) override;
  web::JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(
      web::WebState* source) override;
  void HandlePermissionsDecisionRequest(
      web::WebState* source,
      NSArray<NSNumber*>* permissions,
      web::WebStatePermissionDecisionHandler handler) override;
  void OnAuthRequired(web::WebState* source,
                      NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      AuthCallback callback) override;
  UIView* GetWebViewContainer(web::WebState* source) override;
  void ContextMenuConfiguration(
      web::WebState* source,
      const web::ContextMenuParams& params,
      void (^completion_handler)(UIContextMenuConfiguration*)) override;
  void ContextMenuWillCommitWithAnimator(
      web::WebState* source,
      id<UIContextMenuInteractionCommitAnimating> animator) override;
  id<CRWResponderInputView> GetResponderInputView(
      web::WebState* source) override;
  void OnNewWebViewCreated(web::WebState* source) override;

  // Helper methods to set/clear the WebState delegate if it is realized,
  // or to listen for the realization of the WebState.
  void SetWebStateDelegate(web::WebState* web_state);
  void ClearWebStateDelegate(web::WebState* web_state);

  raw_ptr<WebStateList> web_state_list_ = nullptr;
  raw_ptr<TabInsertionBrowserAgent> tab_insertion_agent_ = nullptr;

  OverlayJavaScriptDialogPresenter java_script_dialog_presenter_;

  // The browser associated with this agent.
  raw_ptr<Browser> browser_;

  // Scoped observations of Browser, WebStateList and WebStates.
  base::ScopedObservation<Browser, BrowserObserver> browser_observation_{this};

  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};

  // These providers are owned by other objects.
  __weak ContextMenuConfigurationProvider* context_menu_provider_;
  __weak id<CRWResponderInputView> input_view_provider_;
  __weak id<WebStateContainerViewProvider> container_view_provider_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_DELEGATE_BROWSER_AGENT_H_
