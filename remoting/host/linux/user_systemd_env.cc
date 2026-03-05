// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <concepts>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_systemd1_Manager.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/passwd_utils.h"

namespace remoting {

namespace {

using gvariant::GVariantRef;

constexpr base::TimeDelta kTimeout = base::Seconds(10);
constexpr base::TimeDelta kPollInterval = base::Seconds(1);

class UserSystemdEnvManager {
 public:
  template <typename ReturnType>
  using CallCallback =
      base::OnceCallback<void(base::expected<ReturnType, Loggable>)>;

  UserSystemdEnvManager() = default;
  ~UserSystemdEnvManager() = default;

  UserSystemdEnvManager(const UserSystemdEnvManager&) = delete;
  UserSystemdEnvManager& operator=(const UserSystemdEnvManager&) = delete;

  void ConnectDbus(CallCallback<void> callback) {
    GDBusConnectionRef::CreateForSessionBus(
        base::BindOnce(&UserSystemdEnvManager::OnDbusConnected,
                       base::Unretained(this), std::move(callback)));
  }

  void FetchEnvironment(CallCallback<base::EnvironmentMap> callback) {
    AttemptFetchEnvironment(base::TimeTicks::Now(), std::move(callback));
  }

  template <typename ReturnType, typename F>
    requires std::invocable<F, UserSystemdEnvManager*, CallCallback<ReturnType>>
  base::expected<ReturnType, Loggable> CallSync(F&& method) {
    base::RunLoop run_loop;
    base::expected<ReturnType, Loggable> result;
    auto cb = base::BindOnce(
                  [](base::expected<ReturnType, Loggable>& out_result,
                     base::expected<ReturnType, Loggable> result) {
                    out_result = std::move(result);
                  },
                  std::ref(result))
                  .Then(run_loop.QuitClosure());
    std::invoke(std::forward<F>(method), this, std::move(cb));
    run_loop.Run();
    return result;
  }

 private:
  void OnDbusConnected(
      CallCallback<void> callback,
      base::expected<GDBusConnectionRef, Loggable> connection) {
    if (!connection.has_value()) {
      std::move(callback).Run(base::unexpected(connection.error()));
      return;
    }
    connection_ = *connection;
    std::move(callback).Run(base::ok());
  }

  void AttemptFetchEnvironment(base::TimeTicks start_time,
                               CallCallback<base::EnvironmentMap> callback) {
    connection_.GetProperty<org_freedesktop_systemd1_Manager::Environment>(
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        base::BindOnce(&UserSystemdEnvManager::OnEnvironmentProperty,
                       base::Unretained(this), start_time,
                       std::move(callback)));
  }

  void OnEnvironmentProperty(
      base::TimeTicks start_time,
      CallCallback<base::EnvironmentMap> callback,
      base::expected<std::vector<std::string>, Loggable> result) {
    if (!result.has_value()) {
      std::move(callback).Run(base::unexpected(result.error()));
      return;
    }

    base::EnvironmentMap env_map;
    for (const std::string& env_str : result.value()) {
      auto split_result = base::SplitStringOnce(env_str, '=');
      if (!split_result.has_value() || split_result->first.empty()) {
        LOG(WARNING) << "Invalid line in systemctl output: " << env_str;
        continue;
      }
      env_map[std::string(split_result->first)] = split_result->second;
    }

    // The compositor isn't immediately ready after the session is created, so
    // we poll the environment until we see either WAYLAND_DISPLAY or DISPLAY.
    auto session_type_it = env_map.find("XDG_SESSION_TYPE");
    bool is_wayland = session_type_it != env_map.end() &&
                      session_type_it->second == "wayland";
    if ((is_wayland && env_map.contains("WAYLAND_DISPLAY")) ||
        (!is_wayland && env_map.contains("DISPLAY"))) {
      std::move(callback).Run(std::move(env_map));
      return;
    }
    if (base::TimeTicks::Now() - start_time >= kTimeout) {
      std::move(callback).Run(base::unexpected(Loggable(
          FROM_HERE, "Timeout waiting for DISPLAY or WAYLAND_DISPLAY")));
    } else {
      HOST_LOG << "DISPLAY or WAYLAND_DISPLAY not found in systemd environment."
               << "Retrying in " << kPollInterval;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&UserSystemdEnvManager::AttemptFetchEnvironment,
                         base::Unretained(this), start_time,
                         std::move(callback)),
          kPollInterval);
    }
  }

