// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_PRINTING_API_H_
#define PPAPI_THUNK_PPB_PRINTING_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/dev/ppb_printing_dev.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPB_Printing_API {
 public:
  virtual ~PPB_Printing_API() {}

  virtual int32_t GetDefaultPrintSettings(
      PP_PrintSettings_Dev *print_settings,
      scoped_refptr<TrackedCallback> callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_PRINTING_API_H_
