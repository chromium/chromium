// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_context_menu_builder.h"

#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_option.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/composebox/ui/composebox_model_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_config.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// The corner radius of the favicon in attach current tab action.
const CGFloat kAttachCurrentTabIconRadius = 2.0f;

// Helper struct storing the availability of the given menu item.
struct MenuItemAvailability {
  bool disabled;
  bool hidden;
  bool selected;

  MenuItemAvailability(ComposeboxUIInputState* input_state,
                       ComposeboxMode tool) {
    this->disabled = [input_state isToolDisabled:tool];
    this->hidden = [input_state isToolHidden:tool];
    this->selected = input_state.activeTool == tool;
  }

  MenuItemAvailability(ComposeboxUIInputState* input_state,
                       ComposeboxAttachmentOption attachment) {
    this->disabled = [input_state isAttachmentDisabled:attachment];
    this->hidden = [input_state isAttachmentHidden:attachment];
    this->selected = false;
  }

  MenuItemAvailability() : disabled(true), hidden(true), selected(false) {}
};

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
  using enum ComposeboxMenuItemType;

  NSMutableArray<UIMenuElement*>* sections = [[NSMutableArray alloc] init];

  if (IsPlusButtonMenuMoreOptionsSubmenu()) {
    UIMenu* attachmentMenu = [self
        inlineMenuFromItemTypes:{kCurrentTab, kAttachmentGallery,
                                 kAttachmentCamera, kAttachmentTabs}
                      withTitle:_inputState.uiConfig.attachmentsSectionHeader];
    UIMenu* aimMenu = [self inlineMenuFromItemTypes:{kAIM}];
    UIMenu* moreOptions = [self createMoreOptionsSubmenu];

    if (attachmentMenu) {
      [sections addObject:attachmentMenu];
    }
    if (aimMenu) {
      [sections addObject:aimMenu];
    }
    if (moreOptions) {
      [sections addObject:moreOptions];
    }
  } else {
    UIMenu* attachmentMenu =
        [self inlineMenuFromItemTypes:{kCurrentTab, kAttachmentTabs,
                                       kAttachmentCamera, kAttachmentGallery,
                                       kAttachmentFiles, kAttachmentDrive}];
    UIMenu* modeMenu = [self
        inlineMenuFromItemTypes:{kAIM, kCreateImage, kDeepSearch, kCanvas}
                      withTitle:[_inputState.uiConfig toolsSectionHeader]];
    UIMenu* modelPickerMenu = [self createModelPickerMenu];

    if (attachmentMenu) {
      [sections addObject:attachmentMenu];
    }
    if (modeMenu) {
      [sections addObject:modeMenu];
    }
    if (modelPickerMenu) {
      [sections addObject:modelPickerMenu];
    }
  }
  return [UIMenu menuWithTitle:@"" children:sections];
}

#pragma mark - Menu creation

/// Creates the more options submenu.
- (UIMenu*)createMoreOptionsSubmenu {
  using enum ComposeboxMenuItemType;

  UIMenu* attachmentSubmenu =
      [self inlineMenuFromItemTypes:{kAttachmentFiles, kAttachmentDrive}];
  UIMenu* modeSubmenu =
      [self inlineMenuFromItemTypes:{kCreateImage, kDeepSearch, kCanvas}];
  UIMenu* modelPickerSubmenu = [self createModelPickerMenu];

  NSMutableArray<UIMenuElement*>* submenuSections =
      [[NSMutableArray alloc] init];
  if (attachmentSubmenu) {
    [submenuSections addObject:attachmentSubmenu];
  }
  if (modeSubmenu) {
    [submenuSections addObject:modeSubmenu];
  }
  if (modelPickerSubmenu) {
    [submenuSections addObject:modelPickerSubmenu];
  }

  if ([submenuSections count] == 0) {
    return nil;
  }

  return [UIMenu menuWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_COMPOSEBOX_MORE_OPTIONS_MENU_TITLE)
                         image:SymbolWithPointSize(SymbolEllipsisCircle,
                                                   kSymbolActionPointSize)
                    identifier:nil
                       options:0
                      children:submenuSections];
}

