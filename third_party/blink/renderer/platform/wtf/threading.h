/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREADING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREADING_H_

#include <stdint.h>
#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

WTF_EXPORT base::PlatformThreadId CurrentThread();

#if DCHECK_IS_ON()
WTF_EXPORT bool IsBeforeThreadCreated();
WTF_EXPORT void WillCreateThread();
#endif

class AtomicStringTable;
struct ICUConverterWrapper;

class WTF_EXPORT Threading {
  DISALLOW_NEW();

 public:
  Threading();
  ~Threading();

  AtomicStringTable& GetAtomicStringTable() { return *atomic_string_table_; }

  ICUConverterWrapper& CachedConverterICU() { return *cached_converter_icu_; }

  base::PlatformThreadId ThreadId() const { return thread_id_; }

  // Must be called on the main thread before any callers to wtfThreadData().
  static void Initialize();

#if defined(OS_WIN) && defined(COMPILER_MSVC)
  static size_t ThreadStackSize();
#endif

 private:
  std::unique_ptr<AtomicStringTable> atomic_string_table_;
  std::unique_ptr<ICUConverterWrapper> cached_converter_icu_;

  base::PlatformThreadId thread_id_;

#if defined(OS_WIN) && defined(COMPILER_MSVC)
  size_t thread_stack_size_ = 0u;
#endif

  static ThreadSpecific<Threading>* static_data_;
  friend Threading& WtfThreading();

  DISALLOW_COPY_AND_ASSIGN(Threading);
};

inline Threading& WtfThreading() {
  DCHECK(Threading::static_data_);
  return **Threading::static_data_;
}

}  // namespace WTF

using WTF::CurrentThread;
using WTF::Threading;
using WTF::WtfThreading;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREADING_H_
