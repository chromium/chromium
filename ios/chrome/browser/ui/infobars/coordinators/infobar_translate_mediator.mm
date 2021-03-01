// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_language_selection_consumer.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_modal_consumer.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const int kInvalidLanguageIndex = -1;
}  // namespace

@interface InfobarTranslateMediator ()

// Delegate that holds the Translate Infobar information and actions.
@property(nonatomic, assign, readonly)
    translate::TranslateInfoBarDelegate* translateInfobarDelegate;

// Holds the new source language selected by the user. kInvalidLanguageIndex if
// the user has not made any such selection.
@property(nonatomic, assign) int newSourceLanguageIndex;

// Holds the new target language selected by the user. kInvalidLanguageIndex if
// the user has not made any such selection.
@property(nonatomic, assign) int newTargetLanguageIndex;

@end

@implementation InfobarTranslateMediator

- (instancetype)initWithInfoBarDelegate:
    (translate::TranslateInfoBarDelegate*)infobarDelegate {
  self = [super init];
  if (self) {
    DCHECK(infobarDelegate);
    _translateInfobarDelegate = infobarDelegate;
    _newSourceLanguageIndex = kInvalidLanguageIndex;
    _newTargetLanguageIndex = kInvalidLanguageIndex;
  }
  return self;
}

- (void)updateLanguagesIfNecessary {
  if (self.newSourceLanguageIndex != kInvalidLanguageIndex) {
    self.translateInfobarDelegate->UpdateSourceLanguage(
        self.translateInfobarDelegate->language_code_at(
            self.newSourceLanguageIndex));
    self.newSourceLanguageIndex = kInvalidLanguageIndex;
  }
  if (self.newTargetLanguageIndex != kInvalidLanguageIndex) {
    self.translateInfobarDelegate->UpdateTargetLanguage(
        self.translateInfobarDelegate->language_code_at(
            self.newTargetLanguageIndex));
    self.newTargetLanguageIndex = kInvalidLanguageIndex;
  }
}

