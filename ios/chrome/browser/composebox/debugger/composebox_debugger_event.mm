// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/debugger/composebox_debugger_event.h"

namespace {

NSString* GetElementDescription(
    composebox_debugger::element::InputPlate element) {
  using enum composebox_debugger::element::InputPlate;
  switch (element) {
    case kAIMNudge:
      return @"AIM Nudge button";
    case kImageGenerationNudge:
      return @"image generation nudge button";
    case kLens:
      return @"Lens entrypoint";
    case kVoice:
      return @"voice search entrypoint";
    case kClearTextButton:
      return @"clear text in omnbiox";
    case kSendButton:
      return @"send query button";
    case kCloseButton:
      return @"close composebox button";
    case kPlusMenu:
      return @"Plus context menu button";
  }
}

NSString* GetElementDescription(
    composebox_debugger::element::ContextMenu element) {
  using enum composebox_debugger::element::ContextMenu;
  switch (element) {
    case kAddCurrentTab:
      return @"add current tab option";
    case kTabsAttachment:
      return @"add tab attachments";
    case kCameraAttachment:
      return @"take camera image";
    case kGalleryAttachment:
      return @"add gallery image";
    case kAIM:
      return @"enable AI Mode";
    case kCreateImage:
      return @"create image";
  }
}

NSString* GetElementDescription(
    composebox_debugger::element::TabPicker element) {
  using enum composebox_debugger::element::TabPicker;
  switch (element) {
    case kCloseButton:
      return @"tab picker close button";
    case kConfirmSelection:
      return @"tab picker confirm selection button";
  }
}

NSString* GetEventDescription(composebox_debugger::event::Tabs event) {
  using enum composebox_debugger::event::Tabs;
  switch (event) {
    case kWillRealizeTab:
      return @"Composebox will realize tab";
    case kDidRealizeTab:
      return @"Composebox did realize tab";
    case kWillLoadTab:
      return @"Composebox will load tab";
    case kDidLoadTab:
      return @"Composebox did load tab";
    case kFailedToLoadTab:
      return @"Composebox failed to load tab";
    case kDidSelectTab:
      return @"User selected tab in picker";
    case kDidDeselectTab:
      return @"User deselected tab in picker";
  }
}

NSString* GetEventDescription(composebox_debugger::event::APC event) {
  using enum composebox_debugger::event::APC;
  switch (event) {
    case kRetrievedFromCache:
      return @"APC retrieved from cache for tab";
    case kExtractionCompletedSuccessfully:
      return @"APC extracted successfully for tab";
    case kExtractionFailed:
      return @"APC extraction failed for tab";
  }
}

NSString* GetEventDescription(composebox_debugger::event::Composebox event) {
  using enum composebox_debugger::event::Composebox;
  switch (event) {
    case kOpened:
      return @"Composebox view opened";
    case kClosed:
      return @"Composebox view closed";
    case kCompactModeEnabled:
      return @"Composebox view compact mode enabled";
    case kCompactModeDisabled:
      return @"Composebox view compact mode disabled";
    case kTabPickerShown:
      return @"Tab picker shown to user";
    case kTabPickerHidden:
      return @"Tab picker was hidden";
    case kCameraViewFinderShown:
      return @"Camera view finder shown to user";
    case kTabPickerSwipeDismiss:
      return @"User dismissed tab picker with swipe";
  }
}

NSString* GetEventDescription(composebox_debugger::event::Environment event) {
  using enum composebox_debugger::event::Environment;
  switch (event) {
    case kDeviceOrientationPortrait:
      return @"Device orientation switch to portrait";
    case kDeviceOrientationLandscape:
      return @"Device orientation switch to landscape";
    case kLowMemoryWarning:
      return @"Low memory warning received";
    case kKeyboardShown:
      return @"Keyboard shown for user";
    case kKeyboardHidden:
      return @"Keyboard hidden for user";
    case kKeyboardReturnKeyPressed:
      return @"Keyboard return key pressed";
  }
}

NSString* GetEventDescription(
    composebox_debugger::event::QueryAttachment event) {
  using enum composebox_debugger::event::QueryAttachment;
  switch (event) {
    case kAdded:
      return @"Attachment added to query";
    case kRemoved:
      return @"Attachment removed from query";
    case kUploadCompletedSuccessfully:
      return @"Attachment uploaded succesfully";
    case kUploadFailed:
      return @"Attachment failed to upload";
  }
}

NSString* GetAttachmentTypeName(composebox_debugger::AttachmentType type) {
  using enum composebox_debugger::AttachmentType;
  switch (type) {
    case kFile:
      return @"File";
    case kTab:
      return @"Tab";
    case kImage:
      return @"Image";
  }
}

}  // namespace

