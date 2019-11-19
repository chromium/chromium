// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_H_

#include "mojo/public/cpp/system/message_pipe.h"

namespace blink {

// PlzDedicatedWorker:
// WebDedicatedWorker is the interface to access blink::DedicatedWorker from
// content::DedicatedWorkerHostFactoryClient.
class WebDedicatedWorker {
 public:
  virtual ~WebDedicatedWorker() = default;

  // Called when content::DedicatedWorkerHost is created in the browser process.
  virtual void OnWorkerHostCreated(
      mojo::ScopedMessagePipeHandle interface_provider,
      mojo::ScopedMessagePipeHandle browser_interface_broker) = 0;

  // Called when content::DedicatedWorkerHost started loading the main worker
  // script in the browser process, and the script information is sent back to
  // the content::DedicatedWorkerHostFactoryClient.
  virtual void OnScriptLoadStarted() = 0;

  // Called when content::DedicatedWorkerHost failed to start loading the main
  // worker script in the browser process.
  virtual void OnScriptLoadStartFailed() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_H_
