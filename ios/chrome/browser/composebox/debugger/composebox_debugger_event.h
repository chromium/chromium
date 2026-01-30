// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_EVENT_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_EVENT_H_

#import <Foundation/Foundation.h>

namespace composebox_debugger {
namespace element {
enum class InputPlate {
  kAIMNudge,
  kImageGenerationNudge,
  kLens,
  kVoice,
  kClearTextButton,
  kSendButton,
  kCloseButton,
  kPlusMenu,
};

enum class ContextMenu {
  kAddCurrentTab,
  kTabsAttachment,
  kCameraAttachment,
  kGalleryAttachment,
  kAIM,
  kCreateImage
};

enum class TabPicker {
  kCloseButton,
  kConfirmSelection,
};
}  // namespace element

namespace event {
enum class QueryAttachment {
  kAdded,
  kRemoved,
  kUploadCompletedSuccessfully,
  kUploadFailed
};

enum class Tabs {
  kWillRealizeTab,
  kDidRealizeTab,
  kWillLoadTab,
  kDidLoadTab,
  kFailedToLoadTab,
  kDidSelectTab,
  kDidDeselectTab,
};

enum class APC {
  kRetrievedFromCache,
  kExtractionCompletedSuccessfully,
  kExtractionFailed
};

enum class Composebox {
  kOpened,
  kClosed,
  kCompactModeEnabled,
  kCompactModeDisabled,
  kTabPickerShown,
  kTabPickerHidden,
  kCameraViewFinderShown,
  kTabPickerSwipeDismiss,
};

enum class Environment {
  kDeviceOrientationPortrait,
  kDeviceOrientationLandscape,
  kLowMemoryWarning,
  kKeyboardShown,
  kKeyboardHidden,
  kKeyboardReturnKeyPressed
};
}  // namespace event

enum class AttachmentType {
  kFile,
  kTab,
  kImage,
};
}  // namespace composebox_debugger

@interface ComposeboxDebuggerEvent : NSObject

@property(nonatomic, copy) NSString* eventDescription;
@property(nonatomic, copy) NSString* eventMetadata;
@property(nonatomic, copy) NSDate* timestamp;

+ (instancetype)inputPlateTapOnElement:
    (composebox_debugger::element::InputPlate)element;
+ (instancetype)contextMenuTapOnElement:
    (composebox_debugger::element::ContextMenu)element;
+ (instancetype)tabPickerTapOnElement:
    (composebox_debugger::element::TabPicker)element;
+ (instancetype)tabEvent:(composebox_debugger::event::Tabs)event
               withTitle:(NSString*)tabTitle
                   tabID:(int32_t)tabID;
+ (instancetype)apcEvent:(composebox_debugger::event::APC)event
               withTitle:(NSString*)tabTitle
                   tabID:(int32_t)tabID;

+ (instancetype)composeboxGeneralEvent:
    (composebox_debugger::event::Composebox)event;
+ (instancetype)environmentEvent:(composebox_debugger::event::Environment)event;
+ (instancetype)
    queryAttachmentEvent:(composebox_debugger::event::QueryAttachment)event
                withType:(composebox_debugger::AttachmentType)attachmentType
                   title:(NSString*)attachmentTitle;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_EVENT_H_
