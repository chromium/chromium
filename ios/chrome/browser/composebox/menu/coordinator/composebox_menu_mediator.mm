// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_mediator.h"

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item_type.h"

@implementation ComposeboxMenuMediator {
  // The entrypoint associated with this menu invocation.
  ComposeboxEntrypoint _entrypoint;
}

- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint {
  self = [super init];
  if (self) {
    _entrypoint = entrypoint;
  }

  return self;
}

#pragma mark - Public

- (void)processImageItems:(NSArray<ComposeboxPickerImageResult*>*)imageItems {
  ComposeboxFocusParams* focusParams =
      [[ComposeboxFocusParams alloc] initWithEntrypoint:_entrypoint];
  focusParams.initialImages = imageItems;
  [self.delegate composeboxMenuMediatorDidProduceFocusParams:focusParams];
}

- (BOOL)canAddMoreAttachments {
  // When presented as a standalone menu, there are no prior restrictions.
  if (_entrypoint == ComposeboxEntrypoint::kNTPPlusButton) {
    return YES;
  }

  // TODO(crbug.com/506956060): Take current attachments into account.
  return YES;
}

- (NSUInteger)remainingNumberOfImagesAllowed {
  // TODO(crbug.com/506956765): Implement.
  return 5;
}

#pragma mark - ComposeboxMenuMutator

- (void)handleItemPickedWithType:(ComposeboxMenuItemType)type {
  ComposeboxFocusParams* focusParams =
      [[ComposeboxFocusParams alloc] initWithEntrypoint:_entrypoint];

  // TODO (crbug.com/505269628): Implement menu items selection for attachments.
  switch (type) {
    case ComposeboxMenuItemType::kAIM:
      focusParams.initialMode = ComposeboxMode::kAIM;
      break;
    case ComposeboxMenuItemType::kCreateImage:
      focusParams.initialMode = ComposeboxMode::kImageGeneration;
      break;
    case ComposeboxMenuItemType::kDeepSearch:
      focusParams.initialMode = ComposeboxMode::kDeepSearch;
      break;
    case ComposeboxMenuItemType::kCanvas:
      focusParams.initialMode = ComposeboxMode::kCanvas;
      break;
    case ComposeboxMenuItemType::kModelRegular:
      focusParams.initialModelOption = ComposeboxModelOption::kRegular;
      break;
    case ComposeboxMenuItemType::kModelAuto:
      focusParams.initialModelOption = ComposeboxModelOption::kAuto;
      break;
    case ComposeboxMenuItemType::kModelThinking:
      focusParams.initialModelOption = ComposeboxModelOption::kThinking;
      break;
    case ComposeboxMenuItemType::kAttachmentTabs:
      break;
    case ComposeboxMenuItemType::kAttachmentCamera:
      [self.delegate composeboxMenuMediatorDidRequestCameraSelection:self];
      return;
    case ComposeboxMenuItemType::kAttachmentGallery:
      [self.delegate composeboxMenuMediatorDidRequestGallerySelection:self];
      return;
    case ComposeboxMenuItemType::kAttachmentFiles:
      break;
    case ComposeboxMenuItemType::kUnknown:
      break;
  }

  [self.delegate composeboxMenuMediatorDidProduceFocusParams:focusParams];
}

@end
