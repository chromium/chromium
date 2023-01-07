// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_file_ref.h"

#include <stddef.h>
#include <stdio.h>

#include <sstream>
#include <vector>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/cpp/dev/file_chooser_dev.h"
#include "ppapi/cpp/directory_entry.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FileRef);

namespace {

const char* kPersFileName = "persistent";
const char* kTempFileName = "temporary";
const char* kParentPath = "/foo/bar";
const char* kPersFilePath = "/foo/bar/persistent";
const char* kTempFilePath = "/foo/bar/temporary";
const char* kTerribleName = "!@#$%^&*()-_=+{}[] ;:'\"|`~\t\n\r\b?";

typedef std::vector<pp::DirectoryEntry> DirEntries;

std::string ReportMismatch(const std::string& method_name,
                           const std::string& returned_result,
                           const std::string& expected_result) {
  return method_name + " returned '" + returned_result + "'; '" +
      expected_result + "' expected.";
}

}  // namespace

bool TestFileRef::Init() {
  return CheckTestingInterface() && EnsureRunningOverHTTP();
}

std::string TestFileRef::MakeExternalFileRef(pp::FileRef* file_ref_ext) {
  pp::FileChooser_Dev file_chooser(instance(), PP_FILECHOOSERMODE_OPEN, "*");
  ASSERT_FALSE(file_chooser.is_null());

  TestCompletionCallbackWithOutput<std::vector<pp::FileRef> >
      filechooser_callback(instance_->pp_instance(), callback_type());
  filechooser_callback.WaitForResult(
      file_chooser.Show(filechooser_callback.GetCallback()));

  const std::vector<pp::FileRef>& output_ref = filechooser_callback.output();
  ASSERT_EQ(1u, output_ref.size());

  *file_ref_ext = output_ref[0];
  ASSERT_EQ(PP_FILESYSTEMTYPE_EXTERNAL, file_ref_ext->GetFileSystemType());
  PASS();
}

int32_t TestFileRef::DeleteDirectoryRecursively(pp::FileRef* dir) {
  if (!dir)
    return PP_ERROR_BADARGUMENT;

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  TestCompletionCallbackWithOutput<DirEntries> output_callback(
      instance_->pp_instance(), callback_type());

  output_callback.WaitForResult(
      dir->ReadDirectoryEntries(output_callback.GetCallback()));
  int32_t rv = output_callback.result();
  if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
    return rv;

  DirEntries entries = output_callback.output();
  for (DirEntries::const_iterator it = entries.begin();
       it != entries.end();
       ++it) {
    pp::FileRef file_ref = it->file_ref();
    if (it->file_type() == PP_FILETYPE_DIRECTORY) {
      rv = DeleteDirectoryRecursively(&file_ref);
      if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
        return rv;
    } else {
      callback.WaitForResult(file_ref.Delete(callback.GetCallback()));
      rv = callback.result();
      if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
        return rv;
    }
  }
  callback.WaitForResult(dir->Delete(callback.GetCallback()));
  return callback.result();
}

void TestFileRef::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestFileRef, Create, filter);
  RUN_CALLBACK_TEST(TestFileRef, GetFileSystemType, filter);
  RUN_CALLBACK_TEST(TestFileRef, GetName, filter);
  RUN_CALLBACK_TEST(TestFileRef, GetPath, filter);
  RUN_CALLBACK_TEST(TestFileRef, GetParent, filter);
  RUN_CALLBACK_TEST(TestFileRef, MakeDirectory, filter);
  RUN_CALLBACK_TEST(TestFileRef, QueryAndTouchFile, filter);
  RUN_CALLBACK_TEST(TestFileRef, DeleteFileAndDirectory, filter);
  RUN_CALLBACK_TEST(TestFileRef, RenameFileAndDirectory, filter);
  RUN_CALLBACK_TEST(TestFileRef, Query, filter);
  RUN_CALLBACK_TEST(TestFileRef, FileNameEscaping, filter);
  RUN_CALLBACK_TEST(TestFileRef, ReadDirectoryEntries, filter);
}

