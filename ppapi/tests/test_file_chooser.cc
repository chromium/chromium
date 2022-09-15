// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_file_chooser.h"

#include <stddef.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/dev/file_chooser_dev.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/trusted/file_chooser_trusted.h"
#include "ppapi/cpp/var.h"

namespace {

// Test data written by WriteDefaultContentsToFile().
const char kTestFileContents[] = "Hello from PPAPI";
const size_t kTestFileContentsSizeBytes = sizeof(kTestFileContents) - 1;

}  // namespace

REGISTER_TEST_CASE(FileChooser);

bool TestFileChooser::Init() {
  return CheckTestingInterface() && EnsureRunningOverHTTP();
}

void TestFileChooser::RunTests(const std::string& filter) {
  RUN_TEST(OpenSimple, filter);
  RUN_TEST(OpenCancel, filter);
  RUN_TEST(SaveAsSafeDefaultName, filter);
  RUN_TEST(SaveAsUnsafeDefaultName, filter);
  RUN_TEST(SaveAsCancel, filter);
  RUN_TEST(SaveAsDangerousExecutableAllowed, filter);
  RUN_TEST(SaveAsDangerousExecutableDisallowed, filter);
  RUN_TEST(SaveAsDangerousExtensionListDisallowed, filter);
}

bool TestFileChooser::WriteDefaultContentsToFile(const pp::FileRef& file_ref) {
  TestCompletionCallback fileio_callback(instance_->pp_instance(),
                                         callback_type());
  pp::FileIO fileio(instance());

  fileio_callback.WaitForResult(
      fileio.Open(file_ref, PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE,
                  fileio_callback.GetCallback()));
  if (fileio_callback.result() != PP_OK)
    return false;

  fileio_callback.WaitForResult(fileio.Write(0, kTestFileContents,
                                             kTestFileContentsSizeBytes,
                                             fileio_callback.GetCallback()));
  return fileio_callback.result() == kTestFileContentsSizeBytes;
}

// Tests that the plugin can invoke a simple file chooser and that the returned
// file can be read from. Note that this test doesn't test that the accepted
// file type list is honored.
std::string TestFileChooser::TestOpenSimple() {
  pp::FileChooser_Dev file_chooser(instance(), PP_FILECHOOSERMODE_OPEN, "*");
  ASSERT_FALSE(file_chooser.is_null());

  TestCompletionCallbackWithOutput<std::vector<pp::FileRef>>
      filechooser_callback(instance_->pp_instance(), callback_type());
  filechooser_callback.WaitForResult(
      file_chooser.Show(filechooser_callback.GetCallback()));

  const std::vector<pp::FileRef>& output_ref = filechooser_callback.output();
  ASSERT_EQ(1u, output_ref.size());

  TestCompletionCallback fileio_callback(instance_->pp_instance(),
                                         callback_type());
  pp::FileIO fileio(instance());
  fileio_callback.WaitForResult(fileio.Open(
      output_ref.front(), PP_FILEOPENFLAG_READ, fileio_callback.GetCallback()));
  ASSERT_EQ(PP_OK, fileio_callback.result());
  PASS();
}

// Tests the behavior when the user cancels the file chooser. Browser-side logic
// for simulating the cancellation can be found at
// ppapi_filechooser_browsertest.cc
std::string TestFileChooser::TestOpenCancel() {
  pp::FileChooser_Dev file_chooser(instance(), PP_FILECHOOSERMODE_OPEN, "*");
  ASSERT_FALSE(file_chooser.is_null());

  TestCompletionCallbackWithOutput<std::vector<pp::FileRef>>
      filechooser_callback(instance_->pp_instance(), callback_type());
  filechooser_callback.WaitForResult(
      file_chooser.Show(filechooser_callback.GetCallback()));

  const std::vector<pp::FileRef>& output_ref = filechooser_callback.output();
  ASSERT_EQ(0u, output_ref.size());
  PASS();
}

// Tests that the plugin can invoke a "Save as" dialog using the
// FileChooser_Trusted API and that the returned FileRef can be written to.
std::string TestFileChooser::TestSaveAsSafeDefaultName() {
  pp::FileChooser_Trusted file_chooser(instance(), PP_FILECHOOSERMODE_OPEN,
                                       ".txt", true /* save_as */,
                                       "innocuous.txt");
  ASSERT_FALSE(file_chooser.is_null());

  TestCompletionCallbackWithOutput<std::vector<pp::FileRef>>
      filechooser_callback(instance_->pp_instance(), callback_type());
  filechooser_callback.WaitForResult(
      file_chooser.Show(filechooser_callback.GetCallback()));

  const std::vector<pp::FileRef>& output_ref = filechooser_callback.output();
  ASSERT_EQ(1u, output_ref.size());

  ASSERT_TRUE(WriteDefaultContentsToFile(output_ref.front()));
  PASS();
}

