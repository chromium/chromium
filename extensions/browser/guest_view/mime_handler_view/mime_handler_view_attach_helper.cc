// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_attach_helper.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_embedder.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/skia/include/core/SkColor.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

using content::BrowserThread;
using content::RenderFrameHost;

namespace extensions {

namespace {

// TODO(ekaramad): Make this a proper resource (https://crbug.com/659750).
const char kFullPageMimeHandlerViewHTML[] =
    "<!doctype html><html><body style='height: 100%%; width: 100%%; overflow: "
    "hidden; margin:0px; background-color: rgb(%d, %d, %d);'><embed "
    "name='%s' "
    "style='position:absolute; left: 0; top: 0;'width='100%%' height='100%%'"
    " src='about:blank' type='%s' "
    "internalid='%s'></body></html>";
const uint32_t kFullPageMimeHandlerViewDataPipeSize = 512U;

SkColor GetBackgroundColorStringForMimeType(const GURL& url,
                                            const std::string& mime_type) {
#if BUILDFLAG(ENABLE_PLUGINS)
  std::vector<content::WebPluginInfo> web_plugin_info_array;
  std::vector<std::string> unused_actual_mime_types;
  content::PluginService::GetInstance()->GetPluginInfoArray(
      url, mime_type, true, &web_plugin_info_array, &unused_actual_mime_types);
  if (!web_plugin_info_array.empty())
    return web_plugin_info_array.front().background_color;
#endif
  return content::WebPluginInfo::kDefaultBackgroundColor;
}

using ProcessIdToHelperMap =
    base::flat_map<int32_t, std::unique_ptr<MimeHandlerViewAttachHelper>>;
ProcessIdToHelperMap* GetProcessIdToHelperMap() {
  static base::NoDestructor<ProcessIdToHelperMap> instance;
  return instance.get();
}

}  // namespace


// static
MimeHandlerViewAttachHelper* MimeHandlerViewAttachHelper::Get(
    int32_t render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& map = *GetProcessIdToHelperMap();
  if (!base::Contains(map, render_process_id)) {
    auto* process_host = content::RenderProcessHost::FromID(render_process_id);
    if (!process_host)
      return nullptr;
    map[render_process_id] = base::WrapUnique<MimeHandlerViewAttachHelper>(
        new MimeHandlerViewAttachHelper(process_host));
  }
  return map[render_process_id].get();
}

// static
bool MimeHandlerViewAttachHelper::OverrideBodyForInterceptedResponse(
    int32_t navigating_frame_tree_node_id,
    const GURL& resource_url,
    const std::string& mime_type,
    const std::string& stream_id,
    std::string* payload,
    uint32_t* data_pipe_size,
    base::OnceClosure resume_load) {
  auto color = GetBackgroundColorStringForMimeType(resource_url, mime_type);
  std::string token = base::UnguessableToken::Create().ToString();
  auto html_str = base::StringPrintf(
      kFullPageMimeHandlerViewHTML, SkColorGetR(color), SkColorGetG(color),
      SkColorGetB(color), token.c_str(), mime_type.c_str(), token.c_str());
  payload->assign(html_str);
  *data_pipe_size = kFullPageMimeHandlerViewDataPipeSize;
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(CreateFullPageMimeHandlerView,
                     navigating_frame_tree_node_id, resource_url, stream_id,
                     token),
      std::move(resume_load));
  return true;
}

void MimeHandlerViewAttachHelper::RenderProcessHostDestroyed(
    content::RenderProcessHost* render_process_host) {
  if (render_process_host != render_process_host_)
    return;
  render_process_host->RemoveObserver(this);
  GetProcessIdToHelperMap()->erase(render_process_host_->GetID());
}

void MimeHandlerViewAttachHelper::AttachToOuterWebContents(
    std::unique_ptr<MimeHandlerViewGuest> guest_view,
    int32_t embedder_render_process_id,
    content::RenderFrameHost* outer_contents_frame,
    int32_t element_instance_id,
    bool is_full_page_plugin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  outer_contents_frame->PrepareForInnerWebContentsAttach(
      base::BindOnce(&MimeHandlerViewAttachHelper::ResumeAttachOrDestroy,
                     weak_factory_.GetWeakPtr(), std::move(guest_view),
                     element_instance_id, is_full_page_plugin));
}

// static
void MimeHandlerViewAttachHelper::CreateFullPageMimeHandlerView(
    int32_t frame_tree_node_id,
    const GURL& resource_url,
    const std::string& stream_id,
    const std::string& token) {
  MimeHandlerViewEmbedder::Create(frame_tree_node_id, resource_url, stream_id,
                                  token);
}

MimeHandlerViewAttachHelper::MimeHandlerViewAttachHelper(
    content::RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host) {
  render_process_host->AddObserver(this);
}

MimeHandlerViewAttachHelper::~MimeHandlerViewAttachHelper() = default;

void MimeHandlerViewAttachHelper::ResumeAttachOrDestroy(
    std::unique_ptr<MimeHandlerViewGuest> guest_view,
    int32_t element_instance_id,
    bool is_full_page_plugin,
    content::RenderFrameHost* plugin_rfh) {
  if (resume_attach_callback_for_testing_) {
    std::move(resume_attach_callback_for_testing_)
        .Run(base::BindOnce(&MimeHandlerViewAttachHelper::ResumeAttachOrDestroy,
                            weak_factory_.GetWeakPtr(), std::move(guest_view),
                            element_instance_id, is_full_page_plugin,
                            plugin_rfh));
    return;
  }

  DCHECK(!plugin_rfh || (plugin_rfh->GetProcess() == render_process_host_));
  if (!guest_view)
    return;
  if (!plugin_rfh) {
    auto* embedder_frame = guest_view->GetEmbedderFrame();
    if (embedder_frame && embedder_frame->IsRenderFrameLive()) {
      mojo::AssociatedRemote<mojom::MimeHandlerViewContainerManager>
          container_manager;
      embedder_frame->GetRemoteAssociatedInterfaces()->GetInterface(
          &container_manager);
      container_manager->DestroyFrameContainer(element_instance_id);
    }
    guest_view.reset();
    return;
  }

  auto* raw_guest_view = guest_view.get();
  raw_guest_view->AttachToOuterWebContentsFrame(
      std::move(guest_view), plugin_rfh, element_instance_id,
      is_full_page_plugin, base::NullCallback());
}
}  // namespace extensions
