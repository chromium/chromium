// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/translate/translate_infobar_modal_overlay_mediator.h"

#import "base/metrics/histogram_macros.h"
#import "base/metrics/sparse_histogram.h"
#import "base/strings/sys_string_conversions.h"
#import "components/metrics/metrics_log.h"
#import "components/translate/core/browser/translate_step.h"
#import "components/translate/core/common/translate_constants.h"
#import "components/translate/core/common/translate_util.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/translate_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/translate_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/translate/translate_constants.h"
#import "ios/chrome/browser/translate/translate_infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const int kInvalidLanguageIndex = -1;
}  // namespace

using translate_infobar_overlays::TranslateModalRequestConfig;

@interface TranslateInfobarModalOverlayMediator ()
// The translate modal config from the request.
@property(nonatomic, readonly) TranslateModalRequestConfig* config;
// Holds the new source language selected by the user. kInvalidLanguageIndex if
// the user has not made any such selection.
@property(nonatomic, assign) int newSourceLanguageIndex;
// Whether the source language (initial or selected) is unknown.
@property(nonatomic, assign) BOOL sourceLanguageIsUnknown;
// Whether the source language was initially unknown.
@property(nonatomic, assign) BOOL sourceLanguageIsInitiallyUnknown;

// Holds the new target language selected by the user. kInvalidLanguageIndex if
// the user has not made any such selection.
@property(nonatomic, assign) int newTargetLanguageIndex;

// Maps the index from the source language selection view to
// `config->language_names()`.
@property(nonatomic, assign) std::vector<int> sourceLanguageMapping;

// Maps the index from the target language selection view to
// `config->language_names()`.
@property(nonatomic, assign) std::vector<int> targetLanguageMapping;
@end

@implementation TranslateInfobarModalOverlayMediator

#pragma mark - Accessors

- (TranslateModalRequestConfig*)config {
  return self.request ? self.request->GetConfig<TranslateModalRequestConfig>()
                      : nullptr;
}

- (void)setConsumer:(id<InfobarTranslateModalConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;

  TranslateModalRequestConfig* config = self.config;
  if (!_consumer || !config)
    return;

  // Since this is displaying a new Modal, any new source/target language state
  // should be reset.
  self.newSourceLanguageIndex = kInvalidLanguageIndex;
  self.newTargetLanguageIndex = kInvalidLanguageIndex;
  self.sourceLanguageIsUnknown = self.config->unknown_language_name() ==
                                 self.config->source_language_name();
  self.sourceLanguageIsInitiallyUnknown =
      self.config->unknown_language_name() ==
      self.config->initial_source_language_name();

  // The Translate button should be enabled whenever the page is untranslated,
  // which may be before any translation has been triggered or after an error
  // caused translation to fail.
  BOOL currentStepUntranslated =
      self.config->current_step() ==
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE ||
      self.config->current_step() ==
          translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR;

  [self.consumer
      setupModalViewControllerWithPrefs:
          [self
              createPrefDictionaryForSourceLanguage:
                  base::SysUTF16ToNSString(self.config->source_language_name())
                                     targetLanguage:
                                         base::SysUTF16ToNSString(
                                             self.config
                                                 ->target_language_name())
                             translateButtonEnabled:currentStepUntranslated]];
}

- (void)setSourceLanguageSelectionConsumer:
    (id<InfobarTranslateLanguageSelectionConsumer>)
        sourceLanguageSelectionConsumer {
  _sourceLanguageSelectionConsumer = sourceLanguageSelectionConsumer;
  NSArray<TableViewTextItem*>* items =
      [self loadTranslateLanguageItemsForSelectingLanguage:YES];
  [self.sourceLanguageSelectionConsumer setTranslateLanguageItems:items];
}

- (void)setTargetLanguageSelectionConsumer:
    (id<InfobarTranslateLanguageSelectionConsumer>)
        targetLanguageSelectionConsumer {
  _targetLanguageSelectionConsumer = targetLanguageSelectionConsumer;
  NSArray<TableViewTextItem*>* items =
      [self loadTranslateLanguageItemsForSelectingLanguage:NO];
  [self.targetLanguageSelectionConsumer setTranslateLanguageItems:items];
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return TranslateModalRequestConfig::RequestSupport();
}

