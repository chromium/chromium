// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_server_strings.h"

@implementation ComposeboxServerStringBundle

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

@implementation ComposeboxServerStrings {
  std::unordered_map<ComposeboxInputPlateControls,
                     ComposeboxServerStringBundle*>
      _controlMapping;
  std::unordered_map<ComposeboxModelOption, ComposeboxServerStringBundle*>
      _modelMapping;
}

- (instancetype)
    initWithToolMapping:
        (std::unordered_map<ComposeboxInputPlateControls,
                            ComposeboxServerStringBundle*>)controlMapping
           modelMapping:
               (std::unordered_map<ComposeboxModelOption,
                                   ComposeboxServerStringBundle*>)modelMapping
     modelSectionHeader:(NSString*)modelSectionHeader
     toolsSectionHeader:(NSString*)toolsSectionHeader {
  self = [super init];
  if (self) {
    _controlMapping = controlMapping;
    _modelMapping = modelMapping;
    _modelSectionHeader = modelSectionHeader;
    _toolsSectionHeader = toolsSectionHeader;
  }

  return self;
}

- (ComposeboxServerStringBundle*)stringsForControl:
    (ComposeboxInputPlateControls)control {
  if (_controlMapping.find(control) != _controlMapping.end()) {
    return _controlMapping[control];
  }
  return nil;
}

- (ComposeboxServerStringBundle*)stringsForModel:
    (ComposeboxModelOption)modelOption {
  if (_modelMapping.find(modelOption) != _modelMapping.end()) {
    return _modelMapping[modelOption];
  }

  return nil;
}

@end
