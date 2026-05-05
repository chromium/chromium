// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"

static BOOL gAllToolsEnabled = NO;

@implementation ComposeboxUIInputState

+ (BOOL)allToolsEnabled {
  return gAllToolsEnabled;
}

+ (void)setAllToolsEnabled:(BOOL)enabled {
  gAllToolsEnabled = enabled;
}

- (BOOL)isAttachmentHidden:(ComposeboxAttachmentOption)option {
  if (gAllToolsEnabled) {
    return NO;
  }
  return !_allowedAttachments.contains(option);
}

- (BOOL)isAttachmentDisabled:(ComposeboxAttachmentOption)option {
  if (gAllToolsEnabled) {
    return NO;
  }
  return _disabledAttachments.contains(option);
}

- (BOOL)isAttachmentAvailable:(ComposeboxAttachmentOption)option {
  if (gAllToolsEnabled) {
    return YES;
  }
  return _allowedAttachments.contains(option) &&
         !_disabledAttachments.contains(option);
}

- (BOOL)isToolHidden:(ComposeboxMode)option {
  if (gAllToolsEnabled) {
    return NO;
  }
  return !_allowedTools.contains(option);
}

- (BOOL)isToolDisabled:(ComposeboxMode)option {
  if (gAllToolsEnabled) {
    return NO;
  }
  return _disabledTools.contains(option);
}

- (BOOL)isToolAvailable:(ComposeboxMode)option {
  if (gAllToolsEnabled) {
    return YES;
  }
  return _allowedTools.contains(option) && !_disabledTools.contains(option);
}

- (BOOL)isModelHidden:(ComposeboxModelOption)option {
  if (gAllToolsEnabled) {
    return NO;
  }
  return !_allowedModels.contains(option);
}

- (BOOL)isModelDisabled:(ComposeboxModelOption)option {
  if (gAllToolsEnabled) {
    return NO;
  }
  return _disabledModels.contains(option);
}

- (BOOL)isModelAvailable:(ComposeboxModelOption)option {
  if (gAllToolsEnabled) {
    return YES;
  }
  return _allowedModels.contains(option) && !_disabledModels.contains(option);
}

- (std::unordered_set<ComposeboxAttachmentOption>)allowedAttachments {
  if (gAllToolsEnabled) {
    std::unordered_set<ComposeboxAttachmentOption> attachments;
    for (ComposeboxAttachmentOption opt :
         ComposeboxAttachmentOptionSet::All()) {
      attachments.insert(opt);
    }
    return attachments;
  }
  return _allowedAttachments;
}

- (std::unordered_set<ComposeboxAttachmentOption>)disabledAttachments {
  if (gAllToolsEnabled) {
    return {};
  }
  return _disabledAttachments;
}

- (std::unordered_set<ComposeboxMode>)allowedTools {
  if (gAllToolsEnabled) {
    std::unordered_set<ComposeboxMode> tools;
    for (ComposeboxMode opt : ComposeboxModeSet::All()) {
      tools.insert(opt);
    }
    return tools;
  }
  return _allowedTools;
}

- (std::unordered_set<ComposeboxMode>)disabledTools {
  if (gAllToolsEnabled) {
    return {};
  }
  return _disabledTools;
}

- (std::unordered_set<ComposeboxModelOption>)allowedModels {
  if (gAllToolsEnabled) {
    std::unordered_set<ComposeboxModelOption> models;
    for (ComposeboxModelOption opt : ComposeboxModelOptionSet::All()) {
      models.insert(opt);
    }
    return models;
  }
  return _allowedModels;
}

- (std::unordered_set<ComposeboxModelOption>)disabledModels {
  if (gAllToolsEnabled) {
    return {};
  }
  return _disabledModels;
}

- (BOOL)allowModelPicker {
  if (gAllToolsEnabled) {
    return YES;
  }
  return _allowModelPicker;
}

@end
