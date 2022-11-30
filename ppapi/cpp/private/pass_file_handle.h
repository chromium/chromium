// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_PASS_FILE_HANDLE_H_
#define PPAPI_CPP_PRIVATE_PASS_FILE_HANDLE_H_

#include <string.h>

#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/cpp/output_traits.h"

namespace pp {

// A wrapper class for PP_FileHandle to make sure a file handle is
// closed. This object takes the ownership of the file handle when it
// is constructed. This loses the ownership when this object is
// assigned to another object, just like auto_ptr.
class PassFileHandle {
 public:
  PassFileHandle();
  // This constructor takes the ownership of |handle|.
  explicit PassFileHandle(PP_FileHandle handle);
  // Moves the ownership of |handle| to this object.
  PassFileHandle(PassFileHandle& handle);
  ~PassFileHandle();

  // Releases |handle_|. The caller must close the file handle returned.
  PP_FileHandle Release();

 private:
  // PassFileHandleRef allows users to return PassFileHandle as a
  // value. This technique is also used by auto_ptr_ref.
  struct PassFileHandleRef {
    PP_FileHandle handle;
    explicit PassFileHandleRef(PP_FileHandle h)
        : handle(h) {
    }
  };

 public:
  PassFileHandle(PassFileHandleRef ref)
      : handle_(ref.handle) {
  }

  operator PassFileHandleRef() {
    return PassFileHandleRef(Release());
  }

 private:
  void operator=(const PassFileHandle&);

  void Close();

  PP_FileHandle handle_;
};

namespace internal {

template<>
struct CallbackOutputTraits<PassFileHandle> {
  typedef PP_FileHandle* APIArgType;
  typedef PP_FileHandle StorageType;

  static inline APIArgType StorageToAPIArg(StorageType& t) {
    return &t;
  }

  static inline PassFileHandle StorageToPluginArg(StorageType& t) {
    return PassFileHandle(t);
  }

  static inline void Initialize(StorageType* t) {
    memset(t, 0, sizeof(*t));
  }
};

}  // namespace internal
}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_PASS_FILE_HANDLE_H_
