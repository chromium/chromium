/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_BLINK_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_BLINK_H_

#include "third_party/blink/public/platform/platform.h"
#include "v8/include/v8.h"

namespace mojo {
class BinderMap;
}

namespace blink {

// Initialize the entire Blink (wtf, platform, core, modules and web).
// If you just need wtf and platform, use Platform::Initialize instead.
//
// Must be called on the thread that will be the main thread before
// using any other public APIs. The provided Platform must be non-null and
// must remain valid until the current thread calls shutdown.
BLINK_EXPORT void Initialize(
    Platform*,
    mojo::BinderMap*,
    scheduler::WebThreadScheduler* main_thread_scheduler);

// The same as above, but this only supports simple single-threaded execution
// environment. The main thread WebThread object is owned by Platform when this
// version is used. This version is mainly for tests and other components
// requiring only the simple environment.
//
// When this version is used, your Platform implementation needs to follow
// a certain convention on CurrentThread(); see the comments at
// Platform::CreateMainThreadAndInitialize().
BLINK_EXPORT void CreateMainThreadAndInitialize(Platform*, mojo::BinderMap*);

// Get the V8 Isolate for the main thread.
// initialize must have been called first.
BLINK_EXPORT v8::Isolate* MainThreadIsolate();

// Alters the rendering of content to conform to a fixed set of rules.
BLINK_EXPORT void SetWebTestMode(bool);
BLINK_EXPORT bool WebTestMode();

// Alters the rendering of fonts for web tests.
BLINK_EXPORT void SetFontAntialiasingEnabledForTest(bool);
BLINK_EXPORT bool FontAntialiasingEnabledForTest();

// Purge the plugin list cache. This can cause a web-visible and out-of-spec
// change to |navigator.plugins| if the plugin list has changed (see
// https://crbug.com/735854). |reloadPages| is unsupported and must be false.
BLINK_EXPORT void ResetPluginCache(bool reload_pages = false);

// The embedder should call this periodically in an attempt to balance overall
// performance and memory usage.
BLINK_EXPORT void DecommitFreeableMemory();

// Send memory pressure notification to worker thread isolate.
BLINK_EXPORT void MemoryPressureNotificationToWorkerThreadIsolates(
    v8::MemoryPressureLevel);

// Set the RAIL performance mode on all worker thread isolates.
BLINK_EXPORT void SetRAILModeOnWorkerThreadIsolates(v8::RAILMode);

// Logs Runtime Call Stats table for Blink.
BLINK_EXPORT void LogRuntimeCallStats();

}  // namespace blink

#endif
