// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEMORY_DUMP_PROVIDER_PROXY_H_
#define MEDIA_BASE_MEMORY_DUMP_PROVIDER_PROXY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/memory_dump_provider.h"
#include "media/base/media_export.h"

namespace media {

using MemoryDumpCB =
    base::RepeatingCallback<void(const base::trace_event::MemoryDumpArgs& args,
                                 base::trace_event::ProcessMemoryDump* pmd)>;

class MEDIA_EXPORT MemoryDumpProviderProxy final
    : public base::trace_event::MemoryDumpProvider {
 public:
  MemoryDumpProviderProxy(
      const char* name,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MemoryDumpCB dump_cb);

  ~MemoryDumpProviderProxy() override;

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  MemoryDumpCB dump_cb_;

  DISALLOW_COPY_AND_ASSIGN(MemoryDumpProviderProxy);
};

}  // namespace media

#endif  // MEDIA_BASE_MEMORY_DUMP_PROVIDER_PROXY_H_
