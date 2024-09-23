// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEMORY_DUMP_PROVIDER_PROXY_H_
#define MEDIA_BASE_MEMORY_DUMP_PROVIDER_PROXY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_provider.h"
#include "media/base/media_export.h"

namespace media {

using MemoryDumpCB =
    base::RepeatingCallback<void(const base::trace_event::MemoryDumpArgs& args,
                                 base::trace_event::ProcessMemoryDump* pmd)>;

class MEDIA_EXPORT MemoryDumpProviderProxy final
    : public base::trace_event::MemoryDumpProvider {
 public:
  MemoryDumpProviderProxy(const char* name,
                          scoped_refptr<base::SequencedTaskRunner> task_runner,
                          MemoryDumpCB dump_cb);

  MemoryDumpProviderProxy(const MemoryDumpProviderProxy&) = delete;
  MemoryDumpProviderProxy& operator=(const MemoryDumpProviderProxy&) = delete;

  ~MemoryDumpProviderProxy() override;

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  MemoryDumpCB dump_cb_;
};

}  // namespace media

#endif  // MEDIA_BASE_MEMORY_DUMP_PROVIDER_PROXY_H_
