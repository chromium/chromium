// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_mediator.h"

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_consumer.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item_type.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_selection.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_drive_result.h"
#import "ios/chrome/browser/composebox/shared/metrics/composebox_metrics_recorder.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ios/web/public/web_state.h"

@implementation ComposeboxMenuMediator {
  // The entrypoint associated with this menu invocation.
  ComposeboxEntrypoint _entrypoint;
  // The initial UI input state.
  ComposeboxUIInputState* _inputState;
  // The web state list of the browser.
  raw_ptr<WebStateList> _webStateList;
  // Preselected/attached tab and image context.
  ComposeboxAttachmentSelection* _preselection;
  // Metrics recorder.
  ComposeboxMetricsRecorder* _metricsRecorder;
}

- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint
                        inputState:(ComposeboxUIInputState*)inputState
                      webStateList:(WebStateList*)webStateList
            preselectedAttachments:
                (ComposeboxAttachmentSelection*)preselectedAttachments
                   metricsRecorder:(ComposeboxMetricsRecorder*)metricsRecorder {
  self = [super init];
  if (self) {
    _entrypoint = entrypoint;
    _inputState = inputState;
    _webStateList = webStateList;
    _preselection = preselectedAttachments;
    _metricsRecorder = metricsRecorder;
  }

  return self;
}

- (void)disconnect {
  _metricsRecorder = nil;
  _webStateList = nullptr;
  _preselection = nil;
  _inputState = nil;
  self.consumer = nil;
  self.delegate = nil;
}

#pragma mark - Public

- (void)processImageItems:(NSArray<ComposeboxPickerImageResult*>*)imageItems {
  NSMutableArray<ComposeboxPickerImageResult*>* updatedImageResults =
      [[NSMutableArray alloc] init];

  if (_preselection.images) {
    updatedImageResults = [_preselection.images mutableCopy];
  }

  [updatedImageResults addObjectsFromArray:imageItems];

  ComposeboxAttachmentSelection* selection =
      [[ComposeboxAttachmentSelection alloc]
             initWithTabIDs:_preselection.tabIDs
          cachedWebStateIDs:_preselection.cachedWebStateIDs
                     images:updatedImageResults
                      files:_preselection.files
                 driveItems:_preselection.driveItems];

  [self.delegate composeboxMenuMediator:self didUpdateAttachments:selection];
}

- (BOOL)canAddMoreAttachments {
  // When presented as a standalone menu, there are no prior restrictions.
  if (_entrypoint == ComposeboxEntrypoint::kNTPPlusButton) {
    return YES;
  }

  CHECK(_inputState);
  return _inputState.remainingAttachmentCapacity > 0;
}

- (void)processFileURLs:(NSArray<NSURL*>*)urls {
  NSMutableSet<NSURL*>* updatedURLs = [NSMutableSet setWithArray:urls];
  [updatedURLs addObjectsFromArray:_preselection.files];

  ComposeboxAttachmentSelection* selection =
      [[ComposeboxAttachmentSelection alloc]
             initWithTabIDs:_preselection.tabIDs
          cachedWebStateIDs:_preselection.cachedWebStateIDs
                     images:_preselection.images
                      files:[updatedURLs allObjects]
                 driveItems:_preselection.driveItems];
  [self.delegate composeboxMenuMediator:self didUpdateAttachments:selection];
}

- (void)processDriveItems:(NSArray<ComposeboxPickerDriveResult*>*)driveItems {
  NSMutableArray<ComposeboxPickerDriveResult*>* updatedDriveItems =
      [[NSMutableArray alloc] init];

  if (_preselection.driveItems) {
    updatedDriveItems = [_preselection.driveItems mutableCopy];
  }

  [updatedDriveItems addObjectsFromArray:driveItems];

  ComposeboxAttachmentSelection* selection =
      [[ComposeboxAttachmentSelection alloc]
             initWithTabIDs:_preselection.tabIDs
          cachedWebStateIDs:_preselection.cachedWebStateIDs
                     images:_preselection.images
                      files:_preselection.files
                 driveItems:updatedDriveItems];
  [self.delegate composeboxMenuMediator:self didUpdateAttachments:selection];
}

- (void)processWebStateIDs:(std::set<web::WebStateID>)selectedWebStateIDs
         cachedWebStateIDs:(std::set<web::WebStateID>)cachedWebStateIDs {
  // As the tab picker is prepopulated, there's no need to handle the set
  // difference, as the web state IDs already take into account the
  // preselection.
  ComposeboxAttachmentSelection* selection =
      [[ComposeboxAttachmentSelection alloc]
             initWithTabIDs:selectedWebStateIDs
          cachedWebStateIDs:cachedWebStateIDs
                     images:_preselection.images
                      files:_preselection.files
                 driveItems:_preselection.driveItems];
  [self.delegate composeboxMenuMediator:self didUpdateAttachments:selection];
}

