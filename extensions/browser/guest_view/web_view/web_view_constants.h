// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebView API.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONSTANTS_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONSTANTS_H_

#include <stdint.h>

namespace webview {

// Attributes.
extern const char kAttributeAllowTransparency[];
extern const char kAttributeAllowScaling[];
extern const char kAttributeName[];
extern const char kAttributeSrc[];

// API namespace.
// TODO(kalman): Consolidate this with the other API constants.
extern const char kAPINamespace[];

// API error messages.
extern const char kAPILoadDataInvalidDataURL[];
extern const char kAPILoadDataInvalidBaseURL[];
extern const char kAPILoadDataInvalidVirtualURL[];

// Events.
extern const char kEventAudioStateChanged[];
extern const char kEventClose[];
extern const char kEventConsoleMessage[];
extern const char kEventContentLoad[];
extern const char kEventContextMenuShow[];
extern const char kEventDialog[];
extern const char kEventExit[];
extern const char kEventExitFullscreen[];
extern const char kEventFindReply[];
extern const char kEventFrameNameChanged[];
extern const char kEventHeadersReceived[];
extern const char kEventLoadAbort[];
extern const char kEventLoadCommit[];
extern const char kEventLoadProgress[];
extern const char kEventLoadRedirect[];
extern const char kEventLoadStart[];
extern const char kEventLoadStop[];
extern const char kEventMessage[];
extern const char kEventNewWindow[];
extern const char kEventPermissionRequest[];
extern const char kEventResponseStarted[];
extern const char kEventResponsive[];
extern const char kEventSizeChanged[];
extern const char kEventUnresponsive[];
extern const char kEventZoomChange[];

// WebRequest API events.
extern const char kEventAuthRequired[];
extern const char kEventBeforeRedirect[];
extern const char kEventBeforeRequest[];
extern const char kEventBeforeSendHeaders[];
extern const char kEventCompleted[];
extern const char kEventErrorOccurred[];
extern const char kEventSendHeaders[];

// Event related constants.
extern const char kWebViewEventPrefix[];

// Parameters/properties on events.
extern const char kAudible[];
extern const char kContextMenuItems[];
extern const char kDefaultPromptText[];
extern const char kFindSearchText[];
extern const char kFindFinalUpdate[];
extern const char kInitialHeight[];
extern const char kInitialWidth[];
extern const char kLastUnlockedBySelf[];
extern const char kLevel[];
extern const char kLine[];
extern const char kMessage[];
extern const char kMessageText[];
extern const char kMessageType[];
extern const char kName[];
extern const char kNewHeight[];
extern const char kNewURL[];
extern const char kNewWidth[];
extern const char kOldHeight[];
extern const char kOldURL[];
extern const char kOrigin[];
extern const char kPermission[];
extern const char kPermissionTypeDialog[];
extern const char kPermissionTypeDownload[];
extern const char kPermissionTypeFileSystem[];
extern const char kPermissionTypeFullscreen[];
extern const char kPermissionTypeGeolocation[];
extern const char kPermissionTypeLoadPlugin[];
extern const char kPermissionTypeMedia[];
extern const char kPermissionTypeNewWindow[];
extern const char kPermissionTypePointerLock[];
extern const char kOldWidth[];
extern const char kProcessId[];
extern const char kProgress[];
extern const char kReason[];
extern const char kRequestId[];
extern const char kRequestInfo[];
extern const char kSourceId[];
extern const char kTargetURL[];
extern const char kWindowID[];
extern const char kWindowOpenDisposition[];
extern const char kOldZoomFactor[];
extern const char kNewZoomFactor[];

// Internal parameters/properties on events.
extern const char kInternalBaseURLForDataURL[];
extern const char kInternalCurrentEntryIndex[];
extern const char kInternalEntryCount[];
extern const char kInternalProcessId[];
extern const char kInternalVisibleUrl[];

// Parameters to callback functions.
extern const char kFindNumberOfMatches[];
extern const char kFindActiveMatchOrdinal[];
extern const char kFindSelectionRect[];
extern const char kFindRectLeft[];
extern const char kFindRectTop[];
extern const char kFindRectWidth[];
extern const char kFindRectHeight[];
extern const char kFindCanceled[];
extern const char kFindDone[];

// Initialization parameters.
extern const char kInitialZoomFactor[];
extern const char kParameterUserAgentOverride[];

// Miscellaneous.
extern const char kMenuItemCommandId[];
extern const char kMenuItemLabel[];
extern const char kPersistPrefix[];
extern const char kStoragePartitionId[];
extern const unsigned int kMaxOutstandingPermissionRequests;
extern const int kInvalidPermissionRequestID;

// ClearData API constants.
//
// Note that these are not in an enum because using enums to declare bitmasks
// results in the enum values being signed.
extern const uint32_t WEB_VIEW_REMOVE_DATA_MASK_CACHE;
extern const uint32_t WEB_VIEW_REMOVE_DATA_MASK_COOKIES;
extern const uint32_t WEB_VIEW_REMOVE_DATA_MASK_FILE_SYSTEMS;
extern const uint32_t WEB_VIEW_REMOVE_DATA_MASK_INDEXEDDB;
extern const uint32_t WEB_VIEW_REMOVE_DATA_MASK_LOCAL_STORAGE;
extern const uint32_t WEB_VIEW_REMOVE_DATA_MASK_WEBSQL;
extern const uint32_t WEB_VIEW_REMOVE_DATA_MASK_SESSION_COOKIES;
extern const uint32_t WEB_VIEW_REMOVE_DATA_MASK_PERSISTENT_COOKIES;

// Other.
extern const char kWebViewContentScriptManagerKeyName[];

}  // namespace webview

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONSTANTS_H_
