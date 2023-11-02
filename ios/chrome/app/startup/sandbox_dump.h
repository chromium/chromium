// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_SANDBOX_DUMP_H_
#define IOS_CHROME_APP_STARTUP_SANDBOX_DUMP_H_

// Dumps the sandboxed directory accessible by Chrome to the document directory.
// The dump is then accessible using finder.
// The document directory is not copied as it is already accessible.
// Some files or directories cannot be copied, so they are silently skipped.
// This function makes file operation on main thread and can block.
void DumpSandboxIfRequested();

#endif  // IOS_CHROME_APP_STARTUP_SANDBOX_DUMP_H_
