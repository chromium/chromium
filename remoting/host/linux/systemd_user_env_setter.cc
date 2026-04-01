// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/systemd_user_env_setter.h"

#include <systemd/sd-bus.h>
#include <unistd.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "remoting/base/logging.h"

namespace remoting {

namespace {

constexpr base::TimeDelta kTimeout = base::Seconds(10);
constexpr base::TimeDelta kPollInterval = base::Seconds(1);

using ScopedSdBus =
    std::unique_ptr<sd_bus, decltype([](sd_bus* bus) { sd_bus_unref(bus); })>;
using ScopedSdBusMessage =
    std::unique_ptr<sd_bus_message, decltype([](sd_bus_message* message) {
                      sd_bus_message_unref(message);
                    })>;

struct SdBusError {
  SdBusError() = default;
  SdBusError(const SdBusError&) = delete;
  ~SdBusError() {
    // This only frees resources held by `error`, but not `error` itself.
    sd_bus_error_free(&error);
  }
  sd_bus_error* get() { return &error; }
  sd_bus_error* operator->() { return &error; }

 private:
  sd_bus_error error = SD_BUS_ERROR_NULL;
};

base::expected<base::EnvironmentMap, Loggable> ParseEnvironment(
    sd_bus_message* message) {
  int r = sd_bus_message_enter_container(message, 'a', "s");
  if (r < 0) {
    return base::unexpected(
        Loggable(FROM_HERE,
                 base::StringPrintf("Failed to enter Environment container: %s",
                                    strerror(-r))));
  }

  // `env_str` is never owned by us.
  const char* env_str = nullptr;
  base::EnvironmentMap env_map;
  while (sd_bus_message_read(message, "s", &env_str) > 0) {
    auto split_result = base::SplitStringOnce(env_str, '=');
    if (split_result.has_value() && !split_result->first.empty()) {
      env_map[std::string(split_result->first)] = split_result->second;
    }
  }
  return env_map;
}

}  // namespace

base::expected<void, Loggable> SetSystemdUserEnvironment() {
  uid_t uid = getuid();
  base::FilePath dbus_socket =
      base::FilePath(base::StringPrintf("/run/user/%u/bus", uid));

  base::TimeTicks deadline = base::TimeTicks::Now() + kTimeout;
  bool is_initial_poll = true;

  while (base::TimeTicks::Now() < deadline) {
    if (is_initial_poll) {
      is_initial_poll = false;
    } else {
      base::PlatformThread::Sleep(kPollInterval);
    }

    if (!base::PathExists(dbus_socket)) {
      HOST_LOG << "D-Bus socket " << dbus_socket << " not ready. Retrying in "
               << kPollInterval;
      continue;
    }

    std::string dbus_address = "unix:path=" + dbus_socket.value();
    setenv("DBUS_SESSION_BUS_ADDRESS", dbus_address.c_str(),
           /*replace=*/true);

    sd_bus* bus_ptr = nullptr;
    int r = sd_bus_open_user(&bus_ptr);
    if (r < 0) {
      LOG(WARNING) << "Failed to connect to user D-Bus: "
                   << base::safe_strerror(-r) << ". Retrying in "
                   << kPollInterval;
      continue;
    }

    ScopedSdBus bus(bus_ptr);
    SdBusError error;
    sd_bus_message* message_ptr = nullptr;
    r = sd_bus_get_property(bus.get(), "org.freedesktop.systemd1",
                            "/org/freedesktop/systemd1",
                            "org.freedesktop.systemd1.Manager", "Environment",
                            error.get(), &message_ptr, "as");
    if (r < 0) {
      LOG(WARNING) << "Failed to get Environment property from systemd: "
                   << (error->message ? error->message
                                      : base::safe_strerror(-r))
                   << ". Retrying in " << kPollInterval;
      continue;
    }

    ScopedSdBusMessage message(message_ptr);
    auto result = ParseEnvironment(message.get());
    if (!result.has_value()) {
      return base::unexpected(result.error());
    }

    auto& env_map = result.value();
    // The compositor isn't immediately ready after the session is created, so
    // we poll the environment until we see either WAYLAND_DISPLAY or DISPLAY.
    auto session_type_it = env_map.find("XDG_SESSION_TYPE");
    bool is_wayland = session_type_it != env_map.end() &&
                      session_type_it->second == "wayland";
    if ((is_wayland && env_map.contains("WAYLAND_DISPLAY")) ||
        (!is_wayland && env_map.contains("DISPLAY"))) {
      for (const auto& [key, value] : env_map) {
        setenv(key.c_str(), value.c_str(), /*replace=*/true);
      }
      return base::ok();
    }

    HOST_LOG << "DISPLAY or WAYLAND_DISPLAY not found in systemd environment. "
             << "Retrying in " << kPollInterval;
  }

  return base::unexpected(Loggable(FROM_HERE,
                                   "Timeout waiting for DISPLAY or "
                                   "WAYLAND_DISPLAY in systemd environment."));
}

}  // namespace remoting
