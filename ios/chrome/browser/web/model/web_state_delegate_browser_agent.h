// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_DELEGATE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_DELEGATE_BROWSER_AGENT_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/memory/raw_ptr.h"
#include "ios/chrome/browser/dialogs/ui_bundled/overlay_java_script_dialog_presenter.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#include "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#include "ios/web/public/navigation/form_warning_type.h"
#include "ios/web/public/web_state_delegate.h"

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
    : public BrowserUserData<WebStateDelegateBrowserAgent>,
      public TabsDependencyInstaller,
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

  WebStateDelegateBrowserAgent(Browser* browser,
                               TabInsertionBrowserAgent* tab_insertion_agent);

  // TabsDependencyInstaller implementation.
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

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
  void ShouldAllowCopy(web::WebState* source,
                       base::OnceCallback<void(bool)> callback) override;
  void ShouldAllowPaste(web::WebState* source,
                        base::OnceCallback<void(bool)> callback) override;
  void ShouldAllowCut(web::WebState* source,
                      base::OnceCallback<void(bool)> callback) override;
  void DidFinishClipboardRead(web::WebState* source) override;

  raw_ptr<WebStateList> web_state_list_ = nullptr;
  raw_ptr<TabInsertionBrowserAgent, DanglingUntriaged> tab_insertion_agent_ =
      nullptr;

  OverlayJavaScriptDialogPresenter java_script_dialog_presenter_;

  // These providers are owned by other objects.
  __weak ContextMenuConfigurationProvider* context_menu_provider_;
  __weak id<CRWResponderInputView> input_view_provider_;
  __weak id<WebStateContainerViewProvider> container_view_provider_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_DELEGATE_BROWSER_AGENT_H_
