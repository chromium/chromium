// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_state_delegate_browser_agent.h"

#import "base/strings/sys_string_conversions.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider.h"
#import "ios/chrome/browser/dialogs/ui_bundled/nsurl_protection_space_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/insecure_form_overlay.h"
#import "ios/chrome/browser/permissions/model/permissions_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/blocked_popup_tab_helper.h"
#import "ios/chrome/browser/web/model/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/model/web_state_container_view_provider.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"

BROWSER_USER_DATA_KEY_IMPL(WebStateDelegateBrowserAgent)

namespace {
// Callback for HTTP authentication dialogs. This callback is a standalone
// function rather than an instance method. This is to ensure that the callback
// can be executed regardless of whether the browser agent has been destroyed.
void OnHTTPAuthOverlayFinished(web::WebStateDelegate::AuthCallback callback,
                               OverlayResponse* response) {
  if (response) {
    HTTPAuthOverlayResponseInfo* auth_info =
        response->GetInfo<HTTPAuthOverlayResponseInfo>();
    if (auth_info) {
      std::move(callback).Run(base::SysUTF8ToNSString(auth_info->username()),
                              base::SysUTF8ToNSString(auth_info->password()));
      return;
    }
  }
  std::move(callback).Run(nil, nil);
}

void OnInsecureFormWarningResponse(base::OnceCallback<void(bool)> callback,
                                   OverlayResponse* response) {
  if (response) {
    InsecureFormDialogResponse* info =
        response->GetInfo<InsecureFormDialogResponse>();
    if (info) {
      std::move(callback).Run(info->allow_send());
      return;
    }
  }
  std::move(callback).Run(false);
}

// Returns true if a supervised user attempts to access the microphone or camera
// content setting when a parent has explicitly set site settings controls to
// block permissions.
bool IsMicOrCameraAccessSubjectToParentalControls(
    ProfileIOS* profile,
    NSArray<NSNumber*>* permissions) {
  if (!profile || !supervised_user::IsSubjectToParentalControls(profile)) {
    return false;
  }

  HostContentSettingsMap* host_content_settings_map =
      ios::HostContentSettingsMapFactory::GetForProfile(profile);
  CHECK(host_content_settings_map);

  ContentSetting default_mic_setting =
      host_content_settings_map->GetDefaultContentSetting(
          ContentSettingsType::MEDIASTREAM_MIC, /*provider_id=*/nullptr);
  ContentSetting default_camera_setting =
      host_content_settings_map->GetDefaultContentSetting(
          ContentSettingsType::MEDIASTREAM_CAMERA, /*provider_id=*/nullptr);

  return ([permissions containsObject:@(web::PermissionMicrophone)] &&
          default_mic_setting == ContentSetting::CONTENT_SETTING_BLOCK) ||
         ([permissions containsObject:@(web::PermissionCamera)] &&
          default_camera_setting == ContentSetting::CONTENT_SETTING_BLOCK);
}

}  // namespace

WebStateDelegateBrowserAgent::WebStateDelegateBrowserAgent(
    Browser* browser,
    TabInsertionBrowserAgent* tab_insertion_agent)
    : web_state_list_(browser->GetWebStateList()),
      tab_insertion_agent_(tab_insertion_agent) {
  DCHECK(tab_insertion_agent_);
  browser_ = browser;
  browser_observation_.Observe(browser);
  web_state_list_observation_.Observe(web_state_list_.get());

  // All the BrowserAgent are attached to the Browser during the creation,
  // the WebStateList must be empty at this point.
  DCHECK(web_state_list_->empty())
      << "WebStateDelegateBrowserAgent created for a Browser with a non-empty "
         "WebStateList.";
}

WebStateDelegateBrowserAgent::~WebStateDelegateBrowserAgent() {}

void WebStateDelegateBrowserAgent::SetUIProviders(
    ContextMenuConfigurationProvider* context_menu_provider,
    id<CRWResponderInputView> input_view_provider,
    id<WebStateContainerViewProvider> container_view_provider) {
  context_menu_provider_ = context_menu_provider;
  input_view_provider_ = input_view_provider;
  container_view_provider_ = container_view_provider;
}

void WebStateDelegateBrowserAgent::ClearUIProviders() {
  context_menu_provider_ = nil;
  input_view_provider_ = nil;
  container_view_provider_ = nil;
}

#pragma mark - WebStateListObserver

void WebStateDelegateBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      ClearWebStateDelegate(detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      ClearWebStateDelegate(replace_change.replaced_web_state());
      SetWebStateDelegate(replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      SetWebStateDelegate(insert_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}

#pragma mark - BrowserObserver

void WebStateDelegateBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK(browser_observation_.IsObservingSource(browser));

  WebStateList* web_state_list = browser->GetWebStateList();
  DCHECK(web_state_list_observation_.IsObservingSource(web_state_list));
  DCHECK_EQ(web_state_list_, web_state_list);

  // Remove all web state delegates.
  for (int index = 0; index < web_state_list_->count(); ++index)
    web_state_list_->GetWebStateAt(index)->SetDelegate(nullptr);

  web_state_observations_.RemoveAllObservations();
  web_state_list_observation_.Reset();
  browser_observation_.Reset();
}

#pragma mark - WebStateObserver

void WebStateDelegateBrowserAgent::WebStateRealized(web::WebState* web_state) {
  SetWebStateDelegate(web_state);
  web_state_observations_.RemoveObservation(web_state);
}

void WebStateDelegateBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
}

// WebStateDelegate::
web::WebState* WebStateDelegateBrowserAgent::CreateNewWebState(
    web::WebState* source,
    const GURL& url,
    const GURL& opener_url,
    bool initiated_by_user) {
  // Under some circumstances, this callback may be triggered from WebKit
  // synchronously as part of handling some other WebStateList mutation
  // (typically deleting a WebState and then activating another as a side
  // effect). See crbug.com/988504 for details. In this case, the request to
  // create a new WebState is silently dropped.
  if (web_state_list_->IsMutating())
    return nullptr;

  // Check if requested web state is a popup and block it if necessary.
  if (!initiated_by_user) {
    auto* helper = BlockedPopupTabHelper::GetOrCreateForWebState(source);
    if (helper->ShouldBlockPopup(opener_url)) {
      // It's possible for a page to inject a popup into a window created via
      // window.open before its initial load is committed.  Rather than relying
      // on the last committed or pending NavigationItem's referrer policy, just
      // use ReferrerPolicyDefault.
      // TODO(crbug.com/41317904): Update this to a more appropriate referrer
      // policy once referrer policies are correctly recorded in
      // NavigationItems.
      web::Referrer referrer(opener_url, web::ReferrerPolicyDefault);
      helper->HandlePopup(url, referrer);
      return nullptr;
    }
  }

  // Requested web state should not be blocked from opening.
  SnapshotTabHelper::FromWebState(source)->UpdateSnapshotWithCallback(nil);

  return tab_insertion_agent_->InsertWebStateOpenedByDOM(source);
}

void WebStateDelegateBrowserAgent::CloseWebState(web::WebState* source) {
  int index = web_state_list_->GetIndexOfWebState(source);
  if (index != WebStateList::kInvalidIndex)
    web_state_list_->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
}

web::WebState* WebStateDelegateBrowserAgent::OpenURLFromWebState(
    web::WebState* source,
    const web::WebState::OpenURLParams& params) {
  web::NavigationManager::WebLoadParams load_params(params.url);
  load_params.referrer = params.referrer;
  load_params.transition_type = params.transition;
  load_params.is_renderer_initiated = params.is_renderer_initiated;
  load_params.virtual_url = params.virtual_url;

  TabInsertion::Params insertion_params;
  insertion_params.parent = source;

  switch (params.disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB: {
      insertion_params.in_background =
          params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;
      return tab_insertion_agent_->InsertWebState(load_params,
                                                  insertion_params);
    }
    case WindowOpenDisposition::CURRENT_TAB: {
      source->GetNavigationManager()->LoadURLWithParams(load_params);
      return source;
    }
    case WindowOpenDisposition::NEW_POPUP: {
      insertion_params.opened_by_dom = true;
      return tab_insertion_agent_->InsertWebState(load_params,
                                                  insertion_params);
    }
    default:
      NOTIMPLEMENTED();
      return nullptr;
  };
}

