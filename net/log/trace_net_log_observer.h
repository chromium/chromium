// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_TRACE_NET_LOG_OBSERVER_H_
#define NET_LOG_TRACE_NET_LOG_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/base/tracing.h"
#include "net/log/net_log.h"

namespace net {

// TraceNetLogObserver watches for TraceLog enable, and sends NetLog
// events to TraceLog if it is enabled.
class NET_EXPORT TraceNetLogObserver
    : public NetLog::ThreadSafeObserver,
      public base::trace_event::TraceLog::AsyncEnabledStateObserver {
 public:
  TraceNetLogObserver();

  TraceNetLogObserver(const TraceNetLogObserver&) = delete;
  TraceNetLogObserver& operator=(const TraceNetLogObserver&) = delete;

  ~TraceNetLogObserver() override;

  // net::NetLog::ThreadSafeObserver implementation:
  void OnAddEntry(const NetLogEntry& entry) override;

  // Start to watch for TraceLog enable and disable events.
  // This can't be called if already watching for events.
  // Watches NetLog only when tracing is enabled.
  void WatchForTraceStart(NetLog* net_log);

  // Stop watching for TraceLog enable and disable events.
  // If WatchForTraceStart is called, this must be called before
  // TraceNetLogObserver is destroyed.
  void StopWatchForTraceStart();

  // base::trace_event::TraceLog::EnabledStateChangedObserver implementation:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

 private:
  raw_ptr<NetLog> net_log_to_watch_ = nullptr;
  base::WeakPtrFactory<TraceNetLogObserver> weak_factory_{this};
};

}  // namespace net

#endif  // NET_LOG_TRACE_NET_LOG_OBSERVER_H_
