// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_ALARM_FACTORY_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_ALARM_FACTORY_H_

#include "net/third_party/quic/core/quic_alarm.h"
#include "net/third_party/quic/core/quic_one_block_arena.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

// Creates platform-specific alarms used throughout QUIC.
class QUIC_EXPORT_PRIVATE QuicAlarmFactory {
 public:
  virtual ~QuicAlarmFactory() {}

  // Creates a new platform-specific alarm which will be configured to notify
  // |delegate| when the alarm fires. Returns an alarm allocated on the heap.
  // Caller takes ownership of the new alarm, which will not yet be "set" to
  // fire.
  virtual QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) = 0;

  // Creates a new platform-specific alarm which will be configured to notify
  // |delegate| when the alarm fires. Caller takes ownership of the new alarm,
  // which will not yet be "set" to fire. If |arena| is null, then the alarm
  // will be created on the heap. Otherwise, it will be created in |arena|.
  virtual QuicArenaScopedPtr<QuicAlarm> CreateAlarm(
      QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
      QuicConnectionArena* arena) = 0;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_ALARM_FACTORY_H_
