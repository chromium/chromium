// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_file_io.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <vector>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/file_io_private.h"
#include "ppapi/cpp/private/pass_file_handle.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

#if defined(PPAPI_OS_WIN)
# include <io.h>
# include <windows.h>
// TODO(hamaji): Use standard windows APIs instead of compatibility layer?
# define lseek _lseek
# define read _read
# define write _write
# define ssize_t int
#else
# include <sys/mman.h>
# include <unistd.h>
#endif

REGISTER_TEST_CASE(FileIO);

namespace {

std::string ReportMismatch(const std::string& method_name,
                           const std::string& returned_result,
                           const std::string& expected_result) {
  return method_name + " returned '" + returned_result + "'; '" +
      expected_result + "' expected.";
}

std::string ReportOpenError(int32_t open_flags) {
  static const char* kFlagNames[] = {
    "PP_FILEOPENFLAG_READ",
    "PP_FILEOPENFLAG_WRITE",
    "PP_FILEOPENFLAG_CREATE",
    "PP_FILEOPENFLAG_TRUNCATE",
    "PP_FILEOPENFLAG_EXCLUSIVE"
  };

  std::string result = "FileIO:Open had unexpected behavior with flags: ";
  bool first_flag = true;
  for (int32_t mask = 1, index = 0; mask <= PP_FILEOPENFLAG_EXCLUSIVE;
       mask <<= 1, ++index) {
    if (mask & open_flags) {
      if (first_flag) {
        first_flag = false;
      } else {
        result += " | ";
      }
      result += kFlagNames[index];
    }
  }
  if (first_flag)
    result += "[None]";

  return result;
}

int32_t ReadEntireFile(PP_Instance instance,
                       pp::FileIO* file_io,
                       int32_t offset,
                       std::string* data,
                       CallbackType callback_type) {
  TestCompletionCallback callback(instance, callback_type);
  char buf[256];
  int32_t read_offset = offset;

  for (;;) {
    callback.WaitForResult(
        file_io->Read(read_offset, buf, sizeof(buf), callback.GetCallback()));
    if (callback.result() < 0)
      return callback.result();
    if (callback.result() == 0)
      break;
    read_offset += callback.result();
    data->append(buf, callback.result());
  }

  return PP_OK;
}

int32_t ReadToArrayEntireFile(PP_Instance instance,
                              pp::FileIO* file_io,
                              int32_t offset,
                              std::string* data,
                              CallbackType callback_type) {
  TestCompletionCallbackWithOutput< std::vector<char> > callback(
      instance, callback_type);

  for (;;) {
    callback.WaitForResult(file_io->Read(offset, 256, callback.GetCallback()));
    int32_t rv = callback.result();
    if (rv < 0)
      return rv;
    if (rv == 0)
      break;
    const std::vector<char>& output = callback.output();
    assert(rv == static_cast<int32_t>(output.size()));
    offset += rv;
    data->append(output.begin(), output.end());
  }

  return PP_OK;
}

#if !defined(PPAPI_OS_WIN)
bool ReadEntireFileFromFileHandle(int fd, std::string* data) {
  if (lseek(fd, 0, SEEK_SET) < 0)
    return false;
  data->clear();

  int ret;
  do {
    char buf[8192];
    ret = read(fd, buf, sizeof(buf));
    if (ret > 0)
      data->append(buf, ret);
  } while (ret > 0);
  return ret == 0;
}
#endif  // !defined(PPAPI_OS_WIN)

int32_t WriteEntireBuffer(PP_Instance instance,
                          pp::FileIO* file_io,
                          int32_t offset,
                          const std::string& data,
                          CallbackType callback_type) {
  TestCompletionCallback callback(instance, callback_type);
  int32_t write_offset = offset;
  const char* buf = data.c_str();
  int32_t size = static_cast<int32_t>(data.size());

  while (write_offset < offset + size) {
    callback.WaitForResult(file_io->Write(write_offset,
                                          &buf[write_offset - offset],
                                          size - write_offset + offset,
                                          callback.GetCallback()));
    if (callback.result() < 0)
      return callback.result();
    if (callback.result() == 0)
      return PP_ERROR_FAILED;
    write_offset += callback.result();
  }

  return PP_OK;
}

}  // namespace

bool TestFileIO::Init() {
  return CheckTestingInterface() && EnsureRunningOverHTTP();
}

void TestFileIO::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestFileIO, Open, filter);
  RUN_CALLBACK_TEST(TestFileIO, OpenDirectory, filter);
  RUN_CALLBACK_TEST(TestFileIO, ReadWriteSetLength, filter);
  RUN_CALLBACK_TEST(TestFileIO, ReadToArrayWriteSetLength, filter);
  RUN_CALLBACK_TEST(TestFileIO, TouchQuery, filter);
  RUN_CALLBACK_TEST(TestFileIO, AbortCalls, filter);
  RUN_CALLBACK_TEST(TestFileIO, ParallelReads, filter);
  RUN_CALLBACK_TEST(TestFileIO, ParallelWrites, filter);
  RUN_CALLBACK_TEST(TestFileIO, NotAllowMixedReadWrite, filter);
  RUN_CALLBACK_TEST(TestFileIO, RequestOSFileHandle, filter);
  RUN_CALLBACK_TEST(TestFileIO, RequestOSFileHandleWithOpenExclusive, filter);
  RUN_CALLBACK_TEST(TestFileIO, Mmap, filter);

  // TODO(viettrungluu): add tests:
  //  - that PP_ERROR_PENDING is correctly returned
  //  - that operations respect the file open modes (flags)
}

