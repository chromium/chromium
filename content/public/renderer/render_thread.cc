// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_thread.h"

#include "base/no_destructor.h"
#include "base/threading/thread_checker_impl.h"
#include "base/threading/thread_local.h"

namespace content {

namespace {

// Keep the global RenderThread in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
base::ThreadLocalPointer<RenderThread>& GetRenderThreadLocalPointer() {
  static base::NoDestructor<base::ThreadLocalPointer<RenderThread>> tls;
  return *tls;
}

static const base::ThreadCheckerImpl& GetThreadChecker() {
  static base::NoDestructor<base::ThreadCheckerImpl> checker;
  return *checker;
}

}  // namespace

RenderThread* RenderThread::Get() {
  return GetRenderThreadLocalPointer().Get();
}

bool RenderThread::IsMainThread() {
  // TODO(avi): Eventually move to be based on WTF::IsMainThread().
  return GetThreadChecker().CalledOnValidThread();
}

RenderThread::RenderThread() {
  GetRenderThreadLocalPointer().Set(this);
}

RenderThread::~RenderThread() {
  GetRenderThreadLocalPointer().Set(nullptr);
}

}  // namespace content