#pragma mark - InfobarTranslateModalDelegate

- (void)showSourceLanguage {
  [self recordInfobarEvent:translate::InfobarEvent::INFOBAR_REVERT];
  [self dispatchResponse:
            OverlayResponse::CreateWithInfo<
                translate_infobar_modal_responses::RevertTranslation>()];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::ShowOriginal];
  [self dismissOverlay];
}

- (void)translateWithNewLanguages {
  [self updateLanguagesIfNecessary];
  [self
      recordInfobarEvent:translate::InfobarEvent::INFOBAR_TARGET_TAB_TRANSLATE];

  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             InfobarModalMainActionResponse>()];
  [self dismissOverlay];
}

- (void)showChangeSourceLanguageOptions {
  [self recordInfobarEvent:translate::InfobarEvent::INFOBAR_PAGE_NOT_IN];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::ChangeSourceLanguage];

  [self.translateMediatorDelegate showChangeSourceLanguageOptions];
}

- (void)showChangeTargetLanguageOptions {
  [self recordInfobarEvent:translate::InfobarEvent::INFOBAR_MORE_LANGUAGES];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::ChangeTargetLanguage];

  [self.translateMediatorDelegate showChangeTargetLanguageOptions];
}

- (void)alwaysTranslateSourceLanguage {
  [self recordInfobarEvent:translate::InfobarEvent::INFOBAR_ALWAYS_TRANSLATE];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::
                           TappedAlwaysTranslate];

  [self dispatchResponse:
            OverlayResponse::CreateWithInfo<
                translate_infobar_modal_responses::ToggleAlwaysTranslate>()];

  // Since toggle turned on always translate, translate now if not already
  // translated.
  if (self.config->current_step() ==
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE) {
    [self dispatchResponse:OverlayResponse::CreateWithInfo<
                               InfobarModalMainActionResponse>()];
  }

  [self dismissOverlay];
}

- (void)undoAlwaysTranslateSourceLanguage {
  DCHECK(self.config->is_translatable_language());
  [self recordInfobarEvent:translate::InfobarEvent::
                               INFOBAR_ALWAYS_TRANSLATE_UNDO];
  [self dispatchResponse:
            OverlayResponse::CreateWithInfo<
                translate_infobar_modal_responses::ToggleAlwaysTranslate>()];
  [self dismissOverlay];
}

- (void)neverTranslateSourceLanguage {
  DCHECK(self.config->is_translatable_language());
  [self recordInfobarEvent:translate::InfobarEvent::INFOBAR_NEVER_TRANSLATE];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::
                           TappedNeverForSourceLanguage];
  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             translate_infobar_modal_responses::
                                 ToggleNeverTranslateSourceLanguage>()];

  [self dismissOverlay];
}

- (void)undoNeverTranslateSourceLanguage {
  DCHECK(!self.config->is_translatable_language());
  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             translate_infobar_modal_responses::
                                 ToggleNeverTranslateSourceLanguage>()];
  [self dismissOverlay];
}

- (void)neverTranslateSite {
  DCHECK(!self.config->is_site_on_never_prompt_list());
  [self
      recordInfobarEvent:translate::InfobarEvent::INFOBAR_NEVER_TRANSLATE_SITE];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::
                           TappedNeverForThisSite];
  [self dispatchResponse:
            OverlayResponse::CreateWithInfo<
                translate_infobar_modal_responses::ToggleNeverPromptSite>()];
  [self dismissOverlay];
}

- (void)undoNeverTranslateSite {
  DCHECK(self.config->is_site_on_never_prompt_list());
  [self dispatchResponse:
            OverlayResponse::CreateWithInfo<
                translate_infobar_modal_responses::ToggleNeverPromptSite>()];
  [self dismissOverlay];
}

#pragma mark - InfobarTranslateLanguageSelectionDelegate

