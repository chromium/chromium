// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_impl_ppapi_support.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/not_fatal_until.h"
#include "base/process/process_handle.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/pepper/pepper_renderer_connection.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/pepper_plugin.mojom.h"
#include "content/public/common/webplugininfo.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace content {

class PepperPluginInstanceHost : public mojom::PepperPluginInstanceHost {
 public:
  PepperPluginInstanceHost(
      int32_t instance_id,
      RenderFrameHostImpl& frame_host,
      mojo::PendingAssociatedReceiver<mojom::PepperPluginInstanceHost> host,
      mojo::PendingAssociatedRemote<mojom::PepperPluginInstance> instance,
      base::OnceClosure on_instance_disconnect)
      : instance_id_(instance_id),
        frame_host_(frame_host),
        receiver_(this, std::move(host)),
        remote_(std::move(instance)) {
    frame_host_->delegate()->OnPepperInstanceCreated(&*frame_host_,
                                                     instance_id);
    remote_.set_disconnect_handler(std::move(on_instance_disconnect));
  }
  ~PepperPluginInstanceHost() override = default;

  // mojom::PepperPluginInstanceHost overrides.
  void StartsPlayback() override {
    frame_host_->delegate()->OnPepperStartsPlayback(&*frame_host_,
                                                    instance_id_);
  }

  void StopsPlayback() override {
    frame_host_->delegate()->OnPepperStopsPlayback(&*frame_host_, instance_id_);
  }

  void InstanceCrashed(const base::FilePath& plugin_path,
                       base::ProcessId plugin_pid) override {
    frame_host_->delegate()->OnPepperPluginCrashed(&*frame_host_, plugin_path,
                                                   plugin_pid);
  }

  void SetVolume(double volume) { remote_->SetVolume(volume); }

 private:
  int32_t const instance_id_;
  const raw_ref<RenderFrameHostImpl> frame_host_;
  mojo::AssociatedReceiver<mojom::PepperPluginInstanceHost> receiver_;
  mojo::AssociatedRemote<mojom::PepperPluginInstance> remote_;
};

RenderFrameHostImplPpapiSupport::RenderFrameHostImplPpapiSupport(
    RenderFrameHostImpl& render_frame_host)
    : render_frame_host_(render_frame_host) {}

RenderFrameHostImplPpapiSupport::~RenderFrameHostImplPpapiSupport() = default;

void RenderFrameHostImplPpapiSupport::Bind(
    mojo::PendingAssociatedReceiver<mojom::PepperHost> receiver) {
  receiver_.Bind(std::move(receiver));
  receiver_.SetFilter(
      render_frame_host().CreateMessageFilterForAssociatedReceiver(
          mojom::PepperHost::Name_));
}

void RenderFrameHostImplPpapiSupport::SetVolume(int32_t instance_id,
                                                double volume) {
  auto it = pepper_plugin_instances_.find(instance_id);
  CHECK(it != pepper_plugin_instances_.end(), base::NotFatalUntil::M130);
  it->second->SetVolume(volume);
}

void RenderFrameHostImplPpapiSupport::InstanceCreated(
    int32_t instance_id,
    mojo::PendingAssociatedRemote<mojom::PepperPluginInstance> instance,
    mojo::PendingAssociatedReceiver<mojom::PepperPluginInstanceHost> host) {
  pepper_plugin_instances_.insert(
      {instance_id,
       std::make_unique<PepperPluginInstanceHost>(
           instance_id, render_frame_host(), std::move(host),
           std::move(instance),
           // base::Unretained() is safe here because this is used as the
           // disconnect handler for a remote which is indirectly owned by
           // `this`.
           base::BindOnce(&RenderFrameHostImplPpapiSupport::InstanceClosed,
                          base::Unretained(this), instance_id))});
}

void RenderFrameHostImplPpapiSupport::BindHungDetectorHost(
    mojo::PendingReceiver<mojom::PepperHungDetectorHost> hung_host,
    int32_t plugin_child_id,
    const base::FilePath& path) {
  pepper_hung_detectors_.Add(this, std::move(hung_host),
                             {plugin_child_id, path});
}

void RenderFrameHostImplPpapiSupport::GetPluginInfo(
    const GURL& url,
    const std::string& mime_type,
    GetPluginInfoCallback callback) {
  bool allow_wildcard = true;
  WebPluginInfo info;
  std::string actual_mime_type;
  bool found = PluginServiceImpl::GetInstance()->GetPluginInfo(
      render_frame_host().GetBrowserContext(), url, mime_type, allow_wildcard,
      nullptr, &info, &actual_mime_type);
  std::move(callback).Run(found, info, actual_mime_type);
}

void RenderFrameHostImplPpapiSupport::DidCreateInProcessInstance(
    int32_t instance,
    int32_t render_frame_id,
    const GURL& document_url,
    const GURL& plugin_url) {
  GetProcess()->pepper_renderer_connection()->DidCreateInProcessInstance(
      instance, render_frame_id, document_url, plugin_url);
}

void RenderFrameHostImplPpapiSupport::DidDeleteInProcessInstance(
    int32_t instance) {
  GetProcess()->pepper_renderer_connection()->DidDeleteInProcessInstance(
      instance);
}

void RenderFrameHostImplPpapiSupport::DidCreateOutOfProcessPepperInstance(
    int32_t plugin_child_id,
    int32_t pp_instance,
    bool is_external,
    int32_t render_frame_id,
    const GURL& document_url,
    const GURL& plugin_url,
    bool is_privileged_context,
    DidCreateOutOfProcessPepperInstanceCallback callback) {
  GetProcess()
      ->pepper_renderer_connection()
      ->DidCreateOutOfProcessPepperInstance(
          plugin_child_id, pp_instance, is_external, render_frame_id,
          document_url, plugin_url, is_privileged_context, std::move(callback));
}

void RenderFrameHostImplPpapiSupport::DidDeleteOutOfProcessPepperInstance(
    int32_t plugin_child_id,
    int32_t pp_instance,
    bool is_external) {
  GetProcess()
      ->pepper_renderer_connection()
      ->DidDeleteOutOfProcessPepperInstance(plugin_child_id, pp_instance,
                                            is_external);
}

void RenderFrameHostImplPpapiSupport::OpenChannelToPepperPlugin(
    const url::Origin& embedder_origin,
    const base::FilePath& path,
    const std::optional<url::Origin>& origin_lock,
    OpenChannelToPepperPluginCallback callback) {
  GetProcess()->pepper_renderer_connection()->OpenChannelToPepperPlugin(
      embedder_origin, path, origin_lock, std::move(callback));
}

void RenderFrameHostImplPpapiSupport::PluginHung(bool is_hung) {
  const HungDetectorContext& context = pepper_hung_detectors_.current_context();
  render_frame_host().delegate()->OnPepperPluginHung(
      &render_frame_host(), context.plugin_child_id, context.plugin_path,
      is_hung);
}

void RenderFrameHostImplPpapiSupport::InstanceClosed(int32_t instance_id) {
  render_frame_host().delegate()->OnPepperInstanceDeleted(&render_frame_host(),
                                                          instance_id);
  pepper_plugin_instances_.erase(instance_id);
}

RenderProcessHostImpl* RenderFrameHostImplPpapiSupport::GetProcess() {
  return static_cast<RenderProcessHostImpl*>(render_frame_host().GetProcess());
}

}  // namespace content
