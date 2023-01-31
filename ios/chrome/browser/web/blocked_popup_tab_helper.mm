// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/blocked_popup_tab_helper.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>

#import "base/format_macros.h"
#import "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/utf_string_conversions.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/infobars/confirm_infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/referrer.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"
#import "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The infobar to display when a popup is blocked.

// The size of the symbol image.
const CGFloat kSymbolImagePointSize = 18.;

// The name if the popup symbol.
NSString* const kPopupBadgeMinusSymbol = @"popup_badge_minus";

class BlockPopupInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  BlockPopupInfoBarDelegate(
      ChromeBrowserState* browser_state,
      web::WebState* web_state,
      const std::vector<BlockedPopupTabHelper::Popup>& popups)
      : browser_state_(browser_state), web_state_(web_state), popups_(popups) {
    delegate_creation_time_ = [NSDate timeIntervalSinceReferenceDate];
  }

  ~BlockPopupInfoBarDelegate() override {}

  InfoBarIdentifier GetIdentifier() const override {
    return POPUP_BLOCKED_INFOBAR_DELEGATE_MOBILE;
  }

  ui::ImageModel GetIcon() const override {
    if (icon_.IsEmpty()) {
      // This symbol is not created using CustomSymbolWithPointSize() because
      // "ios/chrome/browser/ui/icons/symbols.h" cannot be imported here.
      UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
          configurationWithPointSize:kSymbolImagePointSize
                              weight:UIImageSymbolWeightMedium
                               scale:UIImageSymbolScaleMedium];
      UIImage* image = [UIImage imageNamed:kPopupBadgeMinusSymbol
                                  inBundle:nil
                         withConfiguration:configuration];
      icon_ = gfx::Image(image);
    }
    return ui::ImageModel::FromImage(icon_);
  }

  std::u16string GetMessageText() const override {
    return l10n_util::GetStringFUTF16(
        IDS_IOS_POPUPS_BLOCKED_MOBILE,
        base::UTF8ToUTF16(base::StringPrintf("%" PRIuS, popups_.size())));
  }

  std::u16string GetButtonLabel(InfoBarButton button) const override {
    DCHECK(button == BUTTON_OK);
    return l10n_util::GetStringUTF16(IDS_IOS_POPUPS_ALWAYS_SHOW_MOBILE);
  }

  bool Accept() override {
    NSTimeInterval duration =
        [NSDate timeIntervalSinceReferenceDate] - delegate_creation_time_;
    [ConfirmInfobarMetricsRecorder
        recordConfirmAcceptTime:duration
          forInfobarConfirmType:InfobarConfirmType::
                                    kInfobarConfirmTypeBlockPopups];
    [ConfirmInfobarMetricsRecorder
        recordConfirmInfobarEvent:MobileMessagesConfirmInfobarEvents::Accepted
            forInfobarConfirmType:InfobarConfirmType::
                                      kInfobarConfirmTypeBlockPopups];
    scoped_refptr<HostContentSettingsMap> host_content_map_settings(
        ios::HostContentSettingsMapFactory::GetForBrowserState(browser_state_));
    for (auto& popup : popups_) {
      web::WebState::OpenURLParams params(
          popup.popup_url, popup.referrer, WindowOpenDisposition::NEW_POPUP,
          ui::PAGE_TRANSITION_LINK, true /* is_renderer_initiated */);
      web_state_->OpenURL(params);
      host_content_map_settings->SetContentSettingCustomScope(
          ContentSettingsPattern::FromURL(popup.referrer.url),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::POPUPS,
          CONTENT_SETTING_ALLOW);
    }
    return true;
  }

  void InfoBarDismissed() override {
    [ConfirmInfobarMetricsRecorder
        recordConfirmInfobarEvent:MobileMessagesConfirmInfobarEvents::Dismissed
            forInfobarConfirmType:InfobarConfirmType::
                                      kInfobarConfirmTypeBlockPopups];
  }

  int GetButtons() const override { return BUTTON_OK; }

 private:
  ChromeBrowserState* browser_state_;
  web::WebState* web_state_;
  // The popups to open.
  std::vector<BlockedPopupTabHelper::Popup> popups_;
  // The icon to display.
  mutable gfx::Image icon_;
  // TimeInterval when the delegate was created.
  NSTimeInterval delegate_creation_time_;
};
}  // namespace

BlockedPopupTabHelper::BlockedPopupTabHelper(web::WebState* web_state)
    : web_state_(web_state), infobar_(nullptr) {}

BlockedPopupTabHelper::~BlockedPopupTabHelper() = default;

bool BlockedPopupTabHelper::ShouldBlockPopup(const GURL& source_url) {
  HostContentSettingsMap* settings_map =
      ios::HostContentSettingsMapFactory::GetForBrowserState(GetBrowserState());
  ContentSetting setting = settings_map->GetContentSetting(
      source_url, source_url, ContentSettingsType::POPUPS);
  return setting != CONTENT_SETTING_ALLOW;
}

void BlockedPopupTabHelper::HandlePopup(const GURL& popup_url,
                                        const web::Referrer& referrer) {
  DCHECK(ShouldBlockPopup(referrer.url));
  popups_.push_back(Popup(popup_url, referrer));
  ShowInfoBar();
}

void BlockedPopupTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                             bool animate) {
  if (infobar == infobar_) {
    infobar_ = nullptr;
    popups_.clear();
    scoped_observation_.Reset();
  }
}

void BlockedPopupTabHelper::OnManagerShuttingDown(
    infobars::InfoBarManager* infobar_manager) {
  DCHECK(scoped_observation_.IsObservingSource(infobar_manager));
  scoped_observation_.Reset();
}

void BlockedPopupTabHelper::ShowInfoBar() {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  if (!popups_.size() || !infobar_manager)
    return;

  RegisterAsInfoBarManagerObserverIfNeeded(infobar_manager);

  std::unique_ptr<BlockPopupInfoBarDelegate> delegate(
      std::make_unique<BlockPopupInfoBarDelegate>(GetBrowserState(), web_state_,
                                                  popups_));

  std::unique_ptr<infobars::InfoBar> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeConfirm, std::move(delegate));

  if (infobar_) {
    infobar_ = infobar_manager->ReplaceInfoBar(infobar_, std::move(infobar));
  } else {
    infobar_ = infobar_manager->AddInfoBar(std::move(infobar));
  }
  [ConfirmInfobarMetricsRecorder
      recordConfirmInfobarEvent:MobileMessagesConfirmInfobarEvents::Presented
          forInfobarConfirmType:InfobarConfirmType::
                                    kInfobarConfirmTypeBlockPopups];
}

ChromeBrowserState* BlockedPopupTabHelper::GetBrowserState() const {
  return ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
}

void BlockedPopupTabHelper::RegisterAsInfoBarManagerObserverIfNeeded(
    infobars::InfoBarManager* infobar_manager) {
  DCHECK(infobar_manager);

  if (scoped_observation_.IsObservingSource(infobar_manager)) {
    return;
  }

  // Verify that this object is never observing more than one InfoBarManager.
  DCHECK(!scoped_observation_.IsObserving());
  scoped_observation_.Observe(infobar_manager);
}

WEB_STATE_USER_DATA_KEY_IMPL(BlockedPopupTabHelper)
