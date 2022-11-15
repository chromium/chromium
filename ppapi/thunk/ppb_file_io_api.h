// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FILE_IO_API_H_
#define PPAPI_THUNK_PPB_FILE_IO_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_FileIO_API {
 public:
  virtual ~PPB_FileIO_API() {}

  virtual int32_t Open(PP_Resource file_ref,
                       int32_t open_flags,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Query(PP_FileInfo* info,
                        scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Touch(PP_Time last_access_time,
                        PP_Time last_modified_time,
                        scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Read(int64_t offset,
                       char* buffer,
                       int32_t bytes_to_read,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t ReadToArray(int64_t offset,
                              int32_t max_read_length,
                              PP_ArrayOutput* buffer,
                              scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Write(int64_t offset,
                        const char* buffer,
                        int32_t bytes_to_write,
                        scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t SetLength(int64_t length,
                            scoped_refptr<TrackedCallback> callback) = 0;
  virtual int64_t GetMaxWrittenOffset() const = 0;
  virtual int64_t GetAppendModeWriteAmount() const = 0;
  virtual void SetMaxWrittenOffset(int64_t max_written_offset) = 0;
  virtual void SetAppendModeWriteAmount(int64_t append_mode_write_amount) = 0;
  virtual int32_t Flush(scoped_refptr<TrackedCallback> callback) = 0;
  virtual void Close() = 0;

  // Private API.
  virtual int32_t RequestOSFileHandle(
      PP_FileHandle* handle,
      scoped_refptr<TrackedCallback> callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FILE_IO_API_H_
