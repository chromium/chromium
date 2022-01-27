// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace content {

bool UpgradeAttributionStorageSqlSchema(sql::Database* db,
                                        sql::MetaTable* meta_table) {
  DCHECK(db);
  DCHECK(meta_table);

  base::ThreadTicks start_timestamp = base::ThreadTicks::Now();

  base::UmaHistogramMediumTimes("Conversions.Storage.MigrationTime",
                                base::ThreadTicks::Now() - start_timestamp);
  return true;
}

}  // namespace content
