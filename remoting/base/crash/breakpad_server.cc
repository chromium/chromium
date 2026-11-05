// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <memory>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "base/win/access_control_list.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "remoting/base/crash/breakpad_utils.h"
#include "remoting/base/crash/breakpad_win.h"
#include "remoting/base/crash/crash_reporting_breakpad.h"
#include "remoting/base/logging.h"
#include "remoting/base/version.h"
#include "third_party/breakpad/breakpad/src/client/windows/crash_generation/client_info.h"
#include "third_party/breakpad/breakpad/src/client/windows/crash_generation/crash_generation_server.h"
#include "third_party/breakpad/breakpad/src/client/windows/crash_generation/minidump_generator.h"

namespace remoting {

namespace {

using base::win::SecurityDescriptor;
using base::win::Sid;
using base::win::SecurityAccessMode::kGrant;
using base::win::WellKnownSid::kLocalSystem;

// Passed as a flag in the named pipe DACL entry to indicate no inheritance.
constexpr bool kNoInheritance = false;

base::FilePath GenerateMinidumpForClient(
    const google_breakpad::ClientInfo& client_info,
    const base::FilePath& minidump_dir) {
  EXCEPTION_POINTERS* client_ex_info = nullptr;
  if (!client_info.GetClientExceptionInfo(&client_ex_info)) {
    LOG(ERROR) << "Failed to retrieve EXCEPTION_POINTERS from OOP client (pid="
               << client_info.pid() << ")";
    return base::FilePath();
  }

  DWORD client_thread_id = 0;
  if (!client_info.GetClientThreadId(&client_thread_id)) {
    LOG(ERROR) << "Failed to retrieve thread ID from OOP client (pid="
               << client_info.pid() << ")";
    return base::FilePath();
  }

  base::FilePath dump_file_path = minidump_dir.AppendASCII(
      base::Uuid::GenerateRandomV4().AsLowercaseString() + ".dmp");
  base::File dump_file(dump_file_path,
                       base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                           base::File::FLAG_WIN_EXCLUSIVE_READ |
                           base::File::FLAG_WIN_EXCLUSIVE_WRITE);
  if (!dump_file.IsValid()) {
    LOG(ERROR) << "Failed to create dump file for OOP client (pid="
               << client_info.pid()
               << "): " << base::File::ErrorToString(dump_file.error_details());
    return base::FilePath();
  }

  bool write_succeeded = false;
  {
    // Always enforce kMinidumpType server-side rather than trusting the
    // client-supplied client_info.dump_type(), and pass an externally owned
    // file handle via SetDumpFile() so dump_file_is_internal_ remains false.
    google_breakpad::MinidumpGenerator dump_generator(
        minidump_dir.value(), client_info.process_handle(), client_info.pid(),
        client_thread_id, GetCurrentThreadId(), client_ex_info,
        client_info.assert_info(), kMinidumpType,
        /*is_client_pointers=*/true);
    dump_generator.SetDumpFile(dump_file.GetPlatformFile());
    write_succeeded = dump_generator.WriteMinidump();
  }
  dump_file.Close();

  if (!write_succeeded) {
    LOG(ERROR) << "Failed to write minidump for OOP client (pid="
               << client_info.pid() << ")";
    base::DeleteFile(dump_file_path);
    return base::FilePath();
  }

  return dump_file_path;
}

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
  if (!client_info || !client_info->process_handle() ||
      client_info->pid() == 0) {
    LOG(ERROR) << "OnClientDumpRequestCallback called with invalid client_info";
    return;
  }

  base::FilePath minidump_dir = GetMinidumpDirectoryPath();
  if (minidump_dir.empty()) {
    return;
  }

  base::FilePath dump_file =
      GenerateMinidumpForClient(*client_info, minidump_dir);
  if (dump_file.empty()) {
    return;
  }

  base::DictValue metadata;
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

  SECURITY_DESCRIPTOR security_descriptor = sd.ToAbsolute();

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = &security_descriptor;
  security_attributes.bInheritHandle = false;

  // Pass generate_dumps=false so OnClientDumpRequestCallback generates the
  // minidump using the server-enforced kMinidumpType rather than trusting the
  // client-supplied dump_type.
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
      /*generate_dumps=*/false,
      /*dump_path=*/nullptr);
  crash_server_->pre_fetch_custom_info(false);
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
