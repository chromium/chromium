// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_SEARCH_SERVICE_H_
#define IOS_CHROME_BROWSER_SEARCH_SEARCH_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

// Service which provides search functionality across the application tabs and
// history.
class SearchService : public KeyedService {
 public:
  SearchService();
  ~SearchService() override;

  SearchService(const SearchService&) = delete;
  SearchService& operator=(const SearchService&) = delete;
};

#endif  // IOS_CHROME_BROWSER_SEARCH_SEARCH_SERVICE_H_
