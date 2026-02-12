// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_manager.h"

#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return base::Singleton<DevToolsManager>::get();
}

DevToolsManager::DevToolsManager()
    : delegate_(
          GetContentClient()->browser()->CreateDevToolsManagerDelegate()) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "DevToolsManager",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void DevToolsManager::ShutdownForTests() {
  base::Singleton<DevToolsManager>::OnExit(nullptr);
}

DevToolsManager::~DevToolsManager() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool DevToolsManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto* dump = pmd->CreateAllocatorDump("devtools/sessions");
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  DevToolsSession::GetRootSessionCount());
  return true;
}

}  // namespace content
