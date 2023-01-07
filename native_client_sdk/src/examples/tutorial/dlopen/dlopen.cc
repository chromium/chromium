// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>

#include <ppapi/cpp/completion_callback.h>
#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/var.h>

#include "eightball.h"
#include "nacl_io/nacl_io.h"
#include "reverse.h"

#if defined(NACL_SDK_DEBUG)
#define CONFIG_NAME "Debug"
#else
#define CONFIG_NAME "Release"
#endif

#if defined __arm__
#define NACL_ARCH "arm"
#elif defined __i686__
#define NACL_ARCH "x86_32"
#elif defined __x86_64__
#define NACL_ARCH "x86_64"
#else
#error "Unknown arch"
#endif

class DlOpenInstance : public pp::Instance {
 public:
  explicit DlOpenInstance(PP_Instance instance)
      : pp::Instance(instance),
        eightball_so_(NULL),
        reverse_so_(NULL),
        eightball_(NULL),
        reverse_(NULL) {}

  virtual ~DlOpenInstance() {}

  // Helper function to post a message back to the JS and stdout functions.
  void logmsg(const char* pStr) {
    PostMessage(pp::Var(std::string("log:") + pStr));
    fprintf(stdout, pStr);
    fprintf(stdout, "\n");
  }

  // Initialize the module, staring a worker thread to load the shared object.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    nacl_io_init_ppapi(pp_instance(),
                       pp::Module::Get()->get_browser_interface());
    // Mount a HTTP mount at /http. All reads from /http/* will read from the
    // server.
    mount("", "/http", "httpfs", 0, "");

    pthread_t thread;
    logmsg("Spawning thread to cache .so files...");
    int rtn = pthread_create(&thread, NULL, LoadLibrariesOnWorker, this);
    if (rtn != 0) {
      logmsg("ERROR; pthread_create() failed.");
      return false;
    }
    rtn = pthread_detach(thread);
    if (rtn != 0) {
      logmsg("ERROR; pthread_detach() failed.");
      return false;
    }
    return true;
  }

  // This function is called on a worker thread, and will call dlopen to load
  // the shared object.  In addition, note that this function does NOT call
  // dlclose, which would close the shared object and unload it from memory.
  void LoadLibrary() {
    eightball_so_ = dlopen("libeightball.so", RTLD_LAZY);
    if (eightball_so_ != NULL) {
      intptr_t offset = (intptr_t) dlsym(eightball_so_, "Magic8Ball");
      eightball_ = (TYPE_eightball) offset;
      if (NULL == eightball_) {
        std::string message = "dlsym() returned NULL: ";
        message += dlerror();
        logmsg(message.c_str());
        return;
      }

      logmsg("Loaded libeightball.so");
    } else {
      logmsg("libeightball.so did not load");
    }

    const char reverse_so_path[] =
        "/http/glibc/" CONFIG_NAME "/libreverse_" NACL_ARCH ".so";
    reverse_so_ = dlopen(reverse_so_path, RTLD_LAZY);
    if (reverse_so_ != NULL) {
      intptr_t offset = (intptr_t) dlsym(reverse_so_, "Reverse");
      reverse_ = (TYPE_reverse) offset;
      if (NULL == reverse_) {
        std::string message = "dlsym() returned NULL: ";
        message += dlerror();
        logmsg(message.c_str());
        return;
      }
      logmsg("Loaded libreverse.so");
    } else {
      logmsg("libreverse.so did not load");
    }
  }

  // Called by the browser to handle the postMessage() call in Javascript.
  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_string()) {
      logmsg("Message is not a string.");
      return;
    }

    std::string message = var_message.AsString();
    if (message == "eightball") {
      if (NULL == eightball_) {
        logmsg("Eightball library not loaded");
        return;
      }

      std::string ballmessage = "The Magic 8-Ball says: ";
      ballmessage += eightball_();
      ballmessage += "!";

      logmsg(ballmessage.c_str());
    } else if (message.find("reverse:") == 0) {
      if (NULL == reverse_) {
        logmsg("Reverse library not loaded");
        return;
      }

      std::string s = message.substr(strlen("reverse:"));
      char* result = reverse_(s.c_str());

      std::string message = "Your string reversed: \"";
      message += result;
      message += "\"";

      free(result);

      logmsg(message.c_str());
    } else {
      std::string errormsg = "Unexpected message: ";
      errormsg += message;
      logmsg(errormsg.c_str());
    }
  }

  static void* LoadLibrariesOnWorker(void* pInst) {
    DlOpenInstance* inst = static_cast<DlOpenInstance*>(pInst);
    inst->LoadLibrary();
    return NULL;
  }

 private:
  void* eightball_so_;
  void* reverse_so_;
  TYPE_eightball eightball_;
  TYPE_reverse reverse_;
};

class DlOpenModule : public pp::Module {
 public:
  DlOpenModule() : pp::Module() {}
  virtual ~DlOpenModule() {}

  // Create and return a DlOpenInstance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new DlOpenInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new DlOpenModule(); }
}  // namespace pp
