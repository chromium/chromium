// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_FILE_IO_H_
#define PPAPI_CPP_FILE_IO_H_

#include <stdint.h>

#include "ppapi/c/pp_time.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the API to create a file i/o object.

struct PP_FileInfo;

namespace pp {

class FileRef;
class InstanceHandle;

/// The <code>FileIO</code> class represents a regular file.
class FileIO : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>FileIO</code>
  /// object.
  FileIO();

  /// A constructor used to create a <code>FileIO</code> and associate it with
  /// the provided <code>Instance</code>.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit FileIO(const InstanceHandle& instance);

  /// The copy constructor for <code>FileIO</code>.
  ///
  /// @param[in] other A reference to a <code>FileIO</code>.
  FileIO(const FileIO& other);
  FileIO& operator=(const FileIO& other);

  /// Open() opens the specified regular file for I/O according to the given
  /// open flags, which is a bit-mask of the PP_FileOpenFlags values.  Upon
  /// success, the corresponding file is classified as "in use" by this FileIO
  /// object until such time as the FileIO object is closed or destroyed.
  ///
  /// @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
  /// reference.
  ///
  /// @param[in] open_flags A bit-mask of the <code>PP_FileOpenFlags</code>
  /// values. Valid values are:
  ///  - PP_FILEOPENFLAG_READ
  ///  - PP_FILEOPENFLAG_WRITE
  ///  - PP_FILEOPENFLAG_CREATE
  ///  - PP_FILEOPENFLAG_TRUNCATE
  ///  - PP_FILEOPENFLAG_EXCLUSIVE
  /// See <code>PP_FileOpenFlags</code> in <code>ppb_file_io.h</code> for more
  /// details on these flags.
  ///
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Open().
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t Open(const FileRef& file_ref,
               int32_t open_flags,
               const CompletionCallback& cc);

