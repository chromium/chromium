// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_DBUS_CLIENT_H_
#define REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_DBUS_CLIENT_H_

#include <gio/gio.h>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/linux/gnome_display_config.h"
#include "remoting/host/linux/scoped_glib.h"
#include "ui/base/glib/scoped_gobject.h"

namespace remoting {

// This class provides a wrapper for the GNOME D-Bus API for querying the
// current display (monitor) configuration, and for applying a modified config.
// This wrapper never blocks the caller thread - the caller provides a
// OnceCallback to receive the current config. The caller can modify the
// returned GnomeDisplayConfig structure, and then ask GNOME to apply the
// changes. Note that applying a display config will take effect immediately,
// but will also cause GNOME to display a warning which allows the user to keep
// or revert the changes.
//
// Instances of this class only support a single callback at a time - later
// requests will cancel earlier ones. Changes to GNOME's display config
// should be infrequent, and if multiple operations are ongoing, only the
// latest operation should take effect.
class GnomeDisplayConfigDBusClient {
 public:
  using Callback = base::OnceCallback<void(GnomeDisplayConfig)>;

  GnomeDisplayConfigDBusClient();
  GnomeDisplayConfigDBusClient(const GnomeDisplayConfigDBusClient&) = delete;
  GnomeDisplayConfigDBusClient& operator=(const GnomeDisplayConfigDBusClient&) =
      delete;
  ~GnomeDisplayConfigDBusClient();

  // Initializes the object by requesting a D-Bus connection. This should be
  // called on the same thread as all other public methods, including the dtor.
  // The private static methods get called by GLib and may execute on a
  // different thread.
  void Init();

  // Request the latest config from GNOME. On success, the config will be
  // provided to the callback.
  void GetMonitorsConfig(Callback callback);

  // Requests GNOME to apply a new config. If successful, the change will
  // take immediate effect, but the user may see a popup window and they may
  // choose to revert back to the previous settings.
  void ApplyMonitorsConfig(GnomeDisplayConfig config);

  // Fakes a GetCurrentState() response from GNOME. This allows unittests to
  // exercise this code without relying on GNOME or DBus services.
  void FakeDisplayConfigForTest(ScopedGVariant config);

 private:
  static void OnDBusGetReply(GObject* source_object,
                             GAsyncResult* result,
                             gpointer user_data);
  static void OnDisplayConfigCurrentStateReply(GObject* source_object,
                                               GAsyncResult* result,
                                               gpointer user_data);
  static void OnApplyMonitorsConfigReply(GObject* source_object,
                                         GAsyncResult* result,
                                         gpointer user_data);

  // Starts an async call to the DBus GetCurrentState() method.
  void CallDBusGetCurrentState();

  // Called by OnDBusGetReply().
  void OnDBusGet(ScopedGObject<GDBusConnection> dbus_connection);

  // Called by OnDisplayConfigCurrentStateReply().
  void OnDisplayConfigCurrentState(ScopedGVariant config);

  // Called by OnDisplayConfigCurrentStateReply() on error.
  void OnDisplayConfigCurrentStateError();

  base::WeakPtr<GnomeDisplayConfigDBusClient> weak_ptr_;

  scoped_refptr<base::SequencedTaskRunner> caller_task_runner_;

  ScopedGObject<GCancellable> cancellable_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ScopedGObject<GDBusConnection> dbus_connection_
      GUARDED_BY_CONTEXT(sequence_checker_);

  Callback pending_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GnomeDisplayConfigDBusClient> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_DBUS_CLIENT_H_
