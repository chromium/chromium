// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/tcp_socket.h"
#include "ppapi/cpp/udp_socket.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace {

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        tcp_socket_(this),
        udp_socket_(this),
        factory_(this) {}
  virtual ~MyInstance() {}

  void DidBindTCPSocket(int32_t result) {
    // We should not be able to bind the TCP socket without permission.
    if (result == PP_OK) {
      PostMessage("FAIL");
      return;
    }
    PP_NetAddress_IPv4 ipv4_address = {80, {127, 0, 0, 1}};
    pp::NetAddress address(this, ipv4_address);
    udp_socket_.Bind(address,
                     factory_.NewCallback(&MyInstance::DidBindUDPSocket));
  }

  void DidBindUDPSocket(int32_t result) {
    // We should not be able to bind the UDP socket without permission.
    if (result == PP_OK)
      PostMessage("FAIL");
    else
      PostMessage("PASS");
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    PP_NetAddress_IPv4 ipv4_address = {80, {127, 0, 0, 1}};
    pp::NetAddress address(this, ipv4_address);
    tcp_socket_.Bind(address,
                     factory_.NewCallback(&MyInstance::DidBindTCPSocket));
    return true;
  }

 private:
  pp::TCPSocket tcp_socket_;
  pp::UDPSocket udp_socket_;
  pp::CompletionCallbackFactory<MyInstance> factory_;
};

class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

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