std::string TestFileIO::TestOpen() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_open");

  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  std::string result;
  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_READ,
      DONT_CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  // Test the behavior of the power set of
  //   { PP_FILEOPENFLAG_CREATE,
  //     PP_FILEOPENFLAG_TRUNCATE,
  //     PP_FILEOPENFLAG_EXCLUSIVE }.

  // First of all, none of them are specified.
  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE,
      DONT_CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE,
      CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_EXCLUSIVE,
      DONT_CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_TRUNCATE,
      DONT_CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE |
      PP_FILEOPENFLAG_EXCLUSIVE,
      CREATE_IF_DOESNT_EXIST | DONT_OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE | PP_FILEOPENFLAG_TRUNCATE,
      CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_EXCLUSIVE |
      PP_FILEOPENFLAG_TRUNCATE,
      DONT_CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE |
      PP_FILEOPENFLAG_EXCLUSIVE | PP_FILEOPENFLAG_TRUNCATE,
      CREATE_IF_DOESNT_EXIST | DONT_OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  // Invalid combination: PP_FILEOPENFLAG_TRUNCATE without
  // PP_FILEOPENFLAG_WRITE.
  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_READ | PP_FILEOPENFLAG_TRUNCATE,
      INVALID_FLAG_COMBINATION);
  if (!result.empty())
    return result;

  PASS();
}

std::string TestFileIO::TestOpenDirectory() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Make a directory.
  pp::FileRef dir_ref(file_system, "/test_dir_open_directory");
  callback.WaitForResult(dir_ref.MakeDirectory(
      PP_MAKEDIRECTORYFLAG_NONE, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Open the directory. This is expected to fail since directories cannot be
  // opened.
  pp::FileIO file_io(instance_);
  callback.WaitForResult(file_io.Open(dir_ref, PP_FILEOPENFLAG_READ,
                                      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_NOTAFILE, callback.result());

  PASS();
}

std::string TestFileIO::TestReadWriteSetLength() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_read_write_setlength");
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileIO file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_READ |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Write something to the file.
  int32_t rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 0,
                                 "test_test", callback_type());
  ASSERT_EQ(PP_OK, rv);

  // Attempt to read a negative number of bytes; it should fail.
  char buf[256];
  callback.WaitForResult(file_io.Read(0,
                                      buf,
                                      -1,
                                      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());

  // Read the entire file.
  std::string read_buffer;
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer,
                      callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("test_test"), read_buffer);

  // Truncate the file.
  callback.WaitForResult(file_io.SetLength(4, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Check the file contents.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer,
                      callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("test"), read_buffer);

  // Try to read past the end of the file.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 100, &read_buffer,
                      callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_TRUE(read_buffer.empty());

  // Write past the end of the file. The file should be zero-padded.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 8, "test",
                         callback_type());
  ASSERT_EQ(PP_OK, rv);

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer,
                      callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("test\0\0\0\0test", 12), read_buffer);

  // Extend the file.
  callback.WaitForResult(file_io.SetLength(16, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer,
                      callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("test\0\0\0\0test\0\0\0\0", 16), read_buffer);

  // Write in the middle of the file.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 4, "test",
                         callback_type());
  ASSERT_EQ(PP_OK, rv);

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer,
                      callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("testtesttest\0\0\0\0", 16), read_buffer);

  // Read from the middle of the file.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 4, &read_buffer,
                      callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("testtest\0\0\0\0", 12), read_buffer);

  // Append to the end of the file.
  pp::FileIO file_io2(instance_);
  callback.WaitForResult(file_io2.Open(file_ref,
                                       PP_FILEOPENFLAG_CREATE |
                                       PP_FILEOPENFLAG_READ |
                                       PP_FILEOPENFLAG_APPEND,
                                       callback.GetCallback()));
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io2, 0, "appended",
                         callback_type());
  ASSERT_EQ(PP_OK, rv);
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io2, 0, &read_buffer,
                      callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("testtesttest\0\0\0\0appended", 24), read_buffer);

  PASS();
}