// Creates a new inline menu without a title.
- (UIMenu*)inlineMenuFromItemTypes:
    (std::vector<ComposeboxMenuItemType>)itemTypes {
  return [self inlineMenuFromItemTypes:itemTypes withTitle:@""];
}

// Creates a new inline menu with a title.
- (UIMenu*)inlineMenuFromItemTypes:
               (std::vector<ComposeboxMenuItemType>)itemTypes
                         withTitle:(NSString*)title {
  NSArray<UIAction*>* actions = [self actionsFromItemTypes:itemTypes];
  if ([actions count] == 0) {
    return nil;
  }
  return [UIMenu menuWithTitle:[title copy]
                         image:nil
                    identifier:nil
                       options:UIMenuOptionsDisplayInline
                      children:actions];
}

// Creates a list of actions from the given menu types.
// None of the actions in the returned list is `nil`.
- (NSArray<UIAction*>*)actionsFromItemTypes:
    (std::vector<ComposeboxMenuItemType>)itemTypes {
  NSMutableArray<UIAction*>* actions = [[NSMutableArray alloc] init];
  for (ComposeboxMenuItemType itemType : itemTypes) {
    if (UIAction* action = [self actionForMenuItem:itemType]) {
      [actions addObject:action];
    }
  }
  return actions;
}

// Creates the menu component for the model picker section.
- (UIMenu*)createModelPickerMenu {
  if (!_inputState.allowModelPicker) {
    return nil;
  }

  NSMutableArray<UIAction*>* actions = [[NSMutableArray alloc] init];
  for (ComposeboxModelItem* item in _inputState.modelItems) {
    [actions addObject:[self actionForModelItem:item]];
  }
  if ([actions count] == 0) {
    return nil;
  }
  return [UIMenu menuWithTitle:[_inputState.uiConfig modelSectionHeader]
                         image:nil
                    identifier:nil
                       options:UIMenuOptionsDisplayInline
                      children:actions];
}

#pragma mark - Private

- (void)handleItemPickedWithType:(ComposeboxMenuItemType)itemType {
  [self.mutator handleItemPickedWithType:itemType];
}

- (void)handleModelItemPicked:(ComposeboxModelItem*)item {
  // Ignore taps on the already selected model.
  if (item.selected) {
    return;
  }
  [self.mutator selectModelItem:item];
}

// Creates a new UIAction based on the given configuration.
- (UIAction*)actionForMenuItem:(ComposeboxMenuItemType)menuItemType {
  MenuItemAvailability availability =
      [self availabilityStatusForMenuItem:menuItemType];
  if (availability.hidden) {
    return nil;
  }

  __weak __typeof(self) weakSelf = self;
  UIAction* action =
      [UIAction actionWithTitle:[self titleForMenuItem:menuItemType]
                          image:[self imageForMenuItem:menuItemType]
                     identifier:nil
                        handler:^(UIAction*) {
                          [weakSelf handleItemPickedWithType:menuItemType];
                        }];
  action.subtitle = [self subtitleForMenuItem:menuItemType];
  action.accessibilityIdentifier =
      AccessibilityIdentifierForMenuItemType(menuItemType);
  if (availability.disabled) {
    action.attributes |= UIMenuElementAttributesDisabled;
  }
  if (availability.selected) {
    [action setState:UIMenuElementStateOn];
  }

  return action;
}

// Creates a new `UIAction` for the given model `item`.
- (UIAction*)actionForModelItem:(ComposeboxModelItem*)item {
  __weak __typeof(self) weakSelf = self;
  UIAction* action = [UIAction actionWithTitle:item.title ?: @""
                                         image:item.icon
                                    identifier:nil
                                       handler:^(UIAction*) {
                                         [weakSelf handleModelItemPicked:item];
                                       }];
  if (item.disabled) {
    action.attributes |= UIMenuElementAttributesDisabled;
  }
  if (item.selected) {
    [action setState:UIMenuElementStateOn];
  }
  return action;
}

