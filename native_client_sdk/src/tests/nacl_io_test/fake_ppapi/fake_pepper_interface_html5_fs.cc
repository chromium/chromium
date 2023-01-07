// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_pepper_interface_html5_fs.h"

#include "fake_ppapi/fake_filesystem.h"
#include "fake_ppapi/fake_util.h"

FakePepperInterfaceHtml5Fs::FakePepperInterfaceHtml5Fs()
    : core_interface_(&resource_manager_),
      var_interface_(&var_manager_),
      file_system_interface_(&core_interface_),
      file_ref_interface_(&core_interface_, &var_interface_),
      file_io_interface_(&core_interface_) {
  Init();
}

FakePepperInterfaceHtml5Fs::FakePepperInterfaceHtml5Fs(
    const FakeFilesystem& filesystem)
    : core_interface_(&resource_manager_),
      var_interface_(&var_manager_),
      filesystem_template_(filesystem),
      file_system_interface_(&core_interface_),
      file_ref_interface_(&core_interface_, &var_interface_),
      file_io_interface_(&core_interface_),
      instance_(0) {
  Init();
}

void FakePepperInterfaceHtml5Fs::Init() {
  FakeHtml5FsResource* instance_resource = new FakeHtml5FsResource;
  instance_resource->filesystem_template = &filesystem_template_;

  instance_ = CREATE_RESOURCE(core_interface_.resource_manager(),
                              FakeHtml5FsResource, instance_resource);
}

FakePepperInterfaceHtml5Fs::~FakePepperInterfaceHtml5Fs() {
  core_interface_.ReleaseResource(instance_);
}

nacl_io::CoreInterface* FakePepperInterfaceHtml5Fs::GetCoreInterface() {
  return &core_interface_;
}

nacl_io::FileSystemInterface*
FakePepperInterfaceHtml5Fs::GetFileSystemInterface() {
  return &file_system_interface_;
}

nacl_io::FileRefInterface* FakePepperInterfaceHtml5Fs::GetFileRefInterface() {
  return &file_ref_interface_;
}

nacl_io::FileIoInterface* FakePepperInterfaceHtml5Fs::GetFileIoInterface() {
  return &file_io_interface_;
}

nacl_io::VarInterface* FakePepperInterfaceHtml5Fs::GetVarInterface() {
  return &var_interface_;
}
