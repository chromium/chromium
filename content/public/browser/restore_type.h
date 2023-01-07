// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESTORE_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_RESTORE_TYPE_H_

namespace content {

// Enumerations of the possible restore types.
enum class RestoreType {
  // The entry has been restored (either from the previous session OR from the
  // current session - e.g. via 'reopen closed tab').
  kRestored,

  // The entry was not restored.
  kNotRestored
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESTORE_TYPE_H_
