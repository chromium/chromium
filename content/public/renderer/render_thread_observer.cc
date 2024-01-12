// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_thread_observer.h"

namespace content {

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
bool RenderThreadObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  return false;
}
#endif

}  // namespace content
