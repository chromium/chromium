// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_MANAGER_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_MANAGER_H_

#include "base/threading/sequence_bound.h"
#include "content/common/content_export.h"

namespace content {

class AggregationServiceKeyStorage;

// Interface that provides access to the storage.
class CONTENT_EXPORT AggregatableReportManager {
 public:
  virtual ~AggregatableReportManager() = default;

  virtual const base::SequenceBound<AggregationServiceKeyStorage>&
  GetKeyStorage() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_MANAGER_H_