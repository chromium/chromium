// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <sstream>

#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/isolated_file_system_private.h"
#include "ppapi/utility/completion_callback_factory.h"

// When compiling natively on Windows, PostMessage can be #define-d to
// something else.
#ifdef PostMessage
#undef PostMessage
#endif

// Buffer size for reading file.
const size_t kBufSize = 1024;

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        handle_(instance) {
    factory_.Initialize(this);
  }
  virtual ~MyInstance() {
  }

  // Handler for the page sending us messages.
  virtual void HandleMessage(const pp::Var& message_data);

 private:
  void OpenCrxFsAndReadFile(const std::string& filename);

  void CrxFileSystemCallback(int32_t pp_error, pp::FileSystem file_system);
  void FileIOOpenCallback(int32_t pp_error);
  void FileIOReadCallback(int32_t pp_error);

  // Forwards the given string to the page.
  void ReportResponse(const char* name, int32_t pp_error);

  // Generates completion callbacks scoped to this class.
  pp::CompletionCallbackFactory<MyInstance> factory_;

  pp::InstanceHandle handle_;
  pp::IsolatedFileSystemPrivate crxfs_;
  pp::FileRef file_ref_;
  pp::FileIO file_io_;
  std::string filename_;
  char read_buf_[kBufSize];
};

void MyInstance::HandleMessage(const pp::Var& message_data) {
  if (!message_data.is_string()) {
    ReportResponse("HandleMessage: not a string", 0);
    return;
  }
  std::string filename = message_data.AsString();
  OpenCrxFsAndReadFile(filename);
}

void MyInstance::OpenCrxFsAndReadFile(const std::string& filename) {
  filename_ = filename;

  pp::CompletionCallbackWithOutput<pp::FileSystem> callback =
      factory_.NewCallbackWithOutput(&MyInstance::CrxFileSystemCallback);

  crxfs_ = pp::IsolatedFileSystemPrivate(
      this, PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_CRX);
  int32_t rv = crxfs_.Open(callback);
  if (rv != PP_OK_COMPLETIONPENDING)
    ReportResponse("ExtCrxFileSystemPrivate::Open", rv);
}

void MyInstance::CrxFileSystemCallback(int32_t pp_error,
                                       pp::FileSystem file_system) {
  if (pp_error != PP_OK) {
    ReportResponse("CrxFileSystemCallback", pp_error);
    return;
  }

  file_io_ = pp::FileIO(handle_);
  file_ref_ = pp::FileRef(file_system, filename_.c_str());
  int32_t rv = file_io_.Open(
      file_ref_, PP_FILEOPENFLAG_READ,
      factory_.NewCallback(&MyInstance::FileIOOpenCallback));
  if (rv != PP_OK_COMPLETIONPENDING)
    ReportResponse("FileIO::Open", rv);
}

void MyInstance::FileIOOpenCallback(int32_t pp_error) {
  if (pp_error != PP_OK) {
    ReportResponse("FileIOOpenCallback", pp_error);
    return;
  }

  int32_t rv = file_io_.Read(0, read_buf_, sizeof(read_buf_),
      factory_.NewCallback(&MyInstance::FileIOReadCallback));
  if (rv != PP_OK_COMPLETIONPENDING) {
    ReportResponse("FileIO::Read", rv);
    return;
  }
}

void MyInstance::FileIOReadCallback(int32_t pp_error) {
  if (pp_error < 0) {
    ReportResponse("FileIOReadCallback", pp_error);
    return;
  }

  std::string content;
  content.append(read_buf_, pp_error);
  PostMessage(pp::Var(content));
}

void MyInstance::ReportResponse(const char* name, int32_t rv) {
  std::ostringstream out;
  out << name << " failed, pp_error: " << rv;
  PostMessage(pp::Var(out.str()));
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp

