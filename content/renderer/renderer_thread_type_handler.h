// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_THREAD_TYPE_HANDLER_H_
#define CONTENT_RENDERER_RENDERER_THREAD_TYPE_HANDLER_H_

#include <map>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_type_delegate.h"

namespace content {

// This class handles thread type changes for sandboxed renderer process, which
// supports proxying the thread type changes to browser process.
class RendererThreadTypeHandler : public base::ThreadTypeDelegate {
 public:
  RendererThreadTypeHandler(const RendererThreadTypeHandler&) = delete;
  RendererThreadTypeHandler& operator=(const RendererThreadTypeHandler&) =
      delete;

  ~RendererThreadTypeHandler() override;

  // Creates a RendererThreadTypeHandler instance and stores it to g_instance.
  // Make sure the g_instance doesn't exist before creation.
  static void Create();

  // Invoked when renderer message filter is ready. If g_instance exists, it
  // asks the g_instance to process pending thread type change requests.
  static void NotifyRenderThreadCreated();

  // Overridden from base::ThreadTypeDelegate.
  bool HandleThreadTypeChange(base::PlatformThreadId thread_id,
                              base::ThreadType thread_type) override;

 private:
  RendererThreadTypeHandler();

  // Sends the current thread type change request to browser process if renderer
  // message filter is available. If message filter not available, it
  // accumulates pending change requests into a map.
  void ProcessCurrentChangeRequest(base::PlatformThreadId tid,
                                   base::ThreadType type);

  // Invoked when renderer message filter is ready. Sends an IPC to the browser
  // process for every pending thread type change request.
  void ProcessPendingChangeRequests();

  typedef std::map<base::PlatformThreadId, base::ThreadType> ThreadIdToTypeMap;

  // Records thread type change request into the map when renderer message
  // filter is not available yet. These recorded thread type change requests
  // would be sent to the browser process after message filter is ready.
  ThreadIdToTypeMap thread_id_to_type_;

  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_THREAD_TYPE_HANDLER_H_