- (void)didSelectSourceLanguageIndex:(int)itemIndex
                            withName:(NSString*)languageName {
  int languageIndex = self.sourceLanguageMapping[itemIndex];
  // Sanity check that `languageIndex` matches the languageName selected.
  DCHECK([languageName
      isEqualToString:base::SysUTF16ToNSString(
                          self.config->language_names().at(languageIndex))]);

  self.newSourceLanguageIndex = languageIndex;
  std::u16string sourceLanguage =
      self.config->language_names().at(languageIndex);

  std::u16string targetLanguage = self.config->target_language_name();
  if (self.newTargetLanguageIndex != kInvalidLanguageIndex) {
    targetLanguage =
        self.config->language_names().at(self.newTargetLanguageIndex);
  }
  self.sourceLanguageIsUnknown =
      sourceLanguage == self.config->unknown_language_name();
  [self.consumer
      setupModalViewControllerWithPrefs:
          [self createPrefDictionaryForSourceLanguage:base::SysUTF16ToNSString(
                                                          sourceLanguage)
                                       targetLanguage:base::SysUTF16ToNSString(
                                                          targetLanguage)
                               translateButtonEnabled:YES]];
}

- (void)didSelectTargetLanguageIndex:(int)itemIndex
                            withName:(NSString*)languageName {
  int languageIndex = self.targetLanguageMapping[itemIndex];
  // Sanity check that `languageIndex` matches the languageName selected.
  DCHECK([languageName
      isEqualToString:base::SysUTF16ToNSString(
                          self.config->language_names().at(languageIndex))]);

  self.newTargetLanguageIndex = languageIndex;
  std::u16string targetLanguage =
      self.config->language_names().at(languageIndex);

  std::u16string sourceLanguage = self.config->source_language_name();
  if (self.newSourceLanguageIndex != kInvalidLanguageIndex) {
    sourceLanguage =
        self.config->language_names().at(self.newSourceLanguageIndex);
  }
  [self.consumer
      setupModalViewControllerWithPrefs:
          [self createPrefDictionaryForSourceLanguage:base::SysUTF16ToNSString(
                                                          sourceLanguage)
                                       targetLanguage:base::SysUTF16ToNSString(
                                                          targetLanguage)
                               translateButtonEnabled:YES]];
}

#pragma mark - Private

- (NSArray<TableViewTextItem*>*)loadTranslateLanguageItemsForSelectingLanguage:
    (BOOL)sourceLanguage {
  // In the instance that the user has already selected a different source
  // language, then we should be using that language as the one to potentially
  // check or not show.
  std::u16string sourceLanguageName =
      self.newSourceLanguageIndex != kInvalidLanguageIndex
          ? self.config->language_names().at(self.newSourceLanguageIndex)
          : self.config->source_language_name();
  // In the instance that the user has already selected a different target
  // language, then we should be using that language as the one to potentially
  // check or not show.
  std::u16string targetLanguageName =
      self.newTargetLanguageIndex != kInvalidLanguageIndex
          ? self.config->language_names().at(self.newTargetLanguageIndex)
          : self.config->target_language_name();

  BOOL shouldSkipFirstLanguage =
      !(sourceLanguage && self.sourceLanguageIsInitiallyUnknown);
  NSMutableArray<TableViewTextItem*>* items = [NSMutableArray array];
  std::vector<int> languageMapping;
  languageMapping.reserve(self.config->language_names().size());

  for (size_t i = 0; i < self.config->language_names().size(); ++i) {
    if (translate::IsForceTranslateEnabled() && shouldSkipFirstLanguage &&
        i == 0) {
      // "Detected Language" is the first item in the languages list and should
      // only be added to the source language menu.
      continue;
    }
    languageMapping.push_back(i);
    TableViewTextItem* item =
        [[TableViewTextItem alloc] initWithType:kItemTypeEnumZero];
    item.text =
        base::SysUTF16ToNSString(self.config->language_names().at((int)i));

    if (self.config->language_names().at((int)i) == sourceLanguageName) {
      if (!sourceLanguage) {
        // Disable for source language if selecting the target
        // language to prevent same language translation. Need to add item,
        // because the row number needs to match language's index in
        // translateInfobarDelegate.
        item.enabled = NO;
      }
    }
    if (self.config->language_names().at((int)i) == targetLanguageName) {
      if (sourceLanguage) {
        // Disable for target language if selecting the source
        // language to prevent same language translation. Need to add item,
        // because the row number needs to match language's index in
        // translateInfobarDelegate.
        item.enabled = NO;
      }
    }

    if ((sourceLanguage &&
         sourceLanguageName == self.config->language_names().at((int)i)) ||
        (!sourceLanguage &&
         targetLanguageName == self.config->language_names().at((int)i))) {
      item.checked = YES;
    }
    [items addObject:item];
  }
  if (sourceLanguage) {
    self.sourceLanguageMapping = languageMapping;
  } else {
    self.targetLanguageMapping = languageMapping;
  }

  return items;
}

