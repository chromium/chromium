// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/thread/web_thread.h"

#import "build/blink_buildflags.h"
#include "ios/web/content/content_thread_impl.h"
#include "ios/web/web_thread_impl.h"

namespace web {

scoped_refptr<base::SingleThreadTaskRunner> GetUIThreadTaskRunner(
    const WebTaskTraits& traits) {
#if BUILDFLAG(USE_BLINK)
  return ContentThreadImpl::GetUIThreadTaskRunner(traits);
#else
  return WebThreadImpl::GetUIThreadTaskRunner(traits);
#endif
}

scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner(
    const WebTaskTraits& traits) {
#if BUILDFLAG(USE_BLINK)
  return ContentThreadImpl::GetIOThreadTaskRunner(traits);
#else
  return WebThreadImpl::GetIOThreadTaskRunner(traits);
#endif
}

bool WebThread::IsThreadInitialized(ID identifier) {
#if BUILDFLAG(USE_BLINK)
  return ContentThreadImpl::IsThreadInitialized(identifier);
#else
  return WebThreadImpl::IsThreadInitialized(identifier);
#endif
}

bool WebThread::CurrentlyOn(ID identifier) {
#if BUILDFLAG(USE_BLINK)
  return ContentThreadImpl::CurrentlyOn(identifier);
#else
  return WebThreadImpl::CurrentlyOn(identifier);
#endif
}

std::string WebThread::GetCurrentlyOnErrorMessage(ID expected) {
#if BUILDFLAG(USE_BLINK)
  return ContentThreadImpl::GetCurrentlyOnErrorMessage(expected);
#else
  return WebThreadImpl::GetCurrentlyOnErrorMessage(expected);
#endif
}

bool WebThread::GetCurrentThreadIdentifier(ID* identifier) {
#if BUILDFLAG(USE_BLINK)
  return ContentThreadImpl::GetCurrentThreadIdentifier(identifier);
#else
  return WebThreadImpl::GetCurrentThreadIdentifier(identifier);
#endif
}

}  // namespace web