// Similar to the previous test, but tests that an unsafe filename passed as the
// suggested name is sanitized.
std::string TestFileChooser::TestSaveAsUnsafeDefaultName() {
  pp::FileChooser_Trusted file_chooser(instance(), PP_FILECHOOSERMODE_OPEN,
                                       "text/plain,.html", true /* save_as */,
                                       "unsafe.txt ");
  ASSERT_FALSE(file_chooser.is_null());

  TestCompletionCallbackWithOutput<std::vector<pp::FileRef>>
      filechooser_callback(instance_->pp_instance(), callback_type());
  filechooser_callback.WaitForResult(
      file_chooser.Show(filechooser_callback.GetCallback()));

  const std::vector<pp::FileRef>& output_ref = filechooser_callback.output();
  ASSERT_EQ(1u, output_ref.size());
  ASSERT_EQ("unsafe.txt_", output_ref.front().GetName().AsString());

  ASSERT_TRUE(WriteDefaultContentsToFile(output_ref.front()));
  PASS();
}

// Tests the behavior when the user cancels a Save As file chooser. Requires
// that the test runner cancel the Save As dialog.
std::string TestFileChooser::TestSaveAsCancel() {
  pp::FileChooser_Trusted file_chooser(instance(), PP_FILECHOOSERMODE_OPEN, "*",
                                       true /* save as */, "anything.txt");
  ASSERT_FALSE(file_chooser.is_null());

  TestCompletionCallbackWithOutput<std::vector<pp::FileRef>>
      filechooser_callback(instance_->pp_instance(), callback_type());
  filechooser_callback.WaitForResult(
      file_chooser.Show(filechooser_callback.GetCallback()));

  const std::vector<pp::FileRef>& output_ref = filechooser_callback.output();
  ASSERT_EQ(0u, output_ref.size());
  PASS();
}

// Checks that a dangerous file is allowed to be downloaded via the
// FileChooser_Trusted API. Chrome should delegate the decision of which files
// are allowed over to SafeBrowsing (if enabled), and the current SafeBrowsing
// configuration should allow downloading of dangerous files for this test to
// work.
std::string TestFileChooser::TestSaveAsDangerousExecutableAllowed() {
  pp::FileChooser_Trusted file_chooser(instance(), PP_FILECHOOSERMODE_OPEN,
                                       ".exe", true /* save_as */,
                                       "dangerous.exe");
  ASSERT_FALSE(file_chooser.is_null());

  TestCompletionCallbackWithOutput<std::vector<pp::FileRef>>
      filechooser_callback(instance_->pp_instance(), callback_type());
  filechooser_callback.WaitForResult(
      file_chooser.Show(filechooser_callback.GetCallback()));

  const std::vector<pp::FileRef>& output_ref = filechooser_callback.output();
  ASSERT_EQ(1u, output_ref.size());
  ASSERT_EQ("dangerous.exe", output_ref.front().GetName().AsString());

  ASSERT_TRUE(WriteDefaultContentsToFile(output_ref.front()));
  PASS();
}

// Checks that a dangerous file is not allowed to be downloaded via the
// FileChooser_Trusted API. Chrome should delegate the decision of which files
// are allowed over to SafeBrowsing (if enabled), and the current SafeBrowsing
// configuration should disallow downloading of dangerous files for this test to
// work.
std::string TestFileChooser::TestSaveAsDangerousExecutableDisallowed() {
  pp::FileChooser_Trusted file_chooser(instance(), PP_FILECHOOSERMODE_OPEN,
                                       ".exe", true /* save_as */,
                                       "dangerous.exe");
  ASSERT_FALSE(file_chooser.is_null());

  TestCompletionCallbackWithOutput<std::vector<pp::FileRef>>
      filechooser_callback(instance_->pp_instance(), callback_type());
  filechooser_callback.WaitForResult(
      file_chooser.Show(filechooser_callback.GetCallback()));

  const std::vector<pp::FileRef>& output_ref = filechooser_callback.output();
  ASSERT_EQ(0u, output_ref.size());
  PASS();
}

// Checks that a dangerous file is not allowed to be downloaded via the
// FileChooser_Trusted API. Chrome should delegate the decision of which files
// are allowed over to SafeBrowsing (if enabled), and the current SafeBrowsing
// configuration should disallow downloading of dangerous files for this test to
// work.
std::string TestFileChooser::TestSaveAsDangerousExtensionListDisallowed() {
  pp::FileChooser_Trusted file_chooser(instance(), PP_FILECHOOSERMODE_OPEN,
                                       ".txt,.exe", true /* save_as */,
                                       "innocuous.txt");
  ASSERT_FALSE(file_chooser.is_null());

  TestCompletionCallbackWithOutput<std::vector<pp::FileRef>>
      filechooser_callback(instance_->pp_instance(), callback_type());
  filechooser_callback.WaitForResult(
      file_chooser.Show(filechooser_callback.GetCallback()));

  const std::vector<pp::FileRef>& output_ref = filechooser_callback.output();
  ASSERT_EQ(0u, output_ref.size());
  PASS();
}
