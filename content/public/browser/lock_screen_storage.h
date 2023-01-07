// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_LOCK_SCREEN_STORAGE_H_
#define CONTENT_PUBLIC_BROWSER_LOCK_SCREEN_STORAGE_H_

#include "content/common/content_export.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {

class BrowserContext;

// Global storage for lock screen data stored by websites. This isn't
// BrowserContext keyed because there can only ever be one lock screen for the
// entire browser (the primary user's BrowserContext).
class CONTENT_EXPORT LockScreenStorage {
 public:
  static LockScreenStorage* GetInstance();

  // LockScreenStorage must be initialized before any data is written to it or
  // read from it by the Lock Screen API. The BrowserContext associated with the
  // lock screen and the base path where data will be stored should be passed
  // in. There can be up to one lock screen for the entire browser, so
  // this can be called only once with the BrowserContext associated with the
  // lock screen.
  virtual void Init(BrowserContext* browser_context,
                    const base::FilePath& base_path) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_LOCK_SCREEN_STORAGE_H_
