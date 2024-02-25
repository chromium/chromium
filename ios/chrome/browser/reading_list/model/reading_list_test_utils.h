// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_TEST_UTILS_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "ios/web/public/browser_state.h"

// Function that can be used as testing factory to construct ReadingListModel
// instances that use a fake persistency layer. The constructed model starts
// loaded and initially contains the entries provided in `initial_entries`.
std::unique_ptr<KeyedService> BuildReadingListModelWithFakeStorage(
    const std::vector<scoped_refptr<ReadingListEntry>>& initial_entries,
    web::BrowserState* context);

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_TEST_UTILS_H_