// Records a histogram for `event`.
- (void)recordInfobarEvent:(translate::InfobarEvent)event {
  UMA_HISTOGRAM_ENUMERATION(kEventHistogram, event);
}
// Records a histogram of `histogram` for `langCode`. This is used to log the
// language distribution of certain Translate events.
- (void)recordLanguageDataHistogram:(const std::string&)histogramName
                       languageCode:(const std::string&)langCode {
  // TODO(crbug.com/1025440): Use function version of macros here and in
  // TranslateInfobarController.
  base::SparseHistogram::FactoryGet(
      histogramName, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(metrics::MetricsLog::Hash(langCode));
}

- (void)updateLanguagesIfNecessary {
  if (self.newSourceLanguageIndex != kInvalidLanguageIndex ||
      self.newTargetLanguageIndex != kInvalidLanguageIndex) {
    [self dispatchResponse:
              OverlayResponse::CreateWithInfo<
                  translate_infobar_modal_responses::UpdateLanguageInfo>(
                  self.newSourceLanguageIndex, self.newTargetLanguageIndex)];
    self.newSourceLanguageIndex = kInvalidLanguageIndex;
    self.newTargetLanguageIndex = kInvalidLanguageIndex;
  }
}

// Returns a dictionary of prefs to send to the modalConsumer depending on
// `sourceLanguage`, `targetLanguage`, `translateButtonEnabled`, and
// `self.currentStep`.
- (NSDictionary*)createPrefDictionaryForSourceLanguage:(NSString*)sourceLanguage
                                        targetLanguage:(NSString*)targetLanguage
                                translateButtonEnabled:
                                    (BOOL)translateButtonEnabled {
  // Modal state following a translate error should be the same as on an
  // untranslated page.
  BOOL currentStepUntranslated =
      self.config->current_step() ==
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE ||
      self.config->current_step() ==
          translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR;
  BOOL currentStepAfterTranslate =
      self.config->current_step() ==
      translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE;
  BOOL updateLanguageBeforeTranslate =
      self.newSourceLanguageIndex != kInvalidLanguageIndex ||
      self.newTargetLanguageIndex != kInvalidLanguageIndex;

  return @{
    kSourceLanguagePrefKey : sourceLanguage,
    kSourceLanguageIsUnknownPrefKey : @(self.sourceLanguageIsUnknown),
    kTargetLanguagePrefKey : targetLanguage,
    kEnableTranslateButtonPrefKey : @(translateButtonEnabled),
    kUpdateLanguageBeforeTranslatePrefKey : @(updateLanguageBeforeTranslate),
    kEnableAndDisplayShowOriginalButtonPrefKey : @(currentStepAfterTranslate),
    kShouldAlwaysTranslatePrefKey :
        @(self.config->is_always_translate_enabled()),
    kDisplayNeverTranslateLanguagePrefKey : @(currentStepUntranslated),
    kDisplayNeverTranslateSiteButtonPrefKey : @(currentStepUntranslated),
    kIsTranslatableLanguagePrefKey : @(self.config->is_translatable_language()),
    kIsSiteOnNeverPromptListPrefKey :
        @(self.config->is_site_on_never_prompt_list()),
  };
}

@end
