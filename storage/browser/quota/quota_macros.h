// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_MACROS_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_MACROS_H_

#include "base/metrics/histogram_macros.h"

#define UMA_HISTOGRAM_MBYTES(name, sample)                                     \
  UMA_HISTOGRAM_CUSTOM_COUNTS((name), static_cast<int>((sample) / kMBytes), 1, \
                              10 * 1024 * 1024 /* 10TB */, 100)

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_MACROS_H_
