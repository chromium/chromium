// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_impl.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/accessibility/render_accessibility_host.h"
#include "content/browser/attribution_reporting/attribution_host.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_system/file_system_manager_impl.h"
#include "content/browser/geolocation/geolocation_service_impl.h"
#include "content/browser/manifest/manifest_manager_host.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/page_lifecycle_state_manager.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/common/dom_automation_controller.mojom.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/active_url_message_filter.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "media/mojo/mojom/media_player.mojom.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "net/base/features.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/mojom/blob/file_backed_blob_factory.mojom.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_observer.mojom.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/browser/renderer_host/render_frame_host_impl_ppapi_support.h"
#include "content/common/pepper_plugin.mojom.h"
#endif

namespace content {

namespace {

// This class is used to chain multiple mojo::MessageFilter. Messages will be
// processed by the filters in the same order as the filters are added with the
// Add() method. WillDispatch() might not be called for all filters or might see
// a modified message if a filter earlier in the chain discards or modifies it.
// Similarly a given filter instance might not receive a DidDispatchOrReject()
// call even if WillDispatch() was called if a filter further down the chain
// discarded it. Long story short, the order in which filters are added is
// important!
class MessageFilterChain final : public mojo::MessageFilter {
 public:
  MessageFilterChain() = default;
  ~MessageFilterChain() final = default;

  bool WillDispatch(mojo::Message* message) override {
    for (auto& filter : filters_) {
      if (!filter->WillDispatch(message))
        return false;
    }
    return true;
  }
  void DidDispatchOrReject(mojo::Message* message, bool accepted) override {
    for (auto& filter : filters_) {
      filter->DidDispatchOrReject(message, accepted);
    }
  }

  // Adds a filter to the end of the chain. See class description for ordering
  // implications.
  void Add(std::unique_ptr<mojo::MessageFilter> filter) {
    filters_.push_back(std::move(filter));
  }

 private:
  std::vector<std::unique_ptr<mojo::MessageFilter>> filters_;
};

// This class can be added as a MessageFilter to a mojo receiver to detect
// messages received while the the associated frame is in the Back-Forward
// Cache. Documents that are in the bfcache should not be sending mojo messages
// back to the browser.
class BackForwardCacheMessageFilter : public mojo::MessageFilter {
 public:
  explicit BackForwardCacheMessageFilter(
      RenderFrameHostImpl* render_frame_host,
      const char* interface_name,
      BackForwardCacheImpl::MessageHandlingPolicyWhenCached policy)
      : render_frame_host_(render_frame_host),
        interface_name_(interface_name),
        policy_(policy) {}

  ~BackForwardCacheMessageFilter() override = default;

 private:
  // mojo::MessageFilter overrides.
  bool WillDispatch(mojo::Message* message) override {
    if (!render_frame_host_->render_view_host())
      return false;
    if (render_frame_host_->render_view_host()
            ->GetPageLifecycleStateManager()
            ->RendererExpectedToSendChannelAssociatedIpcs() ||
        ProcessHoldsNonCachedPages() ||
        policy_ == BackForwardCacheImpl::kMessagePolicyNone) {
      return true;
    }

    BackForwardCacheImpl::VlogUnexpectedRendererToBrowserMessage(
        interface_name_, message->name(), render_frame_host_);

    TRACE_EVENT2(
        "content", "BackForwardCacheMessageFilter::WillDispatch bad_message",
        "interface_name", interface_name_, "message_name", message->name());

    base::UmaHistogramSparse(
        "BackForwardCache.UnexpectedRendererToBrowserMessage.InterfaceName",
        static_cast<int32_t>(base::HashMetricName(interface_name_)));

    switch (policy_) {
      case BackForwardCacheImpl::kMessagePolicyNone:
      case BackForwardCacheImpl::kMessagePolicyLog:
        return true;
      case BackForwardCacheImpl::kMessagePolicyDump:
        base::debug::DumpWithoutCrashing();
        return true;
    }
  }

  void DidDispatchOrReject(mojo::Message* message, bool accepted) override {}

  // TODO(crbug.com/40147948): Remove once a well-behaved frozen
  // RenderFrame never send IPCs messages, even if there are active pages in the
  // process.
  bool ProcessHoldsNonCachedPages() {
    return RenderViewHostImpl::HasNonBackForwardCachedInstancesForProcess(
        render_frame_host_->GetProcess());
  }

