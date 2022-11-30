// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/main_function_params.h"

#include "base/check.h"
#include "content/public/common/startup_data.h"

namespace content {

MainFunctionParams::MainFunctionParams(const base::CommandLine* cl)
    : command_line(cl) {
  DCHECK(command_line);
}

MainFunctionParams::~MainFunctionParams() = default;

MainFunctionParams::MainFunctionParams(MainFunctionParams&&) = default;
MainFunctionParams& MainFunctionParams::operator=(MainFunctionParams&&) =
    default;

}  // namespace content
