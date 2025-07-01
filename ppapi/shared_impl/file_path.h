// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FILE_PATH_H_
#define PPAPI_SHARED_IMPL_FILE_PATH_H_

#include "base/files/file_path.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// TODO(vtl): Once we put |::FilePath| into the |base| namespace, get rid of the
// |Pepper| (or |PEPPER_|) prefixes. Right now, it's just too
// confusing/dangerous!

class PPAPI_SHARED_EXPORT PepperFilePath {
 public:
  enum Domain {
    DOMAIN_INVALID = 0,
    DOMAIN_ABSOLUTE,
    DOMAIN_MODULE_LOCAL,

    // Used for validity-checking.
    DOMAIN_MAX_VALID = DOMAIN_MODULE_LOCAL
  };

  PepperFilePath();
  PepperFilePath(Domain d, const base::FilePath& p);

  Domain domain() const { return domain_; }
  const base::FilePath& path() const { return path_; }

 private:
  Domain domain_;
  base::FilePath path_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FILE_PATH_H_
