// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_messaging_interface.h"

#include "fake_ppapi/fake_var_manager.h"

FakeMessagingInterface::FakeMessagingInterface(
    FakeVarManager* manager, nacl_io::VarInterface* var_interface)
    : manager_(manager), var_interface_(var_interface) {}

FakeMessagingInterface::~FakeMessagingInterface() {
  for (std::vector<PP_Var>::iterator it = messages.begin();
       it != messages.end(); ++it) {
    manager_->Release(*it);
  }
  messages.clear();
}

void FakeMessagingInterface::PostMessage(PP_Instance instance,
                                         PP_Var message) {
  manager_->AddRef(message);
  messages.push_back(message);
}
