// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_attach_helper.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
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
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/skia/include/core/SkColor.h"

#if BUILDFLAG(ENABLE_PDF)
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/grit/components_resources.h"
#include "components/pdf/common/constants.h"
#include "pdf/pdf_features.h"
#include "ui/base/resource/resource_bundle.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

using content::BrowserThread;
using content::RenderFrameHost;

namespace extensions {

namespace {

// TODO(crbug.com/40490789): Make this a proper resource.
constexpr char kFullPageMimeHandlerViewHTML[] =
    "<!doctype html><html><body style='height: 100%%; width: 100%%; overflow: "
    "hidden; margin:0px; background-color: rgb(%d, %d, %d);'><embed "
    "name='%s' "
    "style='position:absolute; left: 0; top: 0;'width='100%%' height='100%%'"
    " src='about:blank' type='%s' "
    "internalid='%s'></body></html>";

SkColor GetBackgroundColorStringForMimeType(const GURL& url,
                                            const std::string& mime_type) {
#if BUILDFLAG(ENABLE_PLUGINS)
  std::vector<content::WebPluginInfo> web_plugin_info_array;
  std::vector<std::string> unused_actual_mime_types;
  content::PluginService::GetInstance()->GetPluginInfoArray(
      url, mime_type, true, &web_plugin_info_array, &unused_actual_mime_types);
  if (!web_plugin_info_array.empty()) {
    return web_plugin_info_array.front().background_color;
  }
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
    if (!process_host) {
      return nullptr;
    }
    map[render_process_id] = base::WrapUnique<MimeHandlerViewAttachHelper>(
        new MimeHandlerViewAttachHelper(process_host));
  }
  return map[render_process_id].get();
}

// static
std::string MimeHandlerViewAttachHelper::CreateTemplateMimeHandlerPage(
    const GURL& resource_url,
    const std::string& mime_type,
    const std::string& internal_id) {
  auto color = GetBackgroundColorStringForMimeType(resource_url, mime_type);
#if BUILDFLAG(ENABLE_PDF)
  if (chrome_pdf::features::IsOopifPdfEnabled() &&
      mime_type == pdf::kPDFMimeType) {
    std::string pdf_embedder_html =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_PDF_EMBEDDER_HTML);
    return base::ReplaceStringPlaceholders(
        pdf_embedder_html,
        {base::NumberToString(SkColorGetR(color)),
         base::NumberToString(SkColorGetG(color)),
         base::NumberToString(SkColorGetB(color)), internal_id, mime_type,
         internal_id},
        /*offsets=*/nullptr);
  }
#endif
  return base::StringPrintf(kFullPageMimeHandlerViewHTML, SkColorGetR(color),
                            SkColorGetG(color), SkColorGetB(color),
                            internal_id.c_str(), mime_type.c_str(),
                            internal_id.c_str());
}

// static
std::string MimeHandlerViewAttachHelper::OverrideBodyForInterceptedResponse(
    content::FrameTreeNodeId navigating_frame_tree_node_id,
    const GURL& resource_url,
    const std::string& mime_type,
    const std::string& stream_id,
    const std::string& internal_id,
    base::OnceClosure resume_load) {
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(CreateFullPageMimeHandlerView,
                     navigating_frame_tree_node_id, resource_url, stream_id,
                     internal_id),
      std::move(resume_load));
  return CreateTemplateMimeHandlerPage(resource_url, mime_type, internal_id);
}

void MimeHandlerViewAttachHelper::RenderProcessHostDestroyed(
    content::RenderProcessHost* render_process_host) {
  if (render_process_host != render_process_host_) {
    return;
  }
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
    content::FrameTreeNodeId frame_tree_node_id,
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
    content::RenderFrameHost* plugin_render_frame_host) {
  if (resume_attach_callback_for_testing_) {
    std::move(resume_attach_callback_for_testing_)
        .Run(base::BindOnce(&MimeHandlerViewAttachHelper::ResumeAttachOrDestroy,
                            weak_factory_.GetWeakPtr(), std::move(guest_view),
                            element_instance_id, is_full_page_plugin,
                            plugin_render_frame_host));
    return;
  }

  DCHECK(!plugin_render_frame_host ||
         (plugin_render_frame_host->GetProcess() == render_process_host_));
  if (!guest_view) {
    return;
  }
  if (!plugin_render_frame_host) {
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
      std::move(guest_view), plugin_render_frame_host, element_instance_id,
      is_full_page_plugin, base::NullCallback());
}
}  // namespace extensions