// This is basically a copy of TestReadWriteSetLength, but with the new Read
// API.  With this test case, we can make sure the two Read's have the same
// behavior.
std::string TestFileIO::TestReadToArrayWriteSetLength() {
  if (callback_type() == PP_BLOCKING) {
    // This test does not make sense for blocking callbacks.
    PASS();
  }
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_read_write_setlength");
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileIO file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_READ |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Write something to the file.
  int32_t rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 0,
                                 "test_test", callback_type());
  ASSERT_EQ(PP_OK, rv);

  TestCompletionCallbackWithOutput< std::vector<char> > callback2(
      instance_->pp_instance(), callback_type());
  // Attempt to read a negative number of bytes; it should fail.
  callback2.WaitForResult(file_io.Read(0, -1, callback2.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback2);
  ASSERT_EQ(PP_ERROR_FAILED, callback2.result());

  // Read the entire file.
  std::string read_buffer;
  read_buffer.reserve(10);
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 0,
                             &read_buffer, callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("test_test"), read_buffer);

  // Truncate the file.
  callback.WaitForResult(file_io.SetLength(4, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, rv);

  // Check the file contents.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 0,
                             &read_buffer, callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("test"), read_buffer);

  // Try to read past the end of the file.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 100,
                             &read_buffer, callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_TRUE(read_buffer.empty());

  // Write past the end of the file. The file should be zero-padded.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 8, "test",
                         callback_type());
  ASSERT_EQ(PP_OK, rv);

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 0,
                             &read_buffer, callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("test\0\0\0\0test", 12), read_buffer);

  // Extend the file.
  callback.WaitForResult(file_io.SetLength(16, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 0,
                             &read_buffer, callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("test\0\0\0\0test\0\0\0\0", 16), read_buffer);

  // Write in the middle of the file.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 4, "test",
                         callback_type());
  ASSERT_EQ(PP_OK, rv);

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 0,
                             &read_buffer, callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("testtesttest\0\0\0\0", 16), read_buffer);

  // Read from the middle of the file.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 4,
                             &read_buffer, callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("testtest\0\0\0\0", 12), read_buffer);

  PASS();
}

