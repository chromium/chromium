// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_with_task_environment.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_util.h"

namespace net {

WithTaskEnvironment::WithTaskEnvironment(
    base::test::TaskEnvironment::TimeSource time_source)
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                        time_source) {
  MaybeStartNetLog();
}

WithTaskEnvironment::~WithTaskEnvironment() {
  if (file_net_log_observer_) {
    base::RunLoop run_loop;
    file_net_log_observer_->StopObserving(/*polled_data=*/nullptr,
                                          run_loop.QuitClosure());
    run_loop.Run();
  }
}

void WithTaskEnvironment::MaybeStartNetLog() {
  // TODO(crbug.com/336167322): Move network::switches::kLogNetLog so that we
  // can use the switch here.
  constexpr const char kLogNetLogSwitch[] = "log-net-log";
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kLogNetLogSwitch)) {
    return;
  }

  base::FilePath log_file_path =
      command_line->GetSwitchValuePath(kLogNetLogSwitch);
  if (log_file_path.empty()) {
    return;
  }

  base::File file = base::File(
      log_file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    return;
  }

  auto constants = std::make_unique<base::Value::Dict>(GetNetConstants());
  base::Value::Dict client_info;
  client_info.Set("name", "net_unittests");
  base::CommandLine::StringType command_line_string =
      command_line->GetCommandLineString();
#if BUILDFLAG(IS_WIN)
  client_info.Set("command_line", base::WideToUTF8(command_line_string));
#else
  client_info.Set("command_line", command_line_string);
#endif
  constants->Set("clientInfo", std::move(client_info));

  file_net_log_observer_ = FileNetLogObserver::CreateUnboundedPreExisting(
      std::move(file), NetLogCaptureMode::kEverything, std::move(constants));
  file_net_log_observer_->StartObserving(NetLog::Get());
}

}  // namespace net
