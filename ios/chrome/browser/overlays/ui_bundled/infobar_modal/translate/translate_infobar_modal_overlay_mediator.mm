// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/translate/translate_infobar_modal_overlay_mediator.h"

#import "base/metrics/histogram_macros.h"
#import "base/metrics/sparse_histogram.h"
#import "base/strings/sys_string_conversions.h"
#import "components/metrics/metrics_log.h"
#import "components/translate/core/browser/translate_infobar_delegate.h"
#import "components/translate/core/browser/translate_step.h"
#import "components/translate/core/common/translate_metrics.h"
#import "components/translate/core/common/translate_util.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/translate/model/translate_infobar_metrics_recorder.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"

namespace {
const int kInvalidLanguageIndex = -1;
}  // namespace

@interface TranslateInfobarModalOverlayMediator ()

// The translate modal config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
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

// Supported language names.
@property(nonatomic, assign) std::vector<std::u16string> languageNames;

@end

@implementation TranslateInfobarModalOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

// Returns the delegate attached to the config.
- (translate::TranslateInfoBarDelegate*)translateDelegate {
  return static_cast<translate::TranslateInfoBarDelegate*>(
      self.config->delegate());
}

- (void)setConsumer:(id<InfobarTranslateModalConsumer>)consumer {
  _consumer = consumer;
  translate::TranslateInfoBarDelegate* delegate = self.translateDelegate;

  std::vector<std::u16string> languageNames;
  for (size_t i = 0; i < delegate->num_languages(); ++i) {
    languageNames.push_back(delegate->language_name_at((int(i))));
  }
  self.languageNames = languageNames;

  // Since this is displaying a new Modal, any new source/target language state
  // should be reset.
  self.newSourceLanguageIndex = kInvalidLanguageIndex;
  self.newTargetLanguageIndex = kInvalidLanguageIndex;
  self.sourceLanguageIsUnknown =
      delegate->unknown_language_name() == delegate->source_language_name();
  self.sourceLanguageIsInitiallyUnknown =
      delegate->unknown_language_name() ==
      delegate->initial_source_language_name();

  // The Translate button should be enabled whenever the page is untranslated,
  // which may be before any translation has been triggered or after an error
  // caused translation to fail.
  const translate::TranslateStep currentStep = delegate->translate_step();
  const BOOL currentStepUntranslated =
      currentStep ==
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE ||
      currentStep == translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR;

  [self.consumer
      setupModalViewControllerWithPrefs:
          [self createPrefDictionaryForSourceLanguage:
                    base::SysUTF16ToNSString(delegate->source_language_name())
                                       targetLanguage:
                                           base::SysUTF16ToNSString(
                                               delegate->target_language_name())
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
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark InfobarModalDelegate

- (void)modalInfobarButtonWasAccepted:(id)infobarModal {
  [self startTranslation];

  [self dismissOverlay];
}

#pragma mark - InfobarTranslateModalDelegate

- (void)showSourceLanguage {
  translate::ReportCompactInfobarEvent(translate::InfobarEvent::INFOBAR_REVERT);

  InfoBarIOS* infobar = GetOverlayRequestInfobar(self.request);
  self.translateDelegate->RevertWithoutClosingInfobar();
  infobar->set_accepted(false);
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::ShowOriginal];

  [self dismissOverlay];
}

- (void)translateWithNewLanguages {
  [self updateLanguagesIfNecessary];
  translate::ReportCompactInfobarEvent(
      translate::InfobarEvent::INFOBAR_TARGET_TAB_TRANSLATE);

  [self startTranslation];

  [self dismissOverlay];
}

- (void)showChangeSourceLanguageOptions {
  translate::ReportCompactInfobarEvent(
      translate::InfobarEvent::INFOBAR_PAGE_NOT_IN);
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::ChangeSourceLanguage];

  [self.translateMediatorDelegate showChangeSourceLanguageOptions];
}

- (void)showChangeTargetLanguageOptions {
  translate::ReportCompactInfobarEvent(
      translate::InfobarEvent::INFOBAR_MORE_LANGUAGES);
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::ChangeTargetLanguage];

  [self.translateMediatorDelegate showChangeTargetLanguageOptions];
}

- (void)alwaysTranslateSourceLanguage {
  translate::ReportCompactInfobarEvent(
      translate::InfobarEvent::INFOBAR_ALWAYS_TRANSLATE);
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::
                           TappedAlwaysTranslate];

  [self toggleAlwaysTranslate];

  // Since toggle turned on always translate, translate now if not already
  // translated.
  if (self.translateDelegate->translate_step() ==
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE) {
    [self startTranslation];
  }

  [self dismissOverlay];
}

- (void)undoAlwaysTranslateSourceLanguage {
  DCHECK(self.translateDelegate->IsTranslatableLanguageByPrefs());
  translate::ReportCompactInfobarEvent(
      translate::InfobarEvent::INFOBAR_ALWAYS_TRANSLATE_UNDO);
  [self toggleAlwaysTranslate];

  [self dismissOverlay];
}

