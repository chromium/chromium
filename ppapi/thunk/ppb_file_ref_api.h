// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FILE_REF_API_H_
#define PPAPI_THUNK_PPB_FILE_REF_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/shared_impl/file_ref_create_info.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

struct FileRefCreateInfo;
class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_FileRef_API {
 public:
  virtual ~PPB_FileRef_API() {}

  virtual PP_FileSystemType GetFileSystemType() const = 0;
  virtual PP_Var GetName() const = 0;
  virtual PP_Var GetPath() const = 0;
  virtual PP_Resource GetParent() = 0;
  virtual int32_t MakeDirectory(int32_t make_directory_flags,
                                scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Touch(PP_Time last_access_time,
                        PP_Time last_modified_time,
                        scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Delete(scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Rename(PP_Resource new_file_ref,
                         scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Query(PP_FileInfo* info,
                        scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t ReadDirectoryEntries(
      const PP_ArrayOutput& output,
      scoped_refptr<TrackedCallback> callback) = 0;

  // Internal function for use in proxying. Returns the internal CreateInfo
  // (the contained resource does not carry a ref on behalf of the caller).
  virtual const FileRefCreateInfo& GetCreateInfo() const = 0;

  // Private API
  virtual PP_Var GetAbsolutePath() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FILE_REF_API_H_
