// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_MACROS_H_
#define MEDIA_GPU_VAAPI_TEST_MACROS_H_

#include "base/logging.h"

#define VA_LOG_ASSERT(va_error, name)         \
  LOG_ASSERT((va_error) == VA_STATUS_SUCCESS) \
      << name << " failed, VA error: " << vaErrorStr(va_error);

#endif  // MEDIA_GPU_VAAPI_TEST_MACROS_H_
