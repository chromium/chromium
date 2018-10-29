// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/spawned_test_server/local_test_server.h"

#include <poll.h>

#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_iterator.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/test_timeouts.h"
#include "net/test/python_utils.h"

namespace {

// Helper class used to detect and kill orphaned python test server processes.
// Checks if the command line of a process contains |path_string| (the path
// from which the test server was launched) and |port_string| (the port used by
// the test server), and if the parent pid of the process is 1 (indicating that
// it is an orphaned process).
class OrphanedTestServerFilter : public base::ProcessFilter {
 public:
  OrphanedTestServerFilter(
      const std::string& path_string, const std::string& port_string)
      : path_string_(path_string),
        port_string_(port_string) {}

  bool Includes(const base::ProcessEntry& entry) const override {
    if (entry.parent_pid() != 1)
      return false;
    bool found_path_string = false;
    bool found_port_string = false;
    for (auto it = entry.cmd_line_args().begin();
         it != entry.cmd_line_args().end(); ++it) {
      if (it->find(path_string_) != std::string::npos)
        found_path_string = true;
      if (it->find(port_string_) != std::string::npos)
        found_port_string = true;
    }
    return found_path_string && found_port_string;
  }

 private:
  std::string path_string_;
  std::string port_string_;
  DISALLOW_COPY_AND_ASSIGN(OrphanedTestServerFilter);
};

// Given a file descriptor, reads into |buffer| until |bytes_max|
// bytes has been read or an error has been encountered.  Returns true
// if the read was successful.  |remaining_time| is used as a timeout.
bool ReadData(int fd,
              ssize_t bytes_max,
              uint8_t* buffer,
              base::TimeDelta* remaining_time) {
  ssize_t bytes_read = 0;
  base::TimeTicks previous_time = base::TimeTicks::Now();
  while (bytes_read < bytes_max) {
    struct pollfd poll_fds[1];

    poll_fds[0].fd = fd;
    poll_fds[0].events = POLLIN | POLLPRI;
    poll_fds[0].revents = 0;

    int rv = HANDLE_EINTR(poll(poll_fds, 1,
                               remaining_time->InMilliseconds()));
    if (rv == 0) {
      LOG(ERROR) << "poll() timed out; bytes_read=" << bytes_read;
      return false;
    } else if (rv < 0) {
      PLOG(ERROR) << "poll() failed for child file descriptor; bytes_read="
                  << bytes_read;
      return false;
    }

    base::TimeTicks current_time = base::TimeTicks::Now();
    base::TimeDelta elapsed_time_cycle = current_time - previous_time;
    DCHECK_GE(elapsed_time_cycle.InMilliseconds(), 0);
    *remaining_time -= elapsed_time_cycle;
    previous_time = current_time;

    ssize_t num_bytes = HANDLE_EINTR(read(fd, buffer + bytes_read,
                                          bytes_max - bytes_read));
    if (num_bytes <= 0)
      return false;
    bytes_read += num_bytes;
  }
  return true;
}

}  // namespace

namespace net {

bool LocalTestServer::LaunchPython(const base::FilePath& testserver_path) {
  // Log is useful in the event you want to run a nearby script (e.g. a test) in
  // the same environment as the TestServer.
  VLOG(1) << "LaunchPython called with PYTHONPATH = " << getenv(kPythonPathEnv);

  base::CommandLine python_command(base::CommandLine::NO_PROGRAM);
  if (!GetPythonCommand(&python_command))
    return false;

  python_command.AppendArgPath(testserver_path);
  if (!AddCommandLineArguments(&python_command))
    return false;

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    PLOG(ERROR) << "Could not create pipe.";
    return false;
  }

  // Save the read half. The write half is sent to the child.
  child_fd_.reset(pipefd[0]);
  base::ScopedFD write_closer(pipefd[1]);

  python_command.AppendArg("--startup-pipe=" + base::IntToString(pipefd[1]));

  // Try to kill any orphaned testserver processes that may be running.
  OrphanedTestServerFilter filter(testserver_path.value(),
                                  base::UintToString(GetPort()));
  if (!base::KillProcesses("python", -1, &filter)) {
    LOG(WARNING) << "Failed to clean up older orphaned testserver instances.";
  }

  // Launch a new testserver process.
  base::LaunchOptions options;

  // Set CWD to source root.
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT,
                              &options.current_directory)) {
    LOG(ERROR) << "Failed to get DIR_SOURCE_ROOT";
    return false;
  }

  options.fds_to_remap.push_back(std::make_pair(pipefd[1], pipefd[1]));
  process_ = base::LaunchProcess(python_command, options);
  if (!process_.IsValid()) {
    LOG(ERROR) << "Failed to launch " << python_command.GetCommandLineString();
    return false;
  }

  return true;
}

bool LocalTestServer::WaitToStart() {
  base::ScopedFD our_fd(child_fd_.release());

  base::TimeDelta remaining_time = TestTimeouts::action_timeout();

  uint32_t server_data_len = 0;
  if (!ReadData(our_fd.get(), sizeof(server_data_len),
                reinterpret_cast<uint8_t*>(&server_data_len),
                &remaining_time)) {
    LOG(ERROR) << "Could not read server_data_len";
    return false;
  }
  std::string server_data(server_data_len, '\0');
  if (!ReadData(our_fd.get(), server_data_len,
                reinterpret_cast<uint8_t*>(&server_data[0]), &remaining_time)) {
    LOG(ERROR) << "Could not read server_data (" << server_data_len
               << " bytes)";
    return false;
  }

  int port;
  if (!SetAndParseServerData(server_data, &port)) {
    LOG(ERROR) << "Could not parse server_data: " << server_data;
    return false;
  }
  SetPort(port);

  return true;
}

}  // namespace net
