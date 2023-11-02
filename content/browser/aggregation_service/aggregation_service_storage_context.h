// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_CONTEXT_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_CONTEXT_H_

#include "base/threading/sequence_bound.h"

namespace content {

class AggregationServiceStorage;

// Internal interface that provides access to the storage.
class AggregationServiceStorageContext {
 public:
  virtual ~AggregationServiceStorageContext() = default;

  // Returns the underlying storage for public keys and report requests.
  virtual const base::SequenceBound<AggregationServiceStorage>&
  GetStorage() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_CONTEXT_H_
