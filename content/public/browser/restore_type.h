// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESTORE_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_RESTORE_TYPE_H_

namespace content {

// Enumerations of the possible restore types.
enum class RestoreType {
  // Restore from the previous session.
  LAST_SESSION,

  // The entry has been restored from the current session. This is used when
  // the user issues 'reopen closed tab'.
  CURRENT_SESSION,

  // The entry was not restored.
  NONE
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESTORE_TYPE_H_