std::string TestFileRef::TestCreate() {
  std::vector<std::string> invalid_paths;
  invalid_paths.push_back("invalid_path");  // no '/' at the first character
  invalid_paths.push_back(std::string());   // empty path
  // The following are directory traversal checks
  invalid_paths.push_back("..");
  invalid_paths.push_back("/../invalid_path");
  invalid_paths.push_back("/../../invalid_path");
  invalid_paths.push_back("/invalid/../../path");
  const size_t num_invalid_paths = invalid_paths.size();

  pp::FileSystem file_system_pers(
      instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  pp::FileSystem file_system_temp(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  for (size_t j = 0; j < num_invalid_paths; ++j) {
    pp::FileRef file_ref_pers(file_system_pers, invalid_paths[j].c_str());
    if (file_ref_pers.pp_resource() != 0) {
      return "file_ref_pers expected to be invalid for path: " +
          invalid_paths[j];
    }
    pp::FileRef file_ref_temp(file_system_temp, invalid_paths[j].c_str());
    if (file_ref_temp.pp_resource() != 0) {
      return "file_ref_temp expected to be invalid for path: " +
          invalid_paths[j];
    }
  }
  PASS();
}

std::string TestFileRef::TestGetFileSystemType() {
  pp::FileSystem file_system_pers(
      instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  pp::FileSystem file_system_temp(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);

  pp::FileRef file_ref_pers(file_system_pers, kPersFilePath);
  if (file_ref_pers.GetFileSystemType() != PP_FILESYSTEMTYPE_LOCALPERSISTENT)
    return "file_ref_pers expected to be persistent.";

  pp::FileRef file_ref_temp(file_system_temp, kTempFilePath);
  if (file_ref_temp.GetFileSystemType() != PP_FILESYSTEMTYPE_LOCALTEMPORARY)
    return "file_ref_temp expected to be temporary.";

  pp::FileRef file_ref_ext;
  std::string result = MakeExternalFileRef(&file_ref_ext);
  if (!result.empty())
    return result;
  PASS();
}

std::string TestFileRef::TestGetName() {
  pp::FileSystem file_system_pers(
      instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  pp::FileSystem file_system_temp(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);

  pp::FileRef file_ref_pers(file_system_pers, kPersFilePath);
  std::string name = file_ref_pers.GetName().AsString();
  if (name != kPersFileName)
    return ReportMismatch("FileRef::GetName", name, kPersFileName);

  pp::FileRef file_ref_temp(file_system_temp, kTempFilePath);
  name = file_ref_temp.GetName().AsString();
  if (name != kTempFileName)
    return ReportMismatch("FileRef::GetName", name, kTempFileName);

  // Test the "/" case.
  pp::FileRef file_ref_slash(file_system_temp, "/");
  name = file_ref_slash.GetName().AsString();
  if (name != "/")
    return ReportMismatch("FileRef::GetName", name, "/");

  pp::FileRef file_ref_ext;
  std::string result = MakeExternalFileRef(&file_ref_ext);
  if (!result.empty())
    return result;
  name = file_ref_ext.GetName().AsString();
  ASSERT_FALSE(name.empty());

  PASS();
}

std::string TestFileRef::TestGetPath() {
  pp::FileSystem file_system_pers(
      instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  pp::FileSystem file_system_temp(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);

  pp::FileRef file_ref_pers(file_system_pers, kPersFilePath);
  ASSERT_EQ(kPersFilePath, file_ref_pers.GetPath().AsString());

  pp::FileRef file_ref_temp(file_system_temp, kTempFilePath);
  ASSERT_EQ(kTempFilePath, file_ref_temp.GetPath().AsString());

  pp::FileRef file_ref_ext;
  std::string result = MakeExternalFileRef(&file_ref_ext);
  if (!result.empty())
    return result;
  ASSERT_TRUE(file_ref_ext.GetPath().is_undefined());

  PASS();
}

std::string TestFileRef::TestGetParent() {
  pp::FileSystem file_system_pers(
      instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  pp::FileSystem file_system_temp(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);

  pp::FileRef file_ref_pers(file_system_pers, kPersFilePath);
  ASSERT_EQ(kParentPath, file_ref_pers.GetParent().GetPath().AsString());

  pp::FileRef file_ref_temp(file_system_temp, kTempFilePath);
  ASSERT_EQ(kParentPath, file_ref_temp.GetParent().GetPath().AsString());

  // Test the "/" case.
  pp::FileRef file_ref_slash(file_system_temp, "/");
  ASSERT_EQ("/", file_ref_slash.GetParent().GetPath().AsString());

  // Test the "/foo" case (the parent is "/").
  pp::FileRef file_ref_with_root_parent(file_system_temp, "/foo");
  ASSERT_EQ("/", file_ref_with_root_parent.GetParent().GetPath().AsString());

  pp::FileRef file_ref_ext;
  std::string result = MakeExternalFileRef(&file_ref_ext);
  if (!result.empty())
    return result;
  ASSERT_TRUE(file_ref_ext.GetParent().is_null());

  PASS();
}

std::string TestFileRef::TestMakeDirectory() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  // Open.
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Make a directory.
  pp::FileRef dir_ref(file_system, "/dir_make_dir");
  callback.WaitForResult(
      dir_ref.MakeDirectory(PP_MAKEDIRECTORYFLAG_NONE, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Make a directory on the existing path without exclusive flag.
  callback.WaitForResult(
      dir_ref.MakeDirectory(PP_MAKEDIRECTORYFLAG_NONE, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Making a directory should be aborted.
  int32_t rv = PP_ERROR_FAILED;
  {
    rv = pp::FileRef(file_system, "/dir_make_dir_abort")
        .MakeDirectory(PP_MAKEDIRECTORYFLAG_NONE, callback.GetCallback());
  }
  callback.WaitForAbortResult(rv);
  CHECK_CALLBACK_BEHAVIOR(callback);

  // Make nested directories.
  dir_ref = pp::FileRef(file_system, "/dir_make_nested_dir_1/dir");
  callback.WaitForResult(
      dir_ref.MakeDirectory(PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS,
                            callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  dir_ref = pp::FileRef(file_system, "/dir_make_nested_dir_2/dir");
  callback.WaitForResult(
      dir_ref.MakeDirectory(PP_MAKEDIRECTORYFLAG_NONE, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FILENOTFOUND, callback.result());

  // Ensure there is no directory on the path to test exclusive cases.
  dir_ref = pp::FileRef(file_system, "/dir_make_dir_exclusive");
  rv = DeleteDirectoryRecursively(&dir_ref);
  ASSERT_TRUE(rv == PP_OK || rv == PP_ERROR_FILENOTFOUND);

  // Make a directory exclusively.
  callback.WaitForResult(
      dir_ref.MakeDirectory(PP_MAKEDIRECTORYFLAG_EXCLUSIVE,
                            callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(
      dir_ref.MakeDirectory(PP_MAKEDIRECTORYFLAG_EXCLUSIVE,
                            callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FILEEXISTS, callback.result());

  PASS();
}

std::string TestFileRef::TestQueryAndTouchFile() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef file_ref(file_system, "/file_touch");
  pp::FileIO file_io(instance_);
  callback.WaitForResult(
      file_io.Open(file_ref,
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

  // Touch.
  const PP_Time last_access_time = 123 * 24 * 3600.0;
  // last_modified_time's granularity is 2 seconds
  // See note in test_file_io.cc for why we use this time.
  const PP_Time last_modified_time = 100 * 24 * 3600.0;
  callback.WaitForResult(file_ref.Touch(last_access_time, last_modified_time,
                                        callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Touch aborted.
  int32_t rv = PP_ERROR_FAILED;
  {
    rv = pp::FileRef(file_system, "/file_touch_abort")
        .Touch(last_access_time, last_modified_time, callback.GetCallback());
  }
  callback.WaitForResult(rv);
  CHECK_CALLBACK_BEHAVIOR(callback);
  if (rv == PP_OK_COMPLETIONPENDING) {
    // Touch tried to run asynchronously and should have been aborted.
    ASSERT_EQ(PP_ERROR_ABORTED, callback.result());
  } else {
    // Touch ran synchronously and should have failed because the file does not
    // exist.
    ASSERT_EQ(PP_ERROR_FILENOTFOUND, callback.result());
  }

  // Query.
  PP_FileInfo info;
  callback.WaitForResult(file_io.Query(&info, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  ASSERT_EQ(4, info.size);
  ASSERT_EQ(PP_FILETYPE_REGULAR, info.type);
  ASSERT_EQ(PP_FILESYSTEMTYPE_LOCALTEMPORARY, info.system_type);

  // Disabled due to DST-related failure: crbug.com/314579
  // ASSERT_EQ(last_access_time, info.last_access_time);
  // ASSERT_EQ(last_modified_time, info.last_modified_time);

  // Cancellation test.
  // TODO(viettrungluu): this test causes a bunch of LOG(WARNING)s; investigate.
  // TODO(viettrungluu): check |info| for late writes.
  {
    rv = pp::FileRef(file_system, "/file_touch").Touch(
        last_access_time, last_modified_time, callback.GetCallback());
  }
  callback.WaitForAbortResult(rv);
  CHECK_CALLBACK_BEHAVIOR(callback);

  PASS();
}

std::string TestFileRef::TestDeleteFileAndDirectory() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef file_ref(file_system, "/file_delete");
  pp::FileIO file_io(instance_);
  callback.WaitForResult(
      file_io.Open(file_ref, PP_FILEOPENFLAG_CREATE, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(file_ref.Delete(callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef dir_ref(file_system, "/dir_delete");
  callback.WaitForResult(dir_ref.MakeDirectory(
      PP_MAKEDIRECTORYFLAG_NONE, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(dir_ref.Delete(callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef nested_dir_ref(file_system, "/dir_delete_1/dir_delete_2");
  callback.WaitForResult(
      nested_dir_ref.MakeDirectory(PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS,
                                   callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Attempt to delete the parent directory (should fail; it's non-empty).
  pp::FileRef parent_dir_ref = nested_dir_ref.GetParent();
  callback.WaitForResult(parent_dir_ref.Delete(callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());

  pp::FileRef nonexistent_file_ref(file_system, "/nonexistent_file_delete");
  callback.WaitForResult(nonexistent_file_ref.Delete(callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FILENOTFOUND, callback.result());

  // Delete aborted.
  int32_t rv = PP_ERROR_FAILED;
  {
    pp::FileRef file_ref_abort(file_system, "/file_delete_abort");
    pp::FileIO file_io_abort(instance_);
    callback.WaitForResult(
        file_io_abort.Open(file_ref_abort, PP_FILEOPENFLAG_CREATE,
                           callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());
    rv = file_ref_abort.Delete(callback.GetCallback());
  }
  callback.WaitForAbortResult(rv);
  CHECK_CALLBACK_BEHAVIOR(callback);

  PASS();
}

std::string TestFileRef::TestRenameFileAndDirectory() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef file_ref(file_system, "/file_rename");
  pp::FileIO file_io(instance_);
  callback.WaitForResult(
      file_io.Open(file_ref, PP_FILEOPENFLAG_CREATE, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef target_file_ref(file_system, "/target_file_rename");
  callback.WaitForResult(
      file_ref.Rename(target_file_ref, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef dir_ref(file_system, "/dir_rename");
  callback.WaitForResult(dir_ref.MakeDirectory(
      PP_MAKEDIRECTORYFLAG_NONE, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef target_dir_ref(file_system, "/target_dir_rename");
  callback.WaitForResult(
      dir_ref.Rename(target_dir_ref, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef nested_dir_ref(file_system, "/dir_rename_1/dir_rename_2");
  callback.WaitForResult(
      nested_dir_ref.MakeDirectory(PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS,
                                   callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Try to rename nested directory to the parent name. Should fail.
  pp::FileRef target_nested_dir_ref(file_system, "/dir_rename_1");
  callback.WaitForResult(
      nested_dir_ref.Rename(target_nested_dir_ref, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());

  // Rename aborted.
  // TODO(viettrungluu): Figure out what we want to do if the target file
  // resource is destroyed before completion.
  int32_t rv = PP_ERROR_FAILED;
  pp::FileRef target_file_ref_abort(file_system,
                                    "/target_file_rename_abort");
  {
    pp::FileRef file_ref_abort(file_system, "/file_rename_abort");
    pp::FileIO file_io_abort(instance_);
    callback.WaitForResult(
        file_io_abort.Open(file_ref_abort, PP_FILEOPENFLAG_CREATE,
                           callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());

    rv = file_ref_abort.Rename(target_file_ref_abort, callback.GetCallback());
  }
  callback.WaitForAbortResult(rv);
  CHECK_CALLBACK_BEHAVIOR(callback);

  PASS();
}

std::string TestFileRef::TestQuery() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef file_ref(file_system, "/file");
  pp::FileIO file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref, PP_FILEOPENFLAG_CREATE,
                                      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // We touch the file so we can easily check access and modified time.
  callback.WaitForResult(file_io.Touch(0, 0, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  TestCompletionCallbackWithOutput<PP_FileInfo> out_callback(
      instance_->pp_instance(), callback_type());
  out_callback.WaitForResult(file_ref.Query(out_callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(out_callback);
  ASSERT_EQ(PP_OK, out_callback.result());

  PP_FileInfo info = out_callback.output();
  ASSERT_EQ(0, info.size);
  ASSERT_EQ(PP_FILETYPE_REGULAR, info.type);
  ASSERT_EQ(PP_FILESYSTEMTYPE_LOCALTEMPORARY, info.system_type);
  ASSERT_DOUBLE_EQ(0.0, info.last_access_time);
  ASSERT_DOUBLE_EQ(0.0, info.last_modified_time);

  // Query a file ref on an external filesystem.
  pp::FileRef file_ref_ext;
  std::string result = MakeExternalFileRef(&file_ref_ext);
  if (!result.empty())
    return result;
  out_callback.WaitForResult(file_ref_ext.Query(out_callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(out_callback);
  if (out_callback.result() != PP_OK)
    return ReportError("Query() result", out_callback.result());
  ASSERT_EQ(PP_OK, out_callback.result());

  info = out_callback.output();
  ASSERT_EQ(PP_FILETYPE_REGULAR, info.type);
  ASSERT_EQ(PP_FILESYSTEMTYPE_EXTERNAL, info.system_type);

  // We can't touch the file, so just sanity check the times.
  ASSERT_TRUE(info.creation_time >= 0.0);
  ASSERT_TRUE(info.last_modified_time >= 0.0);
  ASSERT_TRUE(info.last_access_time >= 0.0);

  // Query a file ref for a file that doesn't exist.
  pp::FileRef missing_file_ref(file_system, "/missing_file");
  out_callback.WaitForResult(missing_file_ref.Query(
      out_callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(out_callback);
  ASSERT_EQ(PP_ERROR_FILENOTFOUND, out_callback.result());

  PASS();
}

std::string TestFileRef::TestFileNameEscaping() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  std::string test_dir_path = "/dir_for_escaping_test";
  // Create a directory in which to test.
  pp::FileRef test_dir_ref(file_system, test_dir_path.c_str());
  callback.WaitForResult(test_dir_ref.MakeDirectory(
      PP_MAKEDIRECTORYFLAG_NONE, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Create the file with the terrible name.
  std::string full_file_path = test_dir_path + "/" + kTerribleName;
  pp::FileRef file_ref(file_system, full_file_path.c_str());
  pp::FileIO file_io(instance_);
  callback.WaitForResult(
      file_io.Open(file_ref, PP_FILEOPENFLAG_CREATE, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // FileRef::ReadDirectoryEntries only works out-of-process.
  if (testing_interface_->IsOutOfProcess()) {
    TestCompletionCallbackWithOutput<DirEntries>
        output_callback(instance_->pp_instance(), callback_type());

    output_callback.WaitForResult(
        test_dir_ref.ReadDirectoryEntries(output_callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(output_callback);
    ASSERT_EQ(PP_OK, output_callback.result());

    DirEntries entries = output_callback.output();
    ASSERT_EQ(1, entries.size());
    ASSERT_EQ(kTerribleName, entries.front().file_ref().GetName().AsString());
  }

  PASS();
}

std::string TestFileRef::TestReadDirectoryEntries() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  pp::FileSystem file_system(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Setup testing directories and files.
  const char* test_dir_name = "/test_get_next_file";
  const char* file_prefix = "file_";
  const char* dir_prefix = "dir_";

  pp::FileRef test_dir(file_system, test_dir_name);
  int32_t rv = DeleteDirectoryRecursively(&test_dir);
  ASSERT_TRUE(rv == PP_OK || rv == PP_ERROR_FILENOTFOUND);

  callback.WaitForResult(test_dir.MakeDirectory(
      PP_MAKEDIRECTORYFLAG_NONE, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  static const int kNumFiles = 3;
  std::set<std::string> expected_file_names;
  for (int i = 1; i <= kNumFiles; ++i) {
    std::ostringstream buffer;
    buffer << test_dir_name << '/' << file_prefix << i;
    pp::FileRef file_ref(file_system, buffer.str().c_str());

    pp::FileIO file_io(instance_);
    callback.WaitForResult(
        file_io.Open(file_ref, PP_FILEOPENFLAG_CREATE, callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());

    expected_file_names.insert(buffer.str());
  }

  static const int kNumDirectories = 3;
  std::set<std::string> expected_dir_names;
  for (int i = 1; i <= kNumDirectories; ++i) {
    std::ostringstream buffer;
    buffer << test_dir_name << '/' << dir_prefix << i;
    pp::FileRef file_ref(file_system, buffer.str().c_str());

    callback.WaitForResult(file_ref.MakeDirectory(
        PP_MAKEDIRECTORYFLAG_NONE, callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());

    expected_dir_names.insert(buffer.str());
  }

  // Test that |ReadDirectoryEntries()| is able to fetch all
  // directories and files that we created.
  {
    TestCompletionCallbackWithOutput<DirEntries> output_callback(
        instance_->pp_instance(), callback_type());

    output_callback.WaitForResult(
        test_dir.ReadDirectoryEntries(output_callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(output_callback);
    ASSERT_EQ(PP_OK, output_callback.result());

    DirEntries entries = output_callback.output();
    size_t sum = expected_file_names.size() + expected_dir_names.size();
    ASSERT_EQ(sum, entries.size());

    for (DirEntries::const_iterator it = entries.begin();
         it != entries.end(); ++it) {
      pp::FileRef file_ref = it->file_ref();
      std::string file_path = file_ref.GetPath().AsString();
      std::set<std::string>::iterator found =
          expected_file_names.find(file_path);
      if (found != expected_file_names.end()) {
        if (it->file_type() != PP_FILETYPE_REGULAR)
          return file_path + " should have been a regular file.";
        expected_file_names.erase(found);
      } else {
        found = expected_dir_names.find(file_path);
        if (found == expected_dir_names.end())
          return "Unexpected file path: " + file_path;
        if (it->file_type() != PP_FILETYPE_DIRECTORY)
          return file_path + " should have been a directory.";
        expected_dir_names.erase(found);
      }
    }
    ASSERT_TRUE(expected_file_names.empty());
    ASSERT_TRUE(expected_dir_names.empty());
  }

  // Test cancellation of asynchronous |ReadDirectoryEntries()|.
  TestCompletionCallbackWithOutput<DirEntries> output_callback(
      instance_->pp_instance(), callback_type());
  {
    rv = pp::FileRef(file_system, test_dir_name)
        .ReadDirectoryEntries(output_callback.GetCallback());
  }
  output_callback.WaitForAbortResult(rv);
  CHECK_CALLBACK_BEHAVIOR(output_callback);


  PASS();
}
