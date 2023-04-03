// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_memory/system_memory_api.h"

#include "base/functional/bind.h"
#include "extensions/browser/api/system_memory/memory_info_provider.h"

namespace extensions {

using api::system_memory::MemoryInfo;

ExtensionFunction::ResponseAction SystemMemoryGetInfoFunction::Run() {
  MemoryInfoProvider::Get()->StartQueryInfo(base::BindOnce(
      &SystemMemoryGetInfoFunction::OnGetMemoryInfoCompleted, this));
  // StartQueryInfo responds asynchronously.
  return RespondLater();
}

void SystemMemoryGetInfoFunction::OnGetMemoryInfoCompleted(bool success) {
  if (success) {
    Respond(WithArguments(MemoryInfoProvider::Get()->memory_info().ToValue()));
  } else {
    Respond(Error("Error occurred when querying memory information."));
  }
}

}  // namespace extensions
