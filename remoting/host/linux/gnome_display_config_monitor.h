// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_MONITOR_H_
#define REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_MONITOR_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/host/linux/gnome_display_config_dbus_client.h"

namespace remoting {

struct GnomeDisplayConfig;

// This class is responsible for monitoring the display configuration on a Gnome
// desktop and notifying observers when the configuration changes. It is not
// thread-safe and all methods must be called on the same sequence.
class GnomeDisplayConfigMonitor {
 public:
  // Represents an active subscription. Once the subscription is destroyed, the
  // registered callback will no longer be called by GnomeDisplayConfigMonitor.
  struct Subscription {
    ~Subscription();

   private:
    friend class GnomeDisplayConfigMonitor;

    Subscription();

    base::CallbackListSubscription subscription_;
    base::WeakPtrFactory<Subscription> weak_factory_{this};
  };

  using CallbackSignature = void(const GnomeDisplayConfig&);
  using Callback = base::RepeatingCallback<CallbackSignature>;

  explicit GnomeDisplayConfigMonitor(
      base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client);
  ~GnomeDisplayConfigMonitor();

  GnomeDisplayConfigMonitor(const GnomeDisplayConfigMonitor&) = delete;
  GnomeDisplayConfigMonitor& operator=(const GnomeDisplayConfigMonitor&) =
      delete;

  // Adds a callback to be notified when the display configuration changes.
  // The returned subscription can be used to unsubscribe.
  // If `call_with_current_config` is true and the current config exists,
  // `callback` will immediately be called with the current config by posting a
  // task to the current sequence (not on the current task frame).
  [[nodiscard]] std::unique_ptr<Subscription> AddCallback(
      Callback callback,
      bool call_with_current_config);

  // Returns the current config, or nullopt if no display config has been
  // received yet.
  const std::optional<GnomeDisplayConfig>& current_config() const {
    return current_config_;
  }

  base::WeakPtr<GnomeDisplayConfigMonitor> GetWeakPtr();

 private:
  void Start();
  void QueryDisplayConfig();
  void OnGnomeDisplayConfigReceived(GnomeDisplayConfig config);
  void CallWithCurrentConfig();

  std::optional<GnomeDisplayConfig> current_config_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<GnomeDisplayConfigDBusClient::Subscription>
      monitors_changed_subscription_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::RepeatingCallbackList<void(const GnomeDisplayConfig&)> callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::OnceCallbackList<void(const GnomeDisplayConfig&)>
      callbacks_pending_current_config_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GnomeDisplayConfigMonitor> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_MONITOR_H_
