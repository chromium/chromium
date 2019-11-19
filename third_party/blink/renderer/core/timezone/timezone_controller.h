// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMEZONE_TIMEZONE_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMEZONE_TIMEZONE_CONTROLLER_H_

#include <memory>

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

  class TimeZoneOverride {
    friend TimeZoneController;
    TimeZoneOverride() = default;

   public:
    ~TimeZoneOverride() { ClearTimeZoneOverride(); }
  };

  static std::unique_ptr<TimeZoneOverride> SetTimeZoneOverride(
      const String& timezone_id);

  static bool HasTimeZoneOverride();

 private:
  TimeZoneController();
  static TimeZoneController& instance();
  static void ClearTimeZoneOverride();

  // device::mojom::blink::TimeZoneMonitorClient:
  void OnTimeZoneChange(const String& timezone_id) override;

  mojo::Receiver<device::mojom::blink::TimeZoneMonitorClient> receiver_{this};

  String host_timezone_id_;
  bool has_timezone_id_override_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMEZONE_TIMEZONE_CONTROLLER_H_