- (void)neverTranslateSourceLanguage {
  DCHECK(self.translateDelegate->IsTranslatableLanguageByPrefs());
  translate::ReportCompactInfobarEvent(
      translate::InfobarEvent::INFOBAR_NEVER_TRANSLATE);
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::
                           TappedNeverForSourceLanguage];
  [self ToggleNeverTranslateSourceLanguage];

  [self dismissOverlay];
}

- (void)undoNeverTranslateSourceLanguage {
  DCHECK(!self.translateDelegate->IsTranslatableLanguageByPrefs());
  [self ToggleNeverTranslateSourceLanguage];

  [self dismissOverlay];
}

- (void)neverTranslateSite {
  DCHECK(!self.translateDelegate->IsSiteOnNeverPromptList());
  translate::ReportCompactInfobarEvent(
      translate::InfobarEvent::INFOBAR_NEVER_TRANSLATE_SITE);
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::
                           TappedNeverForThisSite];
  [self toggleNeverTranslateSite];

  [self dismissOverlay];
}

- (void)undoNeverTranslateSite {
  DCHECK(self.translateDelegate->IsSiteOnNeverPromptList());
  [self toggleNeverTranslateSite];

  [self dismissOverlay];
}

#pragma mark - InfobarTranslateLanguageSelectionDelegate

