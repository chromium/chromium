// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_SCREEN_SAVER_INHIBITOR_H_
#define REMOTING_HOST_LINUX_SCREEN_SAVER_INHIBITOR_H_

#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/host/linux/gdbus_connection_ref.h"

namespace remoting {

// An RAII class that inhibits the screen saver (and the screen lock) while it
// is alive. The inhibitor will be released when the process is terminated.
// Note that it will not block the screen lock if it is manually triggered by
// the user.
// This is a simplified version of device::PowerSaveBlocker, which does not
// crash the process if the D-Bus connection disconnects. Unlike
// PowerSaveBlocker, this class does NOT necessarily block sleeps. However,
// external services or scripts may have already applied a power save block. For
// example, GDM applies a power save block as long as there is a remote display.
class ScreenSaverInhibitor {
 public:
  ScreenSaverInhibitor(GDBusConnectionRef connection,
                       std::string_view reason_for_inhibit);
  ~ScreenSaverInhibitor();

  ScreenSaverInhibitor(const ScreenSaverInhibitor&) = delete;
  ScreenSaverInhibitor& operator=(const ScreenSaverInhibitor&) = delete;

 private:
  void OnInhibited(base::expected<std::tuple<uint32_t>, Loggable> result);

  GDBusConnectionRef connection_;
  std::optional<uint32_t> cookie_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ScreenSaverInhibitor> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_SCREEN_SAVER_INHIBITOR_H_
