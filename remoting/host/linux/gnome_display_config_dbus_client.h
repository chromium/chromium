// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_DBUS_CLIENT_H_
#define REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_DBUS_CLIENT_H_

#include <gio/gio.h>

#include "base/callback_list.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
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
  using CallbackSignature = void(GnomeDisplayConfig);
  using Callback = base::OnceCallback<CallbackSignature>;

  // Represents an active subscription. Once the subscription is destroyed, the
  // registered callback will no longer be called by
  // GnomeDisplayConfigDBusClient.
  class Subscription {
   public:
    ~Subscription();

   private:
    Subscription();

    friend class GnomeDisplayConfigDBusClient;

    std::unique_ptr<GDBusConnectionRef::SignalSubscription>
        signal_subscription_;
    base::WeakPtrFactory<Subscription> weak_factory_{this};
  };

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
  void ApplyMonitorsConfig(const GnomeDisplayConfig& config);

  // Subscribes to the MonitorsChanged signal. `on_changed` will be called
  // whenever the screen layout is changed. Discarding the subscription object
  // will unsubscribe from the signal.
  [[nodiscard]] std::unique_ptr<Subscription> SubscribeMonitorsChanged(
      base::RepeatingClosure on_changed);

  // Fakes a GetCurrentState() response from GNOME. This allows unittests to
  // exercise this code without relying on GNOME or DBus services.
  void FakeDisplayConfigForTest(ScopedGVariant config);

  base::WeakPtr<GnomeDisplayConfigDBusClient> GetWeakPtr();

 private:
  // Represents a subscription that is pending because DBus is not yet
  // initialized.
  struct PendingSubscription {
    PendingSubscription(base::RepeatingClosure callback,
                        base::WeakPtr<Subscription> subscription);
    PendingSubscription();
    PendingSubscription(PendingSubscription&&);
    PendingSubscription& operator=(PendingSubscription&&);
    ~PendingSubscription();

    base::RepeatingClosure callback;

    // Used to check if the subscription has already been discarded by the
    // caller, in which case the pending subscription will also be discarded.
    base::WeakPtr<Subscription> subscription;
  };

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

  // Handles all pending DBus MonitorsChanged signal subscriptions.
  void SubscribeDBusMonitorsChanged();

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
  GDBusConnectionRef dbus_connection_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::OnceCallbackList<CallbackSignature> pending_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::queue<PendingSubscription> pending_subscriptions_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GnomeDisplayConfigDBusClient> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_DBUS_CLIENT_H_
