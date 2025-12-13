// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_HEADLESS_DETECTOR_H_
#define REMOTING_HOST_LINUX_GNOME_HEADLESS_DETECTOR_H_

#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

class GnomeHeadlessDetector {
 public:
  // Callback to receive the result of the headless test. If an error occurs,
  // the GNOME session is assumed to be non-headless and `false` is passed to
  // the callback.
  using Callback = base::OnceCallback<void(bool is_headless)>;

  GnomeHeadlessDetector();
  GnomeHeadlessDetector(const GnomeHeadlessDetector&) = delete;
  GnomeHeadlessDetector& operator=(const GnomeHeadlessDetector&) = delete;
  ~GnomeHeadlessDetector();

  // Asynchronously runs the headless test. `callback` is run after the
  // test completes or an error occurs. This method should only be called
  // once - the headless status cannot change during the GNOME session.
  void Start(GDBusConnectionRef connection, Callback callback);

 private:
  void OnGetUnitReply(
      base::expected<std::tuple<gvariant::ObjectPath>, Loggable> result);
  void OnActiveStateReply(base::expected<std::string, Loggable> result);
  void OnExecStartReply(
      base::expected<std::vector<GVariantRef<"(sasbttttuii)">>, Loggable>
          result);

  GDBusConnectionRef dbus_connection_;
  gvariant::ObjectPath systemd_unit_path_;
  Callback callback_;

  base::WeakPtrFactory<GnomeHeadlessDetector> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_HEADLESS_DETECTOR_H_