- (void)didSelectSourceLanguageIndex:(int)itemIndex
                            withName:(NSString*)languageName {
  translate::TranslateInfoBarDelegate* delegate = self.translateDelegate;
  int languageIndex = self.sourceLanguageMapping[itemIndex];
  std::vector<std::u16string> languageNames = self.languageNames;

  // Sanity check that `languageIndex` matches the languageName selected.
  DCHECK([languageName isEqualToString:base::SysUTF16ToNSString(
                                           languageNames.at(languageIndex))]);

  self.newSourceLanguageIndex = languageIndex;
  std::u16string sourceLanguage = languageNames.at(languageIndex);

  std::u16string targetLanguage = delegate->target_language_name();
  if (self.newTargetLanguageIndex != kInvalidLanguageIndex) {
    targetLanguage = languageNames.at(self.newTargetLanguageIndex);
  }
  self.sourceLanguageIsUnknown =
      sourceLanguage == delegate->unknown_language_name();
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
  translate::TranslateInfoBarDelegate* delegate = self.translateDelegate;
  int languageIndex = self.targetLanguageMapping[itemIndex];
  std::vector<std::u16string> languageNames = self.languageNames;

  // Sanity check that `languageIndex` matches the languageName selected.
  DCHECK([languageName isEqualToString:base::SysUTF16ToNSString(
                                           languageNames.at(languageIndex))]);

  self.newTargetLanguageIndex = languageIndex;
  std::u16string targetLanguage = languageNames.at(languageIndex);

  std::u16string sourceLanguage = delegate->source_language_name();
  if (self.newSourceLanguageIndex != kInvalidLanguageIndex) {
    sourceLanguage = languageNames.at(self.newSourceLanguageIndex);
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

// Returns the language `items` to be displayed.
- (NSArray<TableViewTextItem*>*)loadTranslateLanguageItemsForSelectingLanguage:
    (BOOL)sourceLanguage {
  translate::TranslateInfoBarDelegate* delegate = self.translateDelegate;
  std::vector<std::u16string> languageNames = self.languageNames;

  // In the instance that the user has already selected a different source
  // language, then we should be using that language as the one to potentially
  // check or not show.
  std::u16string sourceLanguageName =
      self.newSourceLanguageIndex != kInvalidLanguageIndex
          ? languageNames.at(self.newSourceLanguageIndex)
          : delegate->source_language_name();
  // In the instance that the user has already selected a different target
  // language, then we should be using that language as the one to potentially
  // check or not show.
  std::u16string targetLanguageName =
      self.newTargetLanguageIndex != kInvalidLanguageIndex
          ? languageNames.at(self.newTargetLanguageIndex)
          : delegate->target_language_name();

  BOOL shouldSkipFirstLanguage =
      !(sourceLanguage && self.sourceLanguageIsInitiallyUnknown);
  NSMutableArray<TableViewTextItem*>* items = [NSMutableArray array];
  std::vector<int> languageMapping;
  languageMapping.reserve(languageNames.size());

  for (size_t i = 0; i < languageNames.size(); ++i) {
    if (shouldSkipFirstLanguage && i == 0) {
      // "Detected Language" is the first item in the languages list and should
      // only be added to the source language menu.
      continue;
    }

    std::u16string languageName = languageNames.at((int)i);
    languageMapping.push_back(i);
    TableViewTextItem* item =
        [[TableViewTextItem alloc] initWithType:kItemTypeEnumZero];
    item.text = base::SysUTF16ToNSString(languageName);

    if (languageName == sourceLanguageName) {
      if (!sourceLanguage) {
        // Disable for source language if selecting the target
        // language to prevent same language translation. Need to add item,
        // because the row number needs to match language's index in
        // translateInfobarDelegate.
        item.enabled = NO;
      }
    }
    if (languageName == targetLanguageName) {
      if (sourceLanguage) {
        // Disable for target language if selecting the source
        // language to prevent same language translation. Need to add item,
        // because the row number needs to match language's index in
        // translateInfobarDelegate.
        item.enabled = NO;
      }
    }

    if ((sourceLanguage && sourceLanguageName == languageName) ||
        (!sourceLanguage && targetLanguageName == languageName)) {
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

// Records a histogram of `histogram` for `langCode`. This is used to log the
// language distribution of certain Translate events.
- (void)recordLanguageDataHistogram:(const std::string&)histogramName
                       languageCode:(const std::string&)langCode {
  // TODO(crbug.com/40107868): Use function version of macros here and in
  // TranslateInfobarController.
  base::SparseHistogram::FactoryGet(
      histogramName, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(metrics::MetricsLog::Hash(langCode));
}

// Updates source and target languages if necessary.
- (void)updateLanguagesIfNecessary {
  int sourceLanguageIndex = self.newSourceLanguageIndex;
  int targetLanguageIndex = self.newTargetLanguageIndex;

  if (sourceLanguageIndex != kInvalidLanguageIndex ||
      targetLanguageIndex != kInvalidLanguageIndex) {
    translate::TranslateInfoBarDelegate* delegate = self.translateDelegate;

    if (sourceLanguageIndex != kInvalidLanguageIndex) {
      std::string sourceLanguageCode =
          delegate->language_code_at(sourceLanguageIndex);
      if (delegate->source_language_code() != sourceLanguageCode) {
        delegate->UpdateSourceLanguage(sourceLanguageCode);
      }
    }

    if (targetLanguageIndex != kInvalidLanguageIndex) {
      std::string targetLanguageCode =
          delegate->language_code_at(targetLanguageIndex);
      if (delegate->target_language_code() != targetLanguageCode) {
        delegate->UpdateTargetLanguage(targetLanguageCode);
      }
    }

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
  translate::TranslateInfoBarDelegate* delegate = self.translateDelegate;
  const translate::TranslateStep currentStep = delegate->translate_step();

  // Modal state following a translate error should be the same as on an
  // untranslated page.
  BOOL currentStepUntranslated =
      currentStep ==
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE ||
      currentStep == translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR;
  BOOL currentStepAfterTranslate =
      currentStep == translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE;
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
    kShouldAlwaysTranslatePrefKey : @(delegate->ShouldAlwaysTranslate()),
    kDisplayNeverTranslateLanguagePrefKey : @(currentStepUntranslated),
    kDisplayNeverTranslateSiteButtonPrefKey : @(currentStepUntranslated),
    kIsTranslatableLanguagePrefKey :
        @(delegate->IsTranslatableLanguageByPrefs()),
    kIsSiteOnNeverPromptListPrefKey : @(delegate->IsSiteOnNeverPromptList()),
  };
}

// Called when the always translate preference has been toggled.
- (void)toggleAlwaysTranslate {
  translate::TranslateInfoBarDelegate* delegate = self.translateDelegate;
  const bool enablingAlwaysTranslate = !delegate->ShouldAlwaysTranslate();
  delegate->ToggleAlwaysTranslate();
  if (enablingAlwaysTranslate) {
    delegate->Translate();
  }
}

// Called when the never translate source language preference has been toggled.
- (void)ToggleNeverTranslateSourceLanguage {
  translate::TranslateInfoBarDelegate* delegate = self.translateDelegate;
  const bool shouldRemoveInfobar = delegate->IsTranslatableLanguageByPrefs();
  delegate->ToggleTranslatableLanguageByPrefs();
  // Remove infobar if turning it on.
  if (shouldRemoveInfobar) {
    InfoBarIOS* infobar = GetOverlayRequestInfobar(self.request);
    infobar->RemoveSelf();
  }
}

// Called when the never translate site preference has been toggled.
- (void)toggleNeverTranslateSite {
  translate::TranslateInfoBarDelegate* delegate = self.translateDelegate;
  const bool shouldRemoveInfobar = !delegate->IsSiteOnNeverPromptList();
  delegate->ToggleNeverPromptSite();
  // Remove infobar if turning it on.
  if (shouldRemoveInfobar) {
    InfoBarIOS* infobar = GetOverlayRequestInfobar(self.request);
    infobar->RemoveSelf();
  }
}

// Starts translation.
- (void)startTranslation {
  translate::TranslateInfoBarDelegate* delegate = self.translateDelegate;
  if (delegate->ShouldAutoAlwaysTranslate()) {
    delegate->ToggleAlwaysTranslate();
  }
  delegate->Translate();
}

@end
