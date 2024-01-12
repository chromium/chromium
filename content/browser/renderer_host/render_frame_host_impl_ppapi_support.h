// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_PPAPI_SUPPORT_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_PPAPI_SUPPORT_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "content/common/pepper_plugin.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

class PepperPluginInstanceHost;
class RenderFrameHostImpl;
class RenderProcessHostImpl;

class RenderFrameHostImplPpapiSupport : public mojom::PepperHost,
                                        public mojom::PepperHungDetectorHost {
 public:
  explicit RenderFrameHostImplPpapiSupport(RenderFrameHostImpl& frame_host);
  ~RenderFrameHostImplPpapiSupport() override;

  void Bind(mojo::PendingAssociatedReceiver<mojom::PepperHost> receiver);
  void SetVolume(int32_t instance_id, double volume);

  // mojom::PepperHost overrides:
  void InstanceCreated(
      int32_t instance_id,
      mojo::PendingAssociatedRemote<mojom::PepperPluginInstance> instance,
      mojo::PendingAssociatedReceiver<mojom::PepperPluginInstanceHost> host)
      override;
  void BindHungDetectorHost(
      mojo::PendingReceiver<mojom::PepperHungDetectorHost> hung_host,
      int32_t plugin_child_id,
      const base::FilePath& path) override;
  void GetPluginInfo(const GURL& url,
                     const std::string& mime_type,
                     GetPluginInfoCallback callback) override;
  void DidCreateInProcessInstance(int32_t instance,
                                  int32_t render_frame_id,
                                  const GURL& document_url,
                                  const GURL& plugin_url) override;
  void DidDeleteInProcessInstance(int32_t instance) override;
  void DidCreateOutOfProcessPepperInstance(
      int32_t plugin_child_id,
      int32_t pp_instance,
      bool is_external,
      int32_t render_frame_id,
      const GURL& document_url,
      const GURL& plugin_url,
      bool is_priviledged_context,
      DidCreateOutOfProcessPepperInstanceCallback callback) override;
  void DidDeleteOutOfProcessPepperInstance(int32_t plugin_child_id,
                                           int32_t pp_instance,
                                           bool is_external) override;
  void OpenChannelToPepperPlugin(
      const url::Origin& embedder_origin,
      const base::FilePath& path,
      const std::optional<url::Origin>& origin_lock,
      OpenChannelToPepperPluginCallback callback) override;

  // mojom::PepperHungDetectorHost overrides:
  void PluginHung(bool is_hung) override;

 private:
  void InstanceClosed(int32_t instance_id);

  RenderFrameHostImpl& render_frame_host() { return *render_frame_host_; }
  RenderProcessHostImpl* GetProcess();

  const raw_ref<RenderFrameHostImpl> render_frame_host_;

  mojo::AssociatedReceiver<mojom::PepperHost> receiver_{this};

  std::map<int32_t, std::unique_ptr<PepperPluginInstanceHost>>
      pepper_plugin_instances_;

  struct HungDetectorContext {
    int32_t plugin_child_id;
    const base::FilePath plugin_path;
  };
  mojo::ReceiverSet<mojom::PepperHungDetectorHost, HungDetectorContext>
      pepper_hung_detectors_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_PPAPI_SUPPORT_H_
