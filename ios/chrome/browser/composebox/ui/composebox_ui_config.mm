// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_ui_config.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation ComposeboxItemUIConfig

- (instancetype)initWithMenuLabel:(NSString*)menuLabel
                        chipLabel:(NSString*)chipLabel
                         hintText:(NSString*)hintText {
  return [self initWithMenuLabel:menuLabel
                       chipLabel:chipLabel
                        hintText:hintText
                            icon:nil];
}

- (instancetype)initWithMenuLabel:(NSString*)menuLabel
                        chipLabel:(NSString*)chipLabel
                         hintText:(NSString*)hintText
                             icon:(UIImage*)icon {
  self = [super init];
  if (self) {
    _menuLabel = [menuLabel copy];
    _chipLabel = [chipLabel copy];
    _hintText = [hintText copy];
    _icon = icon;
  }

  return self;
}

@end

@implementation ComposeboxUIConfig {
  // Maps tools to their server-provided configs.
  std::unordered_map<ComposeboxMode, ComposeboxItemUIConfig*> _controlMapping;
  // Maps models to their server-provided configs.
  std::unordered_map<ComposeboxModelOption, ComposeboxItemUIConfig*>
      _modelMapping;
  // The server-provided header for the models section.
  NSString* _modelSectionHeader;
  // The server-provided header for the tools section.
  NSString* _toolsSectionHeader;
}

+ (instancetype)localFallbackUIConfig {
  return [[ComposeboxUIConfig alloc] initWithToolMapping:{}
                                            modelMapping:{}
                                      modelSectionHeader:nil
                                      toolsSectionHeader:nil];
}

- (instancetype)
    initWithToolMapping:
        (std::unordered_map<ComposeboxMode, ComposeboxItemUIConfig*>)
            controlMapping
           modelMapping:
               (std::unordered_map<ComposeboxModelOption,
                                   ComposeboxItemUIConfig*>)modelMapping
     modelSectionHeader:(NSString*)modelSectionHeader
     toolsSectionHeader:(NSString*)toolsSectionHeader {
  self = [super init];
  if (self) {
    _controlMapping = controlMapping;
    _modelMapping = modelMapping;
    _modelSectionHeader = [modelSectionHeader copy];
    _toolsSectionHeader = [toolsSectionHeader copy];
  }

  return self;
}

- (NSString*)menuLabelForTool:(ComposeboxMode)tool {
  ComposeboxItemUIConfig* config = [self uiConfigForTool:tool];
  if (config.menuLabel.length > 0) {
    return config.menuLabel;
  }
  return [self localFallbackForTool:tool isHint:NO];
}

- (NSString*)chipLabelForTool:(ComposeboxMode)tool {
  ComposeboxItemUIConfig* config = [self uiConfigForTool:tool];
  if (config.chipLabel.length > 0) {
    return config.chipLabel;
  }
  return [self localFallbackForTool:tool isHint:NO];
}

- (NSString*)removeToolAccessibilityLabelForTool:(ComposeboxMode)tool {
  NSString* chipLabel = [self chipLabelForTool:tool];

  DCHECK(chipLabel.length);
  if (!chipLabel.length) {
    return @"";
  }
  return l10n_util::GetNSStringF(
      IDS_IOS_COMPOSEBOX_REMOVE_TOOL_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(chipLabel));
}

- (NSString*)hintTextForTool:(ComposeboxMode)tool {
  ComposeboxItemUIConfig* config = [self uiConfigForTool:tool];
  if (config.hintText.length > 0) {
    return config.hintText;
  }
  return [self localFallbackForTool:tool isHint:YES];
}

- (UIImage*)iconForTool:(ComposeboxMode)tool {
  ComposeboxItemUIConfig* config = [self uiConfigForTool:tool];
  if (config.icon) {
    return config.icon;
  }
  return [self localFallbackIconForTool:tool];
}

- (NSString*)menuLabelForModel:(ComposeboxModelOption)model {
  ComposeboxItemUIConfig* config = [self configForModel:model];
  if (config.menuLabel.length > 0) {
    return config.menuLabel;
  }
  return [self localFallbackForModel:model isHint:NO];
}

- (NSString*)hintTextForModel:(ComposeboxModelOption)model {
  ComposeboxItemUIConfig* config = [self configForModel:model];
  if (config.hintText.length > 0) {
    return config.hintText;
  }
  return [self localFallbackForModel:model isHint:YES];
}

- (UIImage*)iconForModel:(ComposeboxModelOption)model {
  ComposeboxItemUIConfig* config = [self configForModel:model];
  if (config.icon) {
    return config.icon;
  }
  return [self localFallbackIconForModel:model];
}

