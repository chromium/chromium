// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_context_menu_builder.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_option.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_config.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// The corner radius of the favicon in attach current tab action.
const CGFloat kAttachCurrentTabIconRadius = 2.0f;

}  // namespace

@implementation ComposeboxMenuContextMenuBuilder {
  // The input state this builder is based on.
  ComposeboxUIInputState* _inputState;
}

#pragma mark - Public

- (instancetype)initWithInputState:(ComposeboxUIInputState*)inputState {
  self = [super init];
  if (self) {
    _inputState = inputState;
  }

  return self;
}

- (UIMenu*)createMenu {
  __weak __typeof(self) weakSelf = self;
  using enum ComposeboxAttachmentOption;
  UIAction* galleryAction = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_GALLERY_ACTION)
                image:SymbolWithPointSize(SymbolPhoto, kSymbolActionPointSize)
               hidden:[_inputState isAttachmentHidden:kGallery]
             disabled:[_inputState isAttachmentDisabled:kGallery]
             selected:NO
              handler:^{
                [weakSelf handleItemPickedWithType:ComposeboxMenuItemType::
                                                       kAttachmentGallery];
              }];
  galleryAction.accessibilityIdentifier =
      kComposeboxGalleryActionAccessibilityIdentifier;

  UIAction* cameraAction = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CAMERA_ACTION)
                image:SymbolWithPointSize(SymbolSystemCamera,
                                          kSymbolActionPointSize)
               hidden:[_inputState isAttachmentHidden:kCamera]
             disabled:[_inputState isAttachmentDisabled:kCamera]
             selected:NO
              handler:^{
                [weakSelf handleItemPickedWithType:ComposeboxMenuItemType::
                                                       kAttachmentCamera];
              }];
  cameraAction.accessibilityIdentifier =
      kComposeboxCameraActionAccessibilityIdentifier;

  UIAction* fileAction = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_FILES_ACTION)
                image:SymbolWithPointSize(SymbolDoc, kSymbolActionPointSize)
               hidden:[_inputState isAttachmentHidden:kFile]
             disabled:[_inputState isAttachmentDisabled:kFile]
             selected:NO
              handler:^{
                [weakSelf handleItemPickedWithType:ComposeboxMenuItemType::
                                                       kAttachmentFiles];
              }];

  fileAction.accessibilityIdentifier =
      kComposeboxAttachFileActionAccessibilityIdentifier;

  UIImage* favicon = _inputState.currentTabFavicon;
  if (favicon) {
    favicon = ImageWithCornerRadius(favicon, kAttachCurrentTabIconRadius);
  }
  UIAction* attachCurrentTabAction =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_COMPOSEBOX_ADD_CURRENT_TAB_ACTION)
                      image:favicon
                                ?: SymbolWithPointSize(SymbolNewTabGroupAction,
                                                       kSymbolActionPointSize)
                     hidden:[_inputState isAttachmentHidden:kCurrentTab]
                   disabled:[_inputState isAttachmentDisabled:kCurrentTab]
                   selected:NO
                    handler:^{
                      [weakSelf handleItemPickedWithType:
                                    ComposeboxMenuItemType::kCurrentTab];
                    }];
  attachCurrentTabAction.accessibilityIdentifier =
      kComposeboxAttachCurrentTabActionAccessibilityIdentifier;

  UIAction* selectTabsAction =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_COMPOSEBOX_SELECT_TAB_ACTION)
                      image:SymbolWithPointSize(SymbolNewTabGroupAction,
                                                kSymbolActionPointSize)
                     hidden:[_inputState isAttachmentHidden:kTab]
                   disabled:[_inputState isAttachmentDisabled:kTab]
                   selected:NO
                    handler:^{
                      [weakSelf handleItemPickedWithType:
                                    ComposeboxMenuItemType::kAttachmentTabs];
                    }];
  selectTabsAction.accessibilityIdentifier =
      kComposeboxSelectTabsActionAccessibilityIdentifier;

  UIAction* aimAction = [self
      actionWithTitle:[_inputState.uiConfig
                          menuLabelForTool:ComposeboxMode::kAIM]
                image:[_inputState.uiConfig iconForTool:ComposeboxMode::kAIM]
               hidden:[_inputState isToolHidden:ComposeboxMode::kAIM]
             disabled:NO
             selected:_inputState.activeTool == ComposeboxMode::kAIM
              handler:^{
                [weakSelf
                    handleItemPickedWithType:ComposeboxMenuItemType::kAIM];
              }];
  aimAction.accessibilityIdentifier =
      kComposeboxAIMActionAccessibilityIdentifier;

  UIAction* createImageAction = [self
      actionWithTitle:[_inputState.uiConfig
                          menuLabelForTool:ComposeboxMode::kImageGeneration]
                image:[_inputState.uiConfig
                          iconForTool:ComposeboxMode::kImageGeneration]
               hidden:[_inputState
                          isToolHidden:ComposeboxMode::kImageGeneration]
             disabled:[_inputState
                          isToolDisabled:ComposeboxMode::kImageGeneration]
             selected:_inputState.activeTool == ComposeboxMode::kImageGeneration
              handler:^{
                [weakSelf handleItemPickedWithType:ComposeboxMenuItemType::
                                                       kCreateImage];
              }];
  createImageAction.accessibilityIdentifier =
      kComposeboxImageGenerationActionAccessibilityIdentifier;

  UIAction* canvasAction = [self
      actionWithTitle:[_inputState.uiConfig
                          menuLabelForTool:ComposeboxMode::kCanvas]
                image:[_inputState.uiConfig iconForTool:ComposeboxMode::kCanvas]
               hidden:[_inputState isToolHidden:ComposeboxMode::kCanvas]
             disabled:[_inputState isToolDisabled:ComposeboxMode::kCanvas]
             selected:_inputState.activeTool == ComposeboxMode::kCanvas
              handler:^{
                [weakSelf
                    handleItemPickedWithType:ComposeboxMenuItemType::kCanvas];
              }];

  UIAction* deepSearchAction = [self
      actionWithTitle:[_inputState.uiConfig
                          menuLabelForTool:ComposeboxMode::kDeepSearch]
                image:[_inputState.uiConfig
                          iconForTool:ComposeboxMode::kDeepSearch]
               hidden:[_inputState isToolHidden:ComposeboxMode::kDeepSearch]
             disabled:[_inputState isToolDisabled:ComposeboxMode::kDeepSearch]
             selected:_inputState.activeTool == ComposeboxMode::kDeepSearch
              handler:^{
                [weakSelf handleItemPickedWithType:ComposeboxMenuItemType::
                                                       kDeepSearch];
              }];

  NSMutableArray<UIMenuElement*>* attachmentMenuElements =
      [[NSMutableArray alloc] init];
  [attachmentMenuElements addObjectsFromArray:@[
    attachCurrentTabAction, selectTabsAction, cameraAction, galleryAction,
    fileAction
  ]];

  if (IsComposeboxDriveOptionEnabled()) {
    UIImage* driveSymbol =
        SymbolWithPointSize(SymbolFolder, kSymbolActionPointSize);
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
    driveSymbol =
        SymbolWithPointSize(SymbolGoogleDrive, kSymbolActionPointSize);
#endif
    UIAction* driveAction = [self
        actionWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_DRIVE_ACTION)
                  image:driveSymbol
                 hidden:[_inputState isAttachmentHidden:kDrive]
               disabled:[_inputState isAttachmentDisabled:kDrive]
               selected:NO
                handler:^{
                  [weakSelf handleItemPickedWithType:ComposeboxMenuItemType::
                                                         kAttachmentDrive];
                }];
    [attachmentMenuElements addObject:driveAction];
  }

  UIMenu* attachmentMenu = [UIMenu menuWithTitle:@""
                                           image:nil
                                      identifier:nil
                                         options:UIMenuOptionsDisplayInline
                                        children:attachmentMenuElements];

  NSString* toolsSectionTitle = [_inputState.uiConfig toolsSectionHeader];
  UIMenu* modeMenu = [UIMenu
      menuWithTitle:toolsSectionTitle
              image:nil
         identifier:nil
            options:UIMenuOptionsDisplayInline
           children:@[
             aimAction, createImageAction, deepSearchAction, canvasAction
           ]];

  NSMutableArray<UIMenuElement*>* sections =
      [[NSMutableArray alloc] initWithArray:@[ attachmentMenu, modeMenu ]];
  if (_inputState.allowModelPicker) {
    BOOL regularHidden =
        [_inputState isModelHidden:ComposeboxModelOption::kRegular] ||
        ![_inputState isModelHidden:ComposeboxModelOption::kAuto];
    // Note: When possible, this is meant to be replaced by 'Auto'.
    UIAction* regularModelOption = [self
        actionWithTitle:[_inputState.uiConfig
                            menuLabelForModel:ComposeboxModelOption::kRegular]
                  image:[_inputState.uiConfig
                            iconForModel:ComposeboxModelOption::kRegular]
                 hidden:regularHidden
               disabled:[_inputState
                            isModelDisabled:ComposeboxModelOption::kRegular]
               selected:_inputState.activeModel ==
                        ComposeboxModelOption::kRegular
                handler:^{
                  [weakSelf handleItemPickedWithType:ComposeboxMenuItemType::
                                                         kModelRegular];
                }];

    UIAction* autoModelOption = [self
        actionWithTitle:[_inputState.uiConfig
                            menuLabelForModel:ComposeboxModelOption::kAuto]
                  image:[_inputState.uiConfig
                            iconForModel:ComposeboxModelOption::kAuto]
                 hidden:[_inputState isModelHidden:ComposeboxModelOption::kAuto]
               disabled:[_inputState
                            isModelDisabled:ComposeboxModelOption::kAuto]
               selected:_inputState.activeModel == ComposeboxModelOption::kAuto
                handler:^{
                  [weakSelf handleItemPickedWithType:ComposeboxMenuItemType::
                                                         kModelAuto];
                }];

    UIAction* thinkingModelOption = [self
        actionWithTitle:[_inputState.uiConfig
                            menuLabelForModel:ComposeboxModelOption::kThinking]
                  image:[_inputState.uiConfig
                            iconForModel:ComposeboxModelOption::kThinking]
                 hidden:[_inputState
                            isModelHidden:ComposeboxModelOption::kThinking]
               disabled:[_inputState
                            isModelDisabled:ComposeboxModelOption::kThinking]
               selected:_inputState.activeModel ==
                        ComposeboxModelOption::kThinking
                handler:^{
                  [weakSelf handleItemPickedWithType:ComposeboxMenuItemType::
                                                         kModelThinking];
                }];

    UIAction* thinkingModelNoGenUIOption = [self
        actionWithTitle:
            [_inputState.uiConfig
                menuLabelForModel:ComposeboxModelOption::kThinkingNoGenUI]
                  image:
                      [_inputState.uiConfig
                          iconForModel:ComposeboxModelOption::kThinkingNoGenUI]
                 hidden:[_inputState isModelHidden:ComposeboxModelOption::
                                                       kThinkingNoGenUI]
               disabled:[_inputState isModelDisabled:ComposeboxModelOption::
                                                         kThinkingNoGenUI]
               selected:_inputState.activeModel ==
                        ComposeboxModelOption::kThinkingNoGenUI
                handler:^{
                  [weakSelf handleItemPickedWithType:ComposeboxMenuItemType::
                                                         kModelThinkingNoGenUI];
                }];

    UIAction* flashModelOption = [self
        actionWithTitle:[_inputState.uiConfig
                            menuLabelForModel:ComposeboxModelOption::kFlash]
                  image:[_inputState.uiConfig
                            iconForModel:ComposeboxModelOption::kFlash]
                 hidden:[_inputState
                            isModelHidden:ComposeboxModelOption::kFlash]
               disabled:[_inputState
                            isModelDisabled:ComposeboxModelOption::kFlash]
               selected:_inputState.activeModel == ComposeboxModelOption::kFlash
                handler:^{
                  [weakSelf handleItemPickedWithType:ComposeboxMenuItemType::
                                                         kModelFlash];
                }];

    NSString* modelPickerTitle = [_inputState.uiConfig modelSectionHeader];
    UIMenu* modelPickerMenu =
        [UIMenu menuWithTitle:modelPickerTitle
                        image:nil
                   identifier:nil
                      options:UIMenuOptionsDisplayInline
                     children:@[
                       regularModelOption, autoModelOption, thinkingModelOption,
                       thinkingModelNoGenUIOption, flashModelOption
                     ]];

    [sections addObject:modelPickerMenu];
  }

  return [UIMenu menuWithTitle:@"" children:sections];
}

#pragma mark - Private

- (void)handleItemPickedWithType:(ComposeboxMenuItemType)itemType {
  // Ignore taps on already selected models.
  if (_inputState.activeModel != ComposeboxModelOption::kNone &&
      itemType == MenuItemTypeForModel(_inputState.activeModel)) {
    return;
  }

  [self.mutator handleItemPickedWithType:itemType];
}

// Creates a new UIAction based on the given configuration.
- (UIAction*)actionWithTitle:(NSString*)title
                       image:(UIImage*)image
                      hidden:(BOOL)hidden
                    disabled:(BOOL)disabled
                    selected:(BOOL)selected
                     handler:(void (^)(void))handler {
  UIAction* action = [UIAction actionWithTitle:[title copy]
                                         image:image
                                    identifier:nil
                                       handler:^(UIAction*) {
                                         if (handler) {
                                           handler();
                                         }
                                       }];

  if (hidden) {
    action.attributes |= UIMenuElementAttributesHidden;
  }
  if (disabled) {
    action.attributes |= UIMenuElementAttributesDisabled;
  }
  if (selected) {
    [action setState:UIMenuElementStateOn];
  }

  return action;
}

@end
