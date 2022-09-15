// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_TEMP_SCOPED_DIR_CLEANER_H_
#define SERVICES_PREFERENCES_TRACKED_TEMP_SCOPED_DIR_CLEANER_H_

#include "base/memory/ref_counted.h"

// Helper object to clear additional data for scoped temporary pref stores.
class TempScopedDirCleaner
    : public base::RefCountedThreadSafe<TempScopedDirCleaner> {
 protected:
  friend class base::RefCountedThreadSafe<TempScopedDirCleaner>;
  virtual ~TempScopedDirCleaner() {}
};

#endif  // SERVICES_PREFERENCES_TRACKED_TEMP_SCOPED_DIR_CLEANER_H_
