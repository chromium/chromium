// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_PERMISSIONS_PERMISSIONS_H_
#define IOS_WEB_PUBLIC_PERMISSIONS_PERMISSIONS_H_

namespace web {

// Enum specifying different data or device hardwares that the app/site needs
// access permissions to.
enum class Permission {
  CAMERA = 0,
  MICROPHONE = 1,
};

// Enum specifying whether a subject of permission is accessible and allowed.
enum class PermissionState {
  // The device has not granted the site access to the requested permission.
  // When this is the state of a given permission, the app is unable to change
  // it to ALLOWED or BLOCKED.
  NOT_ACCESSIBLE = 0,
  // The site has access to the requested permission and is able to use it when
  // needed.
  ALLOWED = 1,
  // The site has access to the requested permission, but the user has disabled
  // or blocked the subject of permission so it cannot be used.
  BLOCKED = 2,
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_PERMISSIONS_PERMISSIONS_H_
