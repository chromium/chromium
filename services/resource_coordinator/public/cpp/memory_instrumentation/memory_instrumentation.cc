// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#include <atomic>

#include "base/functional/bind.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom-data-view.h"

namespace memory_instrumentation {
namespace {

std::atomic<MemoryInstrumentation*> g_instance = nullptr;

void WrapGlobalMemoryDump(
    MemoryInstrumentation::RequestGlobalDumpCallback callback,
    mojom::RequestOutcome outcome,
    mojom::GlobalMemoryDumpPtr dump) {
  std::move(callback).Run(outcome, GlobalMemoryDump::MoveFrom(std::move(dump)));
}
}  // namespace

// static
void MemoryInstrumentation::CreateInstance(
    mojo::PendingRemote<memory_instrumentation::mojom::Coordinator> coordinator,
    bool is_browser_process) {
  DCHECK(!g_instance);
  g_instance.store(
      new MemoryInstrumentation(std::move(coordinator), is_browser_process),
      std::memory_order_release);
}

// static
MemoryInstrumentation* MemoryInstrumentation::GetInstance() {
  // Called from a different thread from CreateInstance(), make sure that the
  // updates are visible.
  return g_instance.load(std::memory_order_acquire);
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
