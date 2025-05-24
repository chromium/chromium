// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_DESKTOP_SESSION_TYPE_UTIL_H_
#define REMOTING_HOST_WEBAUTHN_DESKTOP_SESSION_TYPE_UTIL_H_

namespace remoting {

enum class DesktopSessionType {
  // The current desktop session may be used remotely or locally, or we don't
  // have information about it.
  UNSPECIFIED,
  // The current desktop session can only be used remotely. E.g. a dedicated
  // remote session, or the device is a VM.
  REMOTE_ONLY,
  // The current desktop session can only be used locally. E.g. a dedicated
  // local session.
  LOCAL_ONLY,
};

DesktopSessionType GetDesktopSessionType();

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_DESKTOP_SESSION_TYPE_UTIL_H_
