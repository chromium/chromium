// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_NACL_PLUGIN_ARGS_H_
#define PPAPI_SHARED_IMPL_PPAPI_NACL_PLUGIN_ARGS_H_

#include <string>
#include <vector>

#include "ppapi/shared_impl/ppapi_permissions.h"

namespace ppapi {

struct PPAPI_SHARED_EXPORT PpapiNaClPluginArgs {
 public:
  PpapiNaClPluginArgs();
  ~PpapiNaClPluginArgs();

  bool off_the_record;
  PpapiPermissions permissions;
  bool supports_dev_channel;

  // Switches from the command-line.
  std::vector<std::string> switch_names;
  std::vector<std::string> switch_values;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_NACL_PLUGIN_ARGS_H_
