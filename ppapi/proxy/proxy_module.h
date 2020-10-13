// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PROXY_MODULE_H_
#define PPAPI_PROXY_PROXY_MODULE_H_

#include <string>

#include "base/macros.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace base {
template<typename T> struct DefaultSingletonTraits;
}

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT ProxyModule {
 public:
  // The global singleton getter.
  static ProxyModule* GetInstance();

  // TODO(viettrungluu): Generalize this for use with other plugins if it proves
  // necessary. (Currently, we can't do this easily, since we can't tell from
  // |PpapiPluginMain()| which plugin will be loaded.)
  const std::string& GetFlashCommandLineArgs();
  void SetFlashCommandLineArgs(const std::string& args);

 private:
  friend struct base::DefaultSingletonTraits<ProxyModule>;

  std::string flash_command_line_args_;

  ProxyModule();
  ~ProxyModule();

  DISALLOW_COPY_AND_ASSIGN(ProxyModule);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PROXY_MODULE_H_
