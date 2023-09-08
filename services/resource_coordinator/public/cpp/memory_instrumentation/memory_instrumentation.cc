// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#include "base/functional/bind.h"

namespace memory_instrumentation {
namespace {

MemoryInstrumentation* g_instance = nullptr;

void WrapGlobalMemoryDump(
    MemoryInstrumentation::RequestGlobalDumpCallback callback,
    bool success,
    mojom::GlobalMemoryDumpPtr dump) {
  std::move(callback).Run(success, GlobalMemoryDump::MoveFrom(std::move(dump)));
}
}  // namespace

// static
void MemoryInstrumentation::CreateInstance(
    mojo::PendingRemote<memory_instrumentation::mojom::Coordinator> coordinator,
    bool is_browser_process) {
  DCHECK(!g_instance);
  g_instance =
      new MemoryInstrumentation(std::move(coordinator), is_browser_process);
}

// static
MemoryInstrumentation* MemoryInstrumentation::GetInstance() {
  return g_instance;
}

MemoryInstrumentation::MemoryInstrumentation(
    mojo::PendingRemote<memory_instrumentation::mojom::Coordinator> coordinator,
    bool is_browser_process)
    : coordinator_(std::move(coordinator)),
      is_browser_process_(is_browser_process) {}

MemoryInstrumentation::~MemoryInstrumentation() {
  g_instance = nullptr;
}

void MemoryInstrumentation::RequestGlobalDump(
    const std::vector<std::string>& allocator_dump_names,
    RequestGlobalDumpCallback callback) {
  CHECK(is_browser_process_);
  coordinator_->RequestGlobalMemoryDump(
      MemoryDumpType::kSummaryOnly, MemoryDumpLevelOfDetail::kBackground,
      MemoryDumpDeterminism::kNone, allocator_dump_names,
      base::BindOnce(&WrapGlobalMemoryDump, std::move(callback)));
}

void MemoryInstrumentation::RequestPrivateMemoryFootprint(
    base::ProcessId pid,
    RequestGlobalDumpCallback callback) {
  CHECK(is_browser_process_);
  coordinator_->RequestPrivateMemoryFootprint(
      pid, base::BindOnce(&WrapGlobalMemoryDump, std::move(callback)));
}

void MemoryInstrumentation::RequestGlobalDumpForPid(
    base::ProcessId pid,
    const std::vector<std::string>& allocator_dump_names,
    RequestGlobalDumpCallback callback) {
  CHECK(is_browser_process_);
  coordinator_->RequestGlobalMemoryDumpForPid(
      pid, allocator_dump_names,
      base::BindOnce(&WrapGlobalMemoryDump, std::move(callback)));
}

void MemoryInstrumentation::RequestGlobalDumpAndAppendToTrace(
    MemoryDumpType dump_type,
    MemoryDumpLevelOfDetail level_of_detail,
    MemoryDumpDeterminism determinism,
    RequestGlobalMemoryDumpAndAppendToTraceCallback callback) {
  CHECK(is_browser_process_);
  coordinator_->RequestGlobalMemoryDumpAndAppendToTrace(
      dump_type, level_of_detail, determinism, std::move(callback));
}

}  // namespace memory_instrumentation