  const raw_ptr<RenderFrameHostImpl> render_frame_host_;
  const char* const interface_name_;
  const BackForwardCacheImpl::MessageHandlingPolicyWhenCached policy_;
};

}  // namespace

std::unique_ptr<mojo::MessageFilter>
RenderFrameHostImpl::CreateMessageFilterForAssociatedReceiverInternal(
    const char* interface_name,
    BackForwardCacheImpl::MessageHandlingPolicyWhenCached policy) {
  auto filter_chain = std::make_unique<MessageFilterChain>();
  filter_chain->Add(std::make_unique<BackForwardCacheMessageFilter>(
      this, interface_name, policy));
  // BackForwardCacheMessageFilter might drop messages so add
  // ActiveUrlMessageFilter at the end of the chain as we need to make sure that
  // the debug url is reset, that is, DidDispatchOrReject() is called if
  // WillDispatch().
  filter_chain->Add(std::make_unique<internal::ActiveUrlMessageFilter>(this));
  return filter_chain;
}

void RenderFrameHostImpl::SetUpMojoConnection() {
  CHECK(!associated_registry_);

  associated_registry_ = std::make_unique<blink::AssociatedInterfaceRegistry>();

  auto bind_frame_host_receiver =
      [](RenderFrameHostImpl* impl,
         mojo::PendingAssociatedReceiver<mojom::FrameHost> receiver) {
        impl->frame_host_associated_receiver_.Bind(std::move(receiver));
        impl->frame_host_associated_receiver_.SetFilter(
            impl->CreateMessageFilterForAssociatedReceiver(
                mojom::FrameHost::Name_));
      };
  associated_registry_->AddInterface<mojom::FrameHost>(
      base::BindRepeating(bind_frame_host_receiver, base::Unretained(this)));

  associated_registry_->AddInterface<
      blink::mojom::BackForwardCacheControllerHost>(base::BindRepeating(
      [](RenderFrameHostImpl* impl,
         mojo::PendingAssociatedReceiver<
             blink::mojom::BackForwardCacheControllerHost> receiver) {
        impl->back_forward_cache_controller_host_associated_receiver_.Bind(
            std::move(receiver));
        impl->back_forward_cache_controller_host_associated_receiver_.SetFilter(
            impl->CreateMessageFilterForAssociatedReceiverInternal(
                blink::mojom::BackForwardCacheControllerHost::Name_,
                BackForwardCacheImpl::kMessagePolicyNone));
      },
      base::Unretained(this)));

  associated_registry_->AddInterface<blink::mojom::LocalFrameHost>(
      base::BindRepeating(
          [](RenderFrameHostImpl* impl,
             mojo::PendingAssociatedReceiver<blink::mojom::LocalFrameHost>
                 receiver) {
            impl->local_frame_host_receiver_.Bind(std::move(receiver));
            impl->local_frame_host_receiver_.SetFilter(
                impl->CreateMessageFilterForAssociatedReceiver(
                    blink::mojom::LocalFrameHost::Name_));
          },
          base::Unretained(this)));

  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    associated_registry_->AddInterface<
        blink::mojom::SharedStorageDocumentService>(base::BindRepeating(
        [](RenderFrameHostImpl* impl,
           mojo::PendingAssociatedReceiver<
               blink::mojom::SharedStorageDocumentService> receiver) {
          if (SharedStorageDocumentServiceImpl::GetForCurrentDocument(impl)) {
            // The renderer somehow requested two shared storage worklets
            // associated with the same document. This could indicate a
            // compromised renderer, so let's terminate it.
            mojo::ReportBadMessage(
                "Attempted to request two shared storage worklets associated "
                "with the same document.");
            return;
          }

          SharedStorageDocumentServiceImpl::GetOrCreateForCurrentDocument(impl)
              ->Bind(std::move(receiver));
        },
        base::Unretained(this)));
  }

  if (is_main_frame()) {
    associated_registry_->AddInterface<blink::mojom::LocalMainFrameHost>(
        base::BindRepeating(
            [](RenderFrameHostImpl* impl,
               mojo::PendingAssociatedReceiver<blink::mojom::LocalMainFrameHost>
                   receiver) {
              impl->local_main_frame_host_receiver_.Bind(std::move(receiver));
              impl->local_main_frame_host_receiver_.SetFilter(
                  impl->CreateMessageFilterForAssociatedReceiver(
                      blink::mojom::LocalMainFrameHost::Name_));
            },
            base::Unretained(this)));

    associated_registry_->AddInterface<blink::mojom::ManifestUrlChangeObserver>(
        base::BindRepeating(
            [](RenderFrameHostImpl* impl,
               mojo::PendingAssociatedReceiver<
                   blink::mojom::ManifestUrlChangeObserver> receiver) {
              ManifestManagerHost::GetOrCreateForPage(impl->GetPage())
                  ->BindObserver(std::move(receiver));
            },
            base::Unretained(this)));
  }

  // TODO(crbug.com/40249262): Avoid binding the DomAutomationControllerHost
  // interface outside of tests.
  associated_registry_->AddInterface<mojom::DomAutomationControllerHost>(
      base::BindRepeating(
          [](RenderFrameHostImpl* impl,
             mojo::PendingAssociatedReceiver<mojom::DomAutomationControllerHost>
                 receiver) {
            impl->BindDomOperationControllerHostReceiver(std::move(receiver));
          },
          base::Unretained(this)));

