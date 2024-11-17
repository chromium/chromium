// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/translate_overlay_tab_helper.h"

#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/infobars/model/overlays/translate_infobar_placeholder_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_placeholder_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"

using translate_infobar_overlays::PlaceholderRequestCancelHandler;

namespace {
// Creates a matcher callback for ConfigType and config's InfoBar.
template <class ConfigType>
base::RepeatingCallback<bool(OverlayRequest*)> ConfigAndInfoBarMatcher(
    infobars::InfoBar* infobar) {
  return base::BindRepeating(
      [](infobars::InfoBar* infobar, OverlayRequest* request) -> bool {
        return GetOverlayRequestInfobar(request) ==
                   static_cast<InfoBarIOS*>(infobar) &&
               request->GetConfig<ConfigType>();
      },
      infobar);
}
}  // namespace

WEB_STATE_USER_DATA_KEY_IMPL(TranslateOverlayTabHelper)

TranslateOverlayTabHelper::TranslateOverlayTabHelper(web::WebState* web_state)
    : translate_step_observer_(this),
      translate_infobar_observer_(web_state, this),
      web_state_observer_(web_state, this) {
  banner_queue_ = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kInfobarBanner);
  inserter_ = InfobarOverlayRequestInserter::FromWebState(web_state);
}

TranslateOverlayTabHelper::~TranslateOverlayTabHelper() {
  for (auto& observer : observers_) {
    observer.TranslateOverlayTabHelperDestroyed(this);
  }
}

void TranslateOverlayTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TranslateOverlayTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

#pragma mark - Private

void TranslateOverlayTabHelper::TranslateDidStart(infobars::InfoBar* infobar) {
  size_t insert_index = 0;
  bool translate_banner_found = GetIndexOfMatchingRequest(
      banner_queue_, &insert_index,
      ConfigAndInfoBarMatcher<DefaultInfobarOverlayRequestConfig>(infobar));

  if (!translate_banner_found) {
    return;
  }

  // Add placeholder cancel request.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<InfobarBannerPlaceholderRequestConfig>(
          static_cast<InfoBarIOS*>(infobar));
  std::unique_ptr<PlaceholderRequestCancelHandler> placeholder_cancel_handler =
      std::make_unique<PlaceholderRequestCancelHandler>(
          request.get(), banner_queue_, this,
          static_cast<InfoBarIOS*>(infobar));
  banner_queue_->InsertRequest(insert_index + 1, std::move(request),
                               std::move(placeholder_cancel_handler));
}

void TranslateOverlayTabHelper::TranslateDidFinish(infobars::InfoBar* infobar,
                                                   bool success) {
  static_cast<InfoBarIOS*>(infobar)->set_accepted(success);

  size_t insert_index = 0;
  bool placeholder_found = GetIndexOfMatchingRequest(
      banner_queue_, &insert_index,
      ConfigAndInfoBarMatcher<PlaceholderRequestConfig>(infobar));

  InsertParams params(static_cast<InfoBarIOS*>(infobar));
  params.overlay_type = InfobarOverlayType::kBanner;
  params.insertion_index =
      placeholder_found ? insert_index + 1 : banner_queue_->size();
  params.source = InfobarOverlayInsertionSource::kInfoBarDelegate;
  inserter_->InsertOverlayRequest(params);

  for (auto& observer : observers_) {
    observer.TranslationFinished(this, success);
  }
}

void TranslateOverlayTabHelper::TranslateInfoBarAdded(InfoBarIOS* infobar) {
  translate_step_observer_.SetTranslateInfoBar(infobar);
}

void TranslateOverlayTabHelper::UpdateForWebStateDestroyed() {
  DCHECK(banner_queue_);
  banner_queue_ = nullptr;
  inserter_ = nullptr;
}

#pragma mark - TranslateStepObserver

TranslateOverlayTabHelper::TranslateStepObserver::TranslateStepObserver(
    TranslateOverlayTabHelper* tab_helper)
    : tab_helper_(tab_helper) {}

TranslateOverlayTabHelper::TranslateStepObserver::~TranslateStepObserver() =
    default;

