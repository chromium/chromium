// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include <errno.h>
#include <string.h>

#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_math.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/download/public/common/download_stats.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/pepper/pepper_security_helper.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/content_constants_internal.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/media_log_record.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/request_priority.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "content/public/common/font_cache_dispatcher_win.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/linux_util.h"
#include "base/threading/platform_thread.h"
#include "content/public/browser/child_process_launcher_utils.h"
#endif

namespace content {
namespace {

void GotHasGpuProcess(RenderMessageFilter::HasGpuProcessCallback callback,
                      bool has_gpu) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), has_gpu));
}

void GetHasGpuProcess(RenderMessageFilter::HasGpuProcessCallback callback) {
  GpuProcessHost::GetHasGpuProcess(
      base::BindOnce(GotHasGpuProcess, std::move(callback)));
}

}  // namespace

RenderMessageFilter::RenderMessageFilter(
    int render_process_id,
    BrowserContext* browser_context,
    RenderWidgetHelper* render_widget_helper,
    MediaInternals* media_internals)
    : BrowserAssociatedInterface<mojom::RenderMessageFilter>(this),
      render_widget_helper_(render_widget_helper),
      render_process_id_(render_process_id),
      media_internals_(media_internals) {
  if (render_widget_helper)
    render_widget_helper_->Init(render_process_id_);
}

RenderMessageFilter::~RenderMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

bool RenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void RenderMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void RenderMessageFilter::GenerateRoutingID(
    GenerateRoutingIDCallback callback) {
  std::move(callback).Run(render_widget_helper_->GetNextRoutingID());
}

void RenderMessageFilter::GenerateFrameRoutingID(
    GenerateFrameRoutingIDCallback callback) {
  int32_t routing_id = render_widget_helper_->GetNextRoutingID();
  auto frame_token = blink::LocalFrameToken();
  auto devtools_frame_token = base::UnguessableToken::Create();
  auto document_token = blink::DocumentToken();
  render_widget_helper_->StoreNextFrameRoutingID(
      routing_id, frame_token, devtools_frame_token, document_token);
  std::move(callback).Run(routing_id, frame_token, devtools_frame_token,
                          document_token);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void RenderMessageFilter::SetThreadTypeOnLauncherThread(
    base::PlatformThreadId ns_tid,
    base::ThreadType thread_type) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  bool ns_pid_supported = false;
  pid_t peer_tid = base::FindThreadID(peer_pid(), ns_tid, &ns_pid_supported);
  if (peer_tid == -1) {
    if (ns_pid_supported)
      DLOG(WARNING) << "Could not find tid";
    return;
  }

  if (peer_tid == peer_pid() && thread_type != base::ThreadType::kCompositing) {
    DLOG(WARNING) << "Changing main thread type to another value than "
                  << "kCompositing isn't allowed";
    return;
  }

  base::PlatformThread::SetThreadType(peer_pid(), peer_tid, thread_type);
}
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void RenderMessageFilter::SetThreadType(int32_t ns_tid,
                                        base::ThreadType thread_type) {
  // Post this task to process launcher task runner. All thread type changes
  // (nice value, c-group setting) of renderer process would be performed on the
  // same sequence as renderer process priority changes, to guarantee that
  // there's no race of c-group manipulations.
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderMessageFilter::SetThreadTypeOnLauncherThread, this,
                     static_cast<base::PlatformThreadId>(ns_tid), thread_type));
}
#endif

void RenderMessageFilter::OnMediaLogRecords(
    const std::vector<media::MediaLogRecord>& events) {
  // OnMediaLogRecords() is always dispatched to the UI thread for handling.
  // See OverrideThreadForMessage().
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (media_internals_)
    media_internals_->OnMediaEvents(render_process_id_, events);
}

void RenderMessageFilter::HasGpuProcess(HasGpuProcessCallback callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(GetHasGpuProcess, std::move(callback)));
}

}  // namespace content
