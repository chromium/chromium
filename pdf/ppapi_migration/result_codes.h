// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_RESULT_CODES_H_
#define PDF_PPAPI_MIGRATION_RESULT_CODES_H_

namespace chrome_pdf {

// TODO(crbug.com/702993): After migrating away from PPAPI, re-evaluate where
// this enum should live, whether it should become an enum class, and what
// values it should contain.
enum Result {
  // Must match `PP_OK`.
  kSuccess = 0,

  // Must match `PP_ERROR_FAILED`.
  kErrorFailed = -2,

  // Must match `PP_ERROR_ABORTED`.
  kErrorAborted = -3,

  // Must match `PP_ERROR_BADARGUMENT`.
  kErrorBadArgument = -4,

  // Must match `PP_ERROR_NOACCESS`.
  kErrorNoAccess = -7,
};

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_RESULT_CODES_H_
