// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_TYPES_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_TYPES_H_

enum WebViewPermissionType {
  // Unknown type of permission request.
  WEB_VIEW_PERMISSION_TYPE_UNKNOWN,

  WEB_VIEW_PERMISSION_TYPE_DOWNLOAD,

  WEB_VIEW_PERMISSION_TYPE_FILESYSTEM,

  // html5 fullscreen permission.
  WEB_VIEW_PERMISSION_TYPE_FULLSCREEN,

  WEB_VIEW_PERMISSION_TYPE_GEOLOCATION,
  // Permission to request access to Human Interface Devices.
  WEB_VIEW_PERMISSION_TYPE_HID,

  // JavaScript Dialogs: prompt, alert, confirm
  // Note: Even through dialogs do not use the permission API, the dialog API
  // is sufficiently similiar that it's convenient to consider it a permission
  // type for code reuse.
  WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG,

  WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN,

  // Media access (audio/video) permission request type.
  WEB_VIEW_PERMISSION_TYPE_MEDIA,

  // New window requests.
  // Note: Even though new windows don't use the permission API, the new window
  // API is sufficiently similar that it's convenient to consider it a
  // permission type for code reuse.
  WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW,

  WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK
};

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_TYPES_H_
