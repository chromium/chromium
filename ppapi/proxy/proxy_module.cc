// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/proxy_module.h"

#include "base/memory/singleton.h"

namespace ppapi {
namespace proxy {

ProxyModule::ProxyModule() {
}

ProxyModule::~ProxyModule() {
}

// static
ProxyModule* ProxyModule::GetInstance() {
  return base::Singleton<ProxyModule>::get();
}

const std::string& ProxyModule::GetFlashCommandLineArgs() {
  return flash_command_line_args_;
}

void ProxyModule::SetFlashCommandLineArgs(const std::string& args) {
  flash_command_line_args_ = args;
}

}  // namespace proxy
}  // namespace ppapi
