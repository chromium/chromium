// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_with_source.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_values.h"

namespace net {

namespace {

// Returns parameters for logging data transferred events. At a minimum includes
// the number of bytes transferred. If the capture mode allows logging byte
// contents and |byte_count| > 0, then will include the actual bytes.
base::Value::Dict BytesTransferredParams(int byte_count,
                                         const char* bytes,
                                         NetLogCaptureMode capture_mode) {
  base::Value::Dict dict;
  dict.Set("byte_count", byte_count);
  if (NetLogCaptureIncludesSocketBytes(capture_mode) && byte_count > 0)
    dict.Set("bytes", NetLogBinaryValue(bytes, byte_count));
  return dict;
}

}  // namespace

NetLogWithSource::NetLogWithSource() {
  // Conceptually, default NetLogWithSource have no NetLog*, and will return
  // nullptr when calling |net_log()|. However for performance reasons, we
  // always store a non-null member to the NetLog in order to avoid needing
  // null checks for critical codepaths.
  //
  // The "dummy" net log used here will always return false for IsCapturing(),
  // and have no sideffects should its method be called. In practice the only
  // method that will get called on it is IsCapturing().
  static base::NoDestructor<NetLog> dummy{base::PassKey<NetLogWithSource>()};
  DCHECK(!dummy->IsCapturing());
  non_null_net_log_ = dummy.get();
}

void NetLogWithSource::AddEntry(NetLogEventType type,
                                NetLogEventPhase phase) const {
  non_null_net_log_->AddEntry(type, source_, phase);
}

void NetLogWithSource::AddEvent(NetLogEventType type) const {
  AddEntry(type, NetLogEventPhase::NONE);
}

void NetLogWithSource::AddEventWithStringParams(NetLogEventType type,
                                                std::string_view name,
                                                std::string_view value) const {
  AddEvent(type, [&] { return NetLogParamsWithString(name, value); });
}

void NetLogWithSource::AddEventWithIntParams(NetLogEventType type,
                                             std::string_view name,
                                             int value) const {
  AddEvent(type, [&] { return NetLogParamsWithInt(name, value); });
}

void NetLogWithSource::BeginEventWithIntParams(NetLogEventType type,
                                               std::string_view name,
                                               int value) const {
  BeginEvent(type, [&] { return NetLogParamsWithInt(name, value); });
}

void NetLogWithSource::EndEventWithIntParams(NetLogEventType type,
                                             std::string_view name,
                                             int value) const {
  EndEvent(type, [&] { return NetLogParamsWithInt(name, value); });
}

void NetLogWithSource::AddEventWithInt64Params(NetLogEventType type,
                                               std::string_view name,
                                               int64_t value) const {
  AddEvent(type, [&] { return NetLogParamsWithInt64(name, value); });
}

void NetLogWithSource::BeginEventWithStringParams(
    NetLogEventType type,
    std::string_view name,
    std::string_view value) const {
  BeginEvent(type, [&] { return NetLogParamsWithString(name, value); });
}

void NetLogWithSource::AddEventReferencingSource(
    NetLogEventType type,
    const NetLogSource& source) const {
  AddEvent(type, [&] { return source.ToEventParameters(); });
}

void NetLogWithSource::BeginEventReferencingSource(
    NetLogEventType type,
    const NetLogSource& source) const {
  BeginEvent(type, [&] { return source.ToEventParameters(); });
}

void NetLogWithSource::BeginEvent(NetLogEventType type) const {
  AddEntry(type, NetLogEventPhase::BEGIN);
}

void NetLogWithSource::EndEvent(NetLogEventType type) const {
  AddEntry(type, NetLogEventPhase::END);
}

void NetLogWithSource::AddEventWithNetErrorCode(NetLogEventType event_type,
                                                int net_error) const {
  DCHECK_NE(ERR_IO_PENDING, net_error);
  if (net_error >= 0) {
    AddEvent(event_type);
  } else {
    AddEventWithIntParams(event_type, "net_error", net_error);
  }
}

void NetLogWithSource::EndEventWithNetErrorCode(NetLogEventType event_type,
                                                int net_error) const {
  DCHECK_NE(ERR_IO_PENDING, net_error);
  if (net_error >= 0) {
    EndEvent(event_type);
  } else {
    EndEventWithIntParams(event_type, "net_error", net_error);
  }
}

void NetLogWithSource::AddEntryWithBoolParams(NetLogEventType type,
                                              NetLogEventPhase phase,
                                              std::string_view name,
                                              bool value) const {
  AddEntry(type, phase, [&] { return NetLogParamsWithBool(name, value); });
}

void NetLogWithSource::AddByteTransferEvent(NetLogEventType event_type,
                                            int byte_count,
                                            const char* bytes) const {
  AddEvent(event_type, [&](NetLogCaptureMode capture_mode) {
    return BytesTransferredParams(byte_count, bytes, capture_mode);
  });
}

// static
NetLogWithSource NetLogWithSource::Make(NetLog* net_log,
                                        NetLogSourceType source_type) {
  if (!net_log)
    return NetLogWithSource();

  NetLogSource source(source_type, net_log->NextID());
  return NetLogWithSource(source, net_log);
}

// static
NetLogWithSource NetLogWithSource::Make(NetLogSourceType source_type) {
  return NetLogWithSource::Make(NetLog::Get(), source_type);
}

// static
NetLogWithSource NetLogWithSource::Make(NetLog* net_log,
                                        const NetLogSource& source) {
  if (!net_log || !source.IsValid())
    return NetLogWithSource();
  return NetLogWithSource(source, net_log);
}

// static
NetLogWithSource NetLogWithSource::Make(const NetLogSource& source) {
  return NetLogWithSource::Make(NetLog::Get(), source);
}

NetLog* NetLogWithSource::net_log() const {
  if (source_.IsValid())
    return non_null_net_log_;
  return nullptr;
}

}  // namespace net
