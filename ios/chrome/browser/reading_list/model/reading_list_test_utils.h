// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_TEST_UTILS_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

// Returns a factory for ProfileKeyedServiceFactoryIOS to construct a
// ReadingListModel instance that uses a fake persistency layer. The
// constructed model starts loaded and initially contains the entries
// provided in `initial_entries`.
ProfileKeyedServiceFactoryIOS::TestingFactory
ReadingListModelTestingFactoryWithFakeStorage(
    std::vector<scoped_refptr<ReadingListEntry>> initial_entries);

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_TEST_UTILS_H_