- (NSString*)stringForAttachmentOption:(ComposeboxAttachmentOption)option {
  using enum ComposeboxAttachmentOption;
  switch (option) {
    case kCurrentTab:
      return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_ADD_CURRENT_TAB_ACTION);
    case kTab:
      return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_SELECT_TAB_ACTION);
    case kFile:
      return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_FILES_ACTION);
    case kGallery:
      return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_GALLERY_ACTION);
    case kCamera:
      return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CAMERA_ACTION);
    case kDrive:
      return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_DRIVE_ACTION);
  }
}

- (NSString*)modelSectionHeader {
  if (_modelSectionHeader.length > 0) {
    return _modelSectionHeader;
  }
  return l10n_util::GetNSStringF(IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_TITLE, u"3");
}

- (NSString*)toolsSectionHeader {
  if (_toolsSectionHeader.length > 0) {
    return _toolsSectionHeader;
  }
  return l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MENU_TOOLS_SECTION_TITLE);
}

#pragma mark - Private

// Returns the server config for the given tool, if available.
- (ComposeboxItemUIConfig*)uiConfigForTool:(ComposeboxMode)tool {
  if (tool == ComposeboxMode::kRegularSearch) {
    // Don't use server configs for regular search.
    return nil;
  }
  auto it = _controlMapping.find(tool);
  if (it != _controlMapping.end()) {
    return it->second;
  }
  return nil;
}

// Returns the server config for the given model, if available.
- (ComposeboxItemUIConfig*)configForModel:(ComposeboxModelOption)modelOption {
  auto it = _modelMapping.find(modelOption);
  if (it != _modelMapping.end()) {
    return it->second;
  }
  return nil;
}

// Returns the local fallback string for the given tool.
- (NSString*)localFallbackForTool:(ComposeboxMode)tool isHint:(BOOL)isHint {
  using enum ComposeboxMode;
  switch (tool) {
    case kAIM:
      return isHint ? l10n_util::GetNSString(
                          IDS_IOS_COMPOSEBOX_AIM_ENABLED_PLACEHOLDER)
                    : l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_AIM_ACTION);
    case kImageGeneration:
      return isHint ? l10n_util::GetNSString(
                          IDS_IOS_COMPOSEBOX_IMAGE_GEN_PLACEHOLDER)
                    : l10n_util::GetNSString(
                          IDS_IOS_COMPOSEBOX_CREATE_IMAGE_ACTION);
    case kCanvas:
      return isHint ? l10n_util::GetNSString(
                          IDS_IOS_COMPOSEBOX_CANVAS_ENABLED_PLACEHOLDER)
                    : l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANVAS_ACTION);
    case kDeepSearch:
      return isHint ? l10n_util::GetNSString(
                          IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ENABLED_PLACEHOLDER)
                    : l10n_util::GetNSString(
                          IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ACTION);
    case kRegularSearch:
      return nil;
    default:
      return @"";
  }
}

// Returns the local fallback icon for the given tool.
- (UIImage*)localFallbackIconForTool:(ComposeboxMode)tool {
  using enum ComposeboxMode;
  switch (tool) {
    case kAIM:
      return SymbolWithPointSize(SymbolMagnifyingglassSpark,
                                 kSymbolActionPointSize);
    case kImageGeneration:
      return GetBananaIcon(kSymbolActionPointSize);
    case kDeepSearch:
      return SymbolWithPointSize(SymbolDeepSearch, kSymbolActionPointSize);
    case kCanvas:
      return SymbolWithPointSize(SymbolDocumentBadgeSpark,
                                 kSymbolActionPointSize);
    case kRegularSearch:
      return nil;
    default:
      return SymbolWithPointSize(SymbolMagnifyingglassSpark,
                                 kSymbolActionPointSize);
  }
}

// Returns the local fallback string for the given model.
- (NSString*)localFallbackForModel:(ComposeboxModelOption)model
                            isHint:(BOOL)isHint {
  using enum ComposeboxModelOption;
  switch (model) {
    case kNone:
      return nil;
    case kRegular:
      return l10n_util::GetNSString(
          IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_AUTO);
    case kAuto:
      return l10n_util::GetNSString(
          IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_AUTO);
    case kThinking:
    case kThinkingNoGenUI:
      return l10n_util::GetNSString(
          IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_THINKING);
    case kFlash:
      return l10n_util::GetNSString(
          IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_FLASH);
    default:
      return @"";
  }
}

// Returns the local fallback icon for the given model.
- (UIImage*)localFallbackIconForModel:(ComposeboxModelOption)model {
  using enum ComposeboxModelOption;
  switch (model) {
    case kNone:
      return nil;
    case kRegular:
    case kFlash:
      return SymbolWithPointSize(SymbolBolt, kSymbolActionPointSize);
    case kAuto:
      return SymbolWithPointSize(SymbolArrowTrianglehead2ClockwiseRotate90,
                                 kSymbolActionPointSize);
    case kThinking:
    case kThinkingNoGenUI:
      return SymbolWithPointSize(SymbolClock, kSymbolActionPointSize);
    default:
      return SymbolWithPointSize(SymbolBolt, kSymbolActionPointSize);
  }
}

@end
