// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_UTILS_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_UTILS_H_

#include "components/reading_list/core/reading_list_entry.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_ui_distillation_status.h"

namespace reading_list {

ReadingListUIDistillationStatus UIStatusFromModelStatus(
    ReadingListEntry::DistillationState distillation_state);

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_UTILS_H_
