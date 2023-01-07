// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_FONT_WARMUP_WIN_H_
#define CONTENT_CHILD_FONT_WARMUP_WIN_H_

#include <stddef.h>

#include "base/files/file_path.h"
#include "content/common/content_export.h"

class SkFontMgr;
template <typename T> class sk_sp;

namespace content {

class GdiFontPatchData {
 public:
  GdiFontPatchData(const GdiFontPatchData&) = delete;
  GdiFontPatchData& operator=(const GdiFontPatchData&) = delete;

  virtual ~GdiFontPatchData() {}

 protected:
  GdiFontPatchData() {}
};

// Hook a module's imported GDI font functions to reimplement font enumeration
// and font data retrieval for DLLs which can't be easily modified.
CONTENT_EXPORT GdiFontPatchData* PatchGdiFontEnumeration(
    const base::FilePath& path);

// Testing method to get the number of in-flight emulated GDI handles.
CONTENT_EXPORT size_t GetEmulatedGdiHandleCountForTesting();

// Testing method to reset the table of emulated GDI handles.
CONTENT_EXPORT void ResetEmulatedGdiHandlesForTesting();

// Sets the pre-sandbox warmup font manager directly. This should only be used
// for testing the implementation.
CONTENT_EXPORT void SetPreSandboxWarmupFontMgrForTesting(
    sk_sp<SkFontMgr> fontmgr);

// Directwrite connects to the font cache service to retrieve information about
// fonts installed on the system etc. This works well outside the sandbox and
// within the sandbox as long as the lpc connection maintained by the current
// process with the font cache service remains valid. It appears that there
// are cases when this connection is dropped after which directwrite is unable
// to connect to the font cache service which causes problems with characters
// disappearing.
// Directwrite has fallback code to enumerate fonts if it is unable to connect
// to the font cache service. We need to intercept the following APIs to
// ensure that it does not connect to the font cache service.
// NtALpcConnectPort
// OpenSCManagerW
// OpenServiceW
// StartServiceW
// CloseServiceHandle.
// These are all IAT patched.
CONTENT_EXPORT void PatchServiceManagerCalls();

}  // namespace content

#endif  // CONTENT_CHILD_FONT_WARMUP_WIN_H_
