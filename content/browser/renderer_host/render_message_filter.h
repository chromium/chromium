// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/widget_type.h"
#include "gpu/config/gpu_info.h"
#include "ipc/message_filter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

class GURL;

namespace media {
struct MediaLogEvent;
}

namespace content {
class BrowserContext;
class MediaInternals;
class RenderWidgetHelper;
class ResourceContext;

// This class filters out incoming IPC messages for the renderer process on the
// IPC thread.
class CONTENT_EXPORT RenderMessageFilter
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<mojom::RenderMessageFilter>,
      public mojom::RenderMessageFilter {
 public:
  // Create the filter.
  RenderMessageFilter(int render_process_id,
                      BrowserContext* browser_context,
                      RenderWidgetHelper* render_widget_helper,
                      MediaInternals* media_internals);

  // BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;

  int render_process_id() const { return render_process_id_; }

 protected:
  ~RenderMessageFilter() override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<RenderMessageFilter>;

  void OnGetProcessMemorySizes(size_t* private_bytes, size_t* shared_bytes);

  // mojom::RenderMessageFilter:
  void GenerateRoutingID(GenerateRoutingIDCallback routing_id) override;
  void CreateNewWidget(int32_t opener_id,
                       mojo::PendingRemote<mojom::Widget> widget,
                       CreateNewWidgetCallback callback) override;
  void CreateFullscreenWidget(int opener_id,
                              mojo::PendingRemote<mojom::Widget> widget,
                              CreateFullscreenWidgetCallback callback) override;
  void HasGpuProcess(HasGpuProcessCallback callback) override;
#if defined(OS_LINUX)
  void SetThreadPriority(int32_t ns_tid,
                         base::ThreadPriority priority) override;
#endif

  void OnResolveProxy(const GURL& url, IPC::Message* reply_msg);

#if defined(OS_LINUX)
  void SetThreadPriorityOnFileThread(base::PlatformThreadId ns_tid,
                                     base::ThreadPriority priority);
#endif

  void OnMediaLogEvents(const std::vector<media::MediaLogEvent>&);

  bool CheckBenchmarkingEnabled() const;
  bool CheckPreparsedJsCachingEnabled() const;

  // The ResourceContext which is to be used on the IO thread.
  ResourceContext* resource_context_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  int render_process_id_;

  MediaInternals* media_internals_;

  base::WeakPtrFactory<RenderMessageFilter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
