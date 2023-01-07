// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/simple_thread.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

class Instance : public pp::Instance {
 public:
  explicit Instance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        thread_(this) {}

 private:
  virtual bool Init(uint32_t /*argc*/,
                    const char* /*argn*/ [],
                    const char* /*argv*/ []) {
    thread_.Start();
    return true;
  }

  virtual void HandleMessage(const pp::Var& var_message) {
    // Got a message from JavaScript. We're assuming it is a dictionary with
    // two elements:
    //   {
    //     filesystem: <A Filesystem var>,
    //     fullPath: <A string>
    //   }
    pp::VarDictionary var_dict(var_message);
    pp::Resource filesystem_resource = var_dict.Get("filesystem").AsResource();
    pp::FileSystem filesystem(filesystem_resource);
    std::string full_path = var_dict.Get("fullPath").AsString();

    std::string save_path = full_path + "/hello_from_nacl.txt";
    std::string contents = "Hello, from Native Client!\n";

    thread_.message_loop().PostWork(callback_factory_.NewCallback(
        &Instance::WriteFile, filesystem, save_path, contents));
  }

  void WriteFile(int32_t /* result */,
                 const pp::FileSystem& filesystem,
                 const std::string& path,
                 const std::string& contents) {
    pp::FileRef ref(filesystem, path.c_str());
    pp::FileIO file(this);

    int32_t open_result =
        file.Open(ref, PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE |
                           PP_FILEOPENFLAG_TRUNCATE,
                  pp::BlockUntilComplete());
    if (open_result != PP_OK) {
      PostMessage("Failed to open file.");
      return;
    }

    int64_t offset = 0;
    int32_t bytes_written = 0;
    do {
      bytes_written = file.Write(offset, contents.data() + offset,
                                 contents.length(), pp::BlockUntilComplete());
      if (bytes_written > 0) {
        offset += bytes_written;
      } else {
        PostMessage("Failed to write file.");
        return;
      }
    } while (bytes_written < static_cast<int64_t>(contents.length()));

    // All bytes have been written, flush the write buffer to complete
    int32_t flush_result = file.Flush(pp::BlockUntilComplete());
    if (flush_result != PP_OK) {
      PostMessage("Failed to flush file.");
      return;
    }
    PostMessage(std::string("Wrote file ") + path + ".");
  }

 private:
  pp::CompletionCallbackFactory<Instance> callback_factory_;
  pp::SimpleThread thread_;
};

class Module : public pp::Module {
 public:
  Module() : pp::Module() {}
  virtual ~Module() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new Instance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new ::Module(); }
}  // namespace pp
