// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/file_io.h"

#include <string.h>  // memcpy

#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileIO_1_0>() {
  return PPB_FILEIO_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_FileIO_1_1>() {
  return PPB_FILEIO_INTERFACE_1_1;
}

}  // namespace

FileIO::FileIO() {}

FileIO::FileIO(const InstanceHandle& instance) {
  if (has_interface<PPB_FileIO_1_1>()) {
    PassRefFromConstructor(get_interface<PPB_FileIO_1_1>()->Create(
        instance.pp_instance()));
  } else if (has_interface<PPB_FileIO_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_FileIO_1_0>()->Create(
        instance.pp_instance()));
  }
}

FileIO::FileIO(const FileIO& other) : Resource(other) {}

FileIO& FileIO::operator=(const FileIO& other) {
  Resource::operator=(other);
  return *this;
}

int32_t FileIO::Open(const FileRef& file_ref,
                     int32_t open_flags,
                     const CompletionCallback& cc) {
  if (has_interface<PPB_FileIO_1_1>()) {
    return get_interface<PPB_FileIO_1_1>()->Open(
        pp_resource(), file_ref.pp_resource(), open_flags,
        cc.pp_completion_callback());
  } else if (has_interface<PPB_FileIO_1_0>()) {
    return get_interface<PPB_FileIO_1_0>()->Open(
        pp_resource(), file_ref.pp_resource(), open_flags,
        cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileIO::Query(PP_FileInfo* result_buf,
                      const CompletionCallback& cc) {
  if (has_interface<PPB_FileIO_1_1>()) {
    return get_interface<PPB_FileIO_1_1>()->Query(
        pp_resource(), result_buf, cc.pp_completion_callback());
  } else if (has_interface<PPB_FileIO_1_0>()) {
    return get_interface<PPB_FileIO_1_0>()->Query(
        pp_resource(), result_buf, cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileIO::Touch(PP_Time last_access_time,
                      PP_Time last_modified_time,
                      const CompletionCallback& cc) {
  if (has_interface<PPB_FileIO_1_1>()) {
    return get_interface<PPB_FileIO_1_1>()->Touch(
        pp_resource(), last_access_time, last_modified_time,
        cc.pp_completion_callback());
  } else if (has_interface<PPB_FileIO_1_0>()) {
    return get_interface<PPB_FileIO_1_0>()->Touch(
        pp_resource(), last_access_time, last_modified_time,
        cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileIO::Read(int64_t offset,
                     char* buffer,
                     int32_t bytes_to_read,
                     const CompletionCallback& cc) {
  if (has_interface<PPB_FileIO_1_1>()) {
    return get_interface<PPB_FileIO_1_1>()->Read(pp_resource(),
        offset, buffer, bytes_to_read, cc.pp_completion_callback());
  } else if (has_interface<PPB_FileIO_1_0>()) {
    return get_interface<PPB_FileIO_1_0>()->Read(pp_resource(),
        offset, buffer, bytes_to_read, cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileIO::Read(
    int32_t offset,
    int32_t max_read_length,
    const CompletionCallbackWithOutput< std::vector<char> >& cc) {
  if (has_interface<PPB_FileIO_1_1>()) {
    PP_ArrayOutput array_output = cc.output();
    return get_interface<PPB_FileIO_1_1>()->ReadToArray(pp_resource(),
        offset, max_read_length, &array_output,
        cc.pp_completion_callback());
  } else if (has_interface<PPB_FileIO_1_0>()) {
    // Data for our callback wrapper. The callback handler will delete it and
    // temp_buffer.
    CallbackData1_0* data = new CallbackData1_0;
    data->output = cc.output();
    data->temp_buffer = max_read_length >= 0 ? new char[max_read_length] : NULL;
    data->original_callback = cc.pp_completion_callback();

    // Actual returned bytes might not equals to max_read_length.  We need to
    // read to a temporary buffer first and copy later to make sure the array
    // buffer has correct size.
    return get_interface<PPB_FileIO_1_0>()->Read(
        pp_resource(), offset, data->temp_buffer, max_read_length,
        PP_MakeCompletionCallback(&CallbackConverter, data));
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileIO::Write(int64_t offset,
                      const char* buffer,
                      int32_t bytes_to_write,
                      const CompletionCallback& cc) {
  if (has_interface<PPB_FileIO_1_1>()) {
    return get_interface<PPB_FileIO_1_1>()->Write(
        pp_resource(), offset, buffer, bytes_to_write,
        cc.pp_completion_callback());
  } else if (has_interface<PPB_FileIO_1_0>()) {
    return get_interface<PPB_FileIO_1_0>()->Write(
        pp_resource(), offset, buffer, bytes_to_write,
        cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileIO::SetLength(int64_t length,
                          const CompletionCallback& cc) {
  if (has_interface<PPB_FileIO_1_1>()) {
    return get_interface<PPB_FileIO_1_1>()->SetLength(
        pp_resource(), length, cc.pp_completion_callback());
  } else if (has_interface<PPB_FileIO_1_0>()) {
    return get_interface<PPB_FileIO_1_0>()->SetLength(
        pp_resource(), length, cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileIO::Flush(const CompletionCallback& cc) {
  if (has_interface<PPB_FileIO_1_1>()) {
    return get_interface<PPB_FileIO_1_1>()->Flush(
        pp_resource(), cc.pp_completion_callback());
  } else if (has_interface<PPB_FileIO_1_0>()) {
    return get_interface<PPB_FileIO_1_0>()->Flush(
        pp_resource(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

void FileIO::Close() {
  if (has_interface<PPB_FileIO_1_1>())
    get_interface<PPB_FileIO_1_1>()->Close(pp_resource());
  else if (has_interface<PPB_FileIO_1_0>())
    get_interface<PPB_FileIO_1_0>()->Close(pp_resource());
}

// static
void FileIO::CallbackConverter(void* user_data, int32_t result) {
  CallbackData1_0* data = static_cast<CallbackData1_0*>(user_data);

  if (result >= 0) {
    // Copy to the destination buffer owned by the callback.
    char* buffer = static_cast<char*>(data->output.GetDataBuffer(
        data->output.user_data, result, sizeof(char)));
    memcpy(buffer, data->temp_buffer, result);
    delete[] data->temp_buffer;
  }

  // Now execute the original callback.
  PP_RunCompletionCallback(&data->original_callback, result);
  delete data;
}

}  // namespace pp
