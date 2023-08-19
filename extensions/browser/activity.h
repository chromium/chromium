// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ACTIVITY_H_
#define EXTENSIONS_BROWSER_ACTIVITY_H_

namespace extensions {

struct Activity {
  enum Type {
    // The activity is for accessibility. The extra data is an empty string.
    ACCESSIBILITY,

    // The activity is an Extensions API function call. The extra data is the
    // function name.
    API_FUNCTION,

    // The activity is for the Developer Tools client. The extra data is an
    // empty string.
    DEV_TOOLS,

    // The activity is an event. The extra data is the event name.
    EVENT,

    // The activity is meant to keep the background page alive during an
    // IPC call or during page creation. Use the constants kIPC or
    // kCreatePage.
    LIFECYCLE_MANAGEMENT,

    // The activity is related to the media manager. Use the constant
    // kPictureInPicture.
    MEDIA,

    // The activity is a message. The extra data is the port ID.
    MESSAGE,

    // The activity is a message port opening. The extra data is the port ID.
    MESSAGE_PORT,

    // The activity is meant to keep the background page alive while
    // a modal dialog box is visible. The extra data is the URL of the
    // calling page.
    MODAL_DIALOG,

    // The activity is a Mojo API function call. The extra data is an empty
    // string.
    MOJO,

    // The activity is a network request. The extra data is the request ID.
    NETWORK,

    // The activity is a Pepper API function call. The extra data is an empty
    // string.
    PEPPER_API,

    // The activity is internal ProcessManager bookkeeping. The extra data
    // is one of kCancelSuspend or kRenderFrame.
    PROCESS_MANAGER,

    // The activity is an attached debugger session (i.e., using the
    // chrome.debugger API). This is distinct from `DEV_TOOLS`, which indicates
    // the user is debugging the extension.
    DEBUGGER,
  };

  static const char* ToString(Type type);

  static const char kCancelSuspend[];
  static const char kCreatePage[];
  static const char kIPC[];
  static const char kPictureInPicture[];
  static const char kRenderFrame[];
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ACTIVITY_H_
