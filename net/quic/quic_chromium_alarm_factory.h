// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Chrome-specific helper for quic::QuicConnection which uses
// a TaskRunner for alarms, and uses a DatagramClientSocket for writing data.

#ifndef NET_QUIC_QUIC_CHROMIUM_ALARM_FACTORY_H_
#define NET_QUIC_QUIC_CHROMIUM_ALARM_FACTORY_H_

#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/third_party/quiche/src/quic/core/quic_alarm_factory.h"
#include "net/third_party/quiche/src/quic/core/quic_clock.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_time.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace net {

class NET_EXPORT_PRIVATE QuicChromiumAlarmFactory
    : public quic::QuicAlarmFactory {
 public:
  QuicChromiumAlarmFactory(base::TaskRunner* task_runner,
                           const quic::QuicClock* clock);
  ~QuicChromiumAlarmFactory() override;

  // quic::QuicAlarmFactory
  quic::QuicAlarm* CreateAlarm(quic::QuicAlarm::Delegate* delegate) override;
  quic::QuicArenaScopedPtr<quic::QuicAlarm> CreateAlarm(
      quic::QuicArenaScopedPtr<quic::QuicAlarm::Delegate> delegate,
      quic::QuicConnectionArena* arena) override;

 private:
  base::TaskRunner* task_runner_;
  const quic::QuicClock* clock_;
  base::WeakPtrFactory<QuicChromiumAlarmFactory> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuicChromiumAlarmFactory);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CHROMIUM_ALARM_FACTORY_H_
