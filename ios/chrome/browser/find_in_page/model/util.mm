// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/model/util.h"

bool IsNativeFindInPageAvailable() {
  if (@available(iOS 16.1.1, *)) {
    return true;
  }
  return false;
}
