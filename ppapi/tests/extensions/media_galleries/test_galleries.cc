// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/tests/test_utils.h"

/// The Instance class. Receives the file through HandleMessage(), and returns
/// data to the plugin with PostMessage().
class FilePassingInstance : public pp::Instance {
 public:
  /// The constructor creates the plugin-side instance.
  /// @param[in] instance the handle to the browser-side plugin instance.
  explicit FilePassingInstance(PP_Instance instance);
  virtual ~FilePassingInstance();

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

 private:
  /// Handler for messages coming in from the browser via postMessage().
  /// @param[in] var_message The message posted by the browser.
  virtual void HandleMessage(const pp::Var& var_message);

  /// Takes a fileEntry-dictionary message received from JavaScript and converts
  /// it into a pp::FileRef. On failure, returns a null FileRef.
  pp::FileRef ParseMessage(const pp::Var& var_message, std::string* test_type);

  void TestRead(pp::FileRef file_ref);
  void TestWrite(pp::FileRef file_ref);
};

FilePassingInstance::FilePassingInstance(PP_Instance instance)
    : pp::Instance(instance) {}

FilePassingInstance::~FilePassingInstance() {}

bool FilePassingInstance::Init(uint32_t argc,
                               const char* argn[],
                               const char* argv[]) {
    PostMessage("ready");
    return true;
}

pp::FileRef FilePassingInstance::ParseMessage(const pp::Var& var_message,
                                              std::string* test_type) {
  if (!var_message.is_dictionary()) {
    PostMessage("Message was not a dictionary.");
    return pp::FileRef();
  }
  pp::VarDictionary var_dictionary_message(var_message);
  pp::Var var_filesystem = var_dictionary_message.Get("filesystem");
  pp::Var var_fullpath = var_dictionary_message.Get("fullPath");
  pp::Var var_testtype = var_dictionary_message.Get("testType");

  if (!var_filesystem.is_resource()) {
    PostMessage("Filesystem was missing or not a resource.");
    return pp::FileRef();
  }
  pp::Resource resource_filesystem = var_filesystem.AsResource();
  if (!var_fullpath.is_string()) {
    PostMessage("FullPath was missing or not a string.");
    return pp::FileRef();
  }
  std::string fullpath = var_fullpath.AsString();
  if (!var_testtype.is_string()) {
    PostMessage("TestType was missing or not a string.");
    return pp::FileRef();
  }
  std::string name_of_test = var_testtype.AsString();

  if (!pp::FileSystem::IsFileSystem(resource_filesystem)) {
    PostMessage("Filesystem was not a file system.");
    return pp::FileRef();
  }

  *test_type = name_of_test;
  pp::FileSystem filesystem(resource_filesystem);
  // Note: The filesystem is already open (there is no need to call Open again).

  return pp::FileRef(filesystem, fullpath.c_str());
}

void FilePassingInstance::HandleMessage(const pp::Var& var_message) {
  // Extract the filesystem and fullPath from the message.
  std::string test_type;
  pp::FileRef file_ref = ParseMessage(var_message, &test_type);
  if (file_ref.is_null())
    return;

  if (test_type == "read_test") {
    TestRead(file_ref);
  } else if (test_type == "write_test") {
    TestWrite(file_ref);
  } else {
    PostMessage("Unknown test type");
  }
}

void FilePassingInstance::TestRead(pp::FileRef file_ref) {
  pp::FileIO file_io(pp::InstanceHandle(this));

  {
    TestCompletionCallback callback(pp_instance(), PP_REQUIRED);
    callback.WaitForResult(
        file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback.GetCallback()));
    if (callback.result() != PP_OK) {
      PostMessage("Could not open file");
      return;
    }
  }
  {
    TestCompletionCallbackWithOutput<std::vector<char> > callback(pp_instance(),
                                                                  PP_REQUIRED);
    callback.WaitForResult(
        file_io.Read(0, 1024, callback.GetCallback()));
    if (callback.result() < 0) {
      PostMessage("Could not read file");
      return;
    }

    if (callback.output().size() != 306) {
      PostMessage("Read the wrong number of bytes");
      return;
    }
  }
  PostMessage("read_success");
}

void FilePassingInstance::TestWrite(pp::FileRef file_ref) {
  pp::FileIO file_io(pp::InstanceHandle(this));
  TestCompletionCallback callback(pp_instance(), PP_REQUIRED);
  callback.WaitForResult(
      file_io.Open(file_ref, PP_FILEOPENFLAG_WRITE, callback.GetCallback()));
  if (callback.result() != PP_ERROR_NOACCESS) {
    PostMessage("Opening for write should have failed");
    return;
  }
  PostMessage("write_success");
}

class FilePassingModule : public pp::Module {
 public:
  FilePassingModule() : pp::Module() {}
  virtual ~FilePassingModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new FilePassingInstance(instance);
  }
};

namespace pp {

Module* CreateModule() {
  return new FilePassingModule();
}

}  // namespace pp
