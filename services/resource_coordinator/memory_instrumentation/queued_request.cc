// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/queued_request.h"

namespace memory_instrumentation {

QueuedRequest::Args::Args(MemoryDumpType dump_type,
                          MemoryDumpLevelOfDetail level_of_detail,
                          MemoryDumpDeterminism determinism,
                          const std::vector<std::string>& allocator_dump_names,
                          bool add_to_trace,
                          base::ProcessId pid,
                          bool memory_footprint_only)
    : dump_type(dump_type),
      level_of_detail(level_of_detail),
      determinism(determinism),
      allocator_dump_names(allocator_dump_names),
      add_to_trace(add_to_trace),
      pid(pid),
      memory_footprint_only(memory_footprint_only) {}
QueuedRequest::Args::Args(const Args& args) = default;
QueuedRequest::Args::~Args() = default;

QueuedRequest::PendingResponse::PendingResponse(base::ProcessId process_id,
                                                Type type)
    : process_id(process_id), type(type) {}

bool QueuedRequest::PendingResponse::operator<(
    const PendingResponse& other) const {
  return std::tie(process_id, type) < std::tie(other.process_id, other.type);
}

QueuedRequest::Response::Response() {}
QueuedRequest::Response::Response(Response&& other) = default;
QueuedRequest::Response::~Response() = default;

QueuedRequest::QueuedRequest(const Args& args,
                             uint64_t dump_guid,
                             RequestGlobalMemoryDumpInternalCallback callback)
    : args(args), dump_guid(dump_guid), callback(std::move(callback)) {}
QueuedRequest::~QueuedRequest() = default;

base::trace_event::MemoryDumpRequestArgs QueuedRequest::GetRequestArgs() {
  base::trace_event::MemoryDumpRequestArgs request_args;
  request_args.dump_guid = dump_guid;
  request_args.dump_type = args.dump_type;
  request_args.level_of_detail = args.level_of_detail;
  request_args.determinism = args.determinism;
  return request_args;
}

QueuedVmRegionRequest::Response::Response() = default;
QueuedVmRegionRequest::Response::~Response() = default;

QueuedVmRegionRequest::QueuedVmRegionRequest(
    uint64_t dump_guid,
    mojom::HeapProfilerHelper::GetVmRegionsForHeapProfilerCallback callback)
    : dump_guid(dump_guid), callback(std::move(callback)) {}
QueuedVmRegionRequest::~QueuedVmRegionRequest() = default;

}  // namespace memory_instrumentation
