// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_WITH_SOURCE_H_
#define NET_LOG_NET_LOG_WITH_SOURCE_H_

#include "net/base/net_export.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"

namespace net {
class NetLog;

// Helper that binds a Source to a NetLog, and exposes convenience methods to
// output log messages without needing to pass in the source.
class NET_EXPORT NetLogWithSource {
 public:
  NetLogWithSource();
  ~NetLogWithSource();

  // Adds a log entry to the NetLog for the bound source.
  void AddEntry(NetLogEventType type, NetLogEventPhase phase) const;

  // See "Materializing parameters" in net_log.h for details on |get_params|.
  template <typename ParametersCallback>
  void AddEntry(NetLogEventType type,
                NetLogEventPhase phase,
                const ParametersCallback& get_params) const {
    non_null_net_log_->AddEntry(type, source_, phase, get_params);
  }

  // Convenience methods that call AddEntry with a fixed "capture phase"
  // (begin, end, or none).
  void BeginEvent(NetLogEventType type) const;

  // See "Materializing parameters" in net_log.h for details on |get_params|.
  template <typename ParametersCallback>
  void BeginEvent(NetLogEventType type,
                  const ParametersCallback& get_params) const {
    AddEntry(type, NetLogEventPhase::BEGIN, get_params);
  }

  void EndEvent(NetLogEventType type) const;

  // See "Materializing parameters" in net_log.h for details on |get_params|.
  template <typename ParametersCallback>
  void EndEvent(NetLogEventType type,
                const ParametersCallback& get_params) const {
    AddEntry(type, NetLogEventPhase::END, get_params);
  }

  void AddEvent(NetLogEventType type) const;

  // See "Materializing parameters" in net_log.h for details on |get_params|.
  template <typename ParametersCallback>
  void AddEvent(NetLogEventType type,
                const ParametersCallback& get_params) const {
    AddEntry(type, NetLogEventPhase::NONE, get_params);
  }

  void AddEventWithStringParams(NetLogEventType type,
                                base::StringPiece name,
                                base::StringPiece value) const;

  void AddEventWithIntParams(NetLogEventType type,
                             base::StringPiece name,
                             int value) const;

  void BeginEventWithIntParams(NetLogEventType type,
                               base::StringPiece name,
                               int value) const;

  void EndEventWithIntParams(NetLogEventType type,
                             base::StringPiece name,
                             int value) const;

  void AddEventWithInt64Params(NetLogEventType type,
                               base::StringPiece name,
                               int64_t value) const;

  void BeginEventWithStringParams(NetLogEventType type,
                                  base::StringPiece name,
                                  base::StringPiece value) const;

  void AddEventReferencingSource(NetLogEventType type,
                                 const NetLogSource& source) const;

  void BeginEventReferencingSource(NetLogEventType type,
                                   const NetLogSource& source) const;

  // Just like AddEvent, except |net_error| is a net error code.  A parameter
  // called "net_error" with the indicated value will be recorded for the event.
  // |net_error| must be negative, and not ERR_IO_PENDING, as it's not a true
  // error.
  void AddEventWithNetErrorCode(NetLogEventType event_type,
                                int net_error) const;

  // Just like EndEvent, except |net_error| is a net error code.  If it's
  // negative, a parameter called "net_error" with a value of |net_error| is
  // associated with the event.  Otherwise, the end event has no parameters.
  // |net_error| must not be ERR_IO_PENDING, as it's not a true error.
  void EndEventWithNetErrorCode(NetLogEventType event_type,
                                int net_error) const;

  void AddEntryWithBoolParams(NetLogEventType type,
                              NetLogEventPhase phase,
                              base::StringPiece name,
                              bool value) const;

  // Logs a byte transfer event to the NetLog.  Determines whether to log the
  // received bytes or not based on the current logging level.
  void AddByteTransferEvent(NetLogEventType event_type,
                            int byte_count,
                            const char* bytes) const;

  bool IsCapturing() const { return non_null_net_log_->IsCapturing(); }

  // Helper to create a NetLogWithSource given a NetLog and a NetLogSourceType.
  // Takes care of creating a unique source ID, and handles
  //  the case of NULL net_log.
  static NetLogWithSource Make(NetLog* net_log, NetLogSourceType source_type);

  const NetLogSource& source() const { return source_; }

  // Returns the bound NetLog*, or nullptr.
  NetLog* net_log() const;

 private:
  NetLogWithSource(const NetLogSource& source, NetLog* non_null_net_log)
      : source_(source), non_null_net_log_(non_null_net_log) {}

  NetLogSource source_;

  // There are two types of NetLogWithSource:
  //
  // (a) An ordinary NetLogWithSource for which |source().IsValid()| and
  //     |net_log() != nullptr|
  //
  // (b) A default constructed NetLogWithSource for which
  //     |!source().IsValid()| and |net_log() == nullptr|.
  //
  // As an optimization, both types internally store a non-null NetLog*. This
  // way no null checks are needed before dispatching to the (possibly dummy)
  // NetLog
  NetLog* non_null_net_log_;
};

}  // namespace net

#endif  // NET_LOG_NET_LOG_WITH_SOURCE_H_
