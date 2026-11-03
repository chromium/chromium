// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/geolocation_system_permission_infobar_delegate.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/functional/bind.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/geolocation_prompt_dialog_overlay.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/bindings/callback_helpers.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size for the banner GPS symbol.
const CGFloat kSymbolImagePointSize = 18.0;

// Callback used to resolve the web state permission decision.
using WebStatePermissionDecisionCallback =
    base::OnceCallback<void(web::PermissionDecision)>;

// Callback invoked when the GeolocationPromptDialog alert finishes.
void OnGeolocationPromptDialogResponse(
    WebStatePermissionDecisionCallback callback,
    OverlayResponse* response) {
  if (callback) {
    std::move(callback).Run(web::PermissionDecisionDeny);
  }
  if (response) {
    auto* info = response->GetInfo<GeolocationPromptDialogResponse>();
    if (info && info->tapped_settings()) {
      [[UIApplication sharedApplication]
                    openURL:
                        [NSURL URLWithString:UIApplicationOpenSettingsURLString]
                    options:@{}
          completionHandler:nil];
    }
  }
}
}  // namespace

// static
bool GeolocationSystemPermissionInfoBarDelegate::Create(
    web::WebState* web_state,
    web::WebStatePermissionDecisionHandler handler) {
  DCHECK(web_state);
  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state);
  if (!manager) {
    if (handler) {
      handler(web::PermissionDecisionDeny);
    }
    return false;
  }
  return !!manager->AddInfoBar(CreateConfirmInfoBar(
      std::make_unique<GeolocationSystemPermissionInfoBarDelegate>(
          base::PassKey<GeolocationSystemPermissionInfoBarDelegate>{},
          web_state, handler)));
}

GeolocationSystemPermissionInfoBarDelegate::
    GeolocationSystemPermissionInfoBarDelegate(
        base::PassKey<GeolocationSystemPermissionInfoBarDelegate>,
        web::WebState* web_state,
        web::WebStatePermissionDecisionHandler handler)
    : web_state_(web_state ? web_state->GetWeakPtr() : nullptr),
      handler_([handler copy]) {}

GeolocationSystemPermissionInfoBarDelegate::
    ~GeolocationSystemPermissionInfoBarDelegate() {
  DispatchDecisionIfNecessary(web::PermissionDecisionDeny);
}

infobars::InfoBarDelegate::InfoBarIdentifier
GeolocationSystemPermissionInfoBarDelegate::GetIdentifier() const {
  return IOS_PERMISSIONS_INFOBAR_DELEGATE;
}

ui::ImageModel GeolocationSystemPermissionInfoBarDelegate::GetIcon() const {
  if (icon_.IsEmpty()) {
    UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:kSymbolImagePointSize
                            weight:UIImageSymbolWeightMedium
                             scale:UIImageSymbolScaleMedium];
    UIImage* image = [UIImage imageNamed:@"location"
                                inBundle:nil
                       withConfiguration:configuration];
    if (image) {
      icon_ = gfx::Image(image);
    }
  }
  return ui::ImageModel::FromImage(icon_);
}

std::u16string GeolocationSystemPermissionInfoBarDelegate::GetTitleText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_IOS_GEOLOCATION_SYSTEM_PERMISSION_INFOBAR_BANNER_TITLE);
}

std::u16string GeolocationSystemPermissionInfoBarDelegate::GetMessageText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_IOS_GEOLOCATION_SYSTEM_PERMISSION_INFOBAR_BANNER_MESSAGE);
}

int GeolocationSystemPermissionInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string GeolocationSystemPermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_CONTINUE);
}

bool GeolocationSystemPermissionInfoBarDelegate::Accept() {
  if (web_state_) {
    std::unique_ptr<OverlayRequest> request =
        OverlayRequest::CreateWithConfig<GeolocationPromptDialogRequest>();

    request->GetCallbackManager()->AddCompletionCallback(
        base::BindOnce(&OnGeolocationPromptDialogResponse,
                       mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                           base::BindOnce(std::exchange(handler_, nil)),
                           web::PermissionDecisionDeny)));

    OverlayRequestQueue::FromWebState(web_state_.get(),
                                      OverlayModality::kWebContentArea)
        ->AddRequest(std::move(request));
  } else {
    DispatchDecisionIfNecessary(web::PermissionDecisionDeny);
  }
  return true;
}

void GeolocationSystemPermissionInfoBarDelegate::InfoBarDismissed() {
  DispatchDecisionIfNecessary(web::PermissionDecisionDeny);
}

void GeolocationSystemPermissionInfoBarDelegate::DispatchDecisionIfNecessary(
    web::PermissionDecision decision) {
  if (handler_) {
    std::exchange(handler_, nil)(decision);
  }
}
