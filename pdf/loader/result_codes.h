// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_LOADER_RESULT_CODES_H_
#define PDF_LOADER_RESULT_CODES_H_

namespace chrome_pdf {

// TODO(crbug.com/40511452): After migrating away from PPAPI, re-evaluate where
// this enum should live, whether it should become an enum class, and what
// values it should contain.
enum Result {
  kSuccess = 0,
  kErrorFailed = -2,
  kErrorAborted = -3,
  kErrorBadArgument = -4,
  kErrorNoAccess = -7,
};

}  // namespace chrome_pdf

#endif  // PDF_LOADER_RESULT_CODES_H_