#pragma mark - Options customization

// Returns the image for the menu type.
- (UIImage*)imageForMenuItem:(ComposeboxMenuItemType)menuItemType {
  switch (menuItemType) {
    case ComposeboxMenuItemType::kAIM:
      return [_inputState.uiConfig iconForTool:ComposeboxMode::kAIM];
    case ComposeboxMenuItemType::kCreateImage:
      return
          [_inputState.uiConfig iconForTool:ComposeboxMode::kImageGeneration];
    case ComposeboxMenuItemType::kDeepSearch:
      return [_inputState.uiConfig iconForTool:ComposeboxMode::kDeepSearch];
    case ComposeboxMenuItemType::kCanvas:
      return [_inputState.uiConfig iconForTool:ComposeboxMode::kCanvas];
    case ComposeboxMenuItemType::kCurrentTab: {
      UIImage* favicon = _inputState.currentTabFavicon;
      if (favicon) {
        favicon = ImageWithCornerRadius(favicon, kAttachCurrentTabIconRadius);
      }
      return favicon
                 ?: SymbolWithPointSize(SymbolNewTabGroupAction,
                                        kSymbolActionPointSize);
    }
    case ComposeboxMenuItemType::kAttachmentTabs:
      return SymbolWithPointSize(SymbolNewTabGroupAction,
                                 kSymbolActionPointSize);
    case ComposeboxMenuItemType::kAttachmentCamera:
      return SymbolWithPointSize(SymbolSystemCamera, kSymbolActionPointSize);
    case ComposeboxMenuItemType::kAttachmentGallery:
      return IsPlusButtonMenuStringUpdatesEnabled()
                 ? SymbolWithPointSize(SymbolPhotoBadgePlus,
                                       kSymbolActionPointSize)
                 : SymbolWithPointSize(SymbolPhoto, kSymbolActionPointSize);
    case ComposeboxMenuItemType::kAttachmentFiles:
      return SymbolWithPointSize(SymbolDoc, kSymbolActionPointSize);
    case ComposeboxMenuItemType::kAttachmentDrive: {
      UIImage* driveSymbol =
          SymbolWithPointSize(SymbolFolder, kSymbolActionPointSize);
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
      driveSymbol =
          SymbolWithPointSize(SymbolGoogleDrive, kSymbolActionPointSize);
#endif

      return driveSymbol;
    }
    // Model actions are created from `ComposeboxModelItem`s.
    case ComposeboxMenuItemType::kModel:
    case ComposeboxMenuItemType::kAttachmentSharedTabs:
    case ComposeboxMenuItemType::kMoreOptionsSubmenu:
    case ComposeboxMenuItemType::kUnknown:
      return nil;
  }
}

- (NSString*)titleForMenuItem:(ComposeboxMenuItemType)menuItemType {
  switch (menuItemType) {
    case ComposeboxMenuItemType::kAIM:
      return [_inputState.uiConfig menuLabelForTool:ComposeboxMode::kAIM];
    case ComposeboxMenuItemType::kCreateImage:
      return [_inputState.uiConfig
          menuLabelForTool:ComposeboxMode::kImageGeneration];
    case ComposeboxMenuItemType::kDeepSearch:
      return
          [_inputState.uiConfig menuLabelForTool:ComposeboxMode::kDeepSearch];
    case ComposeboxMenuItemType::kCanvas:
      return [_inputState.uiConfig menuLabelForTool:ComposeboxMode::kCanvas];
    case ComposeboxMenuItemType::kCurrentTab:
      return IsPlusButtonMenuStringUpdatesEnabled()
                 ? l10n_util::GetNSString(
                       IDS_IOS_COMPOSEBOX_ASK_ABOUT_THIS_PAGE_ACTION)
                 : l10n_util::GetNSString(
                       IDS_IOS_COMPOSEBOX_ADD_CURRENT_TAB_ACTION);
    case ComposeboxMenuItemType::kAttachmentTabs:
      return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_SELECT_TAB_ACTION);
    case ComposeboxMenuItemType::kAttachmentCamera:
      return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CAMERA_ACTION);
    case ComposeboxMenuItemType::kAttachmentGallery:
      return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_GALLERY_ACTION);
    case ComposeboxMenuItemType::kAttachmentFiles:
      return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_FILES_ACTION);
    case ComposeboxMenuItemType::kAttachmentDrive:
      return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_DRIVE_ACTION);
    // Model actions are created from `ComposeboxModelItem`s.
    case ComposeboxMenuItemType::kModel:
    case ComposeboxMenuItemType::kAttachmentSharedTabs:
    case ComposeboxMenuItemType::kMoreOptionsSubmenu:
    case ComposeboxMenuItemType::kUnknown:
      return nil;
  }
}

