// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_file_io.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_file_io_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_FileIO::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFileIO(instance);
}

PP_Bool IsFileIO(PP_Resource resource) {
  VLOG(4) << "PPB_FileIO::IsFileIO()";
  EnterResource<PPB_FileIO_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Open(PP_Resource file_io,
             PP_Resource file_ref,
             int32_t open_flags,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileIO::Open()";
  EnterResource<PPB_FileIO_API> enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->Open(file_ref, open_flags, enter.callback()));
}

int32_t Query(PP_Resource file_io,
              struct PP_FileInfo* info,
              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileIO::Query()";
  EnterResource<PPB_FileIO_API> enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Query(info, enter.callback()));
}

int32_t Touch(PP_Resource file_io,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileIO::Touch()";
  EnterResource<PPB_FileIO_API> enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Touch(
      last_access_time, last_modified_time, enter.callback()));
}

int32_t Read(PP_Resource file_io,
             int64_t offset,
             char* buffer,
             int32_t bytes_to_read,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileIO::Read()";
  EnterResource<PPB_FileIO_API> enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->Read(offset, buffer, bytes_to_read, enter.callback()));
}

int32_t Write(PP_Resource file_io,
              int64_t offset,
              const char* buffer,
              int32_t bytes_to_write,
              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileIO::Write()";
  EnterResource<PPB_FileIO_API> enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->Write(offset, buffer, bytes_to_write, enter.callback()));
}

int32_t SetLength(PP_Resource file_io,
                  int64_t length,
                  struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileIO::SetLength()";
  EnterResource<PPB_FileIO_API> enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->SetLength(length, enter.callback()));
}

int32_t Flush(PP_Resource file_io, struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileIO::Flush()";
  EnterResource<PPB_FileIO_API> enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Flush(enter.callback()));
}

void Close(PP_Resource file_io) {
  VLOG(4) << "PPB_FileIO::Close()";
  EnterResource<PPB_FileIO_API> enter(file_io, true);
  if (enter.failed())
    return;
  enter.object()->Close();
}

int32_t ReadToArray(PP_Resource file_io,
                    int64_t offset,
                    int32_t max_read_length,
                    struct PP_ArrayOutput* output,
                    struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileIO::ReadToArray()";
  EnterResource<PPB_FileIO_API> enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->ReadToArray(offset, max_read_length,
                                                     output, enter.callback()));
}

const PPB_FileIO_1_0 g_ppb_fileio_thunk_1_0 = {
    &Create, &IsFileIO, &Open,      &Query, &Touch,
    &Read,   &Write,    &SetLength, &Flush, &Close};

const PPB_FileIO_1_1 g_ppb_fileio_thunk_1_1 = {
    &Create, &IsFileIO,  &Open,  &Query, &Touch,      &Read,
    &Write,  &SetLength, &Flush, &Close, &ReadToArray};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_FileIO_1_0* GetPPB_FileIO_1_0_Thunk() {
  return &g_ppb_fileio_thunk_1_0;
}

PPAPI_THUNK_EXPORT const PPB_FileIO_1_1* GetPPB_FileIO_1_1_Thunk() {
  return &g_ppb_fileio_thunk_1_1;
}

}  // namespace thunk
}  // namespace ppapi
