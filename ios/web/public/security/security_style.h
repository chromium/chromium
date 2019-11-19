// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SECURITY_SECURITY_STYLE_H_
#define IOS_WEB_PUBLIC_SECURITY_SECURITY_STYLE_H_

namespace web {

// Various aspects of the UI change their appearance according to the security
// context in which they are displayed.  For example, the location bar displays
// a lock icon when it is displayed during a valid SSL connection.
// SecuritySyle enumerates these styles, but it is up to the UI elements to
// adjust their display appropriately.
enum SecurityStyle {
  // SECURITY_STYLE_UNKNOWN indicates that we do not know the proper security
  // style for this object.
  SECURITY_STYLE_UNKNOWN,

  // SECURITY_STYLE_UNAUTHENTICATED means the authenticity of this object can
  // not be determined, either because it was retrieved using an unauthenticated
  // protocol, such as HTTP or FTP, or it was retrieved using a protocol that
  // supports authentication, such as HTTPS, but there were errors during
  // transmission that render us uncertain to the object's authenticity.
  SECURITY_STYLE_UNAUTHENTICATED,

  // SECURITY_STYLE_AUTHENTICATION_BROKEN indicates that we tried to retrieve
  // this object in an authenticated manner but were unable to do so.
  SECURITY_STYLE_AUTHENTICATION_BROKEN,

  // SECURITY_STYLE_AUTHENTICATED indicates that we successfully retrieved this
  // object over an authenticated protocol, such as HTTPS.
  SECURITY_STYLE_AUTHENTICATED,
  SECURITY_STYLE_LAST = SECURITY_STYLE_AUTHENTICATED
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SECURITY_SECURITY_STYLE_H_
