/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_INITIALIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_INITIALIZER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

// Specifies how the near V8 heap limit event was handled by the callback.
// This enum is also used for UMA histogram recording. It must be kept in sync
// with the corresponding enum in tools/metrics/histograms/enums.xml. See that
// enum for the detailed description of each case.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NearV8HeapLimitHandling {
  kForwardedToBrowser = 0,
  kIgnoredDueToSmallUptime = 1,
  kIgnoredDueToChangedHeapLimit = 2,
  kIgnoredDueToWorker = 3,
  kIgnoredDueToCooldownTime = 4,
  kMaxValue = kIgnoredDueToCooldownTime
};

// A callback function called when V8 reaches the heap limit.
using NearV8HeapLimitCallback = NearV8HeapLimitHandling (*)();

class CORE_EXPORT V8Initializer {
  STATIC_ONLY(V8Initializer);

 public:
  // This must be called before InitializeMainThread.
  static void SetNearV8HeapLimitOnMainThreadCallback(
      NearV8HeapLimitCallback callback);

  static void InitializeMainThread(const intptr_t* reference_table);
  static void InitializeWorker(v8::Isolate*);

  static void ReportRejectedPromisesOnMainThread();
  static void MessageHandlerInMainThread(v8::Local<v8::Message>,
                                         v8::Local<v8::Value>);
  static void MessageHandlerInWorker(v8::Local<v8::Message>,
                                     v8::Local<v8::Value>);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_INITIALIZER_H_