  GDBusConnectionRef connection_;
};

}  // namespace

// A helper program for RemoteDisplaySessionManager that does the following:
//
//   1. Initially run as root, then drop its privileges to the user specified by
//      the --username switch.
//   2. Set DBUS_SESSION_BUS_ADDRESS to connect to the specified user's session
//      bus.
//   3. Fetch the systemd environment variables for the specified user.
//   4. Wait for the DISPLAY/WAYLAND_DISPLAY variable to be set by the
//      compositor.
//   5. Unset some environment variables that are known to cause problems.
//   6. Write the systemd environment variables to stdout in JSON format.
int UserSystemdEnvMain() {
  std::string username =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kSystemdUserEnvUsernameSwitchName);
  if (username.empty()) {
    LOG(ERROR) << "Missing switch: " << kSystemdUserEnvUsernameSwitchName;
    return EXIT_FAILURE;
  }
  clearenv();

  HOST_LOG << "Dropping privileges to user: " << username;

  auto user_info = GetPasswdUserInfo(username);
  if (!user_info.has_value()) {
    LOG(ERROR) << "Failed to get user info: " << user_info.error();
    return EXIT_FAILURE;
  }
  if (setgid(user_info->gid) != 0) {
    PLOG(ERROR) << "Failed to setgid: " << user_info->gid;
    return EXIT_FAILURE;
  }
  if (setuid(user_info->uid) != 0) {
    PLOG(ERROR) << "Failed to setuid: " << user_info->uid;
    return EXIT_FAILURE;
  }

  base::FilePath dbus_socket =
      base::FilePath(base::StringPrintf("/run/user/%u/bus", user_info->uid));

  std::string dbus_address = "unix:path=" + dbus_socket.value();

  setenv("DBUS_SESSION_BUS_ADDRESS", dbus_address.c_str(), /*replace=*/true);

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("UserSystemdEnv");
  // A UI thread is required for GDBus.
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  base::TimeTicks poll_dbus_socket_deadline = base::TimeTicks::Now() + kTimeout;
  while (!base::PathExists(dbus_socket)) {
    if (base::TimeTicks::Now() > poll_dbus_socket_deadline) {
      LOG(ERROR) << "Timeout waiting for D-Bus socket " << dbus_socket
                 << " to be created.";
      return EXIT_FAILURE;
    }
    HOST_LOG << "D-Bus socket " << dbus_socket << " not ready. Retrying in "
             << kPollInterval;
    base::PlatformThread::Sleep(kPollInterval);
  }

  UserSystemdEnvManager manager;

  base::expected<void, Loggable> connect_dbus_result =
      manager.CallSync<void>(&UserSystemdEnvManager::ConnectDbus);
  if (!connect_dbus_result.has_value()) {
    LOG(ERROR) << connect_dbus_result.error();
    return EXIT_FAILURE;
  }

  base::expected<base::EnvironmentMap, Loggable> fetch_environment_result =
      manager.CallSync<base::EnvironmentMap>(
          &UserSystemdEnvManager::FetchEnvironment);
  if (!fetch_environment_result.has_value()) {
    LOG(ERROR) << fetch_environment_result.error();
    return EXIT_FAILURE;
  }

  // TODO: crbug.com/488713023 - unset offensive environment variables such as
  // GDK_BACKEND here.

  base::DictValue env_vars_dict(
      std::make_move_iterator(fetch_environment_result->begin()),
      std::make_move_iterator(fetch_environment_result->end()));
  std::string output;
  if (!base::JSONWriter::Write(env_vars_dict, &output)) {
    LOG(ERROR) << "Failed to serialize environment variables to JSON.";
    return EXIT_FAILURE;
  }

  std::cout << output << std::endl;

  return EXIT_SUCCESS;
}

}  // namespace remoting
