// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "load_cdm_uma_helper.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/native_library.h"
#include "base/time/time.h"

namespace media {

void ReportLoadResult(const std::string& uma_prefix,
                      CdmLoadResult load_result) {
  DCHECK_LE(load_result, CdmLoadResult::kMaxValue);
  base::UmaHistogramEnumeration(uma_prefix + "LoadResult", load_result);
}

void ReportLoadErrorCode(const std::string& uma_prefix,
                         const base::NativeLibraryLoadError* error) {
// Only report load error code on Windows because that's the only platform that
// has a numerical error value.
#if BUILDFLAG(IS_WIN)
  base::UmaHistogramSparse(uma_prefix + "LoadErrorCode", error->code);
#endif
}

void ReportLoadTime(const std::string& uma_prefix,
                    const base::TimeDelta load_time) {
  base::UmaHistogramTimes(uma_prefix + "LoadTime", load_time);
}

}  // namespace media
