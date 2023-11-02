// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_nacl_plugin_args.h"

namespace ppapi {

// We must provide explicit definitions of these functions for builds on
// Windows.
PpapiNaClPluginArgs::PpapiNaClPluginArgs()
    : off_the_record(false), supports_dev_channel(false) {}

PpapiNaClPluginArgs::~PpapiNaClPluginArgs() {}

}  // namespace ppapi
