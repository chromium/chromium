// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_modal_interaction_handler.h"

#import "base/metrics/histogram_macros.h"
#import "base/metrics/sparse_histogram.h"
#import "components/metrics/metrics_log.h"
#import "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_modal_overlay_request_callback_installer.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/infobars/overlays/translate_infobar_placeholder_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/infobars/overlays/translate_overlay_tab_helper.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_placeholder_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/translate_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/translate/translate_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using translate_infobar_overlays::TranslateBannerRequestConfig;
using translate_infobar_overlays::PlaceholderRequestCancelHandler;
using translate_infobar_overlay::ModalRequestCallbackInstaller;

namespace {
// Records a histogram of `histogram` for `langCode`. This is used to log the
// language distribution of certain Translate events.
void RecordLanguageDataHistogram(const std::string& histogram_name,
                                 const std::string& lang_code) {
  // TODO(crbug.com/1025440): Use function version of macros here and in
  // TranslateInfobarController.
  base::SparseHistogram::FactoryGet(
      histogram_name, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(metrics::MetricsLog::Hash(lang_code));
}
}

TranslateInfobarModalInteractionHandler::
    TranslateInfobarModalInteractionHandler() = default;

TranslateInfobarModalInteractionHandler::
    ~TranslateInfobarModalInteractionHandler() = default;

#pragma mark - Public

void TranslateInfobarModalInteractionHandler::ToggleAlwaysTranslate(
    InfoBarIOS* infobar) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate(infobar);
  bool enabling_always_translate = !delegate->ShouldAlwaysTranslate();
  delegate->ToggleAlwaysTranslate();
  RecordLanguageDataHistogram(kLanguageHistogramAlwaysTranslate,
                              delegate->source_language_code());
  if (enabling_always_translate) {
    StartTranslation(infobar);
  }
}

void TranslateInfobarModalInteractionHandler::ToggleNeverTranslateLanguage(
    InfoBarIOS* infobar) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate(infobar);
  bool should_remove_infobar = delegate->IsTranslatableLanguageByPrefs();
  delegate->ToggleTranslatableLanguageByPrefs();
  RecordLanguageDataHistogram(kLanguageHistogramNeverTranslate,
                              delegate->source_language_code());
  // Remove infobar if turning it on.
  if (should_remove_infobar)
    infobar->RemoveSelf();
}

void TranslateInfobarModalInteractionHandler::ToggleNeverTranslateSite(
    InfoBarIOS* infobar) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate(infobar);
  bool should_remove_infobar = !delegate->IsSiteOnNeverPromptList();
  delegate->ToggleNeverPromptSite();
  // Remove infobar if turning it on.
  if (should_remove_infobar)
    infobar->RemoveSelf();
}

void TranslateInfobarModalInteractionHandler::RevertTranslation(
    InfoBarIOS* infobar) {
  GetDelegate(infobar)->RevertWithoutClosingInfobar();
  infobar->set_accepted(false);
}

void TranslateInfobarModalInteractionHandler::UpdateLanguages(
    InfoBarIOS* infobar,
    int source_language_index,
    int target_language_index) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate(infobar);
  if (source_language_index != -1) {
    std::string source_language_code =
        delegate->language_code_at(source_language_index);
    if (delegate->source_language_code() != source_language_code) {
      delegate->UpdateSourceLanguage(source_language_code);
    }
  }

  if (target_language_index != -1) {
    std::string target_language_code =
        delegate->language_code_at(target_language_index);
    if (delegate->target_language_code() != target_language_code) {
      delegate->UpdateTargetLanguage(target_language_code);
    }
  }
}

#pragma mark - InfobarInteractionHandler::Handler

void TranslateInfobarModalInteractionHandler::InfobarVisibilityChanged(
    InfoBarIOS* infobar,
    bool visible) {
  if (!visible)
    GetDelegate(infobar)->InfoBarDismissed();
}

#pragma mark - InfobarModalInteractionHandler

void TranslateInfobarModalInteractionHandler::PerformMainAction(
    InfoBarIOS* infobar) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate(infobar);
  if (delegate->ShouldAutoAlwaysTranslate())
    delegate->ToggleAlwaysTranslate();
  StartTranslation(infobar);

  RecordLanguageDataHistogram(kLanguageHistogramTranslate,
                              delegate->target_language_code());
}

#pragma mark - Private

void TranslateInfobarModalInteractionHandler::StartTranslation(
    InfoBarIOS* infobar) {
  GetDelegate(infobar)->Translate();
}

std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>
TranslateInfobarModalInteractionHandler::CreateModalInstaller() {
  return std::make_unique<ModalRequestCallbackInstaller>(this);
}

translate::TranslateInfoBarDelegate*
TranslateInfobarModalInteractionHandler::GetDelegate(InfoBarIOS* infobar) {
  translate::TranslateInfoBarDelegate* delegate =
      infobar->delegate()->AsTranslateInfoBarDelegate();
  DCHECK(delegate);
  return delegate;
}
