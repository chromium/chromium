// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_COVERAGE_UTILS_H_
#define CONTENT_COMMON_COVERAGE_UTILS_H_

#include <string>

#include "base/files/file.h"

namespace content {

base::File OpenCoverageFile();

}  // namespace content

#endif  // CONTENT_COMMON_COVERAGE_UTILS_H_
