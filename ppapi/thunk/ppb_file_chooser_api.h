// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FILE_CHOOSER_API_H_
#define PPAPI_THUNK_PPB_FILE_CHOOSER_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPB_FileChooser_API {
 public:
  virtual ~PPB_FileChooser_API() {}

  virtual int32_t Show(const PP_ArrayOutput& output,
                       scoped_refptr<TrackedCallback> callback) = 0;

  // Trusted API.
  virtual int32_t ShowWithoutUserGesture(
      PP_Bool save_as,
      PP_Var suggested_file_name,
      const PP_ArrayOutput& output,
      scoped_refptr<TrackedCallback> callback) = 0;

  // Version 0.5 API.
  virtual int32_t Show0_5(scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Resource GetNextChosenFile() = 0;

  // Trusted version 0.5 API.
  virtual int32_t ShowWithoutUserGesture0_5(
      PP_Bool save_as,
      PP_Var suggested_file_name,
      scoped_refptr<TrackedCallback> callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FILE_CHOOSER_API_H_
