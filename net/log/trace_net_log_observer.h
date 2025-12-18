// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_TRACE_NET_LOG_OBSERVER_H_
#define NET_LOG_TRACE_NET_LOG_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/trace_session_observer.h"
#include "net/base/net_export.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"

namespace net {

// TraceNetLogObserver watches for TraceLog enable, and sends NetLog
// events to TraceLog if it is enabled.
class NET_EXPORT TraceNetLogObserver
    : public NetLog::ThreadSafeObserver,
      public base::trace_event::TraceSessionObserver {
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

    // The name of the root track netlog tracks will be nested under.
    perfetto::StaticString root_track_name = "Chromium NetLog";

    // When true, record trace events verbosely:
    // - Use separate tracks for each NetLogSource.
    // - Add flows to relate NetLog events to threads.
    bool verbose = false;
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

  // base::trace_event::TraceSessionObserver implementation:
  void OnStart(const perfetto::DataSourceBase::StartArgs&) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs&) override;

 private:
  void AddEntry(const NetLogEntry& entry,
                perfetto::StaticString entry_type_string,
                perfetto::StaticString source_type_string,
                base::Value::Dict params);
  void AddEntryVerbose(const NetLogEntry& entry,
                       perfetto::StaticString entry_type_string,
                       perfetto::StaticString source_type_string,
                       base::Value::Dict params);

  // The "root track" is used as the parent track of all NetLog event tracks.
  // Folding all NetLog tracks under a root track serves a number of purposes:
  //  - It looks tidier in the Perfetto UI, as it provides a nice visual
  //    separation from the rest of the process child tracks (threads);
  //  - It can be used to distinguish between multiple TraceNetLogObserver
  //    instances (which can happen e.g. if WebView and Cronet are used in the
  //    same process);
  //  - It allows us to customize the ordering of the child tracks. If we hang
  //    NetLog tracks directly under the process track, we are forced into
  //    lexicographic track name ordering which is not the best ordering for
  //    NetLog sources.
  perfetto::Track MaybeSetUpAndGetRootTrack();

  // Used to derive track ids. We use a random number in an attempt to keep
  // track ids globally unique, which is a requirement of the track event API.
  const uint64_t track_id_base_ = base::RandUint64();

  const NetLogCaptureMode capture_mode_;
  const bool use_sensitive_category_;
  const bool verbose_;
  const perfetto::StaticString root_track_name_;

  std::once_flag root_track_set_up_;

  raw_ptr<NetLog> net_log_to_watch_ = nullptr;
  base::WeakPtrFactory<TraceNetLogObserver> weak_factory_{this};
};

}  // namespace net

#endif  // NET_LOG_TRACE_NET_LOG_OBSERVER_H_