- (NSUInteger)remainingNumberOfImagesAllowed {
  CHECK(_inputState);
  return _inputState.remainingNumberOfImagesAllowed;
}

- (void)setConsumer:(id<ComposeboxMenuConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setUIInputState:_inputState];
}

#pragma mark - ComposeboxMenuMutator

- (void)handleItemPickedWithType:(ComposeboxMenuItemType)type {
  switch (type) {
    case ComposeboxMenuItemType::kAIM:
      [self.delegate composeboxMenuMediator:self
                                 didTapTool:ComposeboxMode::kAIM];
      break;
    case ComposeboxMenuItemType::kCreateImage:
      [self.delegate composeboxMenuMediator:self
                                 didTapTool:ComposeboxMode::kImageGeneration];
      break;
    case ComposeboxMenuItemType::kDeepSearch:
      [self.delegate composeboxMenuMediator:self
                                 didTapTool:ComposeboxMode::kDeepSearch];
      break;
    case ComposeboxMenuItemType::kCanvas:
      [self.delegate composeboxMenuMediator:self
                                 didTapTool:ComposeboxMode::kCanvas];
      break;
    case ComposeboxMenuItemType::kModelRegular:
      [self.delegate composeboxMenuMediator:self
                                didTapModel:ComposeboxModelOption::kRegular];
      break;
    case ComposeboxMenuItemType::kModelAuto:
      [self.delegate composeboxMenuMediator:self
                                didTapModel:ComposeboxModelOption::kAuto];
      break;
    case ComposeboxMenuItemType::kModelThinking:
      [self.delegate composeboxMenuMediator:self
                                didTapModel:ComposeboxModelOption::kThinking];
      break;
    case ComposeboxMenuItemType::kCurrentTab: {
      [self processCurrentTab];
      break;
    }
    case ComposeboxMenuItemType::kModelThinkingNoGenUI:
      [self.delegate
          composeboxMenuMediator:self
                     didTapModel:ComposeboxModelOption::kThinkingNoGenUI];
      break;
    case ComposeboxMenuItemType::kAttachmentTabs:
      [self.delegate composeboxMenuMediatorDidRequestTabSelection:self];
      break;
    case ComposeboxMenuItemType::kAttachmentCamera:
      [self.delegate composeboxMenuMediatorDidRequestCameraSelection:self];
      break;
    case ComposeboxMenuItemType::kAttachmentGallery:
      [self.delegate composeboxMenuMediatorDidRequestGallerySelection:self];
      break;
    case ComposeboxMenuItemType::kAttachmentFiles:
      [self.delegate composeboxMenuMediatorDidRequestFileSelection:self];
      break;
    case ComposeboxMenuItemType::kAttachmentDrive:
      CHECK(IsComposeboxDriveOptionEnabled());
      [self.delegate composeboxMenuMediatorDidRequestDriveFileSelection:self];
      break;
    case ComposeboxMenuItemType::kUnknown:
      break;
  }
}

- (std::set<web::WebStateID>)allAttachedWebStateIDs {
  if (!_preselection) {
    return std::set<web::WebStateID>();
  }
  return _preselection.tabIDs;
}

- (std::set<web::WebStateID>)attachedWebStateIDsInCurrentContext {
  std::set<web::WebStateID> webStateIDs;
  if (!_webStateList || !_preselection) {
    return webStateIDs;
  }

  for (web::WebStateID webStateID : _preselection.tabIDs) {
    if (!webStateID.valid()) {
      continue;
    }

    WebStateSearchCriteria searchCriteria{
        .identifier = webStateID,
        .pinned_state = WebStateSearchCriteria::PinnedState::kAny,
    };

    if (GetWebStateIndex(_webStateList, searchCriteria) !=
        WebStateList::kInvalidIndex) {
      webStateIDs.insert(webStateID);
    }
  }
  return webStateIDs;
}

#pragma mark - Private

// Processes the current tab selection.
- (void)processCurrentTab {
  if (!_webStateList) {
    return;
  }

  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }

  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kCurrentTab];

  web::WebStateID activeWebStateID = activeWebState->GetUniqueIdentifier();

  std::set<web::WebStateID> webStateIDs = _preselection.tabIDs;
  webStateIDs.insert(activeWebStateID);

  [self processWebStateIDs:webStateIDs
         cachedWebStateIDs:_preselection.cachedWebStateIDs];
}

@end