void WebStateDelegateBrowserAgent::ShowRepostFormWarningDialog(
    web::WebState* source,
    web::FormWarningType warning_type,
    base::OnceCallback<void(bool)> callback) {
  CHECK_NE(warning_type, web::FormWarningType::kNone);
  if (!container_view_provider_) {
    // There's no way to show the dialog so treat it as if the user said no.
    std::move(callback).Run(false);
    return;
  }

  switch (warning_type) {
    case web::FormWarningType::kRepost:
      // TODO(crbug.com/40203973) : Clean up this API.
      RepostFormTabHelper::FromWebState(source)->PresentDialog(
          [container_view_provider_ dialogLocation], std::move(callback));
      return;

    case web::FormWarningType::kInsecureForm: {
      // Show the insecure form warning overlay.
      std::unique_ptr<OverlayRequest> request =
          OverlayRequest::CreateWithConfig<InsecureFormOverlayRequestConfig>();
      request->GetCallbackManager()->AddCompletionCallback(
          base::BindOnce(&OnInsecureFormWarningResponse, std::move(callback)));
      OverlayRequestQueue::FromWebState(source,
                                        OverlayModality::kWebContentArea)
          ->AddRequest(std::move(request));
      return;
    }

    case web::FormWarningType::kNone:
      NOTREACHED_IN_MIGRATION();
  }
}

web::JavaScriptDialogPresenter*
WebStateDelegateBrowserAgent::GetJavaScriptDialogPresenter(
    web::WebState* source) {
  return &java_script_dialog_presenter_;
}

void WebStateDelegateBrowserAgent::HandlePermissionsDecisionRequest(
    web::WebState* source,
    NSArray<NSNumber*>* permissions,
    web::WebStatePermissionDecisionHandler handler) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(source->GetBrowserState());
  // For supervised users, sites can be denied permission to access camera or
  // mic by default. In this case, we do not show the dialog.
  if (IsMicOrCameraAccessSubjectToParentalControls(profile, permissions)) {
    handler(web::PermissionDecisionDeny);
    return;
  }

  PermissionsTabHelper::FromWebState(source)
      ->PresentPermissionsDecisionDialogWithCompletionHandler(permissions,
                                                              handler);
}

void WebStateDelegateBrowserAgent::OnAuthRequired(
    web::WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* proposed_credential,
    web::WebStateDelegate::AuthCallback callback) {
  std::string message = base::SysNSStringToUTF8(
      nsurlprotectionspace_util::MessageForHTTPAuth(protection_space));
  std::string default_username;
  if (proposed_credential.user)
    default_username = base::SysNSStringToUTF8(proposed_credential.user);
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<HTTPAuthOverlayRequestConfig>(
          nsurlprotectionspace_util::RequesterOrigin(protection_space), message,
          default_username);
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&OnHTTPAuthOverlayFinished, std::move(callback)));
  OverlayRequestQueue::FromWebState(source, OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

UIView* WebStateDelegateBrowserAgent::GetWebViewContainer(
    web::WebState* source) {
  return [container_view_provider_ containerView];
}

void WebStateDelegateBrowserAgent::ContextMenuConfiguration(
    web::WebState* source,
    const web::ContextMenuParams& params,
    void (^completion_handler)(UIContextMenuConfiguration*)) {
  UIContextMenuConfiguration* configuration =
      [context_menu_provider_ contextMenuConfigurationForWebState:source
                                                           params:params];
  completion_handler(configuration);
}

void WebStateDelegateBrowserAgent::ContextMenuWillCommitWithAnimator(
    web::WebState* source,
    id<UIContextMenuInteractionCommitAnimating> animator) {
  GURL url_to_load = [context_menu_provider_ URLToLoad];
  if (!url_to_load.is_valid())
    return;

  UrlLoadParams params = UrlLoadParams::InCurrentTab(url_to_load);
  UrlLoadingBrowserAgent::FromBrowser(browser_)->Load(params);
}

id<CRWResponderInputView> WebStateDelegateBrowserAgent::GetResponderInputView(
    web::WebState* source) {
  return input_view_provider_;
}

void WebStateDelegateBrowserAgent::OnNewWebViewCreated(web::WebState* source) {
  // Focusing a newly-created web view allows it to request auth-based API. See
  // crbug.com/369996712.
  [source->GetWebViewProxy() becomeFirstResponder];
}

void WebStateDelegateBrowserAgent::SetWebStateDelegate(
    web::WebState* web_state) {
  DCHECK(web_state);
  if (web_state->IsRealized()) {
    web_state->SetDelegate(this);
  } else {
    web_state_observations_.AddObservation(web_state);
  }
}

void WebStateDelegateBrowserAgent::ClearWebStateDelegate(
    web::WebState* web_state) {
  DCHECK(web_state);
  if (web_state->IsRealized()) {
    web_state->SetDelegate(nullptr);
  } else {
    web_state_observations_.RemoveObservation(web_state);
  }
}
