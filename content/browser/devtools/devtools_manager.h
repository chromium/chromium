// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_

#include <memory>

#include "base/memory/singleton.h"
#include "base/trace_event/memory_dump_provider.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

// This class is a singleton that manage global DevTools state for the whole
// browser.
// TODO(dgozman): remove this class entirely.
class CONTENT_EXPORT DevToolsManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  // Returns single instance of this class. The instance is destroyed on the
  // browser main loop exit so this method MUST NOT be called after that point.
  static DevToolsManager* GetInstance();

  DevToolsManager();

  DevToolsManager(const DevToolsManager&) = delete;
  DevToolsManager& operator=(const DevToolsManager&) = delete;

  ~DevToolsManager() override;

  DevToolsManagerDelegate* delegate() const { return delegate_.get(); }

  // It is necessary to recreate the delegate when the ContentBrowserClient gets
  // swapped out.
  static void ShutdownForTests();

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend struct base::DefaultSingletonTraits<DevToolsManager>;

  std::unique_ptr<DevToolsManagerDelegate> delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_
