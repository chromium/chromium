// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/file_type_conversion.h"

#include "base/check_op.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/shared_impl/time_conversion.h"

namespace ppapi {

int FileErrorToPepperError(base::File::Error error_code) {
  switch (error_code) {
    case base::File::FILE_OK:
      return PP_OK;
    case base::File::FILE_ERROR_EXISTS:
      return PP_ERROR_FILEEXISTS;
    case base::File::FILE_ERROR_NOT_FOUND:
      return PP_ERROR_FILENOTFOUND;
    case base::File::FILE_ERROR_ACCESS_DENIED:
    case base::File::FILE_ERROR_SECURITY:
      return PP_ERROR_NOACCESS;
    case base::File::FILE_ERROR_NO_MEMORY:
      return PP_ERROR_NOMEMORY;
    case base::File::FILE_ERROR_NO_SPACE:
      return PP_ERROR_NOSPACE;
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
      return PP_ERROR_FAILED;
    case base::File::FILE_ERROR_NOT_A_FILE:
      return PP_ERROR_NOTAFILE;
    default:
      return PP_ERROR_FAILED;
  }
}

bool PepperFileOpenFlagsToPlatformFileFlags(int32_t pp_open_flags,
                                            int* flags_out) {
  bool pp_read = !!(pp_open_flags & PP_FILEOPENFLAG_READ);
  bool pp_write = !!(pp_open_flags & PP_FILEOPENFLAG_WRITE);
  bool pp_create = !!(pp_open_flags & PP_FILEOPENFLAG_CREATE);
  bool pp_truncate = !!(pp_open_flags & PP_FILEOPENFLAG_TRUNCATE);
  bool pp_exclusive = !!(pp_open_flags & PP_FILEOPENFLAG_EXCLUSIVE);
  bool pp_append = !!(pp_open_flags & PP_FILEOPENFLAG_APPEND);

  // Pepper allows Touch on any open file, so always set this Windows-only flag.
  int flags = base::File::FLAG_WRITE_ATTRIBUTES;

  if (pp_read)
    flags |= base::File::FLAG_READ;
  if (pp_write) {
    flags |= base::File::FLAG_WRITE;
  }
  if (pp_append) {
    if (pp_write)
      return false;
    flags |= base::File::FLAG_APPEND;
  }

  if (pp_truncate && !pp_write)
    return false;

  if (pp_create) {
    if (pp_exclusive) {
      flags |= base::File::FLAG_CREATE;
    } else if (pp_truncate) {
      flags |= base::File::FLAG_CREATE_ALWAYS;
    } else {
      flags |= base::File::FLAG_OPEN_ALWAYS;
    }
  } else if (pp_truncate) {
    flags |= base::File::FLAG_OPEN_TRUNCATED;
  } else {
    flags |= base::File::FLAG_OPEN;
  }

  if (flags_out)
    *flags_out = flags;
  return true;
}

void FileInfoToPepperFileInfo(const base::File::Info& info,
                              PP_FileSystemType fs_type,
                              PP_FileInfo* info_out) {
  DCHECK(info_out);
  info_out->size = info.size;
  info_out->creation_time = TimeToPPTime(info.creation_time);
  info_out->last_access_time = TimeToPPTime(info.last_accessed);
  info_out->last_modified_time = TimeToPPTime(info.last_modified);
  info_out->system_type = fs_type;
  if (info.is_directory) {
    info_out->type = PP_FILETYPE_DIRECTORY;
  } else if (info.is_symbolic_link) {
    DCHECK_EQ(PP_FILESYSTEMTYPE_EXTERNAL, fs_type);
    info_out->type = PP_FILETYPE_OTHER;
  } else {
    info_out->type = PP_FILETYPE_REGULAR;
  }
}

}  // namespace ppapi
