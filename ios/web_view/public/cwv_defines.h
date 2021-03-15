// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_DEFINES_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_DEFINES_H_

// Supports -[CWVWebViewConfiguration nonPersistentConfiguration].
#define IOS_WEB_VIEW_SUPPORTS_CWV_CONFIGURATION_NON_PERSISTENT_CONFIGURATION 1

// Supports -[CWVNavigationDelegate didStartNavigation:] and deprecates
// -[CWVNavigationDelegate didStartProvisionalNavigation:].
#define IOS_WEB_VIEW_SUPPORTS_CWV_NAVIGATION_DELEGATE_DID_START_NAVIGATION 1

// -[CWVPassword isBlocked] instead of -[CWVPassword isBlacklisted].
#define IOS_WEB_VIEW_SUPPORTS_CWV_PASSWORD_BLOCKED 1

// Implements -[CWVWebView becomeFirstResponder] to support keyboard commands.
#define IOS_WEB_VIEW_SUPPORTS_CWV_WEB_VIEW_BECOME_FIRST_RESPONDER 1

// Indicates that CWVWebView already has a custom UIDropInteraction installed.
#define IOS_WEB_VIEW_SUPPORTS_CWV_WEB_VIEW_CUSTOM_DROP_INTERACTION 1

// Allows customization of the keyboard input accessory view for CWVWebView.
#define IOS_WEB_VIEW_SUPPORTS_CWV_WEB_VIEW_INPUT_ACCESSORY_VIEW 1

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_DEFINES_H_
