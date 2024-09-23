// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"

#import <utility>
#import <vector>

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/memory/ptr_util.h"
#import "base/notreached.h"
#import "components/infobars/core/infobar.h"
#import "components/language/core/browser/accept_languages_service.h"
#import "components/language/core/browser/language_model_manager.h"
#import "components/language/core/browser/pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/translate/core/browser/page_translated_details.h"
#import "components/translate/core/browser/translate_infobar_delegate.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/translate/core/browser/translate_metrics_logger_impl.h"
#import "components/translate/core/browser/translate_step.h"
#import "components/translate/core/common/language_detection_details.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/language/model/accept_languages_service_factory.h"
#import "ios/chrome/browser/language/model/language_model_manager_factory.h"
#import "ios/chrome/browser/language/model/url_language_histogram_factory.h"
#import "ios/chrome/browser/language_detection/model/language_detection_model_loader_service_ios_factory.h"
#import "ios/chrome/browser/language_detection/model/language_detection_model_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/translate/model/translate_ranker_factory.h"
#import "ios/chrome/browser/translate/model/translate_service_ios.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "third_party/metrics_proto/translate_event.pb.h"
#import "url/gurl.h"

ChromeIOSTranslateClient::ChromeIOSTranslateClient(web::WebState* web_state)
    : web_state_(web_state),
      translate_driver_(
          web_state,
          LanguageDetectionModelLoaderServiceIOSFactory::GetForProfile(
              ProfileIOS::FromBrowserState(web_state->GetBrowserState()))),
      translate_manager_(std::make_unique<translate::TranslateManager>(
          this,
          translate::TranslateRankerFactory::GetForProfile(
              ProfileIOS::FromBrowserState(web_state->GetBrowserState())),
          LanguageModelManagerFactory::GetForProfile(
              ProfileIOS::FromBrowserState(web_state->GetBrowserState()))
              ->GetPrimaryModel())) {
  translate_driver_.Initialize(
      UrlLanguageHistogramFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state->GetBrowserState())),
      translate_manager_.get()),
      web_state_->AddObserver(this);
}

ChromeIOSTranslateClient::~ChromeIOSTranslateClient() {
  DCHECK(!web_state_);
}

// static
std::unique_ptr<translate::TranslatePrefs>
ChromeIOSTranslateClient::CreateTranslatePrefs(PrefService* prefs) {
  return std::unique_ptr<translate::TranslatePrefs>(
      new translate::TranslatePrefs(prefs));
}

translate::TranslateManager* ChromeIOSTranslateClient::GetTranslateManager() {
  return translate_manager_.get();
}

// TranslateClient implementation:

std::unique_ptr<infobars::InfoBar> ChromeIOSTranslateClient::CreateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  bool skip_banner = delegate->translate_step() ==
                     translate::TranslateStep::TRANSLATE_STEP_TRANSLATING;
  return std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeTranslate,
                                      std::move(delegate), skip_banner);
}

bool ChromeIOSTranslateClient::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool triggered_from_menu) {
  DCHECK(web_state_);
  if (error_type != translate::TranslateErrors::NONE) {
    step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;
  }

  // Infobar UI.
  translate::TranslateInfoBarDelegate::Create(
      step != translate::TRANSLATE_STEP_BEFORE_TRANSLATE || triggered_from_menu,
      translate_manager_->GetWeakPtr(),
      InfoBarManagerImpl::FromWebState(web_state_), step, source_language,
      target_language, error_type, triggered_from_menu);

  return true;
}

translate::IOSTranslateDriver* ChromeIOSTranslateClient::GetTranslateDriver() {
  return &translate_driver_;
}

PrefService* ChromeIOSTranslateClient::GetPrefs() {
  DCHECK(web_state_);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  return profile->GetOriginalProfile()->GetPrefs();
}

std::unique_ptr<translate::TranslatePrefs>
ChromeIOSTranslateClient::GetTranslatePrefs() {
  DCHECK(web_state_);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  return CreateTranslatePrefs(profile->GetPrefs());
}

language::AcceptLanguagesService*
ChromeIOSTranslateClient::GetAcceptLanguagesService() {
  DCHECK(web_state_);
  return AcceptLanguagesServiceFactory::GetForProfile(
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
}

bool ChromeIOSTranslateClient::IsTranslatableURL(const GURL& url) {
  return TranslateServiceIOS::IsTranslatableURL(url);
}

void ChromeIOSTranslateClient::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }

  DidPageLoadComplete();
  if (!IsTranslatableURL(navigation_context->GetUrl())) {
    // If URL is not translatable, do not record metrics as this would skew the
    // data.
    translate_metrics_logger_.reset();
    translate_manager_->RegisterTranslateMetricsLogger(nullptr);
    return;
  }
  // Lifetime of TranslateMetricsLogger should be each page load. So, we need to
  // detect the page load completion, i.e. the tab was closed, new navigation
  // replaced the page load, etc, and clear the logger.
  translate_metrics_logger_ =
      std::make_unique<translate::TranslateMetricsLoggerImpl>(
          translate_manager_->GetWeakPtr());
  translate_metrics_logger_->OnPageLoadStart(web_state->IsVisible());
}

void ChromeIOSTranslateClient::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->GetError()) {
    translate_metrics_logger_.reset();
    return;
  }

  if (!navigation_context->IsSameDocument() && translate_metrics_logger_) {
    translate_metrics_logger_->SetUkmSourceId(
        translate_driver_.GetUkmSourceId());
  }
}

void ChromeIOSTranslateClient::WasShown(web::WebState* web_state) {
  if (translate_metrics_logger_) {
    translate_metrics_logger_->OnForegroundChange(true);
  }
}

void ChromeIOSTranslateClient::WasHidden(web::WebState* web_state) {
  if (translate_metrics_logger_) {
    translate_metrics_logger_->OnForegroundChange(false);
  }
}

void ChromeIOSTranslateClient::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;

  DidPageLoadComplete();

  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with nullptr WebState.
  translate_manager_.reset();
}

void ChromeIOSTranslateClient::DidPageLoadComplete() {
  if (translate_metrics_logger_) {
    translate_metrics_logger_->RecordMetrics(true);
    translate_metrics_logger_.reset();
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(ChromeIOSTranslateClient)
