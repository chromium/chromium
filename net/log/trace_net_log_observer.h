// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_TRACE_NET_LOG_OBSERVER_H_
#define NET_LOG_TRACE_NET_LOG_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "net/base/net_export.h"
#include "net/base/tracing.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"

namespace net {

// TraceNetLogObserver watches for TraceLog enable, and sends NetLog
// events to TraceLog if it is enabled.
class NET_EXPORT TraceNetLogObserver
    : public NetLog::ThreadSafeObserver,
      public base::trace_event::TraceLog::AsyncEnabledStateObserver {
 public:
  struct Options final {
    // Work around https://bugs.llvm.org/show_bug.cgi?id=36684
    static Options Default() { return {}; }

    NetLogCaptureMode capture_mode = NetLogCaptureMode::kDefault;

    // If false, trace events will be logged under the "netlog" category.
    // If true, trace events will be logged under the
    // "disabled-by-default-netlog.sensitive" category.
    //
    // TODO(https://crbug.com/410018349): ideally this should be derived from
    // `capture_mode`, i.e. we should treat this as true if `capture_mode` is
    // not `kHeavilyRedacted`. We'd need to assess the consequences on current
    // trace users, though.
    bool use_sensitive_category = false;
  };
  explicit TraceNetLogObserver(Options options = Options::Default());

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
  // Used to derive track ids. We use a random number in an attempt to keep
  // track ids globally unique, which is a requirement of the track event API.
  const uint64_t track_id_base_ = base::RandUint64();

  const NetLogCaptureMode capture_mode_;
  const bool use_sensitive_category_;
  raw_ptr<NetLog> net_log_to_watch_ = nullptr;
  base::WeakPtrFactory<TraceNetLogObserver> weak_factory_{this};
};

}  // namespace net

#endif  // NET_LOG_TRACE_NET_LOG_OBSERVER_H_