  /// Query() queries info about the file opened by this FileIO object. This
  /// function will fail if the FileIO object has not been opened.
  ///
  /// @param[in] result_buf The <code>PP_FileInfo</code> structure representing
  /// all information about the file.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Query(). <code>result_buf</code> must remain valid until
  /// after the callback runs. If you pass a blocking callback,
  /// <code>result_buf</code> must remain valid until after Query() returns.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t Query(PP_FileInfo* result_buf,
                const CompletionCallback& cc);

  /// Touch() Updates time stamps for the file opened by this FileIO object.
  /// This function will fail if the FileIO object has not been opened.
  ///
  /// @param[in] last_access_time The last time the FileIO was accessed.
  /// @param[in] last_modified_time The last time the FileIO was modified.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Touch().
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t Touch(PP_Time last_access_time,
                PP_Time last_modified_time,
                const CompletionCallback& cc);

  /// Reads from an offset in the file.
  ///
  /// The size of the buffer must be large enough to hold the specified number
  /// of bytes to read.  This function might perform a partial read, meaning
  /// that all the requested bytes might not be returned, even if the end of the
  /// file has not been reached.
  ///
  /// This function reads into a buffer that the caller supplies. This buffer
  /// must remain valid as long as the FileIO resource is alive. If you use
  /// a completion callback factory and it goes out of scope, it will not issue
  /// the callback on your class, BUT the callback factory can NOT cancel
  /// the request from the browser's perspective. This means that the browser
  /// will still try to write to your buffer even if the callback factory is
  /// destroyed!
  ///
  /// So you must ensure that your buffer outlives the FileIO resource. If you
  /// have one class and use the FileIO resource exclusively from that class
  /// and never make any copies, this will be fine: the resource will be
  /// destroyed when your class is. But keep in mind that copying a pp::FileIO
  /// object just creates a second reference to the original resource. For
  /// example, if you have a function like this:
  ///   pp::FileIO MyClass::GetFileIO();
  /// where a copy of your FileIO resource could outlive your class, the
  /// callback will still be pending when your class goes out of scope, creating
  /// the possibility of writing into invalid memory. So it's recommended to
  /// keep your FileIO resource and any output buffers tightly controlled in
  /// the same scope.
  ///
  /// <strong>Caveat:</strong> This Read() is potentially unsafe if you're using
  /// a CompletionCallbackFactory to scope callbacks to the lifetime of your
  /// class.  When your class goes out of scope, the callback factory will not
  /// actually cancel the callback, but will rather just skip issuing the
  /// callback on your class.  This means that if the FileIO object outlives
  /// your class (if you made a copy saved somewhere else, for example), then
  /// the browser will still try to write into your buffer when the
  /// asynchronous read completes, potentially causing a crash.
  ///
  /// See the other version of Read() which avoids this problem by writing into
  /// CompletionCallbackWithOutput, where the output buffer is automatically
  /// managed by the callback.
  ///
  /// @param[in] offset The offset into the file.
  /// @param[in] buffer The buffer to hold the specified number of bytes read.
  /// @param[in] bytes_to_read The number of bytes to read from
  /// <code>offset</code>.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Read(). <code>buffer</code> must remain valid until after
  /// the callback runs. If you pass a blocking callback, <code>buffer</code>
  /// must remain valid until after Read() returns.
  ///
  /// @return An The number of bytes read an error code from
  /// <code>pp_errors.h</code>. If the return value is 0, then end-of-file was
  /// reached. It is valid to call Read() multiple times with a completion
  /// callback to queue up parallel reads from the file at different offsets.
  int32_t Read(int64_t offset,
               char* buffer,
               int32_t bytes_to_read,
               const CompletionCallback& cc);

  /// Read() reads from an offset in the file.  A PP_ArrayOutput must be
  /// provided so that output will be stored in its allocated buffer.  This
  /// function might perform a partial read.
  ///
  /// @param[in] file_io A <code>PP_Resource</code> corresponding to a file
  /// FileIO.
  /// @param[in] offset The offset into the file.
  /// @param[in] max_read_length The maximum number of bytes to read from
  /// <code>offset</code>.
  /// @param[in] output A <code>PP_ArrayOutput</code> to hold the output data.
  /// @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
  /// completion of Read().
  ///
  /// @return The number of bytes read or an error code from
  /// <code>pp_errors.h</code>. If the return value is 0, then end-of-file was
  /// reached. It is valid to call Read() multiple times with a completion
  /// callback to queue up parallel reads from the file, but pending reads
  /// cannot be interleaved with other operations.
  int32_t Read(int32_t offset,
               int32_t max_read_length,
               const CompletionCallbackWithOutput< std::vector<char> >& cc);

  /// Write() writes to an offset in the file.  This function might perform a
  /// partial write. The FileIO object must have been opened with write access.
  ///
  /// @param[in] offset The offset into the file.
  /// @param[in] buffer The buffer to hold the specified number of bytes read.
  /// @param[in] bytes_to_write The number of bytes to write to
  /// <code>offset</code>.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Write().
  ///
  /// @return An The number of bytes written or an error code from
  /// <code>pp_errors.h</code>. If the return value is 0, then end-of-file was
  /// reached. It is valid to call Write() multiple times with a completion
  /// callback to queue up parallel writes to the file at different offsets.
  int32_t Write(int64_t offset,
                const char* buffer,
                int32_t bytes_to_write,
                const CompletionCallback& cc);

  /// SetLength() sets the length of the file.  If the file size is extended,
  /// then the extended area of the file is zero-filled.  The FileIO object must
  /// have been opened with write access.
  ///
  /// @param[in] length The length of the file to be set.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of SetLength().
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t SetLength(int64_t length,
                    const CompletionCallback& cc);

  /// Flush() flushes changes to disk.  This call can be very expensive!
  ///
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Flush().
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t Flush(const CompletionCallback& cc);

  /// Close() cancels any IO that may be pending, and closes the FileIO object.
  /// Any pending callbacks will still run, reporting
  /// <code>PP_ERROR_ABORTED</code> if pending IO was interrupted.  It is not
  /// valid to call Open() again after a call to this method.
  ///
  /// <strong>Note:</strong> If the FileIO object is destroyed, and it is still
  /// open, then it will be implicitly closed, so you are not required to call
  /// Close().
  void Close();

 private:
  struct CallbackData1_0 {
    PP_ArrayOutput output;
    char* temp_buffer;
    PP_CompletionCallback original_callback;
  };

  // Provide backwards-compatibility for older Read versions. Converts the
  // old-style "char*" output buffer of 1.0 to the new "PP_ArrayOutput"
  // interface in 1.1.
  //
  // This takes a heap-allocated CallbackData1_0 struct passed as the user data
  // and deletes it when the call completes.
  static void CallbackConverter(void* user_data, int32_t result);
};

}  // namespace pp

#endif  // PPAPI_CPP_FILE_IO_H_
