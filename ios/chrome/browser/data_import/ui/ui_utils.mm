// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/ui/ui_utils.h"

UIEdgeInsets GetDataImportSeparatorInset(BOOL multiSelectionMode) {
  return UIEdgeInsetsMake(0, multiSelectionMode ? 102 : 60, 0, 0);
}
