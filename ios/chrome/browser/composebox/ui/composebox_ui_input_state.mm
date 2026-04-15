// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"

@implementation ComposeboxUIInputState

- (BOOL)isAttachmentHidden:(ComposeboxAttachmentOption)option {
  return !_allowedAttachments.contains(option);
}

- (BOOL)isAttachmentDisabled:(ComposeboxAttachmentOption)option {
  return _disabledAttachments.contains(option);
}

- (BOOL)isAttachmentAvailable:(ComposeboxAttachmentOption)option {
  return _allowedAttachments.contains(option) &&
         !_disabledAttachments.contains(option);
}

- (BOOL)isToolHidden:(ComposeboxMode)option {
  return !_allowedTools.contains(option);
}

- (BOOL)isToolDisabled:(ComposeboxMode)option {
  return _disabledTools.contains(option);
}

- (BOOL)isToolAvailable:(ComposeboxMode)option {
  return _allowedTools.contains(option) && !_disabledTools.contains(option);
}

- (BOOL)isModelHidden:(ComposeboxModelOption)option {
  return !_allowedModels.contains(option);
}

- (BOOL)isModelDisabled:(ComposeboxModelOption)option {
  return _disabledModels.contains(option);
}

- (BOOL)isModelAvailable:(ComposeboxModelOption)option {
  return _allowedModels.contains(option) && !_disabledModels.contains(option);
}

@end
