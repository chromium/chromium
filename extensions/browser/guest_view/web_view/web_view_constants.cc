// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_constants.h"

namespace webview {

// Attributes.
const char kAttributeAllowTransparency[] = "allowtransparency";
const char kAttributeAllowScaling[] = "allowscaling";
const char kAttributeName[] = "name";
const char kAttributeSrc[] = "src";

// API namespace.
const char kAPINamespace[] = "webViewInternal";

// API error messages.
const char kAPILoadDataInvalidDataURL[] = "Invalid data URL \"%s\".";
const char kAPILoadDataInvalidBaseURL[] = "Invalid base URL \"%s\".";
const char kAPILoadDataInvalidVirtualURL[] = "Invalid virtual URL \"%s\".";

// Events.
const char kEventAudioStateChanged[] = "webViewInternal.onAudioStateChanged";
const char kEventClose[] = "webViewInternal.onClose";
const char kEventConsoleMessage[] = "webViewInternal.onConsoleMessage";
const char kEventContentLoad[] = "webViewInternal.onContentLoad";
const char kEventContextMenuShow[] = "chromeWebViewInternal.onContextMenuShow";
const char kEventDialog[] = "webViewInternal.onDialog";
const char kEventExit[] = "webViewInternal.onExit";
const char kEventExitFullscreen[] = "webViewInternal.onExitFullscreen";
const char kEventFindReply[] = "webViewInternal.onFindReply";
const char kEventFrameNameChanged[] = "webViewInternal.onFrameNameChanged";
const char kEventHeadersReceived[] = "webViewInternal.onHeadersReceived";
const char kEventLoadAbort[] = "webViewInternal.onLoadAbort";
const char kEventLoadCommit[] = "webViewInternal.onLoadCommit";
const char kEventLoadProgress[] = "webViewInternal.onLoadProgress";
const char kEventLoadRedirect[] = "webViewInternal.onLoadRedirect";
const char kEventLoadStart[] = "webViewInternal.onLoadStart";
const char kEventLoadStop[] = "webViewInternal.onLoadStop";
const char kEventMessage[] = "webViewInternal.onMessage";
const char kEventNewWindow[] = "webViewInternal.onNewWindow";
const char kEventPermissionRequest[] = "webViewInternal.onPermissionRequest";
const char kEventResponseStarted[] = "webViewInternal.onResponseStarted";
const char kEventResponsive[] = "webViewInternal.onResponsive";
const char kEventSizeChanged[] = "webViewInternal.onSizeChanged";
const char kEventUnresponsive[] = "webViewInternal.onUnresponsive";
const char kEventZoomChange[] = "webViewInternal.onZoomChange";

// WebRequest API events.
const char kEventAuthRequired[] = "webViewInternal.onAuthRequired";
const char kEventBeforeRedirect[] = "webViewInternal.onBeforeRedirect";
const char kEventBeforeRequest[] = "webViewInternal.onBeforeRequest";
const char kEventBeforeSendHeaders[] = "webViewInternal.onBeforeSendHeaders";
const char kEventCompleted[] = "webViewInternal.onCompleted";
const char kEventErrorOccurred[] = "webViewInternal.onErrorOccurred";
const char kEventSendHeaders[] = "webViewInternal.onSendHeaders";

// Event related constants.
const char kWebViewEventPrefix[] = "webViewInternal.";

// Parameters/properties on events.
const char kAudible[] = "audible";
const char kContextMenuItems[] = "items";
const char kDefaultPromptText[] = "defaultPromptText";
const char kFindSearchText[] = "searchText";
const char kFindFinalUpdate[] = "finalUpdate";
const char kInitialHeight[] = "initialHeight";
const char kInitialWidth[] = "initialWidth";
const char kLastUnlockedBySelf[] = "lastUnlockedBySelf";
const char kLevel[] = "level";
const char kLine[] = "line";
const char kMessage[] = "message";
const char kMessageText[] = "messageText";
const char kMessageType[] = "messageType";
const char kName[] = "name";
const char kNewHeight[] = "newHeight";
const char kNewURL[] = "newUrl";
const char kNewWidth[] = "newWidth";
const char kOldHeight[] = "oldHeight";
const char kOldURL[] = "oldUrl";
const char kOrigin[] = "origin";
const char kPermission[] = "permission";
const char kPermissionTypeDialog[] = "dialog";
const char kPermissionTypeDownload[] = "download";
const char kPermissionTypeFileSystem[] = "filesystem";
const char kPermissionTypeFullscreen[] = "fullscreen";
const char kPermissionTypeGeolocation[] = "geolocation";
const char kPermissionTypeLoadPlugin[] = "loadplugin";
const char kPermissionTypeMedia[] = "media";
const char kPermissionTypeNewWindow[] = "newwindow";
const char kPermissionTypePointerLock[] = "pointerLock";
const char kOldWidth[] = "oldWidth";
const char kProcessId[] = "processId";
const char kProgress[] = "progress";
const char kReason[] = "reason";
const char kRequestId[] = "requestId";
const char kRequestInfo[] = "requestInfo";
const char kSourceId[] = "sourceId";
const char kTargetURL[] = "targetUrl";
const char kWindowID[] = "windowId";
const char kWindowOpenDisposition[] = "windowOpenDisposition";
const char kOldZoomFactor[] = "oldZoomFactor";
const char kNewZoomFactor[] = "newZoomFactor";

// Internal parameters/properties on events.
const char kInternalBaseURLForDataURL[] = "baseUrlForDataUrl";
const char kInternalCurrentEntryIndex[] = "currentEntryIndex";
const char kInternalEntryCount[] = "entryCount";
const char kInternalProcessId[] = "processId";
const char kInternalVisibleUrl[] = "visibleUrl";

// Parameters to callback functions.
const char kFindNumberOfMatches[] = "numberOfMatches";
const char kFindActiveMatchOrdinal[] = "activeMatchOrdinal";
const char kFindSelectionRect[] = "selectionRect";
const char kFindRectLeft[] = "left";
const char kFindRectTop[] = "top";
const char kFindRectWidth[] = "width";
const char kFindRectHeight[] = "height";
const char kFindCanceled[] = "canceled";

// Initialization parameters.
const char kInitialZoomFactor[] = "initialZoomFactor";
const char kParameterUserAgentOverride[] = "userAgentOverride";

// Miscellaneous.
const char kMenuItemCommandId[] = "commandId";
const char kMenuItemLabel[] = "label";
const char kPersistPrefix[] = "persist:";
const char kStoragePartitionId[] = "partition";
const unsigned int kMaxOutstandingPermissionRequests = 1024;
const int kInvalidPermissionRequestID = 0;

// ClearData API constants.
const uint32_t WEB_VIEW_REMOVE_DATA_MASK_CACHE = 1 << 0;
const uint32_t WEB_VIEW_REMOVE_DATA_MASK_COOKIES = 1 << 1;
const uint32_t WEB_VIEW_REMOVE_DATA_MASK_FILE_SYSTEMS = 1 << 2;
const uint32_t WEB_VIEW_REMOVE_DATA_MASK_INDEXEDDB = 1 << 3;
const uint32_t WEB_VIEW_REMOVE_DATA_MASK_LOCAL_STORAGE = 1 << 4;
const uint32_t WEB_VIEW_REMOVE_DATA_MASK_WEBSQL = 1 << 5;
const uint32_t WEB_VIEW_REMOVE_DATA_MASK_SESSION_COOKIES = 1 << 6;
const uint32_t WEB_VIEW_REMOVE_DATA_MASK_PERSISTENT_COOKIES = 1 << 7;

// Other.
const char kWebViewContentScriptManagerKeyName[] =
    "web_view_content_script_manager";
}  // namespace webview
