// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/win/access_control_list.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "remoting/base/breakpad.h"
#include "remoting/base/breakpad_utils.h"
#include "remoting/base/logging.h"
#include "remoting/base/version.h"
#include "third_party/breakpad/breakpad/src/client/windows/crash_generation/crash_generation_server.h"

namespace remoting {

namespace {

using base::win::SecurityDescriptor;
using base::win::Sid;
using base::win::SecurityAccessMode::kGrant;
using base::win::WellKnownSid::kLocalSystem;

// Passed as a flag in the named pipe DACL entry to indicate no inheritance.
constexpr bool kNoInheritance = false;

class BreakpadServer {
 public:
  BreakpadServer();

  BreakpadServer(const BreakpadServer&) = delete;
  BreakpadServer& operator=(const BreakpadServer&) = delete;

  ~BreakpadServer();

  static BreakpadServer& GetInstance();

  void set_client_connected_time(base::Time client_connected_time) {
    client_connected_time_ = client_connected_time;
  }
  base::Time get_client_connected_time() { return client_connected_time_; }

 private:
  base::Time client_connected_time_;
  std::unique_ptr<google_breakpad::CrashGenerationServer> crash_server_;
};

void OnClientConnectedCallback(void* context,
                               const google_breakpad::ClientInfo* client_info) {
  HOST_LOG << "OOP Crash client connected";
  BreakpadServer::GetInstance().set_client_connected_time(
      base::Time::NowFromSystemTime());
}

void OnClientDumpRequestCallback(void* context,
                                 const google_breakpad::ClientInfo* client_info,
                                 const std::wstring* file_path) {
  if (!file_path) {
    LOG(ERROR) << "OnClientDumpRequestCallback called with invalid file_path";
    return;
  }
  base::FilePath dump_file(*file_path);
  if (!GetMinidumpDirectoryPath().IsParent(dump_file)) {
    LOG(ERROR) << "Minidump written to an unexpected location: " << dump_file;
    return;
  }

  base::Value::Dict metadata;
  // Use the crash server version since we update all host components in the
  // same package and we've seen problems trying to use the metadata provided
  // by the client process, see crbug.com/350725178.
  metadata.Set(kBreakpadProductVersionKey, REMOTING_VERSION_STRING);

  // We only have one OOP client so we can use the value of the 'last connected'
  // client to determine an approximate uptime for that process.
  auto process_start_time =
      BreakpadServer::GetInstance().get_client_connected_time();
  auto process_uptime = base::Time::NowFromSystemTime() - process_start_time;
  metadata.Set(kBreakpadProcessUptimeKey,
               base::NumberToString(process_uptime.InMilliseconds()));

  WriteMetadataForMinidump(dump_file, std::move(metadata));
}

BreakpadServer::BreakpadServer() {
  base::win::SecurityDescriptor sd;
  sd.set_owner(Sid(kLocalSystem));
  sd.set_group(Sid(kLocalSystem));

  // Configure the named pipe to prevent non-SYSTEM access unless a handle is
  // created by the server and provided over IPC or STDIO.
  if (!sd.SetDaclEntry(kLocalSystem, kGrant, FILE_ALL_ACCESS, kNoInheritance)) {
    LOG(ERROR) << "Failed to set named pipe security attributes, skipping "
               << "out-of-process crash handler initialization.";
    return;
  }

  SECURITY_DESCRIPTOR security_descriptor;
  sd.ToAbsolute(security_descriptor);

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = &security_descriptor;
  security_attributes.bInheritHandle = false;

  crash_server_ = std::make_unique<google_breakpad::CrashGenerationServer>(
      kCrashServerPipeName, &security_attributes,
      /*connect_callback=*/OnClientConnectedCallback,
      /*connect_context=*/nullptr,
      /*dump_callback=*/OnClientDumpRequestCallback,
      /*dump_context=*/nullptr,
      /*exit_callback=*/nullptr,
      /*exit_context=*/nullptr,
      /*upload_request_callback=*/nullptr,
      /*upload_context=*/nullptr,
      /*generate_dumps=*/true, &GetMinidumpDirectoryPath().value());
  crash_server_->Start();
}

BreakpadServer::~BreakpadServer() = default;

// static
BreakpadServer& BreakpadServer::GetInstance() {
  static base::NoDestructor<BreakpadServer> instance;
  return *instance;
}

}  // namespace

void InitializeOopCrashServer() {
  // Touch the object to make sure it is initialized.
  BreakpadServer::GetInstance();
}

}  // namespace remoting
