// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_CLIPBOARD_CLIPBOARD_CONSTANTS_H_
#define IOS_WEB_JS_FEATURES_CLIPBOARD_CLIPBOARD_CONSTANTS_H_

namespace web {

// The command sent from JavaScript when `navigator.clipboard.read` or
// `navigator.clipboard.readText` is called.
inline constexpr char kReadCommand[] = "read";

// The command sent from JavaScript when `navigator.clipboard.write` or
// `navigator.clipboard.writeText` is called.
inline constexpr char kWriteCommand[] = "write";

// The command sent from JavaScript when a paste event is detected.
inline constexpr char kDidFinishClipboardReadCommand[] =
    "didFinishClipboardRead";

// The name of the script message handler.
inline constexpr char kScriptMessageHandlerName[] = "ClipboardHandler";

// The key for the command in the script message body.
inline constexpr char kCommandKey[] = "command";

// The key for the request ID in the script message body.
inline constexpr char kRequestIdKey[] = "requestId";

// The key for the frame ID in the script message body.
inline constexpr char kFrameIdKey[] = "frameId";

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_CLIPBOARD_CLIPBOARD_CONSTANTS_H_