std::string TestFileIO::TestTouchQuery() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef file_ref(file_system, "/file_touch");
  pp::FileIO file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Write some data to have a non-zero file size.
  callback.WaitForResult(file_io.Write(0, "test", 4, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(4, callback.result());

  const PP_Time last_access_time = 123 * 24 * 3600.0;
  // last_modified_time's granularity is 2 seconds
  // NOTE: In NaCl on Windows, NaClDescIO uses _fstat64 to retrieve file info.
  // This function returns strange values for very small time values (near the
  // Unix Epoch). For a value like 246.0, it returns -1. For larger values, it
  // returns values that are exactly an hour less. The value below is handled
  // correctly, and is only 100 days after the start of Unix time.
  const PP_Time last_modified_time = 100 * 24 * 3600.0;
  callback.WaitForResult(file_io.Touch(last_access_time, last_modified_time,
                                       callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  PP_FileInfo info;
  callback.WaitForResult(file_io.Query(&info, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  if ((info.size != 4) ||
      (info.type != PP_FILETYPE_REGULAR) ||
      (info.system_type != PP_FILESYSTEMTYPE_LOCALTEMPORARY))
      // Disabled due to DST-related failure: crbug.com/314579
      //(info.last_access_time != last_access_time) ||
      //(info.last_modified_time != last_modified_time))
    return "FileIO::Query() has returned bad data.";

  // Call |Query()| again, to make sure it works a second time.
  callback.WaitForResult(file_io.Query(&info, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  PASS();
}

std::string TestFileIO::TestAbortCalls() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_abort_calls");
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  int32_t rv = PP_ERROR_FAILED;
  // First, create a file on which to do ops.
  {
    pp::FileIO file_io(instance_);
    callback.WaitForResult(
        file_io.Open(file_ref,
                     PP_FILEOPENFLAG_CREATE | PP_FILEOPENFLAG_WRITE,
                     callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());

    // N.B.: Should write at least 3 bytes.
    rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 0,
                           "foobarbazquux", callback_type());
    ASSERT_EQ(PP_OK, rv);
  }

  // Abort |Open()|.
  {
    rv = pp::FileIO(instance_)
        .Open(file_ref, PP_FILEOPENFLAG_READ, callback.GetCallback());
  }
  callback.WaitForAbortResult(rv);
  CHECK_CALLBACK_BEHAVIOR(callback);

  // Abort |Query()|.
  {
    PP_FileInfo info = { 0 };
    // Save a copy and make sure |info| doesn't get written to if it is aborted.
    PP_FileInfo info_copy;
    memcpy(&info_copy, &info, sizeof(info));
    {
      pp::FileIO file_io(instance_);
      callback.WaitForResult(file_io.Open(file_ref, PP_FILEOPENFLAG_READ,
                                          callback.GetCallback()));
      CHECK_CALLBACK_BEHAVIOR(callback);
      ASSERT_EQ(PP_OK, callback.result());

      rv = file_io.Query(&info, callback.GetCallback());
    }  // Destroy |file_io|.
    callback.WaitForResult(rv);
    CHECK_CALLBACK_BEHAVIOR(callback);
    if (callback_type() == PP_BLOCKING) {
      ASSERT_EQ(PP_OK, callback.result());
      // The operation completed synchronously, so |info| should have changed.
      ASSERT_NE(0, memcmp(&info_copy, &info, sizeof(info)));
    } else {
      ASSERT_EQ(PP_ERROR_ABORTED, callback.result());
      ASSERT_EQ(0, memcmp(&info_copy, &info, sizeof(info)));
    }
  }

  // Abort |Touch()|.
  {
    {
      pp::FileIO file_io(instance_);
      callback.WaitForResult(file_io.Open(file_ref, PP_FILEOPENFLAG_WRITE,
                                          callback.GetCallback()));
      CHECK_CALLBACK_BEHAVIOR(callback);
      ASSERT_EQ(PP_OK, callback.result());

      rv = file_io.Touch(0, 0, callback.GetCallback());
    }  // Destroy |file_io|.
    callback.WaitForAbortResult(rv);
    CHECK_CALLBACK_BEHAVIOR(callback);
  }

  // Abort |Read()|.
  {
    char buf[3] = { 0 };
    {
      pp::FileIO file_io(instance_);
      callback.WaitForResult(file_io.Open(file_ref, PP_FILEOPENFLAG_READ,
                                          callback.GetCallback()));
      CHECK_CALLBACK_BEHAVIOR(callback);
      ASSERT_EQ(PP_OK, callback.result());

      rv = file_io.Read(0, buf, sizeof(buf), callback.GetCallback());
    }  // Destroy |file_io|.
    // Save a copy to make sure buf isn't written to in the async case.
    char buf_copy[3];
    memcpy(&buf_copy, &buf, sizeof(buf));
    callback.WaitForResult(rv);
    CHECK_CALLBACK_BEHAVIOR(callback);
    if (callback_type() == PP_BLOCKING) {
      ASSERT_EQ(callback.result(), sizeof(buf));
    } else {
      ASSERT_EQ(PP_ERROR_ABORTED, callback.result());
      ASSERT_EQ(0, memcmp(&buf_copy, &buf, sizeof(buf)));
    }
  }

  // Abort |Write()|.
  {
    char buf[3] = { 0 };
    {
      pp::FileIO file_io(instance_);
      callback.WaitForResult(file_io.Open(file_ref, PP_FILEOPENFLAG_WRITE,
                                          callback.GetCallback()));
      CHECK_CALLBACK_BEHAVIOR(callback);
      ASSERT_EQ(PP_OK, callback.result());

      rv = file_io.Write(0, buf, sizeof(buf), callback.GetCallback());
    }  // Destroy |file_io|.
    callback.WaitForResult(rv);
    CHECK_CALLBACK_BEHAVIOR(callback);
    if (callback_type() == PP_BLOCKING)
      ASSERT_EQ(callback.result(), sizeof(buf));
    else
      ASSERT_EQ(PP_ERROR_ABORTED, callback.result());
  }

  // Abort |SetLength()|.
  {
    {
      pp::FileIO file_io(instance_);
      callback.WaitForResult(file_io.Open(file_ref, PP_FILEOPENFLAG_WRITE,
                                          callback.GetCallback()));
      CHECK_CALLBACK_BEHAVIOR(callback);
      ASSERT_EQ(PP_OK, callback.result());

      rv = file_io.SetLength(3, callback.GetCallback());
    }  // Destroy |file_io|.
    callback.WaitForAbortResult(rv);
    CHECK_CALLBACK_BEHAVIOR(callback);
  }

  // Abort |Flush|.
  {
    {
      pp::FileIO file_io(instance_);
      callback.WaitForResult(file_io.Open(file_ref, PP_FILEOPENFLAG_WRITE,
                                          callback.GetCallback()));
      CHECK_CALLBACK_BEHAVIOR(callback);
      ASSERT_EQ(PP_OK, callback.result());

      rv = file_io.Flush(callback.GetCallback());
    }  // Destroy |file_io|.
    callback.WaitForAbortResult(rv);
    CHECK_CALLBACK_BEHAVIOR(callback);
  }

  PASS();
}

std::string TestFileIO::TestParallelReads() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_parallel_reads");
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileIO file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_READ |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Set up testing contents.
  int32_t rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 0,
                                 "abcdefghijkl", callback_type());
  ASSERT_EQ(PP_OK, rv);

  // Parallel read operations.
  const char* border = "__border__";
  const int32_t border_size = static_cast<int32_t>(strlen(border));

  TestCompletionCallback callback_1(instance_->pp_instance(), callback_type());
  int32_t read_offset_1 = 0;
  int32_t size_1 = 3;
  std::vector<char> extended_buf_1(border_size * 2 + size_1);
  char* buf_1 = &extended_buf_1[border_size];
  memcpy(&extended_buf_1[0], border, border_size);
  memcpy(buf_1 + size_1, border, border_size);

  TestCompletionCallback callback_2(instance_->pp_instance(), callback_type());
  int32_t read_offset_2 = size_1;
  int32_t size_2 = 9;
  std::vector<char> extended_buf_2(border_size * 2 + size_2);
  char* buf_2 = &extended_buf_2[border_size];
  memcpy(&extended_buf_2[0], border, border_size);
  memcpy(buf_2 + size_2, border, border_size);

  int32_t rv_1 = PP_OK;
  int32_t rv_2 = PP_OK;
  while (size_1 >= 0 && size_2 >= 0 && size_1 + size_2 > 0) {
    if (size_1 > 0) {
      rv_1 = file_io.Read(read_offset_1, buf_1, size_1,
                          callback_1.GetCallback());
    }
    if (size_2 > 0) {
      rv_2 = file_io.Read(read_offset_2, buf_2, size_2,
                          callback_2.GetCallback());
    }
    if (size_1 > 0) {
      callback_1.WaitForResult(rv_1);
      CHECK_CALLBACK_BEHAVIOR(callback_1);
      ASSERT_TRUE(callback_1.result() > 0);
      read_offset_1 += callback_1.result();
      buf_1 += callback_1.result();
      size_1 -= callback_1.result();
    }

    if (size_2 > 0) {
      callback_2.WaitForResult(rv_2);
      CHECK_CALLBACK_BEHAVIOR(callback_2);
      ASSERT_TRUE(callback_2.result() > 0);
      read_offset_2 += callback_2.result();
      buf_2 += callback_2.result();
      size_2 -= callback_2.result();
    }
  }

  // If |size_1| or |size_2| is not 0, we have invoked wrong callback(s).
  ASSERT_EQ(0, size_1);
  ASSERT_EQ(0, size_2);

  // Make sure every read operation writes into the correct buffer.
  const char expected_result_1[] = "__border__abc__border__";
  const char expected_result_2[] = "__border__defghijkl__border__";
  ASSERT_TRUE(strncmp(&extended_buf_1[0], expected_result_1,
                      strlen(expected_result_1)) == 0);
  ASSERT_TRUE(strncmp(&extended_buf_2[0], expected_result_2,
                      strlen(expected_result_2)) == 0);
  PASS();
}

std::string TestFileIO::TestParallelWrites() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_parallel_writes");
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileIO file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_READ |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Parallel write operations.
  TestCompletionCallback callback_1(instance_->pp_instance(), callback_type());
  int32_t write_offset_1 = 0;
  const char* buf_1 = "abc";
  int32_t size_1 = static_cast<int32_t>(strlen(buf_1));

  TestCompletionCallback callback_2(instance_->pp_instance(), callback_type());
  int32_t write_offset_2 = size_1;
  const char* buf_2 = "defghijkl";
  int32_t size_2 = static_cast<int32_t>(strlen(buf_2));

  int32_t rv_1 = PP_OK;
  int32_t rv_2 = PP_OK;
  while (size_1 >= 0 && size_2 >= 0 && size_1 + size_2 > 0) {
    if (size_1 > 0) {
      // Copy the buffer so we can erase it below.
      std::string str_1(buf_1);
      rv_1 = file_io.Write(
          write_offset_1, &str_1[0], static_cast<int32_t>(str_1.size()),
          callback_1.GetCallback());
      // Erase the buffer to test that async writes copy it.
      std::fill(str_1.begin(), str_1.end(), 0);
    }
    if (size_2 > 0) {
      // Copy the buffer so we can erase it below.
      std::string str_2(buf_2);
      rv_2 = file_io.Write(
          write_offset_2, &str_2[0], static_cast<int32_t>(str_2.size()),
          callback_2.GetCallback());
      // Erase the buffer to test that async writes copy it.
      std::fill(str_2.begin(), str_2.end(), 0);
    }

    if (size_1 > 0) {
      callback_1.WaitForResult(rv_1);
      CHECK_CALLBACK_BEHAVIOR(callback_1);
      ASSERT_TRUE(callback_1.result() > 0);
      write_offset_1 += callback_1.result();
      buf_1 += callback_1.result();
      size_1 -= callback_1.result();
    }

    if (size_2 > 0) {
      callback_2.WaitForResult(rv_2);
      CHECK_CALLBACK_BEHAVIOR(callback_2);
      ASSERT_TRUE(callback_2.result() > 0);
      write_offset_2 += callback_2.result();
      buf_2 += callback_2.result();
      size_2 -= callback_2.result();
    }
  }

  // If |size_1| or |size_2| is not 0, we have invoked wrong callback(s).
  ASSERT_EQ(0, size_1);
  ASSERT_EQ(0, size_2);

  // Check the file contents.
  std::string read_buffer;
  int32_t rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0,
                              &read_buffer, callback_type());
  ASSERT_EQ(PP_OK, rv);
  ASSERT_EQ(std::string("abcdefghijkl"), read_buffer);

  PASS();
}

std::string TestFileIO::TestNotAllowMixedReadWrite() {
  if (callback_type() == PP_BLOCKING) {
    // This test does not make sense for blocking callbacks.
    PASS();
  }
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_not_allow_mixed_read_write");
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileIO file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_READ |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  TestCompletionCallback callback_1(instance_->pp_instance(), PP_REQUIRED);
  int32_t write_offset_1 = 0;
  const char* buf_1 = "mnopqrstuvw";
  int32_t rv_1 = file_io.Write(write_offset_1, buf_1,
                               static_cast<int32_t>(strlen(buf_1)),
                               callback_1.GetCallback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, rv_1);

  TestCompletionCallback callback_2(instance_->pp_instance(), callback_type());
  int32_t read_offset_2 = 4;
  char buf_2[3];
  callback_2.WaitForResult(file_io.Read(read_offset_2, buf_2, sizeof(buf_2),
                                        callback_2.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback_2);
  ASSERT_EQ(PP_ERROR_INPROGRESS, callback_2.result());
  callback_1.WaitForResult(rv_1);
  CHECK_CALLBACK_BEHAVIOR(callback_1);

  // Cannot query while a write is pending.
  rv_1 = file_io.Write(write_offset_1, buf_1,
                       static_cast<int32_t>(strlen(buf_1)),
                       callback_1.GetCallback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, rv_1);
  PP_FileInfo info;
  callback_2.WaitForResult(file_io.Query(&info, callback_2.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback_2);
  ASSERT_EQ(PP_ERROR_INPROGRESS, callback_2.result());
  callback_1.WaitForResult(rv_1);
  CHECK_CALLBACK_BEHAVIOR(callback_1);

  // Cannot touch while a write is pending.
  rv_1 = file_io.Write(write_offset_1, buf_1,
                       static_cast<int32_t>(strlen(buf_1)),
                       callback_1.GetCallback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, rv_1);
  callback_2.WaitForResult(file_io.Touch(1234.0, 5678.0,
                           callback_2.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback_2);
  ASSERT_EQ(PP_ERROR_INPROGRESS, callback_2.result());
  callback_1.WaitForResult(rv_1);
  CHECK_CALLBACK_BEHAVIOR(callback_1);

  // Cannot set length while a write is pending.
  rv_1 = file_io.Write(write_offset_1, buf_1,
                       static_cast<int32_t>(strlen(buf_1)),
                       callback_1.GetCallback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, rv_1);
  callback_2.WaitForResult(file_io.SetLength(123, callback_2.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback_2);
  ASSERT_EQ(PP_ERROR_INPROGRESS, callback_2.result());
  callback_1.WaitForResult(rv_1);
  CHECK_CALLBACK_BEHAVIOR(callback_1);

  PASS();
}

std::string TestFileIO::TestRequestOSFileHandle() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_os_fd");

  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileIO_Private file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_READ |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());

  TestCompletionCallbackWithOutput<pp::PassFileHandle> output_callback(
      instance_->pp_instance(), callback_type());
  output_callback.WaitForResult(
      file_io.RequestOSFileHandle(output_callback.GetCallback()));
  PP_FileHandle handle = output_callback.output().Release();
  ASSERT_EQ(PP_OK, output_callback.result());

  if (handle == PP_kInvalidFileHandle)
    return "FileIO::RequestOSFileHandle() returned a bad file handle.";
#if defined(PPAPI_OS_WIN)
  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle),
                           _O_RDWR | _O_BINARY);
#else
  int fd = handle;
#endif
  if (fd < 0)
    return "FileIO::RequestOSFileHandle() returned a bad file descriptor.";

  // Check write(2) for the native FD.
  const std::string msg = "foobar";
  ssize_t cnt = write(fd, msg.data(), static_cast<unsigned>(msg.size()));
  if (cnt < 0)
    return ReportError("write for native FD returned error", errno);
  if (cnt != static_cast<ssize_t>(msg.size()))
    return ReportError("write for native FD count mismatch", cnt);

  // Check lseek(2) for the native FD.
  off_t off = lseek(fd, 0, SEEK_CUR);
  if (off == static_cast<off_t>(-1))
    return ReportError("lseek for native FD returned error", errno);
  if (off != static_cast<off_t>(msg.size()))
    return ReportError("lseek for native FD offset mismatch", off);

  off = lseek(fd, 0, SEEK_SET);
  if (off == static_cast<off_t>(-1))
    return ReportError("lseek for native FD returned error", errno);
  if (off != 0)
    return ReportError("lseek for native FD offset mismatch", off);

  // Check read(2) for the native FD.
  std::string buf(msg.size(), '\0');
  cnt = read(fd, &buf[0], static_cast<unsigned>(msg.size()));
  if (cnt < 0)
    return ReportError("read for native FD returned error", errno);
  if (cnt != static_cast<ssize_t>(msg.size()))
    return ReportError("read for native FD count mismatch", cnt);
  if (msg != buf)
    return ReportMismatch("read for native FD", buf, msg);
  PASS();
}

// Calling RequestOSFileHandle with the FileIO that is opened with
// PP_FILEOPENFLAG_EXCLUSIVE used to cause NaCl module to crash while loading.
// This is a regression test for crbug.com/243241.
std::string TestFileIO::TestRequestOSFileHandleWithOpenExclusive() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_os_fd2");

  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());

  // Open with PP_FILEOPENFLAG_CREATE and PP_FILEOPENFLAG_EXCLUSIVE will fail
  // if the file already exists. Delete it here to make sure it does not.
  callback.WaitForResult(file_ref.Delete(callback.GetCallback()));

  pp::FileIO_Private file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_READ |
                                      PP_FILEOPENFLAG_WRITE |
                                      PP_FILEOPENFLAG_EXCLUSIVE,
                                      callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());

  TestCompletionCallbackWithOutput<pp::PassFileHandle> output_callback(
      instance_->pp_instance(), callback_type());
  output_callback.WaitForResult(
      file_io.RequestOSFileHandle(output_callback.GetCallback()));
  PP_FileHandle handle = output_callback.output().Release();
  if (handle == PP_kInvalidFileHandle)
    return "FileIO::RequestOSFileHandle() returned a bad file handle.";
  ASSERT_EQ(PP_OK, output_callback.result());

  PASS();
}