void TranslateOverlayTabHelper::TranslateStepObserver::OnTranslateStepChanged(
    translate::TranslateStep step,
    translate::TranslateErrors error_type) {
  switch (step) {
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE: {
      tab_helper_->TranslateDidFinish(translate_infobar_, true);
      break;
    }
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
      tab_helper_->TranslateDidStart(translate_infobar_);
      break;
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      tab_helper_->TranslateDidFinish(translate_infobar_, false);
      break;
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
      break;
  }
}

void TranslateOverlayTabHelper::TranslateStepObserver::OnTargetLanguageChanged(
    const std::string& target_language_code) {
  // Unimplemented on iOS as target language changes are initiated solely by the
  // UI. This method should always be a no-op.
  DCHECK_EQ(translate_infobar_->delegate()
                ->AsTranslateInfoBarDelegate()
                ->target_language_code(),
            target_language_code);
}

bool TranslateOverlayTabHelper::TranslateStepObserver::IsDeclinedByUser() {
  return false;
}

void TranslateOverlayTabHelper::TranslateStepObserver::
    OnTranslateInfoBarDelegateDestroyed(
        translate::TranslateInfoBarDelegate* delegate) {
  DCHECK(translate_scoped_observation_.IsObservingSource(delegate));
  translate_scoped_observation_.Reset();
  translate_infobar_ = nil;
}

void TranslateOverlayTabHelper::TranslateStepObserver::SetTranslateInfoBar(
    InfoBarIOS* infobar) {
  translate_infobar_ = infobar;
  translate_scoped_observation_.Observe(
      infobar->delegate()->AsTranslateInfoBarDelegate());
}

#pragma mark - TranslateInfobarObserver

TranslateOverlayTabHelper::TranslateInfobarObserver::TranslateInfobarObserver(
    web::WebState* web_state,
    TranslateOverlayTabHelper* tab_helper)
    : tab_helper_(tab_helper), tips_manager_(nullptr) {
  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state);
  DCHECK(manager);
  infobar_manager_scoped_observation_.Observe(manager);

  if (IsSegmentationTipsManagerEnabled()) {
    ProfileIOS* const profile =
        ProfileIOS::FromBrowserState(web_state->GetBrowserState());

    tips_manager_ = TipsManagerIOSFactory::GetForProfile(profile);

    CHECK(tips_manager_);
  }
}

TranslateOverlayTabHelper::TranslateInfobarObserver::
    ~TranslateInfobarObserver() {
  tips_manager_ = nullptr;
}

void TranslateOverlayTabHelper::TranslateInfobarObserver::OnInfoBarAdded(
    infobars::InfoBar* infobar) {
  translate::TranslateInfoBarDelegate* delegate =
      infobar->delegate()->AsTranslateInfoBarDelegate();
  if (delegate) {
    tab_helper_->TranslateInfoBarAdded(static_cast<InfoBarIOS*>(infobar));
  }

  // Records a visit to a website in a language different from the user's
  // default language. This allows the Tips Manager to offer assistance
  // with translation features if available.
  if (IsSegmentationTipsManagerEnabled() && tips_manager_) {
    tips_manager_->NotifySignal(segmentation_platform::tips_manager::signals::
                                    kOpenedWebsiteInAnotherLanguage);
  }
}

void TranslateOverlayTabHelper::TranslateInfobarObserver::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  DCHECK(infobar_manager_scoped_observation_.IsObservingSource(manager));
  infobar_manager_scoped_observation_.Reset();
}

#pragma mark - WebStateDestroyedObserver

TranslateOverlayTabHelper::WebStateDestroyedObserver::WebStateDestroyedObserver(
    web::WebState* web_state,
    TranslateOverlayTabHelper* tab_helper)
    : tab_helper_(tab_helper) {
  web_state_scoped_observation_.Observe(web_state);
}

TranslateOverlayTabHelper::WebStateDestroyedObserver::
    ~WebStateDestroyedObserver() = default;

void TranslateOverlayTabHelper::WebStateDestroyedObserver::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK(web_state_scoped_observation_.IsObservingSource(web_state));
  web_state_scoped_observation_.Reset();
  tab_helper_->UpdateForWebStateDestroyed();
}