- (void)setModalConsumer:(id<InfobarTranslateModalConsumer>)modalConsumer {
  _modalConsumer = modalConsumer;

  // Since this is displaying a new Modal, any new source/target language state
  // should be reset.
  self.newSourceLanguageIndex = kInvalidLanguageIndex;
  self.newTargetLanguageIndex = kInvalidLanguageIndex;

  BOOL currentStepBeforeTranslate =
      self.currentStep ==
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE;

  [self.modalConsumer
      setupModalViewControllerWithPrefs:
          [self createPrefDictionaryForSourceLanguage:
                    base::SysUTF16ToNSString(
                        self.translateInfobarDelegate->source_language_name())
                                       targetLanguage:
                                           base::SysUTF16ToNSString(
                                               self.translateInfobarDelegate
                                                   ->target_language_name())
                               translateButtonEnabled:
                                   currentStepBeforeTranslate]];
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

- (NSArray<TableViewTextItem*>*)loadTranslateLanguageItemsForSelectingLanguage:
    (BOOL)sourceLanguage {
  int sourceLanguageIndex = kInvalidLanguageIndex;
  int targetLanguageIndex = kInvalidLanguageIndex;
  // In the instance that the user has already selected a different source
  // language, then we should be using that language as the one to potentially
  // check or not show.
  std::string sourceLanguageCode =
      self.translateInfobarDelegate->source_language_code();
  if (self.newSourceLanguageIndex != kInvalidLanguageIndex) {
    sourceLanguageCode = self.translateInfobarDelegate->language_code_at(
        self.newSourceLanguageIndex);
  }
  // In the instance that the user has already selected a different target
  // language, then we should be using that language as the one to potentially
  // check or not show.
  std::string targetLanguageCode =
      self.translateInfobarDelegate->target_language_code();
  if (self.newTargetLanguageIndex != kInvalidLanguageIndex) {
    targetLanguageCode = self.translateInfobarDelegate->language_code_at(
        self.newTargetLanguageIndex);
  }
  NSMutableArray<TableViewTextItem*>* items = [NSMutableArray array];
  for (size_t i = 0; i < self.translateInfobarDelegate->num_languages(); ++i) {
    TableViewTextItem* item =
        [[TableViewTextItem alloc] initWithType:kItemTypeEnumZero];
    item.text = base::SysUTF16ToNSString(
        self.translateInfobarDelegate->language_name_at((int(i))));

    if (self.translateInfobarDelegate->language_code_at(i) ==
        sourceLanguageCode) {
      sourceLanguageIndex = i;
      if (!sourceLanguage) {
        // Disable for source language if selecting the target
        // language to prevent same language translation. Need to add item,
        // because the row number needs to match language's index in
        // translateInfobarDelegate.
        item.enabled = NO;
      }
    }
    if (self.translateInfobarDelegate->language_code_at(i) ==
        targetLanguageCode) {
      targetLanguageIndex = i;
      if (sourceLanguage) {
        // Disable for target language if selecting the source
        // language to prevent same language translation. Need to add item,
        // because the row number needs to match language's index in
        // translateInfobarDelegate.
        item.enabled = NO;
      }
    }

    if ((sourceLanguage && sourceLanguageIndex == (int)i) ||
        (!sourceLanguage && targetLanguageIndex == (int)i)) {
      item.checked = YES;
    }
    [items addObject:item];
  }
  DCHECK_GT(sourceLanguageIndex, kInvalidLanguageIndex);
  DCHECK_GT(targetLanguageIndex, kInvalidLanguageIndex);

  return items;
}

#pragma mark - InfobarTranslateLanguageSelectionDelegate

- (void)didSelectSourceLanguageIndex:(int)languageIndex
                            withName:(NSString*)languageName {
  // Sanity check that |languageIndex| matches the languageName selected.
  DCHECK([languageName
      isEqualToString:base::SysUTF16ToNSString(
                          self.translateInfobarDelegate->language_name_at(
                              languageIndex))]);
  DCHECK(self.modalConsumer);

  self.newSourceLanguageIndex = languageIndex;

  base::string16 targetLanguage =
      self.translateInfobarDelegate->target_language_name();
  if (self.newTargetLanguageIndex != kInvalidLanguageIndex) {
    targetLanguage = self.translateInfobarDelegate->language_name_at(
        self.newTargetLanguageIndex);
  }
  [self.modalConsumer
      setupModalViewControllerWithPrefs:
          [self createPrefDictionaryForSourceLanguage:
                    base::SysUTF16ToNSString(
                        self.translateInfobarDelegate->language_name_at(
                            languageIndex))
                                       targetLanguage:base::SysUTF16ToNSString(
                                                          targetLanguage)
                               translateButtonEnabled:YES]];
}

- (void)didSelectTargetLanguageIndex:(int)languageIndex
                            withName:(NSString*)languageName {
  // Sanity check that |languageIndex| matches the languageName selected.
  DCHECK([languageName
      isEqualToString:base::SysUTF16ToNSString(
                          self.translateInfobarDelegate->language_name_at(
                              languageIndex))]);
  DCHECK(self.modalConsumer);

  self.newTargetLanguageIndex = languageIndex;

  base::string16 sourceLanguage =
      self.translateInfobarDelegate->source_language_name();
  if (self.newSourceLanguageIndex != kInvalidLanguageIndex) {
    sourceLanguage = self.translateInfobarDelegate->language_name_at(
        self.newSourceLanguageIndex);
  }
  [self.modalConsumer
      setupModalViewControllerWithPrefs:
          [self createPrefDictionaryForSourceLanguage:base::SysUTF16ToNSString(
                                                          sourceLanguage)

                                       targetLanguage:
                                           base::SysUTF16ToNSString(
                                               self.translateInfobarDelegate
                                                   ->language_name_at(
                                                       languageIndex))
                               translateButtonEnabled:YES]];
}

#pragma mark - Private

// Returns a dictionary of prefs to send to the modalConsumer depending on
// |sourceLanguage|, |targetLanguage|, |translateButtonEnabled|, and
// |self.currentStep|.
- (NSDictionary*)createPrefDictionaryForSourceLanguage:(NSString*)sourceLanguage
                                        targetLanguage:(NSString*)targetLanguage
                                translateButtonEnabled:
                                    (BOOL)translateButtonEnabled {
  BOOL currentStepBeforeTranslate =
      self.currentStep ==
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE;
  BOOL currentStepAfterTranslate =
      self.currentStep ==
      translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE;
  BOOL updateLanguageBeforeTranslate =
      self.newSourceLanguageIndex != kInvalidLanguageIndex ||
      self.newTargetLanguageIndex != kInvalidLanguageIndex;

  return @{
    kSourceLanguagePrefKey : sourceLanguage,
    kTargetLanguagePrefKey : targetLanguage,
    kEnableTranslateButtonPrefKey : @(translateButtonEnabled),
    kUpdateLanguageBeforeTranslatePrefKey : @(updateLanguageBeforeTranslate),
    kEnableAndDisplayShowOriginalButtonPrefKey : @(currentStepAfterTranslate),
    kShouldAlwaysTranslatePrefKey :
        @(self.translateInfobarDelegate->ShouldAlwaysTranslate()),
    kDisplayNeverTranslateLanguagePrefKey : @(currentStepBeforeTranslate),
    kDisplayNeverTranslateSiteButtonPrefKey : @(currentStepBeforeTranslate),
    kIsTranslatableLanguagePrefKey :
        @(self.translateInfobarDelegate->IsTranslatableLanguageByPrefs()),
    kIsSiteOnNeverPromptListPrefKey :
        @(self.translateInfobarDelegate->IsSiteOnNeverPromptList()),
  };
}

@end
