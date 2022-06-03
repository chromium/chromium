// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/memory_dump_provider_proxy.h"

#include <utility>

#include "base/trace_event/memory_dump_manager.h"

namespace media {

MemoryDumpProviderProxy::MemoryDumpProviderProxy(
    const char* name,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MemoryDumpCB dump_cb)
    : dump_cb_(std::move(dump_cb)) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, name, std::move(task_runner));
}

MemoryDumpProviderProxy::~MemoryDumpProviderProxy() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool MemoryDumpProviderProxy::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  dump_cb_.Run(args, pmd);
  return true;
}

}  // namespace media