std::string TestFileIO::TestMmap() {
#if !defined(PPAPI_OS_WIN)
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_os_fd");

  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileIO_Private file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_READ |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());

  TestCompletionCallbackWithOutput<pp::PassFileHandle> output_callback(
      instance_->pp_instance(), callback_type());
  output_callback.WaitForResult(
      file_io.RequestOSFileHandle(output_callback.GetCallback()));
  PP_FileHandle handle = output_callback.output().Release();
  ASSERT_EQ(PP_OK, output_callback.result());

  if (handle == PP_kInvalidFileHandle)
    return "FileIO::RequestOSFileHandle() returned a bad file handle.";
  int fd = handle;
  if (fd < 0)
    return "FileIO::RequestOSFileHandle() returned a bad file descriptor.";

  // Check write(2) for the native FD.
  const std::string msg = "foobar";
  ssize_t cnt = write(fd, msg.data(), msg.size());
  if (cnt < 0)
    return ReportError("write for native FD returned error", errno);
  if (cnt != static_cast<ssize_t>(msg.size()))
    return ReportError("write for native FD count mismatch", cnt);

  // BEGIN mmap(2) test with a file handle opened in READ-WRITE mode.
  // Check mmap(2) for read.
  {
    char* mapped = reinterpret_cast<char*>(
        mmap(NULL, msg.size(), PROT_READ, MAP_PRIVATE, fd, 0));
    if (mapped == MAP_FAILED)
      return ReportError("mmap(r) for native FD returned errno", errno);
    // Make sure the buffer is cleared.
    std::string buf = std::string(msg.size(), '\0');
    memcpy(&buf[0], mapped, msg.size());
    if (msg != buf)
      return ReportMismatch("mmap(r) for native FD", buf, msg);
    int r = munmap(mapped, msg.size());
    if (r < 0)
      return ReportError("munmap for native FD returned error", errno);
  }

  // Check mmap(2) for write with MAP_PRIVATE
  {
    char* mapped = reinterpret_cast<char*>(
        mmap(NULL, msg.size(), PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0));
    if (mapped == MAP_FAILED)
      return ReportError("mmap(r) for native FD returned errno", errno);
    // Make sure the file is not polluted by writing to privage mmap.
    strncpy(mapped, "baz", 3);
    std::string read_buffer;
    ASSERT_TRUE(ReadEntireFileFromFileHandle(fd, &read_buffer));
    if (msg != read_buffer)
      return ReportMismatch("file content != msg", read_buffer, msg);
    int r = munmap(mapped, msg.size());
    if (r < 0)
      return ReportError("munmap for native FD returned error", errno);
  }

  // Check mmap(2) for write with MAP_SHARED.
  {
    char* mapped = reinterpret_cast<char*>(
        mmap(NULL, msg.size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (mapped == MAP_FAILED)
      return ReportError("mmap(w) for native FD returned errno", errno);
    // s/foo/baz/
    strncpy(mapped, "baz", 3);
    std::string read_buffer;
    ASSERT_TRUE(ReadEntireFileFromFileHandle(fd, &read_buffer));
    if (read_buffer != "bazbar")
      return ReportMismatch("file content != msg", read_buffer, "bazbar");
    int r = munmap(mapped, msg.size());
    if (r < 0)
      return ReportError("munmap for native FD returned error", errno);
  }
  // END mmap(2) test with a file handle opened in READ-WRITE mode.

  if (close(fd) < 0)
    return ReportError("close for native FD returned error", errno);

  // BEGIN mmap(2) test with a file handle opened in READONLY mode.
  file_io = pp::FileIO_Private(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_READ,
                                      callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());

  output_callback = TestCompletionCallbackWithOutput<pp::PassFileHandle>(
      instance_->pp_instance(), callback_type());
  output_callback.WaitForResult(
      file_io.RequestOSFileHandle(output_callback.GetCallback()));
  handle = output_callback.output().Release();
  ASSERT_EQ(PP_OK, output_callback.result());

  if (handle == PP_kInvalidFileHandle)
    return "FileIO::RequestOSFileHandle() returned a bad file handle.";
  fd = handle;
  if (fd < 0)
    return "FileIO::RequestOSFileHandle() returned a bad file descriptor.";

  const std::string msg2 = "bazbar";

  // Check mmap(2) for read.
  {
    char* mapped = reinterpret_cast<char*>(
        mmap(NULL, msg2.size(), PROT_READ, MAP_PRIVATE, fd, 0));
    if (mapped == MAP_FAILED)
      return ReportError("mmap(r) for native FD returned errno", errno);
    // Make sure the buffer is cleared.
    std::string buf = std::string(msg2.size(), '\0');
    memcpy(&buf[0], mapped, msg2.size());
    if (msg2 != buf)
      return ReportMismatch("mmap(r) for native FD", buf, msg2);
    int r = munmap(mapped, msg2.size());
    if (r < 0)
      return ReportError("munmap for native FD returned error", errno);
  }

  // Check mmap(2) for write with MAP_PRIVATE
  {
    char* mapped = reinterpret_cast<char*>(
        mmap(NULL, msg2.size(), PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0));
    if (mapped == MAP_FAILED)
      return ReportError("mmap(r) for native FD returned errno", errno);
    // Make sure the file is not polluted by writing to privage mmap.
    strncpy(mapped, "baz", 3);
    std::string read_buffer;
    ASSERT_TRUE(ReadEntireFileFromFileHandle(fd, &read_buffer));
    if (msg2 != read_buffer)
      return ReportMismatch("file content != msg2", read_buffer, msg2);
    int r = munmap(mapped, msg2.size());
    if (r < 0)
      return ReportError("munmap for native FD returned error", errno);
  }

  // Check mmap(2) for write with MAP_SHARED.
  {
    char* mapped = reinterpret_cast<char*>(
        mmap(NULL, msg2.size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (mapped != MAP_FAILED)
      return ReportError("mmap(w) for native FD must fail when opened readonly",
                         -1);
  }
  // END mmap(2) test with a file handle opened in READONLY mode.

  if (close(fd) < 0)
    return ReportError("close for native FD returned error", errno);
#endif  // !defined(PPAPI_OS_WIN)

  PASS();
}

std::string TestFileIO::MatchOpenExpectations(pp::FileSystem* file_system,
                                              int32_t open_flags,
                                              int32_t expectations) {
  std::string bad_argument =
      "TestFileIO::MatchOpenExpectations has invalid input arguments.";
  bool invalid_combination = !!(expectations & INVALID_FLAG_COMBINATION);
  if (invalid_combination) {
    if (expectations != INVALID_FLAG_COMBINATION)
      return bad_argument;
  } else {
    // Validate that one and only one of <some_expectation> and
    // DONT_<some_expectation> is specified.
    for (size_t remains = expectations, end = END_OF_OPEN_EXPECATION_PAIRS;
         end != 0; remains >>= 2, end >>= 2) {
      if (!!(remains & 1) == !!(remains & 2))
        return bad_argument;
    }
  }
  bool create_if_doesnt_exist = !!(expectations & CREATE_IF_DOESNT_EXIST);
  bool open_if_exists = !!(expectations & OPEN_IF_EXISTS);
  bool truncate_if_exists = !!(expectations & TRUNCATE_IF_EXISTS);

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  pp::FileRef existent_file_ref(
      *file_system, "/match_open_expectation_existent_non_empty_file");
  pp::FileRef nonexistent_file_ref(
      *file_system, "/match_open_expectation_nonexistent_file");

  // Setup files for test.
  {
    callback.WaitForResult(existent_file_ref.Delete(callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_TRUE(callback.result() == PP_OK ||
                callback.result() == PP_ERROR_FILENOTFOUND);
    callback.WaitForResult(nonexistent_file_ref.Delete(callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_TRUE(callback.result() == PP_OK ||
                callback.result() == PP_ERROR_FILENOTFOUND);

    pp::FileIO existent_file_io(instance_);
    callback.WaitForResult(existent_file_io.Open(
        existent_file_ref,
        PP_FILEOPENFLAG_CREATE | PP_FILEOPENFLAG_WRITE,
        callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());
    int32_t rv = WriteEntireBuffer(instance_->pp_instance(), &existent_file_io,
                                   0, "foobar", callback_type());
    ASSERT_EQ(PP_OK, rv);
  }

  pp::FileIO existent_file_io(instance_);
  callback.WaitForResult(existent_file_io.Open(existent_file_ref, open_flags,
                                               callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  if ((invalid_combination && callback.result() == PP_OK) ||
      (!invalid_combination &&
       ((callback.result() == PP_OK) != open_if_exists))) {
    return ReportOpenError(open_flags);
  }

  if (!invalid_combination && open_if_exists) {
    PP_FileInfo info;
    callback.WaitForResult(existent_file_io.Query(&info,
                                                  callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());
    if (truncate_if_exists != (info.size == 0))
      return ReportOpenError(open_flags);
  }

  pp::FileIO nonexistent_file_io(instance_);
  callback.WaitForResult(nonexistent_file_io.Open(nonexistent_file_ref,
                                                  open_flags,
                                                  callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  if ((invalid_combination && callback.result() == PP_OK) ||
      (!invalid_combination &&
       ((callback.result() == PP_OK) != create_if_doesnt_exist))) {
    return ReportOpenError(open_flags);
  }

  return std::string();
}

// TODO(viettrungluu): Test Close(). crbug.com/69457