  file_system_manager_.reset(new FileSystemManagerImpl(
      GetProcess()->GetID(),
      GetProcess()->GetStoragePartition()->GetFileSystemContext(),
      ChromeBlobStorageContext::GetFor(GetProcess()->GetBrowserContext())));

#if BUILDFLAG(ENABLE_PPAPI)
  associated_registry_->AddInterface<mojom::PepperHost>(base::BindRepeating(
      [](RenderFrameHostImpl* impl,
         mojo::PendingAssociatedReceiver<mojom::PepperHost> receiver) {
        impl->GetPpapiSupport().Bind(std::move(receiver));
      },
      base::Unretained(this)));
#endif

  associated_registry_->AddInterface<media::mojom::MediaPlayerHost>(
      base::BindRepeating(
          [](RenderFrameHostImpl* impl,
             mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerHost>
                 receiver) {
            impl->delegate()->CreateMediaPlayerHostForRenderFrameHost(
                impl, std::move(receiver));
          },
          base::Unretained(this)));

  associated_registry_->AddInterface<blink::mojom::DisplayCutoutHost>(
      base::BindRepeating(
          [](RenderFrameHostImpl* impl,
             mojo::PendingAssociatedReceiver<blink::mojom::DisplayCutoutHost>
                 receiver) {
            impl->delegate()->BindDisplayCutoutHost(impl, std::move(receiver));
          },
          base::Unretained(this)));

  associated_registry_->AddInterface<blink::mojom::AttributionHost>(
      base::BindRepeating(
          [](RenderFrameHostImpl* impl,
             mojo::PendingAssociatedReceiver<blink::mojom::AttributionHost>
                 receiver) {
            AttributionHost::BindReceiver(std::move(receiver), impl);
          },
          base::Unretained(this)));

  associated_registry_->AddInterface<device::mojom::ScreenOrientation>(
      base::BindRepeating(
          [](RenderFrameHostImpl* impl,
             mojo::PendingAssociatedReceiver<device::mojom::ScreenOrientation>
                 receiver) {
            impl->delegate()->BindScreenOrientation(impl, std::move(receiver));
          },
          base::Unretained(this)));

  associated_registry_->AddInterface<blink::mojom::BroadcastChannelProvider>(
      base::BindRepeating(&RenderFrameHostImpl::CreateBroadcastChannelProvider,
                          base::Unretained(this)));

  associated_registry_->AddInterface<blink::mojom::BlobURLStore>(
      base::BindRepeating(
          &RenderFrameHostImpl::BindBlobUrlStoreAssociatedReceiver,
          base::Unretained(this)));

  associated_registry_->AddInterface<blink::mojom::FileBackedBlobFactory>(
      base::BindRepeating(&RenderFrameHostImpl::BindFileBackedBlobFactory,
                          base::Unretained(this)));

  // Allow embedders to register their binders.
  GetContentClient()
      ->browser()
      ->RegisterAssociatedInterfaceBindersForRenderFrameHost(
          *this, *associated_registry_);

  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      remote_interfaces;
  GetMojomFrameInRenderer()->GetInterfaceProvider(
      remote_interfaces.InitWithNewPipeAndPassReceiver());

  remote_interfaces_ = std::make_unique<service_manager::InterfaceProvider>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  remote_interfaces_->Bind(std::move(remote_interfaces));
}

void RenderFrameHostImpl::TearDownMojoConnection() {
  // While not directly a Mojo endpoint, |geolocation_service_| may attempt to
  // cancel permission requests.
  geolocation_service_.reset();

  associated_registry_.reset();

  mojo_image_downloader_.reset();
  find_in_page_.reset();
  local_frame_.reset();
  local_main_frame_.reset();

  frame_host_associated_receiver_.reset();
  associated_interface_provider_receiver_.reset();
  back_forward_cache_controller_host_associated_receiver_.reset();
  frame_.reset();
  frame_bindings_control_.reset();
  local_frame_host_receiver_.reset();
  non_associated_local_frame_host_receiver_.reset();
  local_main_frame_host_receiver_.reset();

  broker_receiver_.reset();

  render_accessibility_.reset();
  render_accessibility_host_.Reset();

  dom_automation_controller_receiver_.reset();

#if BUILDFLAG(ENABLE_PPAPI)
  ppapi_support_.reset();
#endif

  // Audio stream factories are tied to a live RenderFrame: see
  // //content/browser/media/forwarding_audio_stream_factory.h.
  // Eagerly reset now to ensure that it is impossible to create streams
  // associated with a RenderFrameHost without a live RenderFrame;
  // otherwise, the `RenderFrameDeleted()` signal used to clean up streams
  // will never fire.
  audio_service_audio_output_stream_factory_.reset();
  audio_service_audio_input_stream_factory_.reset();
}

}  // namespace content
