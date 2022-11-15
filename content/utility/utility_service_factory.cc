// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_service_factory.h"

#include <utility>

#include "base/debug/crash_logging.h"
#include "base/process/current_process.h"
#include "base/trace_event/trace_log.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "content/utility/utility_thread_impl.h"

namespace content {

UtilityServiceFactory::UtilityServiceFactory() = default;

UtilityServiceFactory::~UtilityServiceFactory() = default;

void UtilityServiceFactory::RunService(
    const std::string& service_name,
    mojo::ScopedMessagePipeHandle service_pipe) {
  if (base::CurrentProcess::GetInstance().IsProcessNameEmpty()) {
    base::CurrentProcess::GetInstance().SetProcessType(
        GetCurrentProcessType(service_name));
  }

  static auto* const service_name_crash_key =
      base::debug::AllocateCrashKeyString("service-name",
                                          base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(service_name_crash_key, service_name);

  if (GetContentClient()->utility()->HandleServiceRequestDeprecated(
          service_name, std::move(service_pipe))) {
    return;
  }

  // Nothing knew how to handle this request. Complain loudly and die.
  LOG(ERROR) << "Ignoring request to start unknown service: " << service_name;
  UtilityThreadImpl* utility_thread =
      static_cast<UtilityThreadImpl*>(UtilityThread::Get());
  utility_thread->Shutdown();
  utility_thread->ReleaseProcess();
}

}  // namespace content
