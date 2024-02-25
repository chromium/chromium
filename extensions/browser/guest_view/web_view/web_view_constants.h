// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebView API.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONSTANTS_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONSTANTS_H_

#include <stdint.h>

namespace webview {

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
extern const char kEventNewWindow[];
extern const char kEventPermissionRequest[];
extern const char kEventResponseStarted[];
extern const char kEventResponsive[];
extern const char kEventSizeChanged[];
extern const char kEventUnresponsive[];
extern const char kEventZoomChange[];

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
extern const char kPermissionTypeHid[];
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

// Miscellaneous.
extern const char kMenuItemCommandId[];
extern const char kMenuItemLabel[];
extern const char kPersistPrefix[];
extern const char kStoragePartitionId[];

inline constexpr unsigned int kMaxOutstandingPermissionRequests = 1024;
inline constexpr int kInvalidPermissionRequestID = 0;

// ClearData API constants.
inline constexpr uint32_t WEB_VIEW_REMOVE_DATA_MASK_CACHE = 1 << 0;
inline constexpr uint32_t WEB_VIEW_REMOVE_DATA_MASK_COOKIES = 1 << 1;
inline constexpr uint32_t WEB_VIEW_REMOVE_DATA_MASK_FILE_SYSTEMS = 1 << 2;
inline constexpr uint32_t WEB_VIEW_REMOVE_DATA_MASK_INDEXEDDB = 1 << 3;
inline constexpr uint32_t WEB_VIEW_REMOVE_DATA_MASK_LOCAL_STORAGE = 1 << 4;
inline constexpr uint32_t WEB_VIEW_REMOVE_DATA_MASK_WEBSQL = 1 << 5;
inline constexpr uint32_t WEB_VIEW_REMOVE_DATA_MASK_SESSION_COOKIES = 1 << 6;
inline constexpr uint32_t WEB_VIEW_REMOVE_DATA_MASK_PERSISTENT_COOKIES = 1 << 7;

// Other.
extern const char kWebViewContentScriptManagerKeyName[];

}  // namespace webview

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONSTANTS_H_
