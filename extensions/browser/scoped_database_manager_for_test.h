// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SCOPED_DATABASE_MANAGER_FOR_TEST_H_
#define EXTENSIONS_BROWSER_SCOPED_DATABASE_MANAGER_FOR_TEST_H_

#include "base/memory/scoped_refptr.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class ScopedDatabaseManagerForTest {
 public:
  explicit ScopedDatabaseManagerForTest(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager);

  ScopedDatabaseManagerForTest(const ScopedDatabaseManagerForTest&) = delete;
  ScopedDatabaseManagerForTest& operator=(const ScopedDatabaseManagerForTest&) =
      delete;

  ~ScopedDatabaseManagerForTest();

 private:
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> original_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SCOPED_DATABASE_MANAGER_FOR_TEST_H_
