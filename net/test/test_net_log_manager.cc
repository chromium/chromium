// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_net_log_manager.h"

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"
#include "net/log/net_log_util.h"

namespace net {

// A simple NetLog::ThreadSafeObserver that dumps NetLog entries to LOG.
class TestNetLogManager::LogNetLogObserver : public NetLog::ThreadSafeObserver {
 public:
  LogNetLogObserver(NetLog* net_log, NetLogCaptureMode capture_mode)
      : net_log_(net_log) {
    net_log_->AddObserver(this, capture_mode);
  }

  LogNetLogObserver(const LogNetLogObserver&) = delete;
  LogNetLogObserver& operator=(const LogNetLogObserver&) = delete;

  ~LogNetLogObserver() override { net_log_->RemoveObserver(this); }

  void OnAddEntry(const NetLogEntry& entry) override {
    LOG(ERROR) << "NetLog: id=" << entry.source.id
               << " source=" << NetLog::SourceTypeToString(entry.source.type)
               << "\n"
               << "event=" << NetLogEventTypeToString(entry.type)
               << " phase=" << NetLog::EventPhaseToString(entry.phase) << "\n"
               << entry.params.DebugString();
  }

 private:
  const raw_ptr<NetLog> net_log_;
};

TestNetLogManager::TestNetLogManager(NetLog* net_log,
                                     NetLogCaptureMode capture_mode) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kLogNetLogSwitch)) {
    Start(net_log, capture_mode);
  }
}

TestNetLogManager::~TestNetLogManager() {
  if (file_net_log_observer_) {
    base::RunLoop run_loop;
    file_net_log_observer_->StopObserving(/*polled_data=*/nullptr,
                                          run_loop.QuitClosure());
    run_loop.Run();
  }
}

void TestNetLogManager::ForceStart() {
  if (log_net_log_observer_ || file_net_log_observer_) {
    return;
  }
  Start(NetLog::Get(), NetLogCaptureMode::kEverything);
}

void TestNetLogManager::Start(NetLog* net_log, NetLogCaptureMode capture_mode) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  base::FilePath log_file_path =
      command_line->GetSwitchValuePath(kLogNetLogSwitch);
  if (log_file_path.empty()) {
    log_net_log_observer_ =
        std::make_unique<LogNetLogObserver>(net_log, capture_mode);
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
      std::move(file), capture_mode, std::move(constants));
  // Try to write events to file as soon as they are added. This will record
  // events as much as possible even when a test fails with a crash.
  file_net_log_observer_->set_num_write_queue_events(1);
  file_net_log_observer_->StartObserving(net_log);
}

}  // namespace net
