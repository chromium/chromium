// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMEZONE_TIMEZONE_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMEZONE_TIMEZONE_CONTROLLER_H_

#include <memory>

#include "base/synchronization/lock.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/time_zone_monitor.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// An instance of TimeZoneController manages renderer time zone. It listens to
// the host system time zone change notifications from the browser and when
// received, updates the default ICU time zone and notifies V8 and workers.
//
// Time zone override mode allows clients to temporarily override host system
// time zone with the one specified by the client. The client can change the
// time zone, however no other client will be allowed to set another override
// until the existing override is removed. When the override is removed, the
// current host system time zone is assumed.
class CORE_EXPORT TimeZoneController final
    : public device::mojom::blink::TimeZoneMonitorClient {
 public:
  ~TimeZoneController() override;

  static void Init();

  enum class TimeZoneOverrideStatus {
    kSuccess,
    kAlreadyInEffect,
    kInvalidTimezone,
  };

  // An RAII-style handle that represents exclusive ownership of the global
  // timezone override.
  //
  // The timezone override remains active as long as an instance of this handle
  // exists. The destructor automatically clears the override, ensuring the
  // resource is always released correctly.
  //
  // This class forms an ownership model with the static `SetTimeZoneOverride()`
  // function:
  // 1. Acquisition: Call the static `SetTimeZoneOverride()` to acquire a
  //    handle.
  // 2. Usage: Call the `change()` method on an acquired handle to modify the
  //    timezone.
  // 3. Release: The override is cleared when the handle is destroyed.
  class TimeZoneOverride {
    friend TimeZoneController;
    TimeZoneOverride() = default;

   public:
    // Modifies the timezone for an existing, active override.
    void change(const String& timezone_id) {
      ChangeTimeZoneOverride(timezone_id);
    }

    ~TimeZoneOverride() { ClearTimeZoneOverride(); }
  };

  struct TimeZoneOverrideResult {
    TimeZoneOverrideStatus status;

    // The handle managing the override's lifetime (RAII). The override remains
    // in effect as long as this handle is alive.
    std::unique_ptr<TimeZoneOverride> handle;
  };

  // Atomically attempts to acquire the global timezone override, succeeding
  // only if no other override is currently active.
  static TimeZoneOverrideResult SetTimeZoneOverride(const String& timezone_id);

  static void ChangeTimeZoneForTesting(const String&);

 private:
  TimeZoneController();
  static TimeZoneController& instance();
  static bool HasTimeZoneOverride();
  static const String& TimeZoneIdOverride();
  static void ClearTimeZoneOverride();
  static void ChangeTimeZoneOverride(const String&);
  static bool SetIcuTimeZoneAndNotifyV8(const String& timezone_id);

  const String& GetHostTimezoneId();

  // device::mojom::blink::TimeZoneMonitorClient:
  void OnTimeZoneChange(const String& timezone_id) override;

  // receiver_ must not use HeapMojoReceiver. TimeZoneController is not managed
  // by Oilpan.
  mojo::Receiver<device::mojom::blink::TimeZoneMonitorClient> receiver_{this};

  base::Lock lock_;

  String host_timezone_id_ GUARDED_BY(lock_);
  String override_timezone_id_ GUARDED_BY(lock_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMEZONE_TIMEZONE_CONTROLLER_H_
