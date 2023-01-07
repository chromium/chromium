// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <sstream>
#include <string>

#include "native_client/src/untrusted/irt/irt.h"

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/tcp_socket.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace {

std::string g_last_error;
pp::Instance* g_instance = NULL;

// This should be larger than or equal to
// MessageAttachmentSet::kMaxDescriptorsPerMessage in
// ipc/ipc_message_attachment_set.h.
const size_t kMaxDescriptorsPerMessage = 16;

// Returns true if the resource file whose name is |key| exists and its content
// matches |content|.
bool LoadManifestInternal(nacl_irt_resource_open* nacl_irt_resource_open,
                          const std::string& key,
                          const std::string& content) {
  int desc;
  int error;
  error = nacl_irt_resource_open->open_resource(key.c_str(), &desc);
  if (0 != error) {
    g_last_error = "Can't open file " + key;
    return false;
  }

  std::string str;

  char buffer[4096];
  int len;
  while ((len = read(desc, buffer, sizeof(buffer) - 1)) > 0) {
    // Null terminate.
    buffer[len] = '\0';
    str += buffer;
  }
  if (close(desc)) {
    g_last_error = "Close failed: file=" + key;
    return false;
  }

  if (str != content) {
    g_last_error = "Wrong file content: file=" + key + ", expected=" + content +
                   ", actual=" + str;
    return false;
  }

  return true;
}

// Tests if open_resource works in a packaged app. This test is similar to
// NaClBrowserTest*.IrtManifestFile, but unlike the NaCl test, this one tests
// the "fast path" in DownloadNexe() in ppb_nacl_private_impl.cc which opens
// resource files without using URLLoader.
void LoadManifest() {
  if (pthread_detach(pthread_self())) {
    g_last_error = "pthread_detach failed";
    return;
  }

  struct nacl_irt_resource_open nacl_irt_resource_open;
  if (sizeof(nacl_irt_resource_open) !=
      nacl_interface_query(NACL_IRT_RESOURCE_OPEN_v0_1,
                           &nacl_irt_resource_open,
                           sizeof(nacl_irt_resource_open))) {
    g_last_error = "NACL_IRT_RESOURCE_OPEN_v0_1 not found";
    return;
  }

  for (size_t i = 0; i <= kMaxDescriptorsPerMessage; ++i) {
    std::stringstream key;
    key << "test_file" << i;
    std::string content = "Example contents for open_resource test" +
        std::string(i % 2 ? "2" : "");
    if (!LoadManifestInternal(&nacl_irt_resource_open, key.str(), content))
      break;
    // Open the same resource file again to make sure each file descriptor
    // returned from open_resource has its own file offset.
    if (!LoadManifestInternal(&nacl_irt_resource_open, key.str(), content))
      break;
  }
}

void PostReply(void* user_data, int32_t status) {
  if (!g_last_error.empty())
    g_instance->PostMessage(g_last_error.c_str());
  else
    g_instance->PostMessage("PASS");
}

void* RunTestsOnBackgroundThread(void* thread_id) {
  LoadManifest();
  pp::Module::Get()->core()->CallOnMainThread(
      0, pp::CompletionCallback(&PostReply, NULL));
  return NULL;
}

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance), socket_(this), factory_(this) {
    g_instance = this;
  }
  virtual ~MyInstance() { }

  void DidBindSocket(int32_t result) {
    // We didn't ask for socket permission in our manifest, so it should fail.
    if (result == PP_ERROR_NOACCESS)
      PostMessage("PASS");
    else
      PostMessage(result);
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    pthread_t thread;
    // irt_open_resource() isn't allowed to be called on the main thread once
    // Pepper starts, so the test must happen on a background thread.
    if (pthread_create(&thread, NULL, &RunTestsOnBackgroundThread, NULL)) {
      g_last_error = "pthread_create failed";
      PostReply(NULL, 0);
    }
    // Attempt to bind a socket. We don't have permissions, so it should fail.
    PP_NetAddress_IPv4 ipv4_address = {80, {127, 0, 0, 1} };
    pp::NetAddress address(this, ipv4_address);
    socket_.Bind(address, factory_.NewCallback(&MyInstance::DidBindSocket));
    return true;
  }

 private:
  pp::TCPSocket socket_;
  pp::CompletionCallbackFactory<MyInstance> factory_;
};

class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() { }
  virtual ~MyModule() { }

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

}  // namespace

namespace pp {

Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
