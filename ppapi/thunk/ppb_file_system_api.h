// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FILE_SYSTEM_API_H_
#define PPAPI_THUNK_PPB_FILE_SYSTEM_API_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/ppb_file_system.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPB_FileSystem_API {
 public:
  virtual ~PPB_FileSystem_API() {}

  virtual int32_t Open(int64_t expected_size,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_FileSystemType GetType() = 0;
  virtual void OpenQuotaFile(PP_Resource file_io) = 0;
  virtual void CloseQuotaFile(PP_Resource file_io) = 0;
  typedef base::OnceCallback<void(int64_t)> RequestQuotaCallback;
  virtual int64_t RequestQuota(int64_t amount,
                               RequestQuotaCallback callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FILE_SYSTEM_API_H_
