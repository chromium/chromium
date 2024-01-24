// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_DELEGATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_DELEGATE_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <set>

#import "base/memory/raw_ptr.h"
#import "ios/web/public/test/fakes/fake_java_script_dialog_presenter.h"
#import "ios/web/public/web_state_delegate.h"

namespace web {

// Encapsulates parameters passed to CreateNewWebState.
struct FakeCreateNewWebStateRequest {
  raw_ptr<WebState> web_state = nullptr;
  GURL url;
  GURL opener_url;
  bool initiated_by_user = false;
};

// Encapsulates parameters passed to CloseWebState.
struct FakeCloseWebStateRequest {
  raw_ptr<WebState> web_state = nullptr;
};

// Encapsulates parameters passed to OpenURLFromWebState.
struct FakeOpenURLRequest {
  FakeOpenURLRequest();
  FakeOpenURLRequest(const FakeOpenURLRequest&);
  ~FakeOpenURLRequest();
  raw_ptr<WebState> web_state = nullptr;
  WebState::OpenURLParams params;
};

// Encapsulates parameters passed to ShowRepostFormWarningDialog.
struct FakeRepostFormRequest {
  FakeRepostFormRequest();
  ~FakeRepostFormRequest();
  raw_ptr<WebState> web_state = nullptr;
  base::OnceCallback<void(bool)> callback;
};

// Encapsulates parameters passed to OnAuthRequired.
struct FakeAuthenticationRequest {
  FakeAuthenticationRequest();
  FakeAuthenticationRequest(FakeAuthenticationRequest&&);
  ~FakeAuthenticationRequest();
  raw_ptr<WebState> web_state = nullptr;
  NSURLProtectionSpace* protection_space;
  NSURLCredential* credential;
  WebStateDelegate::AuthCallback auth_callback;
};

// Encapsulates information about popup.
struct FakePopup {
  FakePopup(const GURL& url, const GURL& opener_url)
      : url(url), opener_url(opener_url) {}
  GURL url;
  GURL opener_url;
};

// Fake WebStateDelegate used for testing purposes.
class FakeWebStateDelegate : public WebStateDelegate {
 public:
  FakeWebStateDelegate();
  ~FakeWebStateDelegate() override;

  // WebStateDelegate overrides:
  WebState* CreateNewWebState(WebState* source,
                              const GURL& url,
                              const GURL& opener_url,
                              bool initiated_by_user) override;
  void CloseWebState(WebState* source) override;
  WebState* OpenURLFromWebState(WebState*,
                                const WebState::OpenURLParams&) override;
  JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(WebState*) override;
  void ShowRepostFormWarningDialog(
      WebState* source,
      web::FormWarningType warning_type,
      base::OnceCallback<void(bool)> callback) override;
  FakeJavaScriptDialogPresenter* GetFakeJavaScriptDialogPresenter();
  void OnAuthRequired(WebState* source,
                      NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      AuthCallback callback) override;
  void HandlePermissionsDecisionRequest(
      WebState* source,
      NSArray<NSNumber*>* permissions,
      WebStatePermissionDecisionHandler handler) override;

  // Allows popups requested by a page with `opener_url`.
  void allow_popups(const GURL& opener_url) {
    allowed_popups_.insert(opener_url);
  }

  // Returns list of all child windows opened via CreateNewWebState.
  const std::vector<std::unique_ptr<WebState>>& child_windows() const {
    return child_windows_;
  }

  // Returns list of all popups requested via CreateNewWebState.
  const std::vector<FakePopup>& popups() const { return popups_; }

  // Returns the last Web State creation request passed to `CreateNewWebState`.
  FakeCreateNewWebStateRequest* last_create_new_web_state_request() const {
    return last_create_new_web_state_request_.get();
  }

  // Returns the last Web State closing request passed to `CloseWebState`.
  FakeCloseWebStateRequest* last_close_web_state_request() const {
    return last_close_web_state_request_.get();
  }

  // Returns the last Open URL request passed to `OpenURLFromWebState`.
  FakeOpenURLRequest* last_open_url_request() const {
    return last_open_url_request_.get();
  }

  // Returns the last Repost Form request passed to
  // `ShowRepostFormWarningDialog`.
  FakeRepostFormRequest* last_repost_form_request() const {
    return last_repost_form_request_.get();
  }

  // True if the WebStateDelegate GetJavaScriptDialogPresenter method has been
  // called.
  bool get_java_script_dialog_presenter_called() const {
    return get_java_script_dialog_presenter_called_;
  }

  // Returns the last HTTP Authentication request passed to `OnAuthRequired`.
  FakeAuthenticationRequest* last_authentication_request() const {
    return last_authentication_request_.get();
  }

  // Clears the last HTTP Authentication request passed to `OnAuthRequired`.
  void ClearLastAuthenticationRequest() {
    last_authentication_request_.reset();
  }

  // Returns the last requested permissions passed to
  // `HandlePermissionsDecisionRequest`.
  NSArray<NSNumber*>* last_requested_permissions() {
    return last_requested_permissions_;
  }

  // Clears the last requested permissions passed to
  // `HandlePermissionsDecisionRequest`.
  void ClearLastRequestedPermissions() { last_requested_permissions_ = nil; }

  // Sets that permission decision the for next time
  // `HandlePermissionsDecisionRequest` is called.
  void SetPermissionDecision(PermissionDecision permission_decision) {
    permission_decision_ = permission_decision;
  }

  // Sets the return value of `ShouldAllowAppLaunching`.
  void SetShouldAllowAppLaunching(bool should_allow_apps) {
    should_allow_app_launching_ = should_allow_apps;
  }

  // Sets whether the delegate should handle permission decision.
  void SetShouldHandlePermissionDecision(
      bool should_handle_permission_decision) {
    should_handle_permission_decision_ = should_handle_permission_decision;
  }

 private:
  std::vector<std::unique_ptr<WebState>> child_windows_;
  // WebStates that were closed via `CloseWebState` callback.
  std::vector<std::unique_ptr<WebState>> closed_child_windows_;
  // A page can open popup if its URL is in this set.
  std::set<GURL> allowed_popups_;
  std::vector<FakePopup> popups_;
  std::unique_ptr<FakeCreateNewWebStateRequest>
      last_create_new_web_state_request_;
  std::unique_ptr<FakeCloseWebStateRequest> last_close_web_state_request_;
  std::unique_ptr<FakeOpenURLRequest> last_open_url_request_;
  std::unique_ptr<FakeRepostFormRequest> last_repost_form_request_;
  bool get_java_script_dialog_presenter_called_ = false;
  FakeJavaScriptDialogPresenter java_script_dialog_presenter_;
  std::unique_ptr<FakeAuthenticationRequest> last_authentication_request_;
  NSArray<NSNumber*>* last_requested_permissions_;
  bool should_allow_app_launching_ = false;
  PermissionDecision permission_decision_ = PermissionDecisionDeny;
  bool should_handle_permission_decision_ = true;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_DELEGATE_H_
