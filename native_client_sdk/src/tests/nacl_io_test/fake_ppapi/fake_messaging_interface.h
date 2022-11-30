// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_MESSAGING_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_MESSAGING_INTERFACE_H_

#include <string>
#include <vector>

#include "nacl_io/pepper_interface.h"
#include "sdk_util/macros.h"

class FakeVarManager;

// Fake version of the MessagingInterface which simply records any
// messages sent to it via PostMessage and makes them available as
// a std::vector of PP_Var.
class FakeMessagingInterface : public nacl_io::MessagingInterface {
 public:
  FakeMessagingInterface(FakeVarManager* manager,
                         nacl_io::VarInterface* var_interface);

  FakeMessagingInterface(const FakeMessagingInterface&) = delete;
  FakeMessagingInterface& operator=(const FakeMessagingInterface&) = delete;

  ~FakeMessagingInterface();

  virtual void PostMessage(PP_Instance instance, PP_Var message);

  std::vector<PP_Var> messages;
 private:
  FakeVarManager* manager_;
  nacl_io::VarInterface* var_interface_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_MESSAGING_INTERFACE_H_
