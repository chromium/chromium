// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_thread.h"

#include "base/no_destructor.h"
#include "base/threading/thread_checker_impl.h"

namespace content {

namespace {

// Keep the global RenderThread in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
constinit thread_local RenderThread* render_thread = nullptr;

static const base::ThreadCheckerImpl& GetThreadChecker() {
  static base::NoDestructor<base::ThreadCheckerImpl> checker;
  return *checker;
}

}  // namespace

RenderThread* RenderThread::Get() {
  return render_thread;
}

bool RenderThread::IsMainThread() {
  // TODO(avi): Eventually move to be based on WTF::IsMainThread().
  return GetThreadChecker().CalledOnValidThread();
}

RenderThread::RenderThread() : resetter_(&render_thread, this) {}

RenderThread::~RenderThread() = default;

}  // namespace content