// Returns whether the given menu item is hidden.
- (MenuItemAvailability)availabilityStatusForMenuItem:
    (ComposeboxMenuItemType)menuItemType {
  switch (menuItemType) {
    case ComposeboxMenuItemType::kAttachmentGallery:
      return MenuItemAvailability(_inputState,
                                  ComposeboxAttachmentOption::kGallery);
    case ComposeboxMenuItemType::kAttachmentCamera:
      return MenuItemAvailability(_inputState,
                                  ComposeboxAttachmentOption::kCamera);
    case ComposeboxMenuItemType::kAttachmentFiles:
      return MenuItemAvailability(_inputState,
                                  ComposeboxAttachmentOption::kFile);
    case ComposeboxMenuItemType::kCurrentTab:
      return MenuItemAvailability(_inputState,
                                  ComposeboxAttachmentOption::kCurrentTab);
    case ComposeboxMenuItemType::kAttachmentTabs:
      return MenuItemAvailability(_inputState,
                                  ComposeboxAttachmentOption::kTab);
    case ComposeboxMenuItemType::kAttachmentDrive: {
      MenuItemAvailability availability(_inputState,
                                        ComposeboxAttachmentOption::kDrive);
      availability.hidden |= !IsComposeboxDriveOptionEnabled();
      return availability;
    }
    case ComposeboxMenuItemType::kAIM: {
      MenuItemAvailability availability(_inputState, ComposeboxMode::kAIM);
      availability.disabled = NO;
      return availability;
    }
    case ComposeboxMenuItemType::kCreateImage:
      return MenuItemAvailability(_inputState,
                                  ComposeboxMode::kImageGeneration);
    case ComposeboxMenuItemType::kDeepSearch:
      return MenuItemAvailability(_inputState, ComposeboxMode::kDeepSearch);
    case ComposeboxMenuItemType::kCanvas:
      return MenuItemAvailability(_inputState, ComposeboxMode::kCanvas);
    // Model actions are created from `ComposeboxModelItem`s.
    case ComposeboxMenuItemType::kModel:
    case ComposeboxMenuItemType::kUnknown:
    case ComposeboxMenuItemType::kMoreOptionsSubmenu:
    case ComposeboxMenuItemType::kAttachmentSharedTabs:
      return MenuItemAvailability();
  }
}

// Returns the optional subtitle of the menu item.
- (NSString*)subtitleForMenuItem:(ComposeboxMenuItemType)menuItemType {
  switch (menuItemType) {
    case ComposeboxMenuItemType::kAIM:
      return [_inputState.uiConfig menuSubtitleForTool:ComposeboxMode::kAIM];
    case ComposeboxMenuItemType::kCreateImage:
      return [_inputState.uiConfig
          menuSubtitleForTool:ComposeboxMode::kImageGeneration];
    case ComposeboxMenuItemType::kCanvas:
      return [_inputState.uiConfig menuSubtitleForTool:ComposeboxMode::kCanvas];
    default:
      return nil;
  }
}

@end