@interface ComposeboxDebuggerEvent ()

- (instancetype)initWithEventDescription:(NSString*)description;
- (instancetype)initWithEventDescription:(NSString*)description
                           eventMetadata:(NSString*)metadata;
- (instancetype)initWithEventDescription:(NSString*)description
                                   tabID:(int32_t)tabID
                                tabTitle:(NSString*)tabTitle;

@end

@implementation ComposeboxDebuggerEvent

+ (instancetype)inputPlateTapOnElement:
    (composebox_debugger::element::InputPlate)element {
  NSString* eventDescription =
      [NSString stringWithFormat:@"Tap on %@", GetElementDescription(element)];
  return [[ComposeboxDebuggerEvent alloc]
      initWithEventDescription:eventDescription];
}

+ (instancetype)contextMenuTapOnElement:
    (composebox_debugger::element::ContextMenu)element {
  NSString* eventDescription =
      [NSString stringWithFormat:@"Tap on %@", GetElementDescription(element)];
  return [[ComposeboxDebuggerEvent alloc]
      initWithEventDescription:eventDescription];
}

+ (instancetype)tabPickerTapOnElement:
    (composebox_debugger::element::TabPicker)element {
  NSString* eventDescription =
      [NSString stringWithFormat:@"Tap on %@", GetElementDescription(element)];
  return [[ComposeboxDebuggerEvent alloc]
      initWithEventDescription:eventDescription];
}

+ (instancetype)tabEvent:(composebox_debugger::event::Tabs)event
               withTitle:(NSString*)tabTitle
                   tabID:(int32_t)tabID {
  return [[ComposeboxDebuggerEvent alloc]
      initWithEventDescription:GetEventDescription(event)
                         tabID:tabID
                      tabTitle:tabTitle];
}

+ (instancetype)apcEvent:(composebox_debugger::event::APC)event
               withTitle:(NSString*)tabTitle
                   tabID:(int32_t)tabID {
  return [[ComposeboxDebuggerEvent alloc]
      initWithEventDescription:GetEventDescription(event)
                         tabID:tabID
                      tabTitle:tabTitle];
}

+ (instancetype)
    queryAttachmentEvent:(composebox_debugger::event::QueryAttachment)event
                withType:(composebox_debugger::AttachmentType)attachmentType
                   title:(NSString*)attachmentTitle {
  return [[ComposeboxDebuggerEvent alloc]
      initWithEventDescription:GetEventDescription(event)
               attachmentTitle:attachmentTitle
                attachmentType:GetAttachmentTypeName(attachmentType)];
}

+ (instancetype)composeboxGeneralEvent:
    (composebox_debugger::event::Composebox)event {
  return [[ComposeboxDebuggerEvent alloc]
      initWithEventDescription:GetEventDescription(event)];
}

+ (instancetype)environmentEvent:
    (composebox_debugger::event::Environment)event {
  return [[ComposeboxDebuggerEvent alloc]
      initWithEventDescription:GetEventDescription(event)];
}

#pragma mark - Private

- (instancetype)initWithEventDescription:(NSString*)description {
  return [self initWithEventDescription:description eventMetadata:nil];
}

- (instancetype)initWithEventDescription:(NSString*)description
                                   tabID:(int32_t)tabID
                                tabTitle:(NSString*)tabTitle {
  NSString* eventMetadata =
      [NSString stringWithFormat:@"[Tab %d] %@", tabID, tabTitle];
  return [self initWithEventDescription:description
                          eventMetadata:eventMetadata];
}

- (instancetype)initWithEventDescription:(NSString*)description
                         attachmentTitle:(NSString*)title
                          attachmentType:(NSString*)attachmentType {
  NSString* eventMetadata =
      [NSString stringWithFormat:@"[Attachment %@] %@", attachmentType, title];
  return [self initWithEventDescription:description
                          eventMetadata:eventMetadata];
}

- (instancetype)initWithEventDescription:(NSString*)description
                           eventMetadata:(NSString*)metadata {
  self = [super init];
  if (self) {
    _eventDescription = [description copy];

    NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
    [dateFormatter setDateFormat:@"HH:mm:ss"];
    NSString* dateString = [dateFormatter stringFromDate:NSDate.now];
    if (metadata) {
      _eventMetadata =
          [NSString stringWithFormat:@"%@ %@", dateString, metadata];
    } else {
      _eventMetadata = dateString;
    }
  }

  return self;
}

@end
