// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_DATA_REMOVER_H_
#define IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_DATA_REMOVER_H_

#import "base/memory/raw_ptr.h"

class ProfileIOS;

// A class that removes data for cross-platform promos.
class CrossPlatformPromosDataRemover {
 public:
  explicit CrossPlatformPromosDataRemover(ProfileIOS* profile);

  // Clears the prefs used by the cross-platform promos service.
  void Remove();

 private:
  raw_ptr<ProfileIOS> profile_;
};

#endif  // IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_DATA_REMOVER_H_
