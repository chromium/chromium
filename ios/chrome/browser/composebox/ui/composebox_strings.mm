// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_strings.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation ComposeboxStringBundle

- (instancetype)initWithMenuLabel:(NSString*)menuLabel
                        chipLabel:(NSString*)chipLabel
                         hintText:(NSString*)hintText {
  self = [super init];
  if (self) {
    _menuLabel = [menuLabel copy];
    _chipLabel = [chipLabel copy];
    _hintText = [hintText copy];
  }

  return self;
}

@end

@implementation ComposeboxStrings {
  // Maps tools to their server-provided strings.
  std::unordered_map<ComposeboxMode, ComposeboxStringBundle*> _controlMapping;
  // Maps models to their server-provided strings.
  std::unordered_map<ComposeboxModelOption, ComposeboxStringBundle*>
      _modelMapping;
  // The server-provided header for the models section.
  NSString* _modelSectionHeader;
  // The server-provided header for the tools section.
  NSString* _toolsSectionHeader;
}

+ (instancetype)localFallbackStrings {
  return [[ComposeboxStrings alloc] initWithToolMapping:{}
                                           modelMapping:{}
                                     modelSectionHeader:nil
                                     toolsSectionHeader:nil];
}

- (instancetype)
    initWithToolMapping:
        (std::unordered_map<ComposeboxMode, ComposeboxStringBundle*>)
            controlMapping
           modelMapping:
               (std::unordered_map<ComposeboxModelOption,
                                   ComposeboxStringBundle*>)modelMapping
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
  ComposeboxStringBundle* bundle = [self stringsForTool:tool];
  if (bundle.menuLabel.length > 0) {
    return bundle.menuLabel;
  }
  return [self localFallbackForTool:tool isHint:NO];
}

- (NSString*)chipLabelForTool:(ComposeboxMode)tool {
  ComposeboxStringBundle* bundle = [self stringsForTool:tool];
  if (bundle.chipLabel.length > 0) {
    return bundle.chipLabel;
  }
  return [self localFallbackForTool:tool isHint:NO];
}

- (NSString*)hintTextForTool:(ComposeboxMode)tool {
  ComposeboxStringBundle* bundle = [self stringsForTool:tool];
  if (bundle.hintText.length > 0) {
    return bundle.hintText;
  }
  return [self localFallbackForTool:tool isHint:YES];
}

- (NSString*)menuLabelForModel:(ComposeboxModelOption)model {
  ComposeboxStringBundle* bundle = [self stringsForModel:model];
  if (bundle.menuLabel.length > 0) {
    return bundle.menuLabel;
  }
  return [self localFallbackForModel:model isHint:NO];
}

- (NSString*)hintTextForModel:(ComposeboxModelOption)model {
  ComposeboxStringBundle* bundle = [self stringsForModel:model];
  if (bundle.hintText.length > 0) {
    return bundle.hintText;
  }
  return [self localFallbackForModel:model isHint:YES];
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

// Returns the server strings for the given tool, if available.
- (ComposeboxStringBundle*)stringsForTool:(ComposeboxMode)tool {
  if (tool == ComposeboxMode::kRegularSearch) {
    // Don't use server strings for regular search.
    return nil;
  }
  auto it = _controlMapping.find(tool);
  if (it != _controlMapping.end()) {
    return it->second;
  }
  return nil;
}

// Returns the server strings for the given model, if available.
- (ComposeboxStringBundle*)stringsForModel:(ComposeboxModelOption)modelOption {
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
  }
}

@end
