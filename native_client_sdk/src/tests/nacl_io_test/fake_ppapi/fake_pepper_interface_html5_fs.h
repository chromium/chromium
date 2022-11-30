// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_PEPPER_INTERFACE_HTML5_FS_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_PEPPER_INTERFACE_HTML5_FS_H_

#include "fake_ppapi/fake_core_interface.h"
#include "fake_ppapi/fake_file_io_interface.h"
#include "fake_ppapi/fake_file_ref_interface.h"
#include "fake_ppapi/fake_file_system_interface.h"
#include "fake_ppapi/fake_filesystem.h"
#include "fake_ppapi/fake_var_interface.h"
#include "fake_ppapi/fake_var_manager.h"
#include "nacl_io/pepper_interface_dummy.h"
#include "sdk_util/macros.h"

// This class is a fake implementation of the interfaces necessary to access
// the HTML5 Filesystem from NaCl.
//
// Example:
//   FakePepperInterfaceHtml5Fs ppapi_html5fs;
//   ...
//   PP_Resource ref_resource = ppapi_html5fs.GetFileRefInterface()->Create(
//       ppapi_html5fs.GetInstance(),
//       "/some/path");
//   ...
//
// NOTE: This pepper interface creates an instance resource that can only be
// used with FakePepperInterfaceHtml5Fs, not other fake pepper implementations.
class FakePepperInterfaceHtml5Fs : public nacl_io::PepperInterfaceDummy {
 public:
  FakePepperInterfaceHtml5Fs();
  explicit FakePepperInterfaceHtml5Fs(const FakeFilesystem& filesystem);

  FakePepperInterfaceHtml5Fs(const FakePepperInterfaceHtml5Fs&) = delete;
  FakePepperInterfaceHtml5Fs& operator=(const FakePepperInterfaceHtml5Fs&) =
      delete;

  ~FakePepperInterfaceHtml5Fs();

  virtual PP_Instance GetInstance() { return instance_; }
  virtual nacl_io::CoreInterface* GetCoreInterface();
  virtual nacl_io::FileSystemInterface* GetFileSystemInterface();
  virtual nacl_io::FileRefInterface* GetFileRefInterface();
  virtual nacl_io::FileIoInterface* GetFileIoInterface();
  virtual nacl_io::VarInterface* GetVarInterface();

  FakeFilesystem* filesystem_template() { return &filesystem_template_; }

 private:
  void Init();

  FakeResourceManager resource_manager_;
  FakeCoreInterface core_interface_;
  FakeVarInterface var_interface_;
  FakeVarManager var_manager_;
  FakeFilesystem filesystem_template_;
  FakeFileSystemInterface file_system_interface_;
  FakeFileRefInterface file_ref_interface_;
  FakeFileIoInterface file_io_interface_;
  PP_Instance instance_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_PEPPER_INTERFACE_HTML5_FS_H_
