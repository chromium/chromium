// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_LOAD_CDM_UMA_HELPER_H_
#define MEDIA_CDM_LOAD_CDM_UMA_HELPER_H_

#include <string>

#include "base/native_library.h"
#include "base/time/time.h"

namespace media {

//  These enums are reported to UMA so values should not be renumbered or
//  reused.
enum class CdmLoadResult {
  kLoadSuccess,
  kFileMissing,        // The CDM does not exist.
  kLoadFailed,         // CDM exists but LoadNativeLibrary() failed.
  kEntryPointMissing,  // CDM loaded but some required entry point missing.
  kMaxValue = kEntryPointMissing  // Max value for Uma Histogram Enumeration.
};

// Reports the result of loading CDM library to UMA.
void ReportLoadResult(const std::string& uma_prefix, CdmLoadResult load_result);

// Reports the error code of loading CDM library to UMA.
void ReportLoadErrorCode(const std::string& uma_prefix,
                         const base::NativeLibraryLoadError* error);

// Reports the loading time of CDM library to UMA.
void ReportLoadTime(const std::string& uma_prefix,
                    const base::TimeDelta load_time);

}  // namespace media

#endif  // MEDIA_CDM_LOAD_CDM_UMA_HELPER_H_
