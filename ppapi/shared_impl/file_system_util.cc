// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/file_system_util.h"

#include <ostream>

#include "base/notreached.h"

namespace ppapi {

bool FileSystemTypeIsValid(PP_FileSystemType type) {
  return (type == PP_FILESYSTEMTYPE_LOCALPERSISTENT ||
          type == PP_FILESYSTEMTYPE_LOCALTEMPORARY ||
          type == PP_FILESYSTEMTYPE_EXTERNAL ||
          type == PP_FILESYSTEMTYPE_ISOLATED);
}

bool FileSystemTypeHasQuota(PP_FileSystemType type) {
  return (type == PP_FILESYSTEMTYPE_LOCALTEMPORARY ||
          type == PP_FILESYSTEMTYPE_LOCALPERSISTENT);
}

std::string IsolatedFileSystemTypeToRootName(
    PP_IsolatedFileSystemType_Private type) {
  switch (type) {
    case PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_CRX:
      return "crxfs";
    default:
      NOTREACHED() << type;
  }
}

}  // namespace ppapi
