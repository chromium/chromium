// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_cpu/system_cpu_api.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "extensions/browser/api/system_cpu/cpu_info_provider.h"

namespace extensions {

using api::system_cpu::CpuInfo;

ExtensionFunction::ResponseAction SystemCpuGetInfoFunction::Run() {
  CpuInfoProvider::Get()->StartQueryInfo(
      base::BindOnce(&SystemCpuGetInfoFunction::OnGetCpuInfoCompleted, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void SystemCpuGetInfoFunction::OnGetCpuInfoCompleted(bool success) {
  if (success) {
    Respond(WithArguments(CpuInfoProvider::Get()->cpu_info().ToValue()));
  } else {
    Respond(Error("Error occurred when querying cpu information."));
  }
}

}  // namespace extensions
