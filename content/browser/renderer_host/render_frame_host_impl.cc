// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_impl.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/i18n/character_encoding.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/kill.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/about_url_loader_factory.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/bad_message.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/contacts/contacts_manager_impl.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/protocol/audits.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/download/data_url_blob_reader.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/file_system/file_system_manager_impl.h"
#include "content/browser/file_system/file_system_url_loader_factory.h"
#include "content/browser/file_system_access/native_file_system_manager_impl.h"
#include "content/browser/generic_sensor/sensor_provider_proxy_impl.h"
#include "content/browser/geolocation/geolocation_service_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/idle/idle_manager_impl.h"
#include "content/browser/installedapp/installed_app_provider_impl.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/log_console_message.h"
#include "content/browser/manifest/manifest_manager_host.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_interface_proxy.h"
#include "content/browser/media/webaudio/audio_context_manager_impl.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/net/cross_origin_embedder_policy_reporter.h"
#include "content/browser/net/cross_origin_opener_policy_reporter.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/browser/portal/portal.h"
#include "content/browser/presentation/presentation_service_impl.h"
#include "content/browser/push_messaging/push_messaging_manager.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/cookie_utils.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/input/input_injector_impl.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/ipc_utils.h"
#include "content/browser/renderer_host/keep_alive_handle_factory.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/page_lifecycle_state_manager.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/scoped_active_url.h"
#include "content/browser/screen_enumeration/screen_enumeration_impl.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/sms/sms_service.h"
#include "content/browser/speech/speech_synthesis_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/web_bundle_handle.h"
#include "content/browser/web_package/web_bundle_handle_tracker.h"
#include "content/browser/web_package/web_bundle_navigation_info.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "content/browser/webauth/authenticator_impl.h"
#include "content/browser/webauth/webauth_request_security_checker.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/worker_host/dedicated_worker_host_factory_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params_mojom_traits.h"
#include "content/common/navigation_params_utils.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/state_transitions.h"
#include "content/common/unfreezable_frame_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/idle_manager.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/referrer_type_converters.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "device/gamepad/gamepad_monitor.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/base/user_input_monitor.h"
#include "media/learning/common/value.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_constants.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/trust_token_operation_authorization.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/feature_policy/document_policy_features.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/geolocation/geolocation_service.mojom.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/loader/url_loader_factory_bundle.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id_registry.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/quad_f.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if defined(OS_ANDROID)
#include "content/browser/android/content_url_loader_factory.h"
#include "content/browser/android/java_interfaces_impl.h"
#include "content/browser/renderer_host/render_frame_host_android.h"
#include "content/public/browser/android/java_interfaces.h"
#else
#include "content/browser/hid/hid_service.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/serial/serial_service.h"
#endif

#if defined(OS_MAC)
#include "content/browser/renderer_host/popup_menu_helper_mac.h"
#endif

using base::TimeDelta;

namespace content {

#if defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
// Wrapping this in ADDRESS_SANITIZER will enable it to run on
// clusterfuzz, which should help narrow down illegal ax trees more quickly.
// static
int RenderFrameHostImpl::max_accessibility_resets_ = 0;
#else
int RenderFrameHostImpl::max_accessibility_resets_ = 4;
#endif

struct RenderFrameHostOrProxy {
  RenderFrameHostImpl* const frame;
  RenderFrameProxyHost* const proxy;

  RenderFrameHostOrProxy(RenderFrameHostImpl* frame,
                         RenderFrameProxyHost* proxy)
      : frame(frame), proxy(proxy) {
    DCHECK(!frame || !proxy)
        << "Both frame and proxy can't be non-null at the same time";
  }

  explicit operator bool() { return frame || proxy; }

  FrameTreeNode* GetFrameTreeNode() {
    if (frame)
      return frame->frame_tree_node();
    if (proxy)
      return proxy->frame_tree_node();
    return nullptr;
  }
};

namespace {

#if defined(OS_ANDROID)
const void* const kRenderFrameHostAndroidKey = &kRenderFrameHostAndroidKey;
#endif  // OS_ANDROID

// The next value to use for the accessibility reset token.
int g_next_accessibility_reset_token = 1;

// Whether to allow injecting javascript into any kind of frame, for Android
// WebView, WebLayer, Fuchsia web.ContextProvider and CastOS content shell.
bool g_allow_injecting_javascript = false;

typedef std::unordered_map<GlobalFrameRoutingId,
                           RenderFrameHostImpl*,
                           GlobalFrameRoutingIdHasher>
    RoutingIDFrameMap;
base::LazyInstance<RoutingIDFrameMap>::DestructorAtExit g_routing_id_frame_map =
    LAZY_INSTANCE_INITIALIZER;

using TokenFrameMap = std::unordered_map<base::UnguessableToken,
                                         RenderFrameHostImpl*,
                                         base::UnguessableTokenHash>;
base::LazyInstance<TokenFrameMap>::Leaky g_token_frame_map =
    LAZY_INSTANCE_INITIALIZER;

// Returns true if |validated_params| represents a WebView loadDataWithBaseUrl
// navigation.
bool IsLoadDataWithBaseURL(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& validated_params) {
  return NavigationRequest::IsLoadDataWithBaseURL(validated_params.url,
                                                  validated_params.base_url);
}

// Ensure that we reset nav_entry_id_ in DidCommitProvisionalLoad if any of
// the validations fail and lead to an early return.  Call disable() once we
// know the commit will be successful.  Resetting nav_entry_id_ avoids acting on
// any UpdateState or UpdateTitle messages after an ignored commit.
class ScopedCommitStateResetter {
 public:
  explicit ScopedCommitStateResetter(RenderFrameHostImpl* render_frame_host)
      : render_frame_host_(render_frame_host), disabled_(false) {}

  ~ScopedCommitStateResetter() {
    if (!disabled_) {
      render_frame_host_->set_nav_entry_id(0);
    }
  }

  void disable() { disabled_ = true; }

 private:
  RenderFrameHostImpl* render_frame_host_;
  bool disabled_;
};

class ActiveURLMessageFilter : public mojo::MessageFilter {
 public:
  explicit ActiveURLMessageFilter(RenderFrameHostImpl* render_frame_host)
      : render_frame_host_(render_frame_host) {}

  ~ActiveURLMessageFilter() override {
    if (debug_url_set_) {
      GetContentClient()->SetActiveURL(GURL(), "");
    }
  }

  // mojo::MessageFilter overrides.
  bool WillDispatch(mojo::Message* message) override {
    debug_url_set_ = true;
    auto* frame_tree_node = render_frame_host_->frame_tree_node();
    GetContentClient()->SetActiveURL(frame_tree_node->current_url(),
                                     frame_tree_node->frame_tree()
                                         ->root()
                                         ->current_origin()
                                         .GetDebugString());
    return true;
  }

  void DidDispatchOrReject(mojo::Message* message, bool accepted) override {
    GetContentClient()->SetActiveURL(GURL(), "");
    debug_url_set_ = false;
  }

 private:
  RenderFrameHostImpl* render_frame_host_;
  bool debug_url_set_ = false;
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
    if (!IsFrameInBackForwardCache() || ProcessHoldsNonCachedPages() ||
        policy_ == BackForwardCacheImpl::kMessagePolicyNone) {
      return true;
    }

    DLOG(ERROR) << "Received message " << message->name() << " on interface "
                << interface_name_ << " from frame in bfcache.";

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

  // Returns true if both the renderer and the browser think the frame is in the
  // bfcache. That is the PageLifecycleState is in sync with regards to bfcache.
  // In the intermediate states (no ACK received yet) messages must be allowed.
  bool IsFrameInBackForwardCache() {
    PageLifecycleStateManager* mgr =
        render_frame_host_->render_view_host()->GetPageLifecycleStateManager();

    return mgr->last_acknowledged_state().is_in_back_forward_cache &&
           mgr->last_state_sent_to_renderer().is_in_back_forward_cache;
  }

  // TODO(https://crbug.com/1125996): Remove once a well-behaved frozen
  // RenderFrame never send IPCs messages, even if there are active pages in the
  // process.
  bool ProcessHoldsNonCachedPages() {
    return RenderViewHostImpl::HasNonBackForwardCachedInstancesForProcess(
        render_frame_host_->GetProcess());
  }

  RenderFrameHostImpl* const render_frame_host_;
  const char* const interface_name_;
  const BackForwardCacheImpl::MessageHandlingPolicyWhenCached policy_;
};

// This class is used to chain multiple mojo::MessageFilter. Messages will be
// processed by the filters in the same order as the filters are added with the
// Add() method. WillDispatch() might not be called for all filters or might see
// a modified message if a filter earlier in the chain discards or modifies it.
// Similarly a given filter instance might not receive a DidDispatchOrReject()
// call even if WillDispatch() was called if a filter further down the chain
// discarded it. Long story short, the order in which filters are added is
// important!
class MessageFilterChain : public mojo::MessageFilter {
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

std::unique_ptr<mojo::MessageFilter>
CreateMessageFilterForAssociatedReceiverImpl(
    RenderFrameHostImpl* render_frame_host,
    const char* interface_name,
    BackForwardCacheImpl::MessageHandlingPolicyWhenCached policy) {
  auto filter_chain = std::make_unique<MessageFilterChain>();
  filter_chain->Add(std::make_unique<BackForwardCacheMessageFilter>(
      render_frame_host, interface_name, policy));
  // BackForwardCacheMessageFilter might drop messages so add
  // ActiveURLMessageFilter at the end of the chain as we need to make sure that
  // the debug url is reset, that is, DidDispatchOrReject() is called if
  // WillDispatch().
  filter_chain->Add(
      std::make_unique<ActiveURLMessageFilter>(render_frame_host));
  return filter_chain;
}

void GrantFileAccess(int child_id,
                     const std::vector<base::FilePath>& file_paths) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  for (const auto& file : file_paths) {
    if (!policy->CanReadFile(child_id, file))
      policy->GrantReadFile(child_id, file);
  }
}

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
// RemoterFactory that delegates Create() calls to the ContentBrowserClient.
//
// Since Create() could be called at any time, perhaps by a stray task being run
// after a RenderFrameHost has been destroyed, the RemoterFactoryImpl uses the
// process/routing IDs as a weak reference to the RenderFrameHostImpl.
class RemoterFactoryImpl final : public media::mojom::RemoterFactory {
 public:
  RemoterFactoryImpl(int process_id, int routing_id)
      : process_id_(process_id), routing_id_(routing_id) {}

 private:
  void Create(mojo::PendingRemote<media::mojom::RemotingSource> source,
              mojo::PendingReceiver<media::mojom::Remoter> receiver) final {
    if (auto* host = RenderFrameHostImpl::FromID(process_id_, routing_id_)) {
      GetContentClient()->browser()->CreateMediaRemoter(host, std::move(source),
                                                        std::move(receiver));
    }
  }

  const int process_id_;
  const int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(RemoterFactoryImpl);
};
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)

using FrameCallback = base::RepeatingCallback<void(RenderFrameHostImpl*)>;
void ForEachFrame(RenderFrameHostImpl* root_frame_host,
                  const FrameCallback& frame_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  FrameTree* frame_tree = root_frame_host->frame_tree();
  DCHECK_EQ(root_frame_host, frame_tree->GetMainFrame());

  for (FrameTreeNode* node : frame_tree->Nodes()) {
    RenderFrameHostImpl* frame_host = node->current_frame_host();
    RenderFrameHostImpl* pending_frame_host =
        node->render_manager()->speculative_frame_host();
    if (frame_host)
      frame_callback.Run(frame_host);
    if (pending_frame_host)
      frame_callback.Run(pending_frame_host);
  }
}

RenderFrameHostOrProxy LookupRenderFrameHostOrProxy(int process_id,
                                                    int routing_id) {
  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(process_id, routing_id);
  RenderFrameProxyHost* proxy = nullptr;
  if (!rfh)
    proxy = RenderFrameProxyHost::FromID(process_id, routing_id);
  return RenderFrameHostOrProxy(rfh, proxy);
}

RenderFrameHostOrProxy LookupRenderFrameHostOrProxy(
    int process_id,
    const base::UnguessableToken& frame_token) {
  auto it = g_token_frame_map.Get().find(frame_token);
  RenderFrameHostImpl* rfh = nullptr;
  RenderFrameProxyHost* proxy = nullptr;
  if (it != g_token_frame_map.Get().end()) {
    // The check against |process_id| isn't strictly necessary, but represents
    // an extra level of protection against a renderer trying to force a frame
    // token.
    rfh =
        process_id == it->second->GetProcess()->GetID() ? it->second : nullptr;
  } else {
    proxy = RenderFrameProxyHost::FromFrameToken(process_id, frame_token);
  }
  return RenderFrameHostOrProxy(rfh, proxy);
}

// Takes the lower 31 bits of the metric-name-hash of a Mojo interface |name|.
base::Histogram::Sample HashInterfaceNameToHistogramSample(
    base::StringPiece name) {
  return base::strict_cast<base::Histogram::Sample>(
      static_cast<int32_t>(base::HashMetricName(name) & 0x7fffffffull));
}

// Set crash keys that will help understand the circumstances of a renderer
// kill.  Note that the commit URL is already reported in a crash key, and
// additional keys are logged in RenderProcessHostImpl::ShutdownForBadMessage.
void LogRendererKillCrashKeys(const SiteInfo& site_info) {
  static auto* site_info_key = base::debug::AllocateCrashKeyString(
      "current_site_info", base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(site_info_key, site_info.GetDebugString());
}

void LogCanCommitOriginAndUrlFailureReason(const std::string& failure_reason) {
  static auto* failure_reason_key = base::debug::AllocateCrashKeyString(
      "rfhi_can_commit_failure_reason", base::debug::CrashKeySize::Size64);
  base::debug::SetCrashKeyString(failure_reason_key, failure_reason);
}

std::unique_ptr<blink::PendingURLLoaderFactoryBundle> CloneFactoryBundle(
    scoped_refptr<blink::URLLoaderFactoryBundle> bundle) {
  return base::WrapUnique(static_cast<blink::PendingURLLoaderFactoryBundle*>(
      bundle->Clone().release()));
}

// Helper method to download a URL on UI thread.
void StartDownload(
    std::unique_ptr<download::DownloadUrlParameters> parameters,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(parameters->render_process_host_id());
  if (!render_process_host)
    return;

  BrowserContext* browser_context = render_process_host->GetBrowserContext();

  DownloadManager* download_manager =
      BrowserContext::GetDownloadManager(browser_context);
  parameters->set_download_source(download::DownloadSource::FROM_RENDERER);
  download_manager->DownloadUrl(std::move(parameters),
                                std::move(blob_url_loader_factory));
}

// Called on the UI thread when the data URL in the BlobDataHandle
// is read.
void OnDataURLRetrieved(
    std::unique_ptr<download::DownloadUrlParameters> parameters,
    GURL data_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!data_url.is_valid())
    return;
  parameters->set_url(std::move(data_url));
  StartDownload(std::move(parameters), nullptr);
}

void RecordCrossOriginIsolationMetrics(RenderFrameHostImpl* rfh) {
  ContentBrowserClient* client = GetContentClient()->browser();
  if (rfh->cross_origin_opener_policy().value ==
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin) {
    client->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCrossOriginOpenerPolicySameOrigin);
  }
  if (rfh->cross_origin_opener_policy().value ==
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups) {
    client->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::
                 kCrossOriginOpenerPolicySameOriginAllowPopups);
  }

  if (rfh->cross_origin_embedder_policy().value ==
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp) {
    client->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCrossOriginEmbedderPolicyRequireCorp);
  }

  if (rfh->cross_origin_opener_policy().value ==
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep) {
    client->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCoopAndCoepIsolated);
  }

  if (rfh->cross_origin_opener_policy().reporting_endpoint ||
      rfh->cross_origin_opener_policy().report_only_reporting_endpoint) {
    client->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCrossOriginOpenerPolicyReporting);
  }
}

// Subframe navigations can optionally have associated Trust Tokens operations
// (https://github.com/wicg/trust-token-api). If the operation's type is
// "redemption" or "signing" (as opposed to "issuance"), the parent's frame
// needs to have the trust-token-redemption Feature Policy feature enabled.
bool ParentNeedsTrustTokenFeaturePolicy(
    const mojom::BeginNavigationParams& begin_params) {
  if (!begin_params.trust_token_params)
    return false;

  return network::DoesTrustTokenOperationRequireFeaturePolicy(
      begin_params.trust_token_params->type);
}

// Analyzes trusted sources of a frame's trust-token-redemption Feature Policy
// feature to see if the feature is definitely disabled or potentially enabled.
//
// This information will be bound to a URLLoaderFactory; if the answer is
// "definitely disabled," the network service will report a bad message if it
// receives a request from the renderer to execute a Trust Tokens redemption or
// signing operation in the frame.
//
// A return value of kForbid denotes that the feature is disabled for the
// frame. A return value of kPotentiallyPermit means that all trusted
// information sources say that the policy is enabled.
network::mojom::TrustTokenRedemptionPolicy
DetermineWhetherToForbidTrustTokenRedemption(
    const RenderFrameHostImpl* parent,
    const mojom::CommitNavigationParams& commit_params,
    const url::Origin& subframe_origin) {
  // For main frame loads, the frame's feature policy is determined entirely by
  // response headers, which are provided by the renderer.
  if (!parent || !commit_params.frame_policy)
    return network::mojom::TrustTokenRedemptionPolicy::kPotentiallyPermit;

  const blink::FeaturePolicy* parent_policy = parent->feature_policy();
  blink::ParsedFeaturePolicy container_policy =
      commit_params.frame_policy->container_policy;

  auto subframe_policy = blink::FeaturePolicy::CreateFromParentPolicy(
      parent_policy, container_policy, subframe_origin);

  if (subframe_policy->IsFeatureEnabled(
          blink::mojom::FeaturePolicyFeature::kTrustTokenRedemption)) {
    return network::mojom::TrustTokenRedemptionPolicy::kPotentiallyPermit;
  }
  return network::mojom::TrustTokenRedemptionPolicy::kForbid;
}

// When a frame creates its initial subresource loaders, it needs to know
// whether the trust-token-redemption Feature Policy feature will be enabled
// after the commit finishes, which is a little involved (see
// DetermineWhetherToForbidTrustTokenRedemption). In contrast, if it needs to
// make this decision once the frame has committted---for instance, to create
// more loaders after the network service crashes---it can directly consult the
// current Feature Policy state to determine whether the feature is enabled.
network::mojom::TrustTokenRedemptionPolicy
DetermineAfterCommitWhetherToForbidTrustTokenRedemption(
    RenderFrameHostImpl* impl) {
  return impl->IsFeatureEnabled(
             blink::mojom::FeaturePolicyFeature::kTrustTokenRedemption)
             ? network::mojom::TrustTokenRedemptionPolicy::kPotentiallyPermit
             : network::mojom::TrustTokenRedemptionPolicy::kForbid;
}

// Returns the string corresponding to LifecycleState, used for logging crash
// keys.
const char* LifecycleStateToString(RenderFrameHostImpl::LifecycleState state) {
  using LifecycleState = RenderFrameHostImpl::LifecycleState;
  switch (state) {
    case LifecycleState::kSpeculative:
      return "Speculative";
    case LifecycleState::kActive:
      return "Active";
    case LifecycleState::kInBackForwardCache:
      return "InBackForwardCache";
    case LifecycleState::kRunningUnloadHandlers:
      return "RunningUnloadHandlers";
    case LifecycleState::kReadyToBeDeleted:
      return "ReadyToDeleted";
  }
}

}  // namespace

bool ShouldCreateNewHostForCrashedFrame() {
  return GetRenderDocumentLevel() >= RenderDocumentLevel::kCrashedFrame;
}

class RenderFrameHostImpl::DroppedInterfaceRequestLogger
    : public blink::mojom::BrowserInterfaceBroker {
 public:
  explicit DroppedInterfaceRequestLogger(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  ~DroppedInterfaceRequestLogger() override {
    UMA_HISTOGRAM_EXACT_LINEAR("RenderFrameHostImpl.DroppedInterfaceRequests",
                               num_dropped_requests_, 20);
  }

 protected:
  // blink::mojom::BrowserInterfaceBroker
  void GetInterface(mojo::GenericPendingReceiver receiver) override {
    ++num_dropped_requests_;
    auto interface_name = receiver.interface_name().value_or("");
    base::UmaHistogramSparse(
        "RenderFrameHostImpl.DroppedInterfaceRequestName",
        HashInterfaceNameToHistogramSample(interface_name));
    DLOG(WARNING)
        << "InterfaceRequest was dropped, the document is no longer active: "
        << interface_name;
  }

 private:
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> receiver_{this};
  int num_dropped_requests_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DroppedInterfaceRequestLogger);
};

struct PendingNavigation {
  mojom::CommonNavigationParamsPtr common_params;
  mojom::BeginNavigationParamsPtr begin_navigation_params;
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client;
  mojo::PendingRemote<blink::mojom::NavigationInitiator> navigation_initiator;

  PendingNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::BeginNavigationParamsPtr begin_navigation_params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>
          navigation_initiator);
};

PendingNavigation::PendingNavigation(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::BeginNavigationParamsPtr begin_navigation_params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
    mojo::PendingRemote<blink::mojom::NavigationInitiator> navigation_initiator)
    : common_params(std::move(common_params)),
      begin_navigation_params(std::move(begin_navigation_params)),
      blob_url_loader_factory(std::move(blob_url_loader_factory)),
      navigation_client(std::move(navigation_client)),
      navigation_initiator(std::move(navigation_initiator)) {}

// static
RenderFrameHost* RenderFrameHost::FromID(GlobalFrameRoutingId id) {
  return RenderFrameHostImpl::FromID(id);
}

// static
RenderFrameHost* RenderFrameHost::FromID(int render_process_id,
                                         int render_frame_id) {
  return RenderFrameHostImpl::FromID(
      GlobalFrameRoutingId(render_process_id, render_frame_id));
}

// static
void RenderFrameHost::AllowInjectingJavaScript() {
  g_allow_injecting_javascript = true;
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromID(GlobalFrameRoutingId id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameMap* frames = g_routing_id_frame_map.Pointer();
  auto it = frames->find(id);
  return it == frames->end() ? nullptr : it->second;
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromID(int render_process_id,
                                                 int render_frame_id) {
  return RenderFrameHostImpl::FromID(
      GlobalFrameRoutingId(render_process_id, render_frame_id));
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromFrameToken(
    int process_id,
    const base::UnguessableToken& frame_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = g_token_frame_map.Get().find(frame_token);
  if (it == g_token_frame_map.Get().end())
    return nullptr;

  // TODO(tonikitoo): Consider killing the renderer when this happens
  if (it->second->GetProcess()->GetID() != process_id)
    return nullptr;

  return it->second;
}

// static
RenderFrameHost* RenderFrameHost::FromAXTreeID(ui::AXTreeID ax_tree_id) {
  return RenderFrameHostImpl::FromAXTreeID(ax_tree_id);
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromAXTreeID(
    ui::AXTreeID ax_tree_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ui::AXTreeIDRegistry::FrameID frame_id =
      ui::AXTreeIDRegistry::GetInstance()->GetFrameID(ax_tree_id);
  return RenderFrameHostImpl::FromID(frame_id.first, frame_id.second);
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromOverlayRoutingToken(
    const base::UnguessableToken& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = g_token_frame_map.Get().find(token);
  return it == g_token_frame_map.Get().end() ? nullptr : it->second;
}

// static
void RenderFrameHostImpl::ClearAllPrefetchedSignedExchangeCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameMap* frames = g_routing_id_frame_map.Pointer();
  for (auto it : *frames)
    it.second->ClearPrefetchedSignedExchangeCache();
}

RenderFrameHostImpl::RenderFrameHostImpl(
    SiteInstance* site_instance,
    scoped_refptr<RenderViewHostImpl> render_view_host,
    RenderFrameHostDelegate* delegate,
    FrameTree* frame_tree,
    FrameTreeNode* frame_tree_node,
    int32_t routing_id,
    const base::UnguessableToken& frame_token,
    bool renderer_initiated_creation,
    LifecycleState lifecycle_state)
    : render_view_host_(std::move(render_view_host)),
      delegate_(delegate),
      site_instance_(static_cast<SiteInstanceImpl*>(site_instance)),
      agent_scheduling_group_(site_instance_->GetAgentSchedulingGroup()),
      frame_tree_(frame_tree),
      frame_tree_node_(frame_tree_node),
      parent_(frame_tree_node_->parent()),
      routing_id_(routing_id),
      is_waiting_for_unload_ack_(false),
      render_frame_created_(false),
      is_waiting_for_beforeunload_completion_(false),
      beforeunload_dialog_request_cancels_unload_(false),
      unload_ack_is_for_navigation_(false),
      beforeunload_timeout_delay_(base::TimeDelta::FromMilliseconds(
          RenderViewHostImpl::kUnloadTimeoutMS)),
      was_discarded_(false),
      is_loading_(false),
      nav_entry_id_(0),
      accessibility_reset_token_(0),
      accessibility_reset_count_(0),
      browser_plugin_embedder_ax_tree_id_(ui::AXTreeIDUnknown()),
      no_create_browser_accessibility_manager_for_testing_(false),
      web_ui_type_(WebUI::kNoWebUI),
      has_selection_(false),
      is_audible_(false),
      should_virtual_keyboard_overlay_content_(false),
      last_navigation_previews_state_(
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED),
      waiting_for_init_(renderer_initiated_creation),
      has_focused_editable_element_(false),
      push_messaging_manager_(
          nullptr,
          base::OnTaskRunnerDeleter(base::CreateSequencedTaskRunner(
              {ServiceWorkerContext::GetCoreThreadId()}))),
      active_sandbox_flags_(network::mojom::WebSandboxFlags::kNone),
      frame_token_(frame_token),
      keep_alive_timeout_(base::TimeDelta::FromSeconds(30)),
      subframe_unload_timeout_(base::TimeDelta::FromMilliseconds(
          RenderViewHostImpl::kUnloadTimeoutMS)),
      commit_callback_interceptor_(nullptr),
      media_device_id_salt_base_(
          BrowserContext::CreateRandomMediaDeviceIDSalt()),
      lifecycle_state_(lifecycle_state) {
  DCHECK(delegate_);
  DCHECK(lifecycle_state_ == LifecycleState::kSpeculative ||
         lifecycle_state_ == LifecycleState::kActive);

  agent_scheduling_group().AddRoute(routing_id_, this);
  g_routing_id_frame_map.Get().emplace(
      GlobalFrameRoutingId(GetProcess()->GetID(), routing_id_), this);
  g_token_frame_map.Get().insert(std::make_pair(frame_token_, this));
  site_instance_->AddObserver(this);
  GetProcess()->AddObserver(this);
  GetSiteInstance()->IncrementActiveFrameCount();

  if (parent_) {
    cross_origin_embedder_policy_ = parent_->cross_origin_embedder_policy();

    // New child frames should inherit the nav_entry_id of their parent.
    set_nav_entry_id(parent_->nav_entry_id());
  }

  SetUpMojoIfNeeded();

  unload_event_monitor_timeout_ =
      std::make_unique<TimeoutMonitor>(base::BindRepeating(
          &RenderFrameHostImpl::OnUnloaded, weak_ptr_factory_.GetWeakPtr()));
  beforeunload_timeout_ = std::make_unique<TimeoutMonitor>(
      base::BindRepeating(&RenderFrameHostImpl::BeforeUnloadTimeout,
                          weak_ptr_factory_.GetWeakPtr()));

  // Local roots are:
  // - main frames; or
  // - subframes in a different SiteInstance from their parent.
  //
  // Local roots require a RenderWidget for input/layout/painting.
  if (!parent_ || IsCrossProcessSubframe()) {
    if (!parent_) {
      // For main frames, the RenderWidgetHost is owned by the RenderViewHost.
      // TODO(https://crbug.com/545684): Once RenderViewHostImpl has-a
      // RenderWidgetHostImpl, the main render frame should probably start
      // owning the RenderWidgetHostImpl itself.
      DCHECK(GetLocalRenderWidgetHost());
      DCHECK(!GetLocalRenderWidgetHost()->owned_by_render_frame_host());
    } else {
      // For local child roots, the RenderFrameHost directly creates and owns
      // its RenderWidgetHost.
      int32_t widget_routing_id =
          site_instance->GetProcess()->GetNextRoutingID();
      DCHECK_EQ(nullptr, GetLocalRenderWidgetHost());
      owned_render_widget_host_ = RenderWidgetHostFactory::Create(
          frame_tree_->render_widget_delegate(), agent_scheduling_group_,
          widget_routing_id, /*hidden=*/true);
      owned_render_widget_host_->set_owned_by_render_frame_host(true);
#if defined(OS_ANDROID)
      owned_render_widget_host_->SetForceEnableZoom(
          delegate_->GetOrCreateWebPreferences().force_enable_zoom);
#endif  // defined(OS_ANDROID)
    }

    if (is_main_frame())
      GetLocalRenderWidgetHost()->SetIntersectsViewport(true);
    GetLocalRenderWidgetHost()->SetFrameDepth(frame_tree_node_->depth());
    GetLocalRenderWidgetHost()->input_router()->SetFrameTreeNodeId(
        frame_tree_node_->frame_tree_node_id());
  }
  ResetFeaturePolicy();

  // Content-Security-Policy: The CSP source 'self' is usually the origin of the
  // current document, set by SetLastCommittedOrigin(). However, before a new
  // frame commits its first navigation, 'self' should correspond to the origin
  // of the parent (in case of a new iframe) or the opener (in case of a new
  // window). This is necessary to correctly enforce CSP during the initial
  // navigation.
  FrameTreeNode* frame_owner =
      frame_tree_node_->parent() ? frame_tree_node_->parent()->frame_tree_node()
                                 : frame_tree_node_->opener();
  if (frame_owner)
    CSPContext::SetSelf(frame_owner->current_origin());

  // New RenderFrameHostImpl are put in their own virtual browsing context
  // group. Then, they can inherit from:
  // 1) Their opener in RenderFrameHostImpl::CreateNewWindow().
  // 2) Their navigation in RenderFrameHostImpl::DidCommitNavigationInternal().
  virtual_browsing_context_group_ =
      CrossOriginOpenerPolicyReporter::NextVirtualBrowsingContextGroup();

  // IdleManager should be unique per RenderFrame to provide proper isolation
  // of overrides.
  idle_manager_ =
      std::make_unique<IdleManagerImpl>(GetProcess()->GetBrowserContext());
}

RenderFrameHostImpl::~RenderFrameHostImpl() {
  // When a RenderFrameHostImpl is deleted, it may still contain children. This
  // can happen with the unload timer. It causes a RenderFrameHost to delete
  // itself even if it is still waiting for its children to complete their
  // unload handlers.
  //
  // Observers expect children to be deleted first. Do it now before notifying
  // them.
  ResetChildren();

  // Destroying |navigation_request_| may call into delegates/observers,
  // so we do it early while |this| object is still in a sane state.
  ResetNavigationRequests();

  // Release the WebUI instances before all else as the WebUI may accesses the
  // RenderFrameHost during cleanup.
  ClearWebUI();

  SetLastCommittedSiteInfo(GURL());
  if (last_committed_document_priority_) {
    GetProcess()->UpdateFrameWithPriority(last_committed_document_priority_,
                                          base::nullopt);
  }

  g_token_frame_map.Get().erase(frame_token_);

  site_instance_->RemoveObserver(this);
  GetProcess()->RemoveObserver(this);

  const bool was_created = render_frame_created_;
  render_frame_created_ = false;
  if (was_created)
    delegate_->RenderFrameDeleted(this);

  // Ensure that the render process host has been notified that all audio
  // streams from this frame have terminated. This is required to ensure the
  // process host has the correct media stream count, which affects its
  // background priority.
  if (is_audible_)
    OnAudibleStateChanged(false);

  // If this was the last active frame in the SiteInstance, the
  // DecrementActiveFrameCount call will trigger the deletion of the
  // SiteInstance's proxies.
  GetSiteInstance()->DecrementActiveFrameCount();

  // Once a RenderFrame is created in the renderer, there are three possible
  // clean-up paths:
  // 1. The RenderFrame can be the main frame. In this case, closing the
  //    associated RenderView will clean up the resources associated with the
  //    main RenderFrame.
  // 2. The RenderFrame can be unloaded. In this case, the browser sends a
  //    UnfreezableFrameMsg_Unload for the RenderFrame to replace itself with a
  //    RenderFrameProxy and release its associated resources.
  //    |lifecycle_state_| is advanced to LifeCycleState::kRunningUnloadHandlers
  //    to track that this IPC is in flight.
  // 3. The RenderFrame can be detached, as part of removing a subtree (due to
  //    navigation, unload, or DOM mutation). In this case, the browser sends
  //    a UnfreezableFrameMsg_Delete for the RenderFrame to detach itself and
  //    release its associated resources. If the subframe contains an unload
  //    handler, |lifecycle_state_| is advanced to
  //    LifeCycleState::kRunningUnloadHandlers to track that the detach is in
  //    progress; otherwise, it is advanced directly to
  //    LifeCycleState::kReadyToBeDeleted.
  //
  // The browser side gives the renderer a small timeout to finish processing
  // unload / detach messages. When the timeout expires, the RFH will be
  // removed regardless of whether or not the renderer acknowledged that it
  // completed the work, to avoid indefinitely leaking browser-side state. To
  // avoid leaks, ~RenderFrameHostImpl still validates that the appropriate
  // cleanup IPC was sent to the renderer, by checking IsPendingDeletion().
  //
  // TODO(dcheng): Due to how frame detach is signalled today, there are some
  // bugs in this area. In particular, subtree detach is reported from the
  // bottom up, so the replicated UnfreezableFrameMsg_Delete messages actually
  // operate on a node-by-node basis rather than detaching an entire subtree at
  // once...
  //
  // Note that this logic is fairly subtle. It needs to include all subframes
  // and all speculative frames, but it should exclude case #1 (a main
  // RenderFrame owned by the RenderView). It can't simply:
  // - check |frame_tree_node_->render_manager()->speculative_frame_host()| for
  //   equality against |this|. The speculative frame host is unset before the
  //   speculative frame host is destroyed, so this condition would never be
  //   matched for a speculative RFH that needs to be destroyed.
  // - check |IsCurrent()|, because the RenderFrames in an InterstitialPageImpl
  //   are never considered current.
  //
  // Directly comparing against |RenderViewHostImpl::GetMainFrame()| still has
  // one additional subtlety though: |GetMainFrame()| can sometimes return a
  // speculative RFH! For subframes, this obviously does not matter: a subframe
  // will always pass the condition |render_view_host_->GetMainFrame() != this|.
  // However, it turns out that a speculative main frame being deleted will
  // *always* pass this condition as well: a speculative RFH being deleted will
  // *always* first be unassociated from its corresponding RFHM. Thus, it
  // follows that |GetMainFrame()| will never return the speculative main frame
  // being deleted, since it must have already been unset.
  if (was_created && render_view_host_->GetMainFrame() != this)
    CHECK(IsPendingDeletion() || IsInBackForwardCache());

  agent_scheduling_group().RemoveRoute(routing_id_);
  g_routing_id_frame_map.Get().erase(
      GlobalFrameRoutingId(GetProcess()->GetID(), routing_id_));

  // Null out the unload timer; in crash dumps this member will be null only if
  // the dtor has run.  (It may also be null in tests.)
  unload_event_monitor_timeout_.reset();

  // Delete this before destroying the widget, to guard against reentrancy
  // by in-process screen readers such as JAWS.
  browser_accessibility_manager_.reset();

  // Note: The RenderWidgetHost of the main frame is owned by the RenderViewHost
  // instead. In this case the RenderViewHost is responsible for shutting down
  // its RenderViewHost.
  if (owned_render_widget_host_)
    owned_render_widget_host_->ShutdownAndDestroyWidget(false);

  render_view_host_.reset();

  // If another frame is waiting for a beforeunload completion callback from
  // this frame, simulate it now.
  RenderFrameHostImpl* beforeunload_initiator = GetBeforeUnloadInitiator();
  if (beforeunload_initiator && beforeunload_initiator != this) {
    base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;
    beforeunload_initiator->ProcessBeforeUnloadCompletedFromFrame(
        true /* proceed */, false /* treat_as_final_completion_callback */,
        this, true /* is_frame_being_destroyed */, approx_renderer_start_time,
        base::TimeTicks::Now());
  }

  if (prefetched_signed_exchange_cache_)
    prefetched_signed_exchange_cache_->RecordHistograms();
}

int RenderFrameHostImpl::GetRoutingID() {
  return routing_id_;
}

const base::UnguessableToken& RenderFrameHostImpl::GetFrameToken() {
  return frame_token_;
}

ui::AXTreeID RenderFrameHostImpl::GetAXTreeID() {
  return ax_tree_id();
}

const base::UnguessableToken& RenderFrameHostImpl::GetTopFrameToken() {
  RenderFrameHostImpl* frame = this;
  while (frame->parent_) {
    frame = frame->parent_;
  }
  return frame->GetFrameToken();
}

void RenderFrameHostImpl::AudioContextPlaybackStarted(int audio_context_id) {
  delegate_->AudioContextPlaybackStarted(this, audio_context_id);
}

void RenderFrameHostImpl::AudioContextPlaybackStopped(int audio_context_id) {
  delegate_->AudioContextPlaybackStopped(this, audio_context_id);
}

// The current frame went into the BackForwardCache.
void RenderFrameHostImpl::DidEnterBackForwardCache() {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::EnterBackForwardCache");
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK_EQ(lifecycle_state_, LifecycleState::kActive);
  SetLifecycleState(LifecycleState::kInBackForwardCache);
  // Pages in the back-forward cache are automatically evicted after a certain
  // time.
  if (!GetParent())
    StartBackForwardCacheEvictionTimer();
  for (auto& child : children_)
    child->current_frame_host()->DidEnterBackForwardCache();

  if (service_worker_container_hosts_.empty())
    return;
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          [](const std::map<std::string,
                            base::WeakPtr<ServiceWorkerContainerHost>>& hosts) {
            for (auto host : hosts) {
              if (host.second) {
                host.second->OnEnterBackForwardCache();
              }
            }
          },
          service_worker_container_hosts_));
}

// The frame as been restored from the BackForwardCache.
void RenderFrameHostImpl::WillLeaveBackForwardCache() {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::LeaveBackForwardCache");
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK_EQ(lifecycle_state_, LifecycleState::kInBackForwardCache);
  if (back_forward_cache_eviction_timer_.IsRunning())
    back_forward_cache_eviction_timer_.Stop();
  for (auto& child : children_)
    child->current_frame_host()->WillLeaveBackForwardCache();

  if (service_worker_container_hosts_.empty())
    return;
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          [](const std::map<std::string,
                            base::WeakPtr<ServiceWorkerContainerHost>>& hosts) {
            for (auto host : hosts) {
              if (host.second)
                host.second->OnRestoreFromBackForwardCache();
            }
          },
          service_worker_container_hosts_));
}

std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
RenderFrameHostImpl::TakeLastCommitParams() {
  return std::move(last_commit_params_);
}

void RenderFrameHostImpl::StartBackForwardCacheEvictionTimer() {
  DCHECK(IsInBackForwardCache());
  base::TimeDelta evict_after =
      BackForwardCacheImpl::GetTimeToLiveInBackForwardCache();

  back_forward_cache_eviction_timer_.SetTaskRunner(
      frame_tree()->controller()->GetBackForwardCache().GetTaskRunner());

  back_forward_cache_eviction_timer_.Start(
      FROM_HERE, evict_after,
      base::BindOnce(&RenderFrameHostImpl::EvictFromBackForwardCacheWithReason,
                     weak_ptr_factory_.GetWeakPtr(),
                     BackForwardCacheMetrics::NotRestoredReason::kTimeout));
}

void RenderFrameHostImpl::DisableBackForwardCache(base::StringPiece reason) {
  back_forward_cache_disabled_reasons_.insert(reason.as_string());
  MaybeEvictFromBackForwardCache();
}

void RenderFrameHostImpl::DisableProactiveBrowsingInstanceSwapForTesting() {
  // This should only be called on main frames.
  DCHECK(!GetParent());
  has_test_disabled_proactive_browsing_instance_swap_ = true;
}

void RenderFrameHostImpl::OnGrantedMediaStreamAccess() {
  was_granted_media_access_ = true;
  MaybeEvictFromBackForwardCache();
}

void RenderFrameHostImpl::OnPortalActivated(
    std::unique_ptr<Portal> predecessor,
    mojo::PendingAssociatedRemote<blink::mojom::Portal> pending_portal,
    mojo::PendingAssociatedReceiver<blink::mojom::PortalClient> client_receiver,
    blink::TransferableMessage data,
    base::OnceCallback<void(blink::mojom::PortalActivateResult)> callback) {
  auto it = portals_.insert(std::move(predecessor)).first;

  GetAssociatedLocalMainFrame()->OnPortalActivated(
      (*it)->portal_token(), std::move(pending_portal),
      std::move(client_receiver), std::move(data),
      base::BindOnce(
          [](base::OnceCallback<void(blink::mojom::PortalActivateResult)>
                 callback,
             blink::mojom::PortalActivateResult result) {
            switch (result) {
              case blink::mojom::PortalActivateResult::kPredecessorWillUnload:
              case blink::mojom::PortalActivateResult::kPredecessorWasAdopted:
                // These values are acceptable from the renderer.
                break;
              case blink::mojom::PortalActivateResult::
                  kRejectedDueToPredecessorNavigation:
              case blink::mojom::PortalActivateResult::
                  kRejectedDueToPortalNotReady:
              case blink::mojom::PortalActivateResult::
                  kRejectedDueToErrorInPortal:
              case blink::mojom::PortalActivateResult::kDisconnected:
              case blink::mojom::PortalActivateResult::kAbortedDueToBug:
                // The renderer is misbehaving.
                mojo::ReportBadMessage(
                    "Unexpected PortalActivateResult from renderer");
                result = blink::mojom::PortalActivateResult::kAbortedDueToBug;
                break;
            }
            std::move(callback).Run(result);
          },
          std::move(callback)));
}

void RenderFrameHostImpl::OnPortalCreatedForTesting(
    std::unique_ptr<Portal> portal) {
  portals_.insert(std::move(portal));
}

Portal* RenderFrameHostImpl::FindPortalByToken(
    const blink::PortalToken& portal_token) {
  auto it =
      std::find_if(portals_.begin(), portals_.end(), [&](const auto& portal) {
        return portal->portal_token() == portal_token;
      });
  return it == portals_.end() ? nullptr : it->get();
}

std::vector<Portal*> RenderFrameHostImpl::GetPortals() const {
  std::vector<Portal*> result;
  for (const auto& portal : portals_)
    result.push_back(portal.get());
  return result;
}

void RenderFrameHostImpl::DestroyPortal(Portal* portal) {
  auto it = portals_.find(portal);
  CHECK(it != portals_.end());
  std::unique_ptr<Portal> owned_portal(std::move(*it));
  portals_.erase(it);
  // |owned_portal| is deleted as it goes out of scope.
}

void RenderFrameHostImpl::ForwardMessageFromHost(
    blink::TransferableMessage message,
    const url::Origin& source_origin) {
  DCHECK_EQ(source_origin, GetLastCommittedOrigin());
  GetAssociatedLocalMainFrame()->ForwardMessageFromHost(std::move(message),
                                                        source_origin);
}

SiteInstanceImpl* RenderFrameHostImpl::GetSiteInstance() {
  return site_instance_.get();
}

RenderProcessHost* RenderFrameHostImpl::GetProcess() {
  return agent_scheduling_group_.GetProcess();
}

RenderFrameHostImpl* RenderFrameHostImpl::GetParent() {
  return parent_;
}

std::vector<RenderFrameHost*> RenderFrameHostImpl::GetFramesInSubtree() {
  std::vector<RenderFrameHost*> frame_hosts;
  for (FrameTreeNode* node : frame_tree_->SubtreeNodes(frame_tree_node()))
    frame_hosts.push_back(node->current_frame_host());
  return frame_hosts;
}

bool RenderFrameHostImpl::IsDescendantOf(RenderFrameHost* ancestor) {
  if (!ancestor || !static_cast<RenderFrameHostImpl*>(ancestor)->child_count())
    return false;

  for (RenderFrameHostImpl* current = GetParent(); current;
       current = current->GetParent()) {
    if (current == ancestor)
      return true;
  }
  return false;
}

int RenderFrameHostImpl::GetFrameTreeNodeId() {
  return frame_tree_node_->frame_tree_node_id();
}

base::UnguessableToken RenderFrameHostImpl::GetDevToolsFrameToken() {
  return frame_tree_node_->devtools_frame_token();
}

base::Optional<base::UnguessableToken>
RenderFrameHostImpl::GetEmbeddingToken() {
  return embedding_token_;
}

const std::string& RenderFrameHostImpl::GetFrameName() {
  return frame_tree_node_->frame_name();
}

bool RenderFrameHostImpl::IsFrameDisplayNone() {
  return frame_tree_node()->frame_owner_properties().is_display_none;
}

const base::Optional<gfx::Size>& RenderFrameHostImpl::GetFrameSize() {
  return frame_size_;
}

size_t RenderFrameHostImpl::GetFrameDepth() {
  return frame_tree_node()->depth();
}

bool RenderFrameHostImpl::IsCrossProcessSubframe() {
  if (!parent_)
    return false;
  return GetSiteInstance() != parent_->GetSiteInstance();
}

const GURL& RenderFrameHostImpl::GetLastCommittedURL() {
  return last_committed_url_;
}

const url::Origin& RenderFrameHostImpl::GetLastCommittedOrigin() {
  return last_committed_origin_;
}

const net::NetworkIsolationKey& RenderFrameHostImpl::GetNetworkIsolationKey() {
  DCHECK(!isolation_info_.IsEmpty());
  return isolation_info_.network_isolation_key();
}

const net::IsolationInfo&
RenderFrameHostImpl::GetIsolationInfoForSubresources() {
  DCHECK(!isolation_info_.IsEmpty());
  return isolation_info_;
}

void RenderFrameHostImpl::GetCanonicalUrlForSharing(
    mojom::Frame::GetCanonicalUrlForSharingCallback callback) {
  // TODO(https://crbug.com/859110): Remove this once frame_ can no longer be
  // null.
  if (IsRenderFrameLive()) {
    frame_->GetCanonicalUrlForSharing(std::move(callback));
  } else {
    std::move(callback).Run(base::nullopt);
  }
}

void RenderFrameHostImpl::GetSerializedHtmlWithLocalLinks(
    const base::flat_map<GURL, base::FilePath>& url_map,
    const base::flat_map<base::UnguessableToken, base::FilePath>&
        frame_token_map,
    bool save_with_empty_url,
    mojo::PendingRemote<mojom::FrameHTMLSerializerHandler> serializer_handler) {
  // TODO(https://crbug.com/859110): Remove once frame_ can no longer be null.
  if (!IsRenderFrameLive())
    return;

  frame_->GetSerializedHtmlWithLocalLinks(url_map, frame_token_map,
                                          save_with_empty_url,
                                          std::move(serializer_handler));
}

void RenderFrameHostImpl::ExecuteMediaPlayerActionAtLocation(
    const gfx::Point& location,
    const blink::mojom::MediaPlayerAction& action) {
  auto media_player_action = blink::mojom::MediaPlayerAction::New();
  media_player_action->type = action.type;
  media_player_action->enable = action.enable;
  gfx::PointF point_in_view = GetView()->TransformRootPointToViewCoordSpace(
      gfx::PointF(location.x(), location.y()));
  GetAssociatedLocalFrame()->MediaPlayerActionAt(
      gfx::Point(point_in_view.x(), point_in_view.y()),
      std::move(media_player_action));
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver) {
  // By providing null |navigation_request| we will always use the last
  // committed Origin and ClientSecurityState (using GetPageUkmSourceId()
  // does the same for UKM). If the caller wanted a factory associated to a
  // navigation about to commit, the params generated won't be correct. There is
  // no good way of fixing this before RenderDocumentHost (ie swapping
  // RenderFrameHost on each navigation).
  NavigationRequest* navigation_request = nullptr;

  return CreateNetworkServiceDefaultFactoryAndObserve(
      CreateURLLoaderFactoryParamsForMainWorld(
          navigation_request, "RFHI::CreateNetworkServiceDefaultFactory"),
      base::UkmSourceId::FromInt64(GetPageUkmSourceId()),
      std::move(default_factory_receiver));
}

void RenderFrameHostImpl::MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
    base::flat_set<url::Origin> isolated_world_origins,
    bool push_to_renderer_now) {
  size_t old_size =
      isolated_worlds_requiring_separate_url_loader_factory_.size();
  isolated_worlds_requiring_separate_url_loader_factory_.insert(
      isolated_world_origins.begin(), isolated_world_origins.end());
  size_t new_size =
      isolated_worlds_requiring_separate_url_loader_factory_.size();
  bool insertion_took_place = (old_size != new_size);

  // Push the updated set of factories to the renderer, but only if
  // 1) the caller requested an immediate push (e.g. for content scripts
  //    injected programmatically chrome.tabs.executeCode, but not for content
  //    scripts declared in the manifest - the difference is that the latter
  //    happen at a commit and the factories can just be send in the commit
  //    IPC).
  // 2) an insertion actually took place / the factories have been modified
  // 3) a commit has taken place before (i.e. the frame has received a factory
  //    bundle before).
  if (push_to_renderer_now && insertion_took_place &&
      has_committed_any_navigation_) {
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories =
            std::make_unique<blink::PendingURLLoaderFactoryBundle>();
    subresource_loader_factories->pending_isolated_world_factories() =
        CreateURLLoaderFactoriesForIsolatedWorlds(
            FindLatestNavigationRequestThatIsStillCommitting(),
            isolated_world_origins);
    GetNavigationControl()->UpdateSubresourceLoaderFactories(
        std::move(subresource_loader_factories));
  }
}

bool RenderFrameHostImpl::IsSandboxed(network::mojom::WebSandboxFlags flags) {
  if (base::FeatureList::IsEnabled(features::kFeaturePolicyForSandbox)) {
    blink::mojom::FeaturePolicyFeature feature =
        blink::FeaturePolicy::FeatureForSandboxFlag(flags);
    if (feature != blink::mojom::FeaturePolicyFeature::kNotFound)
      return !IsFeatureEnabled(feature);
  }
  return static_cast<int>(active_sandbox_flags_) & static_cast<int>(flags);
}

blink::web_pref::WebPreferences
RenderFrameHostImpl::GetOrCreateWebPreferences() {
  return delegate()->GetOrCreateWebPreferences();
}

blink::PendingURLLoaderFactoryBundle::OriginMap
RenderFrameHostImpl::CreateURLLoaderFactoriesForIsolatedWorlds(
    NavigationRequest* navigation_request,
    const base::flat_set<url::Origin>& isolated_world_origins) {
  url::Origin main_world_origin;
  network::mojom::ClientSecurityStatePtr client_security_state;
  network::mojom::TrustTokenRedemptionPolicy trust_token_redemption_policy =
      network::mojom::TrustTokenRedemptionPolicy::kForbid;
  ExtractFactoryParamsFromNavigationRequestOrLastCommittedNavigation(
      navigation_request, &main_world_origin, &client_security_state,
      nullptr /* coep_reporter_remote */, &trust_token_redemption_policy);

  blink::PendingURLLoaderFactoryBundle::OriginMap result;
  for (const url::Origin& isolated_world_origin : isolated_world_origins) {
    network::mojom::URLLoaderFactoryParamsPtr factory_params =
        URLLoaderFactoryParamsHelper::CreateForIsolatedWorld(
            this, isolated_world_origin, main_world_origin,
            mojo::Clone(client_security_state), trust_token_redemption_policy);

    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    CreateNetworkServiceDefaultFactoryAndObserve(
        std::move(factory_params),
        base::kInvalidUkmSourceId, /* isolated from page */
        factory_remote.InitWithNewPipeAndPassReceiver());
    result[isolated_world_origin] = std::move(factory_remote);
  }
  return result;
}

gfx::NativeView RenderFrameHostImpl::GetNativeView() {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (!view)
    return nullptr;
  return view->GetNativeView();
}

void RenderFrameHostImpl::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  AddMessageToConsoleImpl(level, message, false /* discard_duplicates */);
}

void RenderFrameHostImpl::ExecuteJavaScriptMethod(
    const base::string16& object_name,
    const base::string16& method_name,
    base::Value arguments,
    JavaScriptResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(arguments.is_list());
  CHECK(CanExecuteJavaScript());

  const bool wants_result = !callback.is_null();
  GetNavigationControl()->JavaScriptMethodExecuteRequest(
      object_name, method_name, std::move(arguments), wants_result,
      std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScript(const base::string16& javascript,
                                            JavaScriptResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(CanExecuteJavaScript());

  const bool wants_result = !callback.is_null();
  GetNavigationControl()->JavaScriptExecuteRequest(javascript, wants_result,
                                                   std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptInIsolatedWorld(
    const base::string16& javascript,
    JavaScriptResultCallback callback,
    int32_t world_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GT(world_id, ISOLATED_WORLD_ID_GLOBAL);
  DCHECK_LE(world_id, ISOLATED_WORLD_ID_MAX);

  const bool wants_result = !callback.is_null();
  GetNavigationControl()->JavaScriptExecuteRequestInIsolatedWorld(
      javascript, wants_result, world_id, std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptForTests(
    const base::string16& javascript,
    JavaScriptResultCallback callback,
    int32_t world_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const bool has_user_gesture = false;
  const bool wants_result = !callback.is_null();
  GetNavigationControl()->JavaScriptExecuteRequestForTests(  // IN-TEST
      javascript, wants_result, has_user_gesture, world_id,
      std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptWithUserGestureForTests(
    const base::string16& javascript,
    int32_t world_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(mustaq): The render-to-browser state update caused by the below
  // JavaScriptExecuteRequestsForTests call is redundant with this update. We
  // should determine if the redundancy can be removed.
  frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  const bool has_user_gesture = true;
  GetNavigationControl()->JavaScriptExecuteRequestForTests(  // IN-TEST
      javascript, false, has_user_gesture, world_id, base::NullCallback());
}

void RenderFrameHostImpl::CopyImageAt(int x, int y) {
  gfx::PointF point_in_view =
      GetView()->TransformRootPointToViewCoordSpace(gfx::PointF(x, y));
  GetAssociatedLocalFrame()->CopyImageAt(
      gfx::Point(point_in_view.x(), point_in_view.y()));
}

void RenderFrameHostImpl::SaveImageAt(int x, int y) {
  gfx::PointF point_in_view =
      GetView()->TransformRootPointToViewCoordSpace(gfx::PointF(x, y));
  GetAssociatedLocalFrame()->SaveImageAt(
      gfx::Point(point_in_view.x(), point_in_view.y()));
}

RenderViewHost* RenderFrameHostImpl::GetRenderViewHost() {
  return render_view_host_.get();
}

service_manager::InterfaceProvider* RenderFrameHostImpl::GetRemoteInterfaces() {
  return remote_interfaces_.get();
}

blink::AssociatedInterfaceProvider*
RenderFrameHostImpl::GetRemoteAssociatedInterfaces() {
  if (!remote_associated_interfaces_) {
    mojo::AssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
        remote_interfaces;
    if (agent_scheduling_group().GetChannel()) {
      agent_scheduling_group().GetRemoteRouteProvider()->GetRoute(
          GetRoutingID(), remote_interfaces.BindNewEndpointAndPassReceiver());
    } else {
      // The channel may not be initialized in some tests environments. In this
      // case we set up a dummy interface provider.
      ignore_result(
          remote_interfaces.BindNewEndpointAndPassDedicatedReceiver());
    }
    remote_associated_interfaces_ =
        std::make_unique<blink::AssociatedInterfaceProvider>(
            remote_interfaces.Unbind());
  }
  return remote_associated_interfaces_.get();
}

PageVisibilityState RenderFrameHostImpl::GetVisibilityState() {
  // Works around the crashes seen in https://crbug.com/501863, where the
  // active WebContents from a browser iterator may contain a render frame
  // detached from the frame tree. This tries to find a RenderWidgetHost
  // attached to an ancestor frame, and defaults to visibility hidden if
  // it fails.
  // TODO(yfriedman, peter): Ideally this would never be called on an
  // unattached frame and we could omit this check. See
  // https://crbug.com/615867.
  RenderFrameHostImpl* frame = this;
  while (frame) {
    if (frame->GetLocalRenderWidgetHost())
      break;
    frame = frame->GetParent();
  }
  if (!frame)
    return PageVisibilityState::kHidden;

  PageVisibilityState visibility_state = GetRenderWidgetHost()->is_hidden()
                                             ? PageVisibilityState::kHidden
                                             : PageVisibilityState::kVisible;
  GetContentClient()->browser()->OverridePageVisibilityState(this,
                                                             &visibility_state);
  return visibility_state;
}

bool RenderFrameHostImpl::Send(IPC::Message* message) {
  return agent_scheduling_group().Send(message);
}

bool RenderFrameHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // Only process messages if the RenderFrame is alive.
  if (!render_frame_created_)
    return false;

  // Crash reports triggered by IPC messages for this frame should be associated
  // with its URL.
  ScopedActiveURL scoped_active_url(this);

  if (delegate_->OnMessageReceived(this, msg))
    return true;

  RenderFrameProxyHost* proxy =
      frame_tree_node_->render_manager()->GetProxyToParent();
  if (proxy && proxy->cross_process_frame_connector() &&
      proxy->cross_process_frame_connector()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameHostImpl, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Unload_ACK, OnUnloadACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ContextMenu, OnContextMenu)
  IPC_END_MESSAGE_MAP()

  // No further actions here, since we may have been deleted.
  return handled;
}

void RenderFrameHostImpl::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  // TODO(https://crbug.com/1123438) It is not understood why
  // OnAssociatedInterfaceRequest can be received after resetting
  // `associated_registry_`. This is reset in InvalidateMojoConnection(), which
  // means we want to stop receiving messages on behalf of the frame. Ignoring
  // this request sounded like the right way to handle this.
  if (!associated_registry_)
    return;

  if (associated_registry_->TryBindInterface(interface_name, &handle))
    return;

  if (GetContentClient()->browser()->BindAssociatedReceiverFromFrame(
          this, interface_name, &handle)) {
    return;
  }

  delegate_->OnAssociatedInterfaceRequest(this, interface_name,
                                          std::move(handle));
}

void RenderFrameHostImpl::AccessibilityPerformAction(
    const ui::AXActionData& action_data) {
  // Don't perform any Accessibility action on an inactive frame.
  if (IsInactiveAndDisallowReactivation() || !render_accessibility_)
    return;

  if (action_data.action == ax::mojom::Action::kHitTest) {
    AccessibilityHitTest(action_data.target_point,
                         action_data.hit_test_event_to_fire,
                         action_data.request_id, {});
    return;
  }
  // Set the input modality in RenderWidgetHostViewAura to touch so the
  // VK shows up.
  if (action_data.action == ax::mojom::Action::kFocus) {
    RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
        render_view_host_->GetWidget()->GetView());
    if (view)
      view->SetLastPointerType(ui::EventPointerType::kTouch);
  }

  render_accessibility_->PerformAction(action_data);
}

bool RenderFrameHostImpl::AccessibilityViewHasFocus() {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return view->HasFocus();
  return false;
}

void RenderFrameHostImpl::AccessibilityViewSetFocus() {
  // Don't update Accessibility for inactive frames.
  if (IsInactiveAndDisallowReactivation())
    return;

  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    view->Focus();
}

gfx::Rect RenderFrameHostImpl::AccessibilityGetViewBounds() {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return view->GetViewBounds();
  return gfx::Rect();
}

float RenderFrameHostImpl::AccessibilityGetDeviceScaleFactor() {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return GetScaleFactorForView(view);
  return 1.0f;
}

void RenderFrameHostImpl::AccessibilityReset() {
  if (!render_accessibility_)
    return;

  accessibility_reset_token_ = g_next_accessibility_reset_token++;
  render_accessibility_->Reset(accessibility_reset_token_);
}

void RenderFrameHostImpl::AccessibilityFatalError() {
  browser_accessibility_manager_.reset(nullptr);
  if (accessibility_reset_token_ || !render_accessibility_)
    return;

  accessibility_reset_count_++;
  if (accessibility_reset_count_ > max_accessibility_resets_) {
    render_accessibility_->FatalError();
  } else {
    AccessibilityReset();
  }
}

gfx::AcceleratedWidget
RenderFrameHostImpl::AccessibilityGetAcceleratedWidget() {
  // Only the main frame's current frame host is connected to the native
  // widget tree for accessibility, so return null if this is queried on
  // any other frame.
  if (!is_main_frame() || !IsCurrent())
    return gfx::kNullAcceleratedWidget;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetAcceleratedWidget();
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
RenderFrameHostImpl::AccessibilityGetNativeViewAccessible() {
  // If this method is called when the document is in BackForwardCache, evict
  // the document to avoid ignoring any accessibility related events which the
  // document might not expect.
  if (IsInactiveAndDisallowReactivation())
    return nullptr;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetNativeViewAccessible();
  return nullptr;
}

gfx::NativeViewAccessible
RenderFrameHostImpl::AccessibilityGetNativeViewAccessibleForWindow() {
  // If this method is called when the frame is in BackForwardCache, evict
  // the frame to avoid ignoring any accessibility related events which are not
  // expected.
  if (IsInactiveAndDisallowReactivation())
    return nullptr;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetNativeViewAccessibleForWindow();
  return nullptr;
}

WebContents* RenderFrameHostImpl::AccessibilityWebContents() {
  // If this method is called when the frame is in BackForwardCache, evict
  // the frame to avoid ignoring any accessibility related events which are not
  // expected.
  if (IsInactiveAndDisallowReactivation())
    return nullptr;
  return delegate()->GetAsWebContents();
}

void RenderFrameHostImpl::AccessibilityHitTest(
    const gfx::Point& point_in_frame_pixels,
    ax::mojom::Event opt_event_to_fire,
    int opt_request_id,
    base::OnceCallback<void(BrowserAccessibilityManager* hit_manager,
                            int hit_node_id)> opt_callback) {
  // This is called by BrowserAccessibilityManager. During teardown it's
  // possible that render_accessibility_ is null but the corresponding
  // BrowserAccessibilityManager still exists and could call this.
  if (IsInactiveAndDisallowReactivation() || !render_accessibility_) {
    if (opt_callback)
      std::move(opt_callback).Run(nullptr, 0);
    return;
  }

  render_accessibility_->HitTest(
      point_in_frame_pixels, opt_event_to_fire, opt_request_id,
      base::BindOnce(&RenderFrameHostImpl::AccessibilityHitTestCallback,
                     weak_ptr_factory_.GetWeakPtr(), opt_request_id,
                     opt_event_to_fire, std::move(opt_callback)));
}

bool RenderFrameHostImpl::AccessibilityIsMainFrame() {
  return is_main_frame();
}

void RenderFrameHostImpl::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  if (base::FeatureList::IsEnabled(features::kCrashReporting))
    MaybeGenerateCrashReport(info.status, info.exit_code);

  // When a frame's process dies, its RenderFrame no longer exists, which means
  // that its child frames must be cleaned up as well.
  ResetChildren();

  // Reset state for the current RenderFrameHost once the FrameTreeNode has been
  // reset.
  SetRenderFrameCreated(false);
  InvalidateMojoConnection();
  document_scoped_interface_provider_receiver_.reset();
  broker_receiver_.reset();
  SetLastCommittedUrl(GURL());
  web_bundle_handle_.reset();

  must_be_replaced_ = ShouldCreateNewHostForCrashedFrame();
  has_committed_any_navigation_ = false;

#if defined(OS_ANDROID)
  // Execute any pending Samsung smart clip callbacks.
  for (base::IDMap<std::unique_ptr<ExtractSmartClipDataCallback>>::iterator
           iter(&smart_clip_callbacks_);
       !iter.IsAtEnd(); iter.Advance()) {
    std::move(*iter.GetCurrentValue())
        .Run(base::string16(), base::string16(), gfx::Rect());
  }
  smart_clip_callbacks_.Clear();
#endif  // defined(OS_ANDROID)

  // Ensure that future remote interface requests are associated with the new
  // process's channel.
  remote_associated_interfaces_.reset();

  // Any termination disablers in content loaded by the new process will
  // be sent again.
  has_before_unload_handler_ = false;
  has_unload_handler_ = false;
  has_pagehide_handler_ = false;
  has_visibilitychange_handler_ = false;

  if (IsPendingDeletion()) {
    // If the process has died, we don't need to wait for the ACK. Complete the
    // deletion immediately.
    SetLifecycleState(LifecycleState::kReadyToBeDeleted);
    DCHECK(children_.empty());
    PendingDeletionCheckCompleted();
    // |this| is deleted. Don't add any more code at this point in the function.
    return;
  }

  // If this was the current pending or speculative RFH dying, cancel and
  // destroy it.
  frame_tree_node_->render_manager()->CancelPendingIfNecessary(this);

  // Note: don't add any more code at this point in the function because
  // |this| may be deleted. Any additional cleanup should happen before
  // the last block of code here.
}

void RenderFrameHostImpl::RenderProcessGone(
    SiteInstanceImpl* site_instance,
    const ChildProcessTerminationInfo& info) {
  DCHECK_EQ(site_instance_.get(), site_instance);

  if (IsInBackForwardCache()) {
    EvictFromBackForwardCacheWithReason(
        info.status == base::TERMINATION_STATUS_PROCESS_CRASHED
            ? BackForwardCacheMetrics::NotRestoredReason::
                  kRendererProcessCrashed
            : BackForwardCacheMetrics::NotRestoredReason::
                  kRendererProcessKilled);
    return;
  }

  if (owned_render_widget_host_)
    owned_render_widget_host_->RendererExited();

  // The renderer process is gone, so this frame can no longer be loading.
  if (navigation_request())
    navigation_request()->set_net_error(net::ERR_ABORTED);
  ResetNavigationRequests();
  ResetLoadingState();

  // Any future UpdateState or UpdateTitle messages from this or a recreated
  // process should be ignored until the next commit.
  set_nav_entry_id(0);

  if (is_audible_)
    OnAudibleStateChanged(false);
}

void RenderFrameHostImpl::ReportContentSecurityPolicyViolation(
    network::mojom::CSPViolationPtr violation_params) {
  GetAssociatedLocalFrame()->ReportContentSecurityPolicyViolation(
      std::move(violation_params));
}

void RenderFrameHostImpl::SanitizeDataForUseInCspViolation(
    bool is_redirect,
    network::mojom::CSPDirectiveName directive,
    GURL* blocked_url,
    network::mojom::SourceLocation* source_location) const {
  DCHECK(blocked_url);
  DCHECK(source_location);
  GURL source_location_url(source_location->url);

  // The main goal of this is to avoid leaking information between potentially
  // separate renderers, in the event of one of them being compromised.
  // See https://crbug.com/633306.
  bool sanitize_blocked_url = true;
  bool sanitize_source_location = true;

  // There is no need to sanitize data when it is same-origin with the current
  // url of the renderer.
  if (url::Origin::Create(*blocked_url)
          .IsSameOriginWith(last_committed_origin_))
    sanitize_blocked_url = false;
  if (url::Origin::Create(source_location_url)
          .IsSameOriginWith(last_committed_origin_))
    sanitize_source_location = false;

  // When a renderer tries to do a form submission, it already knows the url of
  // the blocked url, except when it is redirected.
  if (!is_redirect && directive == network::mojom::CSPDirectiveName::FormAction)
    sanitize_blocked_url = false;

  if (sanitize_blocked_url)
    *blocked_url = blocked_url->GetOrigin();
  if (sanitize_source_location) {
    source_location->url = source_location_url.GetOrigin().spec();
    source_location->line = 0u;
    source_location->column = 0u;
  }
}

void RenderFrameHostImpl::PerformAction(const ui::AXActionData& data) {
  AccessibilityPerformAction(data);
}

bool RenderFrameHostImpl::RequiresPerformActionPointInPixels() const {
  return true;
}

bool RenderFrameHostImpl::SchemeShouldBypassCSP(
    const base::StringPiece& scheme) {
  // Blink uses its SchemeRegistry to check if a scheme should be bypassed.
  // It can't be used on the browser process. It is used for two things:
  // 1) Bypassing the "chrome-extension" scheme when chrome is built with the
  //    extensions support.
  // 2) Bypassing arbitrary scheme for testing purpose only in blink and in V8.
  // TODO(arthursonzogni): url::GetBypassingCSPScheme() is used instead of the
  // blink::SchemeRegistry. It contains 1) but not 2).
  const auto& bypassing_schemes = url::GetCSPBypassingSchemes();
  return base::Contains(bypassing_schemes, scheme);
}

bool RenderFrameHostImpl::CreateRenderFrame(
    int previous_routing_id,
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int parent_routing_id,
    int previous_sibling_routing_id) {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::CreateRenderFrame");
  DCHECK(!IsRenderFrameLive()) << "Creating frame twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!agent_scheduling_group().InitProcessAndMojos())
    return false;

  DCHECK(GetProcess()->IsInitializedAndNotDead());

  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      interface_provider;
  BindInterfaceProviderReceiver(
      interface_provider.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker;
  BindBrowserInterfaceBrokerReceiver(
      browser_interface_broker.InitWithNewPipeAndPassReceiver());

  mojom::CreateFrameParamsPtr params = mojom::CreateFrameParams::New();
  params->interface_bundle = mojom::DocumentScopedInterfaceBundle::New(
      std::move(interface_provider), std::move(browser_interface_broker));

  params->routing_id = routing_id_;
  params->previous_routing_id = previous_routing_id;
  params->opener_frame_token = opener_frame_token;
  params->parent_routing_id = parent_routing_id;
  params->previous_sibling_routing_id = previous_sibling_routing_id;
  params->replication_state = frame_tree_node()->current_replication_state();
  params->frame_token = frame_token_;
  params->devtools_frame_token = frame_tree_node()->devtools_frame_token();

  // Normally, the replication state contains effective frame policy, excluding
  // sandbox flags and feature policy attributes that were updated but have not
  // taken effect. However, a new RenderFrame should use the pending frame
  // policy, since it is being created as part of the navigation that will
  // commit it. (I.e., the RenderFrame needs to know the policy to use when
  // initializing the new document once it commits).
  params->replication_state.frame_policy =
      frame_tree_node()->pending_frame_policy();

  // If we switched BrowsingInstances because of the COOP header, we should
  // clear the frame name. This below informs the renderer at frame creation.
  NavigationRequest* navigation_request =
      frame_tree_node()->navigation_request();
  if (navigation_request &&
      navigation_request->coop_status().require_browsing_instance_swap()) {
    params->replication_state.name = "";
    // "COOP swaps" only affect main frames, that have an empty unique name.
    DCHECK(params->replication_state.unique_name.empty());
  }

  params->frame_owner_properties =
      frame_tree_node()->frame_owner_properties().Clone();

  params->has_committed_real_load =
      frame_tree_node()->has_committed_real_load();

  // The RenderWidgetHost takes ownership of its view. It is tied to the
  // lifetime of the current RenderProcessHost for this RenderFrameHost.
  // TODO(avi): This will need to change to initialize a
  // RenderWidgetHostViewAura for the main frame once RenderViewHostImpl has-a
  // RenderWidgetHostImpl. https://crbug.com/545684
  if (owned_render_widget_host_) {
    DCHECK(parent_);
    DCHECK_NE(parent_routing_id, MSG_ROUTING_NONE);
    blink::ScreenInfo screen_info;
    parent_->GetRenderWidgetHost()->GetScreenInfo(&screen_info);
    RenderWidgetHostView* rwhv = RenderWidgetHostViewChildFrame::Create(
        owned_render_widget_host_.get(), screen_info);
    // The child frame should be created hidden.
    DCHECK(!rwhv->IsShowing());
  }

  if (auto* rwh = GetLocalRenderWidgetHost()) {
    params->widget_params = mojom::CreateFrameWidgetParams::New();
    params->widget_params->routing_id =
        GetLocalRenderWidgetHost()->GetRoutingID();
    params->widget_params->visual_properties =
        GetLocalRenderWidgetHost()->GetInitialVisualProperties();

    std::tie(params->widget_params->widget_host,
             params->widget_params->widget) = rwh->BindNewWidgetInterfaces();
    std::tie(params->widget_params->frame_widget_host,
             params->widget_params->frame_widget) =
        rwh->BindNewFrameWidgetInterfaces();
  }

  // https://crbug.com/1006814. The renderer needs at least one of these IDs to
  // be able to insert the new frame in the frame tree.
  DCHECK(params->previous_routing_id != MSG_ROUTING_NONE ||
         params->parent_routing_id != MSG_ROUTING_NONE);
  agent_scheduling_group().CreateFrame(std::move(params));

  if (previous_routing_id != MSG_ROUTING_NONE) {
    RenderFrameProxyHost* proxy = RenderFrameProxyHost::FromID(
        GetProcess()->GetID(), previous_routing_id);
    // We have also created a RenderFrameProxy in CreateFrame above, so
    // remember that.
    //
    // RenderDocument: |proxy| can be null, when |previous_routing_id| refers to
    // a RenderFrameHost instead of a RenderFrameProxy.
    if (proxy)
      proxy->SetRenderFrameProxyCreated(true);
  }

  // The renderer now has a RenderFrame for this RenderFrameHost.  Note that
  // this path is only used for out-of-process iframes.  Main frame RenderFrames
  // are created with their RenderView, and same-site iframes are created at the
  // time of OnCreateChildFrame.
  SetRenderFrameCreated(true);

  return true;
}

void RenderFrameHostImpl::DeleteRenderFrame(FrameDeleteIntention intent) {
  if (IsPendingDeletion())
    return;

  if (render_frame_created_) {
    Send(new UnfreezableFrameMsg_Delete(routing_id_, intent));

    if (!frame_tree_node_->IsMainFrame() && IsCurrent()) {
      DCHECK_NE(lifecycle_state(), LifecycleState::kSpeculative);
      // If this subframe has unload handlers (and isn't speculative), ensure
      // that they have a chance to execute by delaying process cleanup. This
      // will prevent the process from shutting down immediately in the case
      // where this is the last active frame in the process.
      // See https://crbug.com/852204.
      if (has_unload_handlers()) {
        RenderProcessHostImpl* process =
            static_cast<RenderProcessHostImpl*>(GetProcess());
        process->DelayProcessShutdownForUnload(subframe_unload_timeout_);
      }

      // If the subframe takes too long to unload, force its removal from the
      // tree.  See https://crbug.com/950625.
      subframe_unload_timer_.Start(FROM_HERE, subframe_unload_timeout_, this,
                                   &RenderFrameHostImpl::OnUnloadTimeout);
    }
  }

  LifecycleState lifecycle_state = has_unload_handlers()
                                       ? LifecycleState::kRunningUnloadHandlers
                                       : LifecycleState::kReadyToBeDeleted;
  SetLifecycleState(lifecycle_state);
}

void RenderFrameHostImpl::SetRenderFrameCreated(bool created) {
  // We should not create new RenderFrames while our delegate is being destroyed
  // (e.g., via a WebContentsObserver during WebContents shutdown).  This seems
  // to have caused crashes in https://crbug.com/717650.
  if (created)
    CHECK(!delegate_->IsBeingDestroyed());

  bool was_created = render_frame_created_;
  render_frame_created_ = created;

  // Clear all the user data associated with this RenderFrameHost when its
  // RenderFrame is recreated after a crash. Checking
  // |was_render_frame_ever_created_| guarantees that the user data isn't
  // cleared for the initial RenderFrame creation.  Note that the user data is
  // intentionally not cleared at the time of crash. Please refer to
  // https://crbug.com/1099237 for more details.
  //
  // Clearing of user data should be called before RenderFrameCreated to ensure:
  // - a) new new state set in RenderFrameCreated doesn't get deleted.
  // - b) the old state is not leaked to a new RenderFrameHost.
  if (!was_created && created && was_render_frame_ever_created_)
    document_associated_data_.ClearAllUserData();

  if (created)
    was_render_frame_ever_created_ = true;

  // If the current status is different than the new status, the delegate
  // needs to be notified.
  if (created != was_created) {
    if (created) {
      SetUpMojoIfNeeded();
      delegate_->RenderFrameCreated(this);
    } else {
      delegate_->RenderFrameDeleted(this);
    }
  }
  // TODO(http://crbug.com/1014212): Change to DCHECK.
  if (created)
    CHECK(frame_);

  if (created && GetLocalRenderWidgetHost()) {
    GetLocalRenderWidgetHost()->input_router()->SetFrameTreeNodeId(
        frame_tree_node_->frame_tree_node_id());
    GetLocalRenderWidgetHost()->InitForFrame();
  }

  if (enabled_bindings_ && created)
    GetFrameBindingsControl()->AllowBindings(enabled_bindings_);

  if (web_ui_) {
    if (created) {
      if (enabled_bindings_ & BINDINGS_POLICY_WEB_UI)
        web_ui_->SetupMojoConnection();
    } else {
      web_ui_->InvalidateMojoConnection();
    }
  }
}

void RenderFrameHostImpl::SwapIn() {
  GetNavigationControl()->SwapIn();
}

void RenderFrameHostImpl::Init() {
  ResumeBlockedRequestsForFrame();
  if (!waiting_for_init_)
    return;

  waiting_for_init_ = false;
  if (pending_navigate_) {
    frame_tree_node()->navigator().OnBeginNavigation(
        frame_tree_node(), std::move(pending_navigate_->common_params),
        std::move(pending_navigate_->begin_navigation_params),
        std::move(pending_navigate_->blob_url_loader_factory),
        std::move(pending_navigate_->navigation_client),
        std::move(pending_navigate_->navigation_initiator),
        EnsurePrefetchedSignedExchangeCache(),
        MaybeCreateWebBundleHandleTracker());
    pending_navigate_.reset();
  }
}

void RenderFrameHostImpl::PropagateEmbeddingTokenToParentFrame() {
  // Protect against calls from WebContentsImpl::AttachInnerWebContents() that
  // happen before RFHI::SetEmbeddingToken() gets called, which is where the
  // token gets set. It's safe to early return in those cases since this method
  // will get called anyway by RFHI::SetEmbeddingToken(), at which time the
  // outer delegate for the inner web contents will have been created already.
  if (!embedding_token_)
    return;

  // We need to propagate the token to the parent frame if it's either remote or
  // part of an outer web contents, therefore we need to figure out the right
  // proxy to send the token to, if any.
  // For local parents the propagation occurs within the renderer process. The
  // token is also present on the main frame for generalization when the main
  // frame in embedded in another context (e.g. browser UI). The main frame is
  // not embedded in the context of the frame tree so it is not propagated here.
  // See RenderFrameHost::GetEmbeddingToken for more details.
  RenderFrameProxyHost* target_render_frame_proxy = nullptr;

  if (IsCrossProcessSubframe()) {
    // Cross-process subframes should have a remote parent frame.
    target_render_frame_proxy =
        frame_tree_node()->render_manager()->GetProxyToParent();
    DCHECK(target_render_frame_proxy);
  } else if (frame_tree_node()->IsMainFrame()) {
    // The main frame in an inner web contents could have a delegate in the
    // outer web contents, so we need to account for that as well.
    target_render_frame_proxy =
        frame_tree_node()->render_manager()->GetProxyToOuterDelegate();
  }

  // Propagate the token to the right process, if a proxy was found.
  if (target_render_frame_proxy) {
    target_render_frame_proxy->GetAssociatedRemoteFrame()->SetEmbeddingToken(
        embedding_token_.value());
  }
}

void RenderFrameHostImpl::OnAudibleStateChanged(bool is_audible) {
  DCHECK_NE(is_audible_, is_audible);
  if (is_audible) {
    GetProcess()->OnMediaStreamAdded();
  } else {
    GetProcess()->OnMediaStreamRemoved();
  }
  is_audible_ = is_audible;
  delegate_->OnFrameAudioStateChanged(this, is_audible_);
}

void RenderFrameHostImpl::DidAddMessageToConsole(
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  if (delegate_->DidAddMessageToConsole(log_level, message, line_no,
                                        source_id)) {
    return;
  }

  // Pass through log severity only on builtin components pages to limit console
  // spew.
  const bool is_builtin_component =
      HasWebUIScheme(delegate_->GetMainFrameLastCommittedURL()) ||
      GetContentClient()->browser()->IsBuiltinComponent(
          GetProcess()->GetBrowserContext(), GetLastCommittedOrigin());
  const bool is_off_the_record =
      GetSiteInstance()->GetBrowserContext()->IsOffTheRecord();

  LogConsoleMessage(log_level, message, line_no, is_builtin_component,
                    is_off_the_record, source_id);
}

void RenderFrameHostImpl::OnCreateChildFrame(
    int new_routing_id,
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
        new_interface_provider_provider_receiver,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker_receiver,
    blink::mojom::TreeScopeType scope,
    const std::string& frame_name,
    const std::string& frame_unique_name,
    bool is_created_by_script,
    const base::UnguessableToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const blink::FramePolicy& frame_policy,
    const blink::mojom::FrameOwnerProperties& frame_owner_properties,
    const blink::mojom::FrameOwnerElementType owner_type) {
  // TODO(lukasza): Call ReceivedBadMessage when |frame_unique_name| is empty.
  DCHECK(!frame_unique_name.empty());
  DCHECK(new_interface_provider_provider_receiver.is_valid());
  DCHECK(browser_interface_broker_receiver.is_valid());
  if (owner_type == blink::mojom::FrameOwnerElementType::kNone) {
    // Any child frame must have a HTMLFrameOwnerElement in its parent document
    // and therefore the corresponding type of kNone (specific to main frames)
    // is invalid.
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CHILD_FRAME_NEEDS_OWNER_ELEMENT_TYPE);
  }

  // The RenderFrame corresponding to this host sent an IPC message to create a
  // child, but by the time we get here, it's possible for the RenderFrameHost
  // to become pending deletion, or for its process to have disconnected (maybe
  // due to browser shutdown). Ignore such messages.
  if (IsInactiveAndDisallowReactivation() || !render_frame_created_)
    return;

  // |new_routing_id|, |new_interface_provider_provider_receiver|,
  // |browser_interface_broker_receiver| and |devtools_frame_token| were
  // generated on the browser's IO thread and not taken from the renderer
  // process.
  frame_tree_->AddFrame(this, GetProcess()->GetID(), new_routing_id,
                        std::move(new_interface_provider_provider_receiver),
                        std::move(browser_interface_broker_receiver), scope,
                        frame_name, frame_unique_name, is_created_by_script,
                        frame_token, devtools_frame_token, frame_policy,
                        frame_owner_properties, was_discarded_, owner_type);
}

void RenderFrameHostImpl::DidNavigate(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    NavigationRequest* navigation_request,
    bool did_create_new_document) {
  // Keep track of the last committed URL and origin in the RenderFrameHost
  // itself.  These allow GetLastCommittedURL and GetLastCommittedOrigin to
  // stay correct even if the render_frame_host later becomes pending deletion.
  // The URL is set regardless of whether it's for a net error or not.
  frame_tree_node_->SetCurrentURL(params.url);
  SetLastCommittedOrigin(params.origin);

  // Separately, update the frame's last successful URL except for net error
  // pages, since those do not end up in the correct process after transfers
  // (see https://crbug.com/560511).  Instead, the next cross-process navigation
  // or transfer should decide whether to swap as if the net error had not
  // occurred.
  // TODO(creis): Remove this block and always set the URL.
  // See https://crbug.com/588314.
  if (!params.url_is_unreachable)
    last_successful_url_ = params.url;

  if (did_create_new_document) {
    DidCommitNewDocument(params, navigation_request);

    // Reset the salt so that media device IDs are reset for the new document
    // if necessary.
    media_device_id_salt_base_ =
        BrowserContext::CreateRandomMediaDeviceIDSalt();
  }
}

void RenderFrameHostImpl::SetLastCommittedOrigin(const url::Origin& origin) {
  last_committed_origin_ = origin;
  CSPContext::SetSelf(origin);
}

void RenderFrameHostImpl::SetLastCommittedOriginForTesting(
    const url::Origin& origin) {
  SetLastCommittedOrigin(origin);
}

const url::Origin& RenderFrameHostImpl::ComputeTopFrameOrigin(
    const url::Origin& frame_origin) const {
  if (frame_tree_node_->IsMainFrame()) {
    return frame_origin;
  }

  DCHECK(parent_);
  // It's important to go through parent_ rather than via
  // frame_free_->root() here in case we're in process of being deleted, as the
  // latter might point to what our ancestor is being replaced with rather than
  // the actual ancestor.
  RenderFrameHostImpl* host = parent_;
  while (host->parent_) {
    host = host->parent_;
  }
  return host->GetLastCommittedOrigin();
}

net::IsolationInfo RenderFrameHostImpl::ComputeIsolationInfoForNavigation(
    const GURL& destination) const {
  net::IsolationInfo::RedirectMode redirect_mode =
      frame_tree_node_->IsMainFrame()
          ? net::IsolationInfo::RedirectMode::kUpdateTopFrame
          : net::IsolationInfo::RedirectMode::kUpdateFrameOnly;
  return ComputeIsolationInfoInternal(url::Origin::Create(destination),
                                      redirect_mode);
}

net::SiteForCookies RenderFrameHostImpl::ComputeSiteForCookies() {
  return isolation_info_.site_for_cookies();
}

net::IsolationInfo RenderFrameHostImpl::ComputeIsolationInfoInternal(
    const url::Origin& frame_origin,
    net::IsolationInfo::RedirectMode redirect_mode) const {
  url::Origin top_frame_origin = ComputeTopFrameOrigin(frame_origin);

  net::SiteForCookies candidate_site_for_cookies =
      net::SiteForCookies::FromOrigin(top_frame_origin);

  if (GetContentClient()
          ->browser()
          ->ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
              top_frame_origin.scheme(),
              GURL::SchemeIsCryptographic(frame_origin.scheme()))) {
    return net::IsolationInfo::Create(redirect_mode, top_frame_origin,
                                      frame_origin, candidate_site_for_cookies);
  }

  // Make sure all ancestors have origins consistent with the candidate
  // SiteForCookies of the main document. Otherwise, SameSite cookies may not be
  // used. For frame requests, it's OK to skip checking the frame itself since
  // each request will be validated against |site_for_cookies| anyway.
  for (const RenderFrameHostImpl* rfh = this->parent_; rfh;
       rfh = rfh->parent_) {
    if (!candidate_site_for_cookies.IsEquivalent(
            net::SiteForCookies::FromOrigin(rfh->last_committed_origin_))) {
      return net::IsolationInfo::Create(redirect_mode, top_frame_origin,
                                        frame_origin, net::SiteForCookies());
    }
    candidate_site_for_cookies.MarkIfCrossScheme(rfh->last_committed_origin_);
  }

  // If |redirect_mode| is kUpdateNothing, then IsolationInfo is being computed
  // for subresource requests. In that case, also need to check the
  // SiteForCookies against the frame origin.
  if (redirect_mode == net::IsolationInfo::RedirectMode::kUpdateNothing &&
      !candidate_site_for_cookies.IsEquivalent(
          net::SiteForCookies::FromOrigin(frame_origin))) {
    return net::IsolationInfo::Create(redirect_mode, top_frame_origin,
                                      frame_origin, net::SiteForCookies());
  }

  return net::IsolationInfo::Create(redirect_mode, top_frame_origin,
                                    frame_origin, candidate_site_for_cookies);
}

void RenderFrameHostImpl::SetOriginDependentStateOfNewFrame(
    const url::Origin& new_frame_creator) {
  // This method should only be called for *new* frames, that haven't committed
  // a navigation yet.
  DCHECK(!has_committed_any_navigation_);
  DCHECK(GetLastCommittedOrigin().opaque());
  DCHECK(isolation_info_.IsEmpty());

  // Calculate and set |new_frame_origin|.
  bool new_frame_should_be_sandboxed =
      network::mojom::WebSandboxFlags::kOrigin ==
      (frame_tree_node()->active_sandbox_flags() &
       network::mojom::WebSandboxFlags::kOrigin);
  url::Origin new_frame_origin = new_frame_should_be_sandboxed
                                     ? new_frame_creator.DeriveNewOpaqueOrigin()
                                     : new_frame_creator;
  isolation_info_ = ComputeIsolationInfoInternal(
      new_frame_origin, net::IsolationInfo::RedirectMode::kUpdateNothing);
  SetLastCommittedOrigin(new_frame_origin);

  // Construct the frame's feature policy only once we know its initial
  // committed origin. It's necessary to wait for the origin because the feature
  // policy's state depends on the origin, so the FeaturePolicy object could be
  // configured incorrectly if it were initialized before knowing the value of
  // |last_committed_origin_|. More at crbug.com/1112959.
  ResetFeaturePolicy();
}

FrameTreeNode* RenderFrameHostImpl::AddChild(
    std::unique_ptr<FrameTreeNode> child,
    int process_id,
    int frame_routing_id,
    const base::UnguessableToken& frame_token) {
  // Child frame must always be created in the same process as the parent.
  CHECK_EQ(process_id, GetProcess()->GetID());

  // Initialize the RenderFrameHost for the new node.  We always create child
  // frames in the same SiteInstance as the current frame, and they can swap to
  // a different one if they navigate away.
  child->render_manager()->InitChild(GetSiteInstance(), frame_routing_id,
                                     frame_token);

  // Other renderer processes in this BrowsingInstance may need to find out
  // about the new frame.  Create a proxy for the child frame in all
  // SiteInstances that have a proxy for the frame's parent, since all frames
  // in a frame tree should have the same set of proxies.
  frame_tree_node_->render_manager()->CreateProxiesForChildFrame(child.get());

  // When the child is added, it hasn't committed any navigation yet - its
  // initial empty document should inherit the origin of its parent (the origin
  // may change after the first commit). See also https://crbug.com/932067.
  child->current_frame_host()->SetOriginDependentStateOfNewFrame(
      GetLastCommittedOrigin());

  children_.push_back(std::move(child));
  return children_.back().get();
}

void RenderFrameHostImpl::RemoveChild(FrameTreeNode* child) {
  for (auto iter = children_.begin(); iter != children_.end(); ++iter) {
    if (iter->get() == child) {
      // Subtle: we need to make sure the node is gone from the tree before
      // observers are notified of its deletion.
      std::unique_ptr<FrameTreeNode> node_to_delete(std::move(*iter));
      children_.erase(iter);
      node_to_delete->current_frame_host()->DeleteRenderFrame(
          FrameDeleteIntention::kNotMainFrame);
      // Speculative RenderFrameHosts are deleted by the FrameTreeNode's
      // RenderFrameHostManager's destructor. RenderFrameProxyHosts send
      // UnfreezableFrameMsg_Delete automatically in the destructor.
      // TODO(dcheng): This is horribly confusing. Refactor this logic so it's
      // more understandable.
      node_to_delete.reset();
      PendingDeletionCheckCompleted();
      return;
    }
  }
}

void RenderFrameHostImpl::ResetChildren() {
  // Remove child nodes from the tree, then delete them. This destruction
  // operation will notify observers. See https://crbug.com/612450 for
  // explanation why we don't just call the std::vector::clear method.
  std::vector<std::unique_ptr<FrameTreeNode>> children;
  children.swap(children_);
  // TODO(dcheng): Ideally, this would be done by messaging all the proxies of
  // this RenderFrameHostImpl to detach the current frame's children, rather
  // than messaging each child's current frame host...
  for (auto& child : children)
    child->current_frame_host()->DeleteRenderFrame(
        FrameDeleteIntention::kNotMainFrame);
}

void RenderFrameHostImpl::SetLastCommittedUrl(const GURL& url) {
  last_committed_url_ = url;
}

void RenderFrameHostImpl::UpdateRenderProcessHostFramePriorities() {
  const auto new_committed_document_priority =
      delegate_->IsFrameLowPriority(this)
          ? RenderProcessHostImpl::FramePriority::kLow
          : RenderProcessHostImpl::FramePriority::kNormal;
  if (last_committed_document_priority_ != new_committed_document_priority) {
    GetProcess()->UpdateFrameWithPriority(last_committed_document_priority_,
                                          new_committed_document_priority);
    last_committed_document_priority_ = new_committed_document_priority;
  }
}

void RenderFrameHostImpl::Detach() {
  if (lifecycle_state() == LifecycleState::kSpeculative)
    return;

  if (!parent_) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_DETACH_MAIN_FRAME);
    return;
  }

  // A frame is removed while replacing this document with the new one. When it
  // happens, delete the frame and both the new and old documents. Unload
  // handlers aren't guaranteed to run here.
  if (is_waiting_for_unload_ack_) {
    parent_->RemoveChild(frame_tree_node_);
    return;
  }

  // Ignore Detach Mojo API, if the RenderFrameHost should be left in pending
  // deletion state.
  if (do_not_delete_for_testing_)
    return;

  if (IsPendingDeletion()) {
    // The frame is pending deletion. Detach Mojo API is used to confirm its
    // unload handlers ran. Note that it is possible for a frame to already be
    // in kReadyToBeDeleted. This happens when this RenderFrameHost is pending
    // deletion and is waiting on one of its children to run its unload
    // handler. While running it, it can request its parent to detach itself.
    // See test: SitePerProcessBrowserTest.PartialUnloadHandler.
    if (lifecycle_state_ != LifecycleState::kReadyToBeDeleted)
      SetLifecycleState(LifecycleState::kReadyToBeDeleted);
    PendingDeletionCheckCompleted();  // Can delete |this|.
    return;
  }

  // This frame is being removed by the renderer, and it has already executed
  // its unload handler.
  SetLifecycleState(LifecycleState::kReadyToBeDeleted);

  // Before completing the removal, we still need to wait for all of its
  // descendant frames to execute unload handlers. Start executing those
  // handlers now.
  StartPendingDeletionOnSubtree();
  frame_tree()->FrameUnloading(frame_tree_node_);

  // Some children with no unload handler may be eligible for immediate
  // deletion. Cut the dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();  // Can delete |this|.
}

void RenderFrameHostImpl::DidFailLoadWithError(const GURL& url,
                                               int32_t error_code) {
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::DidFailLoadWithError",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(),
               "error", error_code);

  GURL validated_url(url);
  GetProcess()->FilterURL(false, &validated_url);

  frame_tree_node_->navigator().DidFailLoadWithError(this, validated_url,
                                                     error_code);
}

void RenderFrameHostImpl::DidFocusFrame() {
  // We don't handle this IPC signal for non-active RenderFrameHost.
  //
  // For RenderFrameHost in BackForwardCache, it is safe to ignore this IPC as
  // there is a renderer side check (see Document::IsFocusedAllowed) which
  // returns false.
  if (lifecycle_state_ != LifecycleState::kActive)
    return;

  // We need to handle receiving this IPC from a frame that is inside a portal
  // despite there being a renderer side check (see Document::IsFocusAllowed).
  // This is because the IPC to notify a page that it is inside a portal (see
  // WebContentsImpl::NotifyInsidePortal) may race with portal activation, and
  // we may run into a situation where a frame inside a portal doesn't know it's
  // inside a portal yet and allows focus.
  if (InsidePortal())
    return;

  delegate_->SetFocusedFrame(frame_tree_node_, GetSiteInstance());
}

void RenderFrameHostImpl::DidCallFocus() {
  delegate_->DidCallFocus();
}

void RenderFrameHostImpl::DidAddContentSecurityPolicies(
    std::vector<network::mojom::ContentSecurityPolicyPtr> policies) {
  TRACE_EVENT1("navigation",
               "RenderFrameHostImpl::OnDidAddContentSecurityPolicies",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  std::vector<network::mojom::ContentSecurityPolicyHeaderPtr> headers;
  for (auto& policy : policies) {
    headers.push_back(policy->header.Clone());
    AddContentSecurityPolicy(std::move(policy));
  }
  frame_tree_node()->AddContentSecurityPolicies(std::move(headers));
}

void RenderFrameHostImpl::CancelInitialHistoryLoad() {
  // A Javascript navigation interrupted the initial history load.  Check if an
  // initial subframe cross-process navigation needs to be canceled as a result.
  // TODO(creis, clamy): Cancel any cross-process navigation.
  NOTIMPLEMENTED();
}

void RenderFrameHostImpl::DidChangeActiveSchedulerTrackedFeatures(
    uint64_t features_mask) {
  renderer_reported_scheduler_tracked_features_ = features_mask;

  MaybeEvictFromBackForwardCache();
}

void RenderFrameHostImpl::OnSchedulerTrackedFeatureUsed(
    blink::scheduler::WebSchedulerTrackedFeature feature) {
  browser_reported_scheduler_tracked_features_ |=
      1ull << static_cast<uint64_t>(feature);

  MaybeEvictFromBackForwardCache();
}

bool RenderFrameHostImpl::IsFrozen() {
  // TODO(crbug.com/1081920): Account for non-bfcache freezing here as well.
  return lifecycle_state_ == LifecycleState::kInBackForwardCache;
}

void RenderFrameHostImpl::DidCommitProvisionalLoad(
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  if (MaybeInterceptCommitCallback(nullptr, params.get(), &interface_params)) {
    DidCommitNavigation(std::move(navigation_request_), std::move(params),
                        std::move(interface_params));
  }
}

void RenderFrameHostImpl::DidCommitBackForwardCacheNavigation(
    NavigationRequest* committing_navigation_request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params) {
  auto request = navigation_requests_.find(committing_navigation_request);
  CHECK(request != navigation_requests_.end());

  std::unique_ptr<NavigationRequest> owned_request = std::move(request->second);
  navigation_requests_.erase(committing_navigation_request);

  // During a normal (uncached) navigation, is_loading_ is set to true in
  // CommitNavigation(). When navigating to a document in the BackForwardCache,
  // CommitNavigation() is never called, so we have to set is_loading_ to true
  // ourselves.
  //
  // If is_start_loading_ is set to false, DidCommitNavigationInternal will
  // re-fire the DidStartLoading event, which we don't want since it has already
  // been fired.
  is_loading_ = true;

  DidCommitNavigationInternal(std::move(owned_request), std::move(params),
                              /*is_same_document_navigation=*/false);

  // The page is already loaded since it came from the cache, so fire the stop
  // loading event.
  DidStopLoading();
}

void RenderFrameHostImpl::DidCommitPerNavigationMojoInterfaceNavigation(
    NavigationRequest* committing_navigation_request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  DCHECK(committing_navigation_request);
  committing_navigation_request->IgnoreCommitInterfaceDisconnection();
  if (!MaybeInterceptCommitCallback(committing_navigation_request, params.get(),
                                    &interface_params)) {
    return;
  }

  auto request = navigation_requests_.find(committing_navigation_request);

  // The committing request should be in the map of NavigationRequests for
  // this RenderFrameHost.
  CHECK(request != navigation_requests_.end());

  std::unique_ptr<NavigationRequest> owned_request = std::move(request->second);
  navigation_requests_.erase(committing_navigation_request);
  DidCommitNavigation(std::move(owned_request), std::move(params),
                      std::move(interface_params));
}

void RenderFrameHostImpl::DidCommitSameDocumentNavigation(
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params) {
  ScopedActiveURL scoped_active_url(params->url,
                                    frame_tree()->root()->current_origin());
  ScopedCommitStateResetter commit_state_resetter(this);

  // When the frame is pending deletion, the browser is waiting for it to unload
  // properly. In the meantime, because of race conditions, it might tries to
  // commit a same-document navigation before unloading. Similarly to what is
  // done with cross-document navigations, such navigation are ignored. The
  // browser already committed to destroying this RenderFrameHost.
  // See https://crbug.com/805705 and https://crbug.com/930132.
  // TODO(ahemery): Investigate to see if this can be removed when the
  // NavigationClient interface is implemented.
  // If this is called when the frame is in BackForwardCache, evict the frame
  // to avoid ignoring the renderer-initiated navigation, which the frame
  // might not expect.
  if (IsInactiveAndDisallowReactivation())
    return;

  TRACE_EVENT2("navigation",
               "RenderFrameHostImpl::DidCommitSameDocumentNavigation",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(), "url",
               params->url.possibly_invalid_spec());

  // Check if the navigation matches a stored same-document NavigationRequest.
  // In that case it is browser-initiated.
  bool is_browser_initiated =
      same_document_navigation_request_ &&
      (same_document_navigation_request_->commit_params().navigation_token ==
       params->navigation_token);
  if (!DidCommitNavigationInternal(
          is_browser_initiated ? std::move(same_document_navigation_request_)
                               : nullptr,
          std::move(params), true /* is_same_document_navigation*/)) {
    return;
  }

  // Since we didn't early return, it's safe to keep the commit state.
  commit_state_resetter.disable();
}

RenderWidgetHostImpl* RenderFrameHostImpl::GetRenderWidgetHost() {
  RenderFrameHostImpl* frame = this;
  while (frame) {
    if (frame->GetLocalRenderWidgetHost())
      return frame->GetLocalRenderWidgetHost();
    frame = frame->GetParent();
  }

  NOTREACHED();
  return nullptr;
}

RenderWidgetHostView* RenderFrameHostImpl::GetView() {
  return GetRenderWidgetHost()->GetView();
}

GlobalFrameRoutingId RenderFrameHostImpl::GetGlobalFrameRoutingId() {
  return GlobalFrameRoutingId(GetProcess()->GetID(), GetRoutingID());
}

bool RenderFrameHostImpl::HasPendingCommitNavigation() const {
  return navigation_request_ || same_document_navigation_request_ ||
         !navigation_requests_.empty();
}

void RenderFrameHostImpl::ResetNavigationRequests() {
  navigation_request_.reset();
  same_document_navigation_request_.reset();
  navigation_requests_.clear();
}

void RenderFrameHostImpl::SetNavigationRequest(
    std::unique_ptr<NavigationRequest> navigation_request) {
  DCHECK(navigation_request);
  if (NavigationTypeUtils::IsSameDocument(
          navigation_request->common_params().navigation_type)) {
    same_document_navigation_request_ = std::move(navigation_request);
    return;
  }
  navigation_requests_[navigation_request.get()] =
      std::move(navigation_request);
}

void RenderFrameHostImpl::Unload(RenderFrameProxyHost* proxy, bool is_loading) {
  // The end of this event is in OnUnloadACK when the RenderFrame has completed
  // the operation and sends back an IPC message.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("navigation", "RenderFrameHostImpl::Unload",
                                    TRACE_ID_LOCAL(this), "frame_tree_node",
                                    frame_tree_node_->frame_tree_node_id());

  // If this RenderFrameHost is already pending deletion, it must have already
  // gone through this, therefore just return.
  if (IsPendingDeletion()) {
    NOTREACHED() << "RFH should be in default state when calling Unload.";
    return;
  }

  if (unload_event_monitor_timeout_ && !do_not_delete_for_testing_) {
    unload_event_monitor_timeout_->Start(base::TimeDelta::FromMilliseconds(
        RenderViewHostImpl::kUnloadTimeoutMS));
  }

  // TODO(nasko): If the frame is not live, the RFH should just be deleted by
  // simulating the receipt of unload ack.
  is_waiting_for_unload_ack_ = true;

  if (proxy) {
    SetLifecycleState(LifecycleState::kRunningUnloadHandlers);
    if (IsRenderFrameLive()) {
      Send(new UnfreezableFrameMsg_Unload(
          routing_id_, proxy->GetRoutingID(), is_loading,
          proxy->frame_tree_node()->current_replication_state(),
          proxy->GetFrameToken()));
      // Remember that a RenderFrameProxy was created as part of processing the
      // Unload message above.
      proxy->SetRenderFrameProxyCreated(true);
    }
  } else {
    // RenderDocument: After a local<->local swap, this function is called with
    // a null |proxy|.
    CHECK(ShouldCreateNewHostForSameSiteSubframe());

    // The unload handlers already ran for this document during the
    // local<->local swap. Hence, there is no need to send
    // UnfreezableFrameMsg_Unload here. It can be marked at completed.
    SetLifecycleState(LifecycleState::kReadyToBeDeleted);
  }

  if (web_ui())
    web_ui()->RenderFrameHostUnloading();

  web_bluetooth_services_.clear();

  StartPendingDeletionOnSubtree();
  // Some children with no unload handler may be eligible for deletion. Cut the
  // dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();
  // |this| is potentially deleted. Do not add code after this.
}

void RenderFrameHostImpl::DetachFromProxy() {
  if (IsPendingDeletion())
    return;

  // Start pending deletion on this frame and its children.
  DeleteRenderFrame(FrameDeleteIntention::kNotMainFrame);
  StartPendingDeletionOnSubtree();
  frame_tree()->FrameUnloading(frame_tree_node_);

  // Some children with no unload handler may be eligible for immediate
  // deletion. Cut the dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();  // May delete |this|.
}

void RenderFrameHostImpl::ProcessBeforeUnloadCompleted(
    bool proceed,
    bool treat_as_final_completion_callback,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "navigation", "RenderFrameHostImpl BeforeUnload", TRACE_ID_LOCAL(this),
      "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());
  // If this renderer navigated while the beforeunload request was in flight, we
  // may have cleared this state in DidCommitProvisionalLoad, in which case we
  // can ignore this message.
  // However renderer might also be swapped out but we still want to proceed
  // with navigation, otherwise it would block future navigations. This can
  // happen when pending cross-site navigation is canceled by a second one just
  // before DidCommitProvisionalLoad while current RVH is waiting for commit
  // but second navigation is started from the beginning.
  RenderFrameHostImpl* initiator = GetBeforeUnloadInitiator();
  if (!initiator)
    return;

  // Continue processing the ACK in the frame that triggered beforeunload in
  // this frame.  This could be either this frame itself or an ancestor frame.
  initiator->ProcessBeforeUnloadCompletedFromFrame(
      proceed, treat_as_final_completion_callback, this,
      false /* is_frame_being_destroyed */, renderer_before_unload_start_time,
      renderer_before_unload_end_time);
}

RenderFrameHostImpl* RenderFrameHostImpl::GetBeforeUnloadInitiator() {
  for (RenderFrameHostImpl* frame = this; frame; frame = frame->GetParent()) {
    if (frame->is_waiting_for_beforeunload_completion_)
      return frame;
  }
  return nullptr;
}

void RenderFrameHostImpl::ProcessBeforeUnloadCompletedFromFrame(
    bool proceed,
    bool treat_as_final_completion_callback,
    RenderFrameHostImpl* frame,
    bool is_frame_being_destroyed,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  // Check if we need to wait for more beforeunload completion callbacks. If
  // |proceed| is false, we know the navigation or window close will be aborted,
  // so we don't need to wait for beforeunload completion callbacks from any
  // other frames. |treat_as_final_completion_callback| also indicates that we
  // shouldn't wait for any other ACKs (e.g., when a beforeunload timeout
  // fires).
  if (!proceed || treat_as_final_completion_callback) {
    beforeunload_pending_replies_.clear();
  } else {
    beforeunload_pending_replies_.erase(frame);
    if (!beforeunload_pending_replies_.empty())
      return;
  }

  DCHECK(!send_before_unload_start_time_.is_null());

  // Sets a default value for before_unload_end_time so that the browser
  // survives a hacked renderer.
  base::TimeTicks before_unload_end_time = renderer_before_unload_end_time;
  if (!renderer_before_unload_start_time.is_null() &&
      !renderer_before_unload_end_time.is_null()) {
    base::TimeTicks before_unload_completed_time = base::TimeTicks::Now();

    if (!base::TimeTicks::IsConsistentAcrossProcesses()) {
      // TimeTicks is not consistent across processes and we are passing
      // TimeTicks across process boundaries so we need to compensate for any
      // skew between the processes. Here we are converting the renderer's
      // notion of before_unload_end_time to TimeTicks in the browser process.
      // See comments in inter_process_time_ticks_converter.h for more.
      InterProcessTimeTicksConverter converter(
          LocalTimeTicks::FromTimeTicks(send_before_unload_start_time_),
          LocalTimeTicks::FromTimeTicks(before_unload_completed_time),
          RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time),
          RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
      LocalTimeTicks browser_before_unload_end_time =
          converter.ToLocalTimeTicks(
              RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
      before_unload_end_time = browser_before_unload_end_time.ToTimeTicks();
    }

    base::TimeDelta on_before_unload_overhead_time =
        (before_unload_completed_time - send_before_unload_start_time_) -
        (renderer_before_unload_end_time - renderer_before_unload_start_time);
    UMA_HISTOGRAM_TIMES("Navigation.OnBeforeUnloadOverheadTime",
                        on_before_unload_overhead_time);

    frame_tree_node_->navigator().LogBeforeUnloadTime(
        renderer_before_unload_start_time, renderer_before_unload_end_time,
        send_before_unload_start_time_);
  }

  // Resets beforeunload waiting state.
  is_waiting_for_beforeunload_completion_ = false;
  has_shown_beforeunload_dialog_ = false;
  if (beforeunload_timeout_)
    beforeunload_timeout_->Stop();
  send_before_unload_start_time_ = base::TimeTicks();

  // If the ACK is for a navigation, send it to the Navigator to have the
  // current navigation stop/proceed. Otherwise, send it to the
  // RenderFrameHostManager which handles closing.
  if (unload_ack_is_for_navigation_) {
    frame_tree_node_->navigator().BeforeUnloadCompleted(
        frame_tree_node_, proceed, before_unload_end_time);
  } else {
    // We could reach this from a subframe destructor for |frame| while we're
    // in the middle of closing the current tab.  In that case, dispatch the
    // ACK to prevent re-entrancy and a potential nested attempt to free the
    // current frame.  See https://crbug.com/866382.
    base::OnceClosure task = base::BindOnce(
        [](base::WeakPtr<RenderFrameHostImpl> self,
           const base::TimeTicks& before_unload_end_time, bool proceed) {
          if (!self)
            return;
          self->frame_tree_node()->render_manager()->BeforeUnloadCompleted(
              proceed, before_unload_end_time);
        },
        weak_ptr_factory_.GetWeakPtr(), before_unload_end_time, proceed);
    if (is_frame_being_destroyed) {
      DCHECK(proceed);
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(task));
    } else {
      std::move(task).Run();
    }
  }

  // If canceled, notify the delegate to cancel its pending navigation entry.
  // This is usually redundant with the dialog closure code in WebContentsImpl's
  // OnDialogClosed, but there may be some cases that Blink returns !proceed
  // without showing the dialog. We also update the address bar here to be safe.
  if (!proceed)
    delegate_->DidCancelLoading();
}

bool RenderFrameHostImpl::IsWaitingForUnloadACK() const {
  return render_view_host_->is_waiting_for_page_close_completion_ ||
         is_waiting_for_unload_ack_;
}

bool RenderFrameHostImpl::BeforeUnloadTimedOut() const {
  return beforeunload_timeout_ &&
         (send_before_unload_start_time_ != base::TimeTicks()) &&
         (base::TimeTicks::Now() - send_before_unload_start_time_) >
             beforeunload_timeout_delay_;
}

void RenderFrameHostImpl::OnUnloadACK() {
  if (frame_tree_node_->render_manager()->is_attaching_inner_delegate()) {
    // This RFH was unloaded while attaching an inner delegate. The RFH
    // will stay around but it will no longer be associated with a RenderFrame.
    SetRenderFrameCreated(false);
    return;
  }

  // Ignore spurious unload ack.
  if (!is_waiting_for_unload_ack_)
    return;

  // Ignore OnUnloadACK if the RenderFrameHost should be left in pending
  // deletion state.
  if (do_not_delete_for_testing_)
    return;

  DCHECK_EQ(LifecycleState::kRunningUnloadHandlers, lifecycle_state_);
  SetLifecycleState(LifecycleState::kReadyToBeDeleted);
  PendingDeletionCheckCompleted();  // Can delete |this|.
}

void RenderFrameHostImpl::OnUnloaded() {
  DCHECK(is_waiting_for_unload_ack_);

  TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", "RenderFrameHostImpl::Unload",
                                  TRACE_ID_LOCAL(this));
  if (unload_event_monitor_timeout_)
    unload_event_monitor_timeout_->Stop();

  ClearWebUI();

  bool deleted =
      frame_tree_node_->render_manager()->DeleteFromPendingList(this);
  CHECK(deleted);
}

void RenderFrameHostImpl::DisableUnloadTimerForTesting() {
  unload_event_monitor_timeout_.reset();
}

void RenderFrameHostImpl::SetSubframeUnloadTimeoutForTesting(
    const base::TimeDelta& timeout) {
  subframe_unload_timeout_ = timeout;
}

void RenderFrameHostImpl::OnContextMenu(
    const UntrustworthyContextMenuParams& params) {
  if (IsInactiveAndDisallowReactivation())
    return;

  // Validate the URLs in |params|.  If the renderer can't request the URLs
  // directly, don't show them in the context menu.
  ContextMenuParams validated_params(params);
  validated_params.page_url = GetMainFrame()->GetLastCommittedURL();
  if (GetParent())  // Only populate |frame_url| for subframes.
    validated_params.frame_url = GetLastCommittedURL();

  // We don't validate |unfiltered_link_url| so that this field can be used
  // when users want to copy the original link URL.
  RenderProcessHost* process = GetProcess();
  process->FilterURL(true, &validated_params.link_url);
  process->FilterURL(true, &validated_params.src_url);
  process->FilterURL(false, &validated_params.page_url);
  process->FilterURL(true, &validated_params.frame_url);

  // It is necessary to transform the coordinates to account for nested
  // RenderWidgetHosts, such as with out-of-process iframes.
  gfx::Point original_point(validated_params.x, validated_params.y);
  gfx::Point transformed_point =
      static_cast<RenderWidgetHostViewBase*>(GetView())
          ->TransformPointToRootCoordSpace(original_point);
  validated_params.x = transformed_point.x();
  validated_params.y = transformed_point.y();

  if (validated_params.selection_start_offset < 0) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_NEGATIVE_SELECTION_START_OFFSET);
  }

  delegate_->ShowContextMenu(this, validated_params);
}

#if defined(OS_ANDROID)
void RenderFrameHostImpl::RequestSmartClipExtract(
    ExtractSmartClipDataCallback callback,
    gfx::Rect rect) {
  int32_t callback_id = smart_clip_callbacks_.Add(
      std::make_unique<ExtractSmartClipDataCallback>(std::move(callback)));
  frame_->ExtractSmartClipData(
      rect, base::BindOnce(&RenderFrameHostImpl::OnSmartClipDataExtracted,
                           base::Unretained(this), callback_id));
}

void RenderFrameHostImpl::OnSmartClipDataExtracted(int32_t callback_id,
                                                   const base::string16& text,
                                                   const base::string16& html,
                                                   const gfx::Rect& clip_rect) {
  std::move(*smart_clip_callbacks_.Lookup(callback_id))
      .Run(text, html, clip_rect);
  smart_clip_callbacks_.Remove(callback_id);
}
#endif  // defined(OS_ANDROID)

void RenderFrameHostImpl::RunModalAlertDialog(
    const base::string16& alert_message,
    RunModalAlertDialogCallback response_callback) {
  auto dialog_closed_callback = base::BindOnce(
      [](RunModalAlertDialogCallback response_callback, bool success,
         const base::string16& response) {
        // The response string is unused but we use a generic mechanism for
        // closing the javascript dialog that returns two arguments.
        std::move(response_callback).Run();
      },
      std::move(response_callback));
  RunJavaScriptDialog(alert_message, base::string16(),
                      JAVASCRIPT_DIALOG_TYPE_ALERT,
                      std::move(dialog_closed_callback));
}

void RenderFrameHostImpl::RunModalConfirmDialog(
    const base::string16& alert_message,
    RunModalConfirmDialogCallback response_callback) {
  auto dialog_closed_callback = base::BindOnce(
      [](RunModalConfirmDialogCallback response_callback, bool success,
         const base::string16& response) {
        // The response string is unused but we use a generic mechanism for
        // closing the javascript dialog that returns two arguments.
        std::move(response_callback).Run(success);
      },
      std::move(response_callback));
  RunJavaScriptDialog(alert_message, base::string16(),
                      JAVASCRIPT_DIALOG_TYPE_CONFIRM,
                      std::move(dialog_closed_callback));
}

void RenderFrameHostImpl::RunModalPromptDialog(
    const base::string16& alert_message,
    const base::string16& default_value,
    RunModalPromptDialogCallback response_callback) {
  RunJavaScriptDialog(alert_message, default_value,
                      JAVASCRIPT_DIALOG_TYPE_PROMPT,
                      std::move(response_callback));
}

void RenderFrameHostImpl::RunJavaScriptDialog(
    const base::string16& message,
    const base::string16& default_prompt,
    JavaScriptDialogType dialog_type,
    JavaScriptDialogCallback ipc_response_callback) {
  // Don't show the dialog if it's triggered on a non-active RenderFrameHost.
  // This happens when the RenderFrameHost is pending deletion or in the
  // back-forward cache.
  if (lifecycle_state_ != LifecycleState::kActive) {
    std::move(ipc_response_callback).Run(true, base::string16());
    return;
  }

  // While a JS message dialog is showing, tabs in the same process shouldn't
  // process input events.
  GetProcess()->SetBlocked(true);

  delegate_->RunJavaScriptDialog(
      this, message, default_prompt, dialog_type,
      base::BindOnce(&RenderFrameHostImpl::JavaScriptDialogClosed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(ipc_response_callback)));
}

void RenderFrameHostImpl::RunBeforeUnloadConfirm(
    bool is_reload,
    RunBeforeUnloadConfirmCallback ipc_response_callback) {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OnRunBeforeUnloadConfirm",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  // Allow at most one attempt to show a beforeunload dialog per navigation.
  RenderFrameHostImpl* beforeunload_initiator = GetBeforeUnloadInitiator();
  if (beforeunload_initiator) {
    // If the running beforeunload handler wants to display a dialog and the
    // before-unload type wants to ignore it, then short-circuit the request and
    // respond as if the user decided to stay on the page, canceling the unload.
    if (beforeunload_initiator->beforeunload_dialog_request_cancels_unload_) {
      std::move(ipc_response_callback).Run(/*success=*/false);
      return;
    }

    if (beforeunload_initiator->has_shown_beforeunload_dialog_) {
      // TODO(alexmos): Pass enough data back to renderer to record histograms
      // for Document.BeforeUnloadDialog and add the intervention console
      // message to match renderer-side behavior in
      // Document::DispatchBeforeUnloadEvent().
      std::move(ipc_response_callback).Run(/*success=*/true);
      return;
    }
    beforeunload_initiator->has_shown_beforeunload_dialog_ = true;
  } else {
    // TODO(alexmos): If a renderer-initiated beforeunload shows a dialog, it
    // won't find a |beforeunload_initiator|. This can happen for a
    // renderer-initiated navigation or window.close().  We should ensure that
    // when the browser process later kicks off subframe unload handlers (if
    // any), they won't be able to show additional dialogs. However, we can't
    // just set |has_shown_beforeunload_dialog_| because we don't know which
    // frame is navigating/closing here.  Plumb enough information here to fix
    // this.
  }

  // While a JS beforeunload dialog is showing, tabs in the same process
  // shouldn't process input events.
  GetProcess()->SetBlocked(true);

  // The beforeunload dialog for this frame may have been triggered by a
  // browser-side request to this frame or a frame up in the frame hierarchy.
  // Stop any timers that are waiting.
  for (RenderFrameHostImpl* frame = this; frame; frame = frame->GetParent()) {
    if (frame->beforeunload_timeout_)
      frame->beforeunload_timeout_->Stop();
  }

  auto ipc_callback_wrapper = base::BindOnce(
      [](RunBeforeUnloadConfirmCallback response_callback, bool success,
         const base::string16& response) {
        // The response string is unused but we use a generic mechanism for
        // closing the javascript dialog that returns two arguments.
        std::move(response_callback).Run(success);
      },
      std::move(ipc_response_callback));
  auto dialog_closed_callback = base::BindOnce(
      &RenderFrameHostImpl::JavaScriptDialogClosed,
      weak_ptr_factory_.GetWeakPtr(), std::move(ipc_callback_wrapper));

  delegate_->RunBeforeUnloadConfirm(this, is_reload,
                                    std::move(dialog_closed_callback));
}

void RenderFrameHostImpl::ScaleFactorChanged(float scale) {
  delegate_->OnPageScaleFactorChanged(this, scale);
}

void RenderFrameHostImpl::ContentsPreferredSizeChanged(
    const gfx::Size& pref_size) {
  render_view_host_->OnDidContentsPreferredSizeChange(pref_size);
}

void RenderFrameHostImpl::TextAutosizerPageInfoChanged(
    blink::mojom::TextAutosizerPageInfoPtr page_info) {
  delegate_->OnTextAutosizerPageInfoChanged(this, std::move(page_info));
}

void RenderFrameHostImpl::FocusPage() {
  render_view_host_->OnFocus();
}

void RenderFrameHostImpl::UpdateTargetURL(
    const GURL& url,
    blink::mojom::LocalMainFrameHost::UpdateTargetURLCallback callback) {
  delegate_->UpdateTargetURL(this, url);
  std::move(callback).Run();
}

void RenderFrameHostImpl::UpdateFaviconURL(
    std::vector<blink::mojom::FaviconURLPtr> favicon_urls) {
  delegate_->UpdateFaviconURL(this, std::move(favicon_urls));
}

void RenderFrameHostImpl::DownloadURL(
    blink::mojom::DownloadURLParamsPtr blink_parameters) {
  if (!VerifyDownloadUrlParams(GetSiteInstance(), *blink_parameters))
    return;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("renderer_initiated_download", R"(
        semantics {
          sender: "Download from Renderer"
          description:
            "The frame has either navigated to a URL that was determined to be "
            "a download via one of the renderer's classification mechanisms, "
            "or WebView has requested a <canvas> or <img> element at a "
            "specific location be to downloaded."
          trigger:
            "The user navigated to a destination that was categorized as a "
            "download, or WebView triggered saving a <canvas> or <img> tag."
          data: "Only the URL we are attempting to download."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            DownloadRestrictions {
              DownloadRestrictions: 3
            }
          }
        })");
  std::unique_ptr<download::DownloadUrlParameters> parameters(
      new download::DownloadUrlParameters(blink_parameters->url,
                                          GetProcess()->GetID(),
                                          GetRenderViewHost()->GetRoutingID(),
                                          GetRoutingID(), traffic_annotation));
  parameters->set_content_initiated(true);
  parameters->set_suggested_name(
      blink_parameters->suggested_name.value_or(base::string16()));
  parameters->set_prompt(false);
  parameters->set_cross_origin_redirects(
      blink_parameters->cross_origin_redirects);
  parameters->set_referrer(
      blink_parameters->referrer ? blink_parameters->referrer->url : GURL());
  parameters->set_referrer_policy(Referrer::ReferrerPolicyForUrlRequest(
      blink_parameters->referrer ? blink_parameters->referrer->policy
                                 : network::mojom::ReferrerPolicy::kDefault));
  parameters->set_initiator(
      blink_parameters->initiator_origin.value_or(url::Origin()));
  parameters->set_download_source(download::DownloadSource::FROM_RENDERER);

  if (blink_parameters->data_url_blob) {
    DataURLBlobReader::ReadDataURLFromBlob(
        std::move(blink_parameters->data_url_blob),
        base::BindOnce(&OnDataURLRetrieved, std::move(parameters)));
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (blink_parameters->blob_url_token) {
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            GetStoragePartition(), std::move(blink_parameters->blob_url_token));
  }

  StartDownload(std::move(parameters), std::move(blob_url_loader_factory));
}

void RenderFrameHostImpl::ReportNoBinderForInterface(const std::string& error) {
  broker_receiver_.ReportBadMessage(error + " for the frame/document scope");
}

ukm::SourceId RenderFrameHostImpl::GetPageUkmSourceId() {
  int64_t navigation_id =
      GetMainFrame()->last_committed_cross_document_navigation_id_;
  if (navigation_id == -1)
    return ukm::kInvalidSourceId;
  return ukm::ConvertToSourceId(navigation_id,
                                ukm::SourceIdType::NAVIGATION_ID);
}

BrowserContext* RenderFrameHostImpl::GetBrowserContext() {
  return GetProcess()->GetBrowserContext();
}

// TODO(crbug.com/1091720): Would be better to do this directly in the chrome
// layer.  See referenced bug for further details.
void RenderFrameHostImpl::ReportHeavyAdIssue(
    blink::mojom::HeavyAdResolutionStatus resolution,
    blink::mojom::HeavyAdReason reason) {
  auto issue =
      devtools_instrumentation::GetHeavyAdIssue(this, resolution, reason);
  devtools_instrumentation::ReportBrowserInitiatedIssue(this, issue.get());
}

void RenderFrameHostImpl::AsValueInto(
    base::trace_event::TracedValue* traced_value) {
  traced_value->SetPointer("this", this);
  traced_value->SetInteger("process_id", GetProcess()->GetID());
  traced_value->SetInteger("render_frame_id", GetRoutingID());
  traced_value->SetString("lifecycle_state",
                          LifecycleStateToString(lifecycle_state_));
  traced_value->SetString(
      "origin", base::trace_event::ValueToString(GetLastCommittedOrigin()));
  traced_value->SetString(
      "url", base::trace_event::ValueToString(GetLastCommittedURL()));
  traced_value->SetInteger("frame_tree_node_id",
                           frame_tree_node_->frame_tree_node_id());
  traced_value->SetInteger("site_instance_id", GetSiteInstance()->GetId());
  traced_value->SetInteger("browsing_instance_id",
                           GetSiteInstance()->GetBrowsingInstanceId());
  traced_value->BeginDictionary("parent");
  if (GetParent()) {
    GetParent()->AsValueInto(traced_value);
  }
  traced_value->EndDictionary();
}

StoragePartition* RenderFrameHostImpl::GetStoragePartition() {
  return BrowserContext::GetStoragePartition(GetBrowserContext(),
                                             GetSiteInstance());
}

void RenderFrameHostImpl::RequestTextSurroundingSelection(
    blink::mojom::LocalFrame::GetTextSurroundingSelectionCallback callback,
    int max_length) {
  DCHECK(!callback.is_null());
  GetAssociatedLocalFrame()->GetTextSurroundingSelection(max_length,
                                                         std::move(callback));
}

bool RenderFrameHostImpl::HasCommittingNavigationRequestForOrigin(
    const url::Origin& origin,
    NavigationRequest* navigation_request_to_exclude) {
  if (navigation_request_ &&
      navigation_request_.get() != navigation_request_to_exclude &&
      navigation_request_->HasCommittingOrigin(origin)) {
    return true;
  }

  for (const auto& it : navigation_requests_) {
    NavigationRequest* request = it.first;
    if (request != navigation_request_to_exclude &&
        request->HasCommittingOrigin(origin)) {
      return true;
    }
  }

  // Note: this function excludes |same_document_navigation_request_|, which
  // should be ok since these cannot change the origin.
  return false;
}

void RenderFrameHostImpl::SendInterventionReport(const std::string& id,
                                                 const std::string& message) {
  GetAssociatedLocalFrame()->SendInterventionReport(id, message);
}

WebUI* RenderFrameHostImpl::GetWebUI() {
  return web_ui();
}

void RenderFrameHostImpl::AllowBindings(int bindings_flags) {
  // Never grant any bindings to browser plugin guests.
  if (GetProcess()->IsForGuestsOnly()) {
    NOTREACHED() << "Never grant bindings to a guest process.";
    return;
  }
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::AllowBindings",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(),
               "bindings flags", bindings_flags);

  int webui_bindings = bindings_flags & kWebUIBindingsPolicyMask;

  // TODO(nasko): Ensure callers that specify non-zero WebUI bindings are
  // doing so on a RenderFrameHost that has WebUI associated with it.

  // The bindings being granted here should not differ from the bindings that
  // the associated WebUI requires.
  if (web_ui_)
    CHECK_EQ(web_ui_->GetBindings(), webui_bindings);

  // Ensure we aren't granting WebUI bindings to a process that has already
  // been used for non-privileged views.
  if (webui_bindings && GetProcess()->IsInitializedAndNotDead() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID())) {
    // This process has no bindings yet. Make sure it does not have more
    // than this single active view.
    // --single-process only has one renderer.
    if (GetProcess()->GetActiveViewCount() > 1 &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSingleProcess))
      return;
  }

  if (webui_bindings) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
        GetProcess()->GetID(), webui_bindings);
  }

  enabled_bindings_ |= bindings_flags;

  if (render_frame_created_) {
    GetFrameBindingsControl()->AllowBindings(enabled_bindings_);
    if (web_ui_ && enabled_bindings_ & BINDINGS_POLICY_WEB_UI)
      web_ui_->SetupMojoConnection();
  }
}

int RenderFrameHostImpl::GetEnabledBindings() {
  return enabled_bindings_;
}

void RenderFrameHostImpl::SetWebUIProperty(const std::string& name,
                                           const std::string& value) {
  // WebUI allows to register SetProperties only for the main frame.
  if (GetParent())
    return;

  // This is a sanity check before telling the renderer to enable the property.
  // It could lie and send the corresponding IPC messages anyway, but we will
  // not act on them if enabled_bindings_ doesn't agree. If we get here without
  // WebUI bindings, terminate the renderer process.
  if (enabled_bindings_ & BINDINGS_POLICY_WEB_UI)
    web_ui_->SetProperty(name, value);
  else
    ReceivedBadMessage(GetProcess(), bad_message::RVH_WEB_UI_BINDINGS_MISMATCH);
}

void RenderFrameHostImpl::DisableBeforeUnloadHangMonitorForTesting() {
  beforeunload_timeout_.reset();
}

bool RenderFrameHostImpl::IsBeforeUnloadHangMonitorDisabledForTesting() {
  return !beforeunload_timeout_;
}

void RenderFrameHostImpl::DoNotDeleteForTesting() {
  do_not_delete_for_testing_ = true;
}

void RenderFrameHostImpl::ResumeDeletionForTesting() {
  do_not_delete_for_testing_ = false;
}

void RenderFrameHostImpl::DetachForTesting() {
  do_not_delete_for_testing_ = false;
  RenderFrameHostImpl::Detach();
}

bool RenderFrameHostImpl::IsFeatureEnabled(
    blink::mojom::FeaturePolicyFeature feature) {
  return feature_policy_ && feature_policy_->IsFeatureEnabledForOrigin(
                                feature, GetLastCommittedOrigin());
}

bool RenderFrameHostImpl::IsFeatureEnabled(
    blink::mojom::DocumentPolicyFeature feature) {
  blink::mojom::PolicyValueType feature_type =
      blink::GetDocumentPolicyFeatureInfoMap().at(feature).default_value.Type();
  return IsFeatureEnabled(
      feature, blink::PolicyValue::CreateMaxPolicyValue(feature_type));
}

bool RenderFrameHostImpl::IsFeatureEnabled(
    blink::mojom::DocumentPolicyFeature feature,
    blink::PolicyValue threshold_value) {
  return document_policy_ &&
         document_policy_->IsFeatureEnabled(feature, threshold_value);
}

void RenderFrameHostImpl::ViewSource() {
  delegate_->ViewSource(this);
}

void RenderFrameHostImpl::FlushNetworkAndNavigationInterfacesForTesting() {
  DCHECK(network_service_disconnect_handler_holder_);
  network_service_disconnect_handler_holder_.FlushForTesting();  // IN-TEST

  if (!navigation_control_)
    GetNavigationControl();
  DCHECK(navigation_control_);
  navigation_control_.FlushForTesting();  // IN-TEST
}

void RenderFrameHostImpl::PrepareForInnerWebContentsAttach(
    PrepareForInnerWebContentsAttachCallback callback) {
  frame_tree_node_->render_manager()->PrepareForInnerDelegateAttach(
      std::move(callback));
}

void RenderFrameHostImpl::UpdateSubresourceLoaderFactories() {
  // We only send loader factory bundle upon navigation, so
  // bail out if the frame hasn't committed any yet.
  if (!has_committed_any_navigation_)
    return;
  DCHECK(!IsOutOfProcessNetworkService() ||
         network_service_disconnect_handler_holder_.is_bound());

  NavigationRequest* latest_nav_request_still_committing =
      FindLatestNavigationRequestThatIsStillCommitting();
  mojo::PendingRemote<network::mojom::URLLoaderFactory> default_factory_remote;
  bool bypass_redirect_checks = false;
  if (recreate_default_url_loader_factory_after_network_service_crash_) {
    bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
        CreateURLLoaderFactoryParamsForMainWorld(
            latest_nav_request_still_committing,
            "RFHI::UpdateSubresourceLoaderFactories"),
        base::UkmSourceId::FromInt64(
            latest_nav_request_still_committing
                ? latest_nav_request_still_committing->GetNextPageUkmSourceId()
                : GetPageUkmSourceId()),
        default_factory_remote.InitWithNewPipeAndPassReceiver());
  }

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories =
          std::make_unique<blink::PendingURLLoaderFactoryBundle>(
              std::move(default_factory_remote),
              blink::PendingURLLoaderFactoryBundle::SchemeMap(),
              CreateURLLoaderFactoriesForIsolatedWorlds(
                  latest_nav_request_still_committing,
                  isolated_worlds_requiring_separate_url_loader_factory_),
              bypass_redirect_checks);
  GetNavigationControl()->UpdateSubresourceLoaderFactories(
      std::move(subresource_loader_factories));
}

blink::mojom::FrameOwnerElementType
RenderFrameHostImpl::GetFrameOwnerElementType() {
  return frame_tree_node_->frame_owner_element_type();
}

bool RenderFrameHostImpl::HasTransientUserActivation() {
  return frame_tree_node_->HasTransientUserActivation();
}

void RenderFrameHostImpl::NotifyUserActivation(
    blink::mojom::UserActivationNotificationType notification_type) {
  GetAssociatedLocalFrame()->NotifyUserActivation(notification_type);
}

void RenderFrameHostImpl::DidAccessInitialDocument() {
  delegate_->DidAccessInitialDocument();
}

void RenderFrameHostImpl::DidChangeName(const std::string& name,
                                        const std::string& unique_name) {
  if (GetParent() != nullptr) {
    // TODO(lukasza): Call ReceivedBadMessage when |unique_name| is empty.
    DCHECK(!unique_name.empty());
  }
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::OnDidChangeName",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(),
               "name length", name.length());

  std::string old_name = frame_tree_node()->frame_name();
  frame_tree_node()->SetFrameName(name, unique_name);
  if (old_name.empty() && !name.empty())
    frame_tree_node_->render_manager()->CreateProxiesForNewNamedFrame();
  delegate_->DidChangeName(this, name);
}

void RenderFrameHostImpl::DidSetFramePolicyHeaders(
    network::mojom::WebSandboxFlags sandbox_flags,
    const blink::ParsedFeaturePolicy& feature_policy_header,
    const blink::DocumentPolicyFeatureState& document_policy_header) {
  // TODO(https://crbug.com/1093268): Investigate why this IPC can be received
  // before the navigation commit. This can be triggered when loading an error
  // page using the test:
  // CrossOriginOpenerPolicyBrowserTest.NetworkErrorOnSandboxedPopups.
  if (lifecycle_state() == LifecycleState::kSpeculative)
    return;

  // We should not be updating policy headers when the RenderFrameHost is in
  // BackForwardCache. If this is called when the RenderFrameHost is in
  // BackForwardCache, evict the document.
  if (IsInactiveAndDisallowReactivation())
    return;

  // We shouldn't update policy headers for non-current frames.
  DCHECK(IsCurrent());

  // Rebuild |feature_policy_| for this frame.
  ResetFeaturePolicy();
  feature_policy_->SetHeaderPolicy(feature_policy_header);

  // Rebuild |document_policy_| for this frame.
  // Note: document_policy_header is the document policy state used to
  // initialize |document_policy_| in SecurityContext on renderer side. It is
  // supposed to be compatible with required_document_policy. If not, kill the
  // renderer.
  if (blink::DocumentPolicy::IsPolicyCompatible(
          frame_tree_node()->effective_frame_policy().required_document_policy,
          document_policy_header)) {
    document_policy_ = blink::DocumentPolicy::CreateWithHeaderPolicy(
        {document_policy_header, {} /* endpoint_map */});
  } else {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_BAD_DOCUMENT_POLICY_HEADER);
    return;
  }

  // Update the feature policy and sandbox flags in the frame tree. This will
  // send any updates to proxies if necessary.
  //
  // Feature policy's inheritance from parent frame's feature policy is through
  // accessing parent frame's security context(either remote or local) when
  // initializing child's security context, so the update to proxies is needed.
  //
  // Document policy's inheritance from parent frame's required document policy
  // is done at |HTMLFrameOwnerElement::UpdateRequiredPolicy|. Parent frame owns
  // both parent required document policy and child frame's frame owner element
  // which contains child's required document policy, so there is no need to
  // store required document policy in proxies.
  frame_tree_node()->UpdateFramePolicyHeaders(sandbox_flags,
                                              feature_policy_header);

  // Save a copy of the now-active sandbox flags on this RFHI.
  active_sandbox_flags_ = frame_tree_node()->active_sandbox_flags();

  CheckSandboxFlags();
}

void RenderFrameHostImpl::EnforceInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {
  frame_tree_node()->SetInsecureRequestPolicy(policy);
}

void RenderFrameHostImpl::EnforceInsecureNavigationsSet(
    const std::vector<uint32_t>& set) {
  frame_tree_node()->SetInsecureNavigationsSet(set);
}

RenderFrameHostImpl* RenderFrameHostImpl::FindAndVerifyChild(
    int32_t child_frame_routing_id,
    bad_message::BadMessageReason reason) {
  auto child_frame_or_proxy = LookupRenderFrameHostOrProxy(
      GetProcess()->GetID(), child_frame_routing_id);
  return FindAndVerifyChildInternal(child_frame_or_proxy, reason);
}

RenderFrameHostImpl* RenderFrameHostImpl::FindAndVerifyChild(
    const base::UnguessableToken& child_frame_token,
    bad_message::BadMessageReason reason) {
  auto child_frame_or_proxy =
      LookupRenderFrameHostOrProxy(GetProcess()->GetID(), child_frame_token);
  return FindAndVerifyChildInternal(child_frame_or_proxy, reason);
}

RenderFrameHostImpl* RenderFrameHostImpl::FindAndVerifyChildInternal(
    RenderFrameHostOrProxy child_frame_or_proxy,
    bad_message::BadMessageReason reason) {
  // A race can result in |child| to be nullptr. Avoid killing the renderer in
  // that case.
  if (!child_frame_or_proxy)
    return nullptr;

  if (child_frame_or_proxy.GetFrameTreeNode()->frame_tree() != frame_tree()) {
    // Ignore the cases when the child lives in a different frame tree.
    // This is possible when we create a proxy for inner WebContents (e.g.
    // for portals) so the |child_frame_or_proxy| points to the root frame
    // of the nested WebContents, which is in a different tree.
    // TODO(altimin, lfg): Reconsider what the correct behaviour here should be.
    return nullptr;
  }

  if (child_frame_or_proxy.GetFrameTreeNode()->parent() != this) {
    bad_message::ReceivedBadMessage(GetProcess(), reason);
    return nullptr;
  }
  return child_frame_or_proxy.proxy
             ? child_frame_or_proxy.proxy->frame_tree_node()
                   ->current_frame_host()
             : child_frame_or_proxy.frame;
}

void RenderFrameHostImpl::UpdateTitle(
    const base::Optional<::base::string16>& title,
    base::i18n::TextDirection title_direction) {
  // This message should only be sent for top-level frames.
  if (!is_main_frame())
    return;

  base::string16 received_title;
  if (title.has_value())
    received_title = title.value();

  if (received_title.length() > blink::mojom::kMaxTitleChars) {
    mojo::ReportBadMessage("Renderer sent too many characters in title.");
    return;
  }

  delegate_->UpdateTitle(this, received_title, title_direction);
}

void RenderFrameHostImpl::UpdateEncoding(const std::string& encoding_name) {
  // This message is only sent for top-level frames. TODO(avi): when frame tree
  // mirroring works correctly, add a check here to enforce it.
  if (encoding_name == last_reported_encoding_)
    return;
  last_reported_encoding_ = encoding_name;

  canonical_encoding_ =
      base::GetCanonicalEncodingNameByAliasName(encoding_name);
}

void RenderFrameHostImpl::FrameSizeChanged(const gfx::Size& frame_size) {
  frame_size_ = frame_size;
  delegate_->FrameSizeChanged(this, frame_size);
}

void RenderFrameHostImpl::FullscreenStateChanged(bool is_fullscreen) {
  if (IsInactiveAndDisallowReactivation())
    return;
  delegate_->FullscreenStateChanged(this, is_fullscreen);
}

void RenderFrameHostImpl::RegisterProtocolHandler(const std::string& scheme,
                                                  const GURL& url,
                                                  const base::string16& title,
                                                  bool user_gesture) {
  delegate_->RegisterProtocolHandler(this, scheme, url, title, user_gesture);
}

void RenderFrameHostImpl::UnregisterProtocolHandler(const std::string& scheme,
                                                    const GURL& url,
                                                    bool user_gesture) {
  delegate_->UnregisterProtocolHandler(this, scheme, url, user_gesture);
}

void RenderFrameHostImpl::DidDisplayInsecureContent() {
  delegate_->DidDisplayInsecureContent();
}

void RenderFrameHostImpl::DidContainInsecureFormAction() {
  delegate_->DidContainInsecureFormAction();
}

void RenderFrameHostImpl::DocumentAvailableInMainFrame(
    bool uses_temporary_zoom_level) {
  if (!frame_tree_node_->IsMainFrame()) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_INVALID_CALL_FROM_NOT_MAIN_FRAME);
    return;
  }
  delegate_->DocumentAvailableInMainFrame();

  if (!uses_temporary_zoom_level)
    return;

#if !defined(OS_ANDROID)
  HostZoomMapImpl* host_zoom_map =
      static_cast<HostZoomMapImpl*>(HostZoomMap::Get(GetSiteInstance()));
  host_zoom_map->SetTemporaryZoomLevel(GetProcess()->GetID(),
                                       render_view_host()->GetRoutingID(),
                                       host_zoom_map->GetDefaultZoomLevel());
#endif  // !defined(OS_ANDROID)
}

void RenderFrameHostImpl::SetNeedsOcclusionTracking(bool needs_tracking) {
  // Do not update the parent on behalf of inactive RenderFrameHost. See also
  // https://crbug.com/972566.
  if (IsInactiveAndDisallowReactivation())
    return;

  RenderFrameProxyHost* proxy =
      frame_tree_node()->render_manager()->GetProxyToParent();
  if (!proxy) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }

  proxy->GetAssociatedRemoteFrame()->SetNeedsOcclusionTracking(needs_tracking);
}

void RenderFrameHostImpl::SetVirtualKeyboardOverlayPolicy(
    bool vk_overlays_content) {
  should_virtual_keyboard_overlay_content_ = vk_overlays_content;
}

bool RenderFrameHostImpl::ShouldVirtualKeyboardOverlayContent() const {
  RenderFrameHostImpl* root_frame_host =
      frame_tree_->root()->current_frame_host();
  return root_frame_host->should_virtual_keyboard_overlay_content_;
}

void RenderFrameHostImpl::NotifyVirtualKeyboardOverlayRect(
    const gfx::Rect& keyboard_rect) {
  DCHECK(ShouldVirtualKeyboardOverlayContent());

  RenderFrameHostImpl* root_frame_host =
      frame_tree_->root()->current_frame_host();
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      root_frame_host->render_view_host_->GetWidget()->GetView());
  if (!view)
    return;

  gfx::PointF root_widget_origin(0.f, 0.f);
  view->TransformPointToRootSurface(&root_widget_origin);

  gfx::Rect root_widget_rect;
  if (!keyboard_rect.IsEmpty()) {
    // If the rect is non-empty, we need to transform it to be widget-relative
    // window (DIP coordinates). The input is client coordinates for the root
    // window.
    // Transform the widget rect origin to root relative coords.
    root_widget_rect = gfx::Rect(root_widget_origin.x(), root_widget_origin.y(),
                                 view->GetViewBounds().width(),
                                 view->GetViewBounds().height());

    // Intersect with the keyboard rect and transform back to widget-relative
    // coordinates, which will be sent to the renderer.
    root_widget_rect.Intersect(keyboard_rect);
    root_widget_rect.Offset(-root_widget_origin.x(), -root_widget_origin.y());
  }

  // Notify each SiteInstance a single time. Renderer will take care of ensuring
  // the event is dispatched to all relevant listeners in the grouping of frames
  // for the SiteInstance.
  // TODO(snianu): Transform from the main frame's coordinates to each
  // individual frame client coordinates so that these are more usable from
  // within iframes.
  std::set<SiteInstance*> notified_instances;
  for (RenderFrameHostImpl* node = this; node; node = node->GetParent()) {
    SiteInstance* site_instance = node->GetSiteInstance();
    if (base::Contains(notified_instances, site_instance))
      continue;

    node->GetAssociatedLocalFrame()->NotifyVirtualKeyboardOverlayRect(
        root_widget_rect);
    notified_instances.insert(site_instance);
  }
}

#if defined(OS_ANDROID)
void RenderFrameHostImpl::UpdateUserGestureCarryoverInfo() {
  delegate_->UpdateUserGestureCarryoverInfo();
}
#endif

void RenderFrameHostImpl::VisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  visibility_ = visibility;
}

void RenderFrameHostImpl::DidChangeThemeColor(
    const base::Optional<SkColor>& theme_color) {
  render_view_host_->OnThemeColorChanged(this, theme_color);
}

void RenderFrameHostImpl::DidChangeBackgroundColor(
    const SkColor& background_color) {
  render_view_host_->DidChangeBackgroundColor(this, background_color);
}

void RenderFrameHostImpl::SetCommitCallbackInterceptorForTesting(
    CommitCallbackInterceptor* interceptor) {
  // This DCHECK's aims to avoid unexpected replacement of an interceptor.
  // If this becomes a legitimate use case, feel free to remove.
  DCHECK(!commit_callback_interceptor_ || !interceptor);
  commit_callback_interceptor_ = interceptor;
}

void RenderFrameHostImpl::DidBlockNavigation(
    const GURL& blocked_url,
    const GURL& initiator_url,
    blink::mojom::NavigationBlockedReason reason) {
  delegate_->OnDidBlockNavigation(blocked_url, initiator_url, reason);
}

void RenderFrameHostImpl::DidChangeLoadProgress(double load_progress) {
  frame_tree_node_->DidChangeLoadProgress(load_progress);
}

void RenderFrameHostImpl::DidFinishLoad(const GURL& validated_url) {
  delegate_->OnDidFinishLoad(this, validated_url);
}

void RenderFrameHostImpl::DispatchLoad() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::DispatchLoad",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  // Don't forward the load event to the parent on behalf of inactive
  // RenderFrameHost. This can happen in a race where this inactive
  // RenderFrameHost finishes loading just after the frame navigates away.
  // See https://crbug.com/626802.
  if (IsInactiveAndDisallowReactivation())
    return;

  // We should never be receiving this message from a speculative RFH.
  DCHECK(IsCurrent());

  // Only frames with an out-of-process parent frame should be sending this
  // message.
  RenderFrameProxyHost* proxy =
      frame_tree_node()->render_manager()->GetProxyToParent();
  if (!proxy) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }

  proxy->GetAssociatedRemoteFrame()->DispatchLoadEventForFrameOwner();
}

void RenderFrameHostImpl::GoToEntryAtOffset(int32_t offset,
                                            bool has_user_gesture) {
  delegate_->OnGoToEntryAtOffset(this, offset, has_user_gesture);
}

void RenderFrameHostImpl::HandleAccessibilityFindInPageResult(
    blink::mojom::FindInPageResultAXParamsPtr params) {
  // Only update FindInPageResult on active RenderFrameHost. Note that, it is
  // safe to ignore this call for BackForwardCache, as we terminate the
  // FindInPage session once the page enters BackForwardCache.
  if (lifecycle_state_ != LifecycleState::kActive)
    return;

  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs)) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager) {
      manager->OnFindInPageResult(params->request_id, params->match_index,
                                  params->start_id, params->start_offset,
                                  params->end_id, params->end_offset);
    }
  }
}

void RenderFrameHostImpl::HandleAccessibilityFindInPageTermination() {
  // Only update FindInPageTermination on active RenderFrameHost. Note that, it
  // is safe to ignore this call for BackForwardCache, as we terminate the
  // FindInPage session once the page enters BackForwardCache.
  if (lifecycle_state_ != LifecycleState::kActive)
    return;

  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs)) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager)
      manager->OnFindInPageTermination();
  }
}

void RenderFrameHostImpl::DocumentOnLoadCompleted() {
  // This message is only sent for top-level frames.
  //
  // TODO(avi): when frame tree mirroring works correctly, add a check here
  // to enforce it.
  delegate_->DocumentOnLoadCompleted(this);
}

void RenderFrameHostImpl::ForwardResourceTimingToParent(
    blink::mojom::ResourceTimingInfoPtr timing) {
  // Don't forward the resource timing of the parent on behalf of inactive
  // RenderFrameHost. This can happen in a race where this RenderFrameHost
  // finishes loading just after the frame navigates away. See
  // https://crbug.com/626802.
  if (IsInactiveAndDisallowReactivation())
    return;

  // We should never be receiving this message from a speculative RFH.
  DCHECK(IsCurrent());

  RenderFrameProxyHost* proxy =
      frame_tree_node()->render_manager()->GetProxyToParent();
  if (!proxy) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }
  proxy->GetAssociatedRemoteFrame()->AddResourceTimingFromChild(
      std::move(timing));
}

RenderWidgetHostViewBase* RenderFrameHostImpl::GetViewForAccessibility() {
  return static_cast<RenderWidgetHostViewBase*>(
      frame_tree_node_->IsMainFrame()
          ? render_view_host_->GetWidget()->GetView()
          : GetMainFrame()->render_view_host_->GetWidget()->GetView());
}

void RenderFrameHostImpl::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  if (frame_)
    frame_->UpdateBrowserControlsState(constraints, current, animate);
}

bool RenderFrameHostImpl::Reload() {
  NavigationControllerImpl* controller = static_cast<NavigationControllerImpl*>(
      frame_tree_node_->navigator().GetController());
  return controller->ReloadFrame(frame_tree_node_);
}

void RenderFrameHostImpl::SendAccessibilityEventsToManager(
    const AXEventNotificationDetails& details) {
  if (browser_accessibility_manager_ &&
      !browser_accessibility_manager_->OnAccessibilityEvents(details)) {
    // OnAccessibilityEvents returns false in IPC error conditions
    AccessibilityFatalError();
  }
}

bool RenderFrameHostImpl::IsInactiveAndDisallowReactivation() {
  switch (lifecycle_state_) {
    case LifecycleState::kRunningUnloadHandlers:
    case LifecycleState::kReadyToBeDeleted:
      return true;
    case LifecycleState::kInBackForwardCache:
      EvictFromBackForwardCacheWithReason(
          BackForwardCacheMetrics::NotRestoredReason::kIgnoreEventAndEvict);
      return true;
    case LifecycleState::kSpeculative:
      // We do not expect speculative RenderFrameHosts to generate events that
      // require an active/inactive check. Don't crash the browser process in
      // case it comes from a compromised renderer, but kill the renderer to
      // avoid further confusion.
      bad_message::ReceivedBadMessage(
          GetProcess(), bad_message::RFH_INACTIVE_CHECK_FROM_SPECULATIVE_RFH);
      return false;
    case LifecycleState::kActive:
      return false;
  }
}

void RenderFrameHostImpl::EvictFromBackForwardCache() {
  // TODO(hajimehoshi): This function should take the reason from the renderer
  // side.
  EvictFromBackForwardCacheWithReason(
      BackForwardCacheMetrics::NotRestoredReason::kJavaScriptExecution);
}

void RenderFrameHostImpl::EvictFromBackForwardCacheWithReason(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  BackForwardCacheCanStoreDocumentResult can_store;
  can_store.No(reason);
  EvictFromBackForwardCacheWithReasons(can_store);
}

void RenderFrameHostImpl::EvictFromBackForwardCacheWithReasons(
    const BackForwardCacheCanStoreDocumentResult& can_store) {
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::EvictFromBackForwardCache",
               "can_store", can_store.ToString(), "rfh", this);
  DCHECK(IsBackForwardCacheEnabled());

  if (is_evicted_from_back_forward_cache_)
    return;

  bool in_back_forward_cache = IsInBackForwardCache();

  RenderFrameHostImpl* top_document = this;
  while (top_document->parent_) {
    top_document = top_document->parent_;
    DCHECK_EQ(top_document->IsInBackForwardCache(), in_back_forward_cache);
  }

  // TODO(hajimehoshi): Record the 'race condition' by JavaScript execution when
  // |in_back_forward_cache| is false.
  BackForwardCacheMetrics* metrics = top_document->GetBackForwardCacheMetrics();
  if (in_back_forward_cache && metrics)
    metrics->MarkNotRestoredWithReason(can_store);

  if (!in_back_forward_cache) {
    TRACE_EVENT0("navigation", "BackForwardCache_EvictAfterDocumentRestored");

    BackForwardCacheMetrics::RecordEvictedAfterDocumentRestored(
        BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason::
            kByJavaScript);

    // A document is evicted from the BackForwardCache, but it has already been
    // restored. The current document should be reloaded, because it is not
    // salvageable.
    frame_tree()->controller()->Reload(ReloadType::NORMAL, false);
    return;
  }

  // Check if there is an in-flight navigation restoring the frame that
  // is being evicted.
  NavigationRequest* in_flight_navigation_request =
      top_document->frame_tree_node()->navigation_request();
  bool is_navigation_to_evicted_frame_in_flight =
      (in_flight_navigation_request &&
       in_flight_navigation_request->rfh_restored_from_back_forward_cache() ==
           top_document);

  if (is_navigation_to_evicted_frame_in_flight) {
    // If we are currently navigating to the frame that was just evicted, we
    // must restart the navigation. This is important because restarting the
    // navigation deletes the NavigationRequest associated with the evicted
    // frame (preventing use-after-free).
    // This should also happen asynchronously as eviction might happen in the
    // middle of another navigation — we should not try to restart the
    // navigation in that case.
    // NOTE: Here we rely on the PostTask inside this function running before
    // the task posted to destroy the evicted frames below.
    in_flight_navigation_request->RestartBackForwardCachedNavigation();
  }

  // Evict the frame and schedule it to be destroyed. Eviction happens
  // immediately, but destruction is delayed, so that callers don't have to
  // worry about use-after-free of |this|.
  top_document->is_evicted_from_back_forward_cache_ = true;
  frame_tree()
      ->controller()
      ->GetBackForwardCache()
      .PostTaskToDestroyEvictedFrames();
}

bool RenderFrameHostImpl::HasSeenRecentXrOverlaySetup() {
  static constexpr base::TimeDelta kMaxInterval =
      base::TimeDelta::FromSeconds(1);
  base::TimeDelta delta = base::TimeTicks::Now() - last_xr_overlay_setup_time_;
  DVLOG(2) << __func__ << ": return " << (delta <= kMaxInterval);
  return delta <= kMaxInterval;
}

void RenderFrameHostImpl::SetIsXrOverlaySetup() {
  DVLOG(2) << __func__;
  last_xr_overlay_setup_time_ = base::TimeTicks::Now();
}

// TODO(alexmos): When the allowFullscreen flag is known in the browser
// process, use it to double-check that fullscreen can be entered here.
void RenderFrameHostImpl::EnterFullscreen(
    blink::mojom::FullscreenOptionsPtr options,
    EnterFullscreenCallback callback) {
  // Consume the user activation when entering fullscreen mode in the browser
  // side when the renderer is compromised and the fullscreen request is denied.
  // Fullscreen can only be triggered by: a user activation, a user-generated
  // screen orientation change, or another feature-specific transient allowance.
  // CanEnterFullscreenWithoutUserActivation is only ever true in tests, to
  // allow fullscreen when mocking screen orientation changes.
  // TODO(lanwei): Investigate whether we can terminate the renderer when the
  // user activation has already been consumed.
  if (!delegate_->HasSeenRecentScreenOrientationChange() &&
      !WindowPlacementAllowsFullscreen() && !HasSeenRecentXrOverlaySetup() &&
      !GetContentClient()
           ->browser()
           ->CanEnterFullscreenWithoutUserActivation()) {
    bool is_consumed = frame_tree_node_->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kConsumeTransientActivation,
        blink::mojom::UserActivationNotificationType::kNone);
    if (!is_consumed) {
      DLOG(ERROR) << "Cannot enter fullscreen because there is no transient "
                  << "user activation, orientation change, or XR overlay.";
      std::move(callback).Run(/*granted=*/false);
      return;
    }
  }

  if (!delegate_->CanEnterFullscreenMode()) {
    std::move(callback).Run(/*granted=*/false);
    return;
  }
  std::move(callback).Run(/*granted=*/true);

  // Entering fullscreen from a cross-process subframe also affects all
  // renderers for ancestor frames, which will need to apply fullscreen CSS to
  // appropriate ancestor <iframe> elements, fire fullscreenchange events, etc.
  // Thus, walk through the ancestor chain of this frame and for each (parent,
  // child) pair, send a message about the pending fullscreen change to the
  // child's proxy in parent's SiteInstance. The renderer process will use this
  // to find the <iframe> element in the parent frame that will need fullscreen
  // styles. This is done at most once per SiteInstance: for example, with a
  // A-B-A-B hierarchy, if the bottom frame goes fullscreen, this only needs to
  // notify its parent, and Blink-side logic will take care of applying
  // necessary changes to the other two ancestors.
  std::set<SiteInstance*> notified_instances;
  notified_instances.insert(GetSiteInstance());
  for (RenderFrameHostImpl* rfh = this; rfh->GetParent();
       rfh = rfh->GetParent()) {
    SiteInstance* parent_site_instance = rfh->GetParent()->GetSiteInstance();
    if (base::Contains(notified_instances, parent_site_instance))
      continue;

    RenderFrameProxyHost* child_proxy =
        rfh->frame_tree_node()->render_manager()->GetRenderFrameProxyHost(
            parent_site_instance);
    child_proxy->GetAssociatedRemoteFrame()->WillEnterFullscreen(
        options.Clone());
    notified_instances.insert(parent_site_instance);
  }

  delegate_->EnterFullscreenMode(this, *options);
  delegate_->FullscreenStateChanged(this, true /* is_fullscreen */);

  // The previous call might change the fullscreen state. We need to make sure
  // the renderer is aware of that, which is done via the resize message.
  // Typically, this will be sent as part of the call on the |delegate_| above
  // when resizing the native windows, but sometimes fullscreen can be entered
  // without causing a resize, so we need to ensure that the resize message is
  // sent in that case. We always send this to the main frame's widget, and if
  // there are any OOPIF widgets, this will also trigger them to resize via
  // frameRectsChanged.
  render_view_host_->GetWidget()->SynchronizeVisualProperties();
}

// TODO(alexmos): When the allowFullscreen flag is known in the browser
// process, use it to double-check that fullscreen can be entered here.
void RenderFrameHostImpl::ExitFullscreen() {
  delegate_->ExitFullscreenMode(/* will_cause_resize */ true);

  // The previous call might change the fullscreen state. We need to make sure
  // the renderer is aware of that, which is done via the resize message.
  // Typically, this will be sent as part of the call on the |delegate_| above
  // when resizing the native windows, but sometimes fullscreen can be entered
  // without causing a resize, so we need to ensure that the resize message is
  // sent in that case. We always send this to the main frame's widget, and if
  // there are any OOPIF widgets, this will also trigger them to resize via
  // frameRectsChanged.
  render_view_host_->GetWidget()->SynchronizeVisualProperties();
}

void RenderFrameHostImpl::SuddenTerminationDisablerChanged(
    bool present,
    blink::mojom::SuddenTerminationDisablerType disabler_type) {
  switch (disabler_type) {
    case blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler:
      DCHECK_NE(has_before_unload_handler_, present);
      has_before_unload_handler_ = present;
      break;
    case blink::mojom::SuddenTerminationDisablerType::kPageHideHandler:
      DCHECK_NE(has_pagehide_handler_, present);
      has_pagehide_handler_ = present;
      break;
    case blink::mojom::SuddenTerminationDisablerType::kUnloadHandler:
      DCHECK_NE(has_unload_handler_, present);
      has_unload_handler_ = present;
      break;
    case blink::mojom::SuddenTerminationDisablerType::kVisibilityChangeHandler:
      DCHECK_NE(has_visibilitychange_handler_, present);
      has_visibilitychange_handler_ = present;
      break;
  }
}

bool RenderFrameHostImpl::GetSuddenTerminationDisablerState(
    blink::mojom::SuddenTerminationDisablerType disabler_type) {
  if (do_not_delete_for_testing_ &&
      disabler_type !=
          blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler) {
    return true;
  }
  switch (disabler_type) {
    case blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler:
      return has_before_unload_handler_;
    case blink::mojom::SuddenTerminationDisablerType::kPageHideHandler:
      return has_pagehide_handler_;
    case blink::mojom::SuddenTerminationDisablerType::kUnloadHandler:
      return has_unload_handler_;
    case blink::mojom::SuddenTerminationDisablerType::kVisibilityChangeHandler:
      return has_visibilitychange_handler_;
  }
}

bool RenderFrameHostImpl::UnloadHandlerExistsInSameSiteInstanceSubtree() {
  DCHECK(!GetParent());
  auto* main_frame_site_instance = GetSiteInstance();
  for (auto* subframe : GetFramesInSubtree()) {
    auto* rfhi = static_cast<RenderFrameHostImpl*>(subframe);
    if (rfhi->GetSiteInstance() == main_frame_site_instance &&
        rfhi->has_unload_handler_) {
      return true;
    }
  }
  return false;
}

bool RenderFrameHostImpl::InsidePortal() {
  return GetRenderViewHost()->GetDelegate()->IsPortal();
}

void RenderFrameHostImpl::DidFinishDocumentLoad() {
  dom_content_loaded_ = true;
  delegate_->DOMContentLoaded(this);
}

void RenderFrameHostImpl::FocusedElementChanged(
    bool is_editable_element,
    const gfx::Rect& bounds_in_frame_widget,
    blink::mojom::FocusType focus_type) {
  if (!GetView())
    return;

  has_focused_editable_element_ = is_editable_element;
  // First convert the bounds to root view.
  delegate_->OnFocusedElementChangedInFrame(
      this,
      gfx::Rect(GetView()->TransformPointToRootCoordSpace(
                    bounds_in_frame_widget.origin()),
                bounds_in_frame_widget.size()),
      focus_type);
}

void RenderFrameHostImpl::TextSelectionChanged(const base::string16& text,
                                               uint32_t offset,
                                               const gfx::Range& range) {
  has_selection_ = !text.empty();
  GetRenderWidgetHost()->SelectionChanged(text, offset, range);
}

void RenderFrameHostImpl::DidReceiveFirstUserActivation() {
  delegate_->DidReceiveFirstUserActivation(this);
}

void RenderFrameHostImpl::UpdateUserActivationState(
    blink::mojom::UserActivationUpdateType update_type,
    blink::mojom::UserActivationNotificationType notification_type) {
  // Don't update UserActivationState for non-active RenderFrameHost. In case
  // of BackForwardCache, this is only called for tests and it is safe to ignore
  // such requests.
  if (lifecycle_state_ != LifecycleState::kActive)
    return;

  frame_tree_node_->UpdateUserActivationState(update_type, notification_type);
}

void RenderFrameHostImpl::HadStickyUserActivationBeforeNavigationChanged(
    bool value) {
  frame_tree_node_->OnSetHadStickyUserActivationBeforeNavigation(value);
}

void RenderFrameHostImpl::ScrollRectToVisibleInParentFrame(
    const gfx::Rect& rect_to_scroll,
    blink::mojom::ScrollIntoViewParamsPtr params) {
  RenderFrameProxyHost* proxy =
      frame_tree_node_->render_manager()->GetProxyToParent();
  if (!proxy)
    return;
  proxy->ScrollRectToVisible(rect_to_scroll, std::move(params));
}

void RenderFrameHostImpl::BubbleLogicalScrollInParentFrame(
    blink::mojom::ScrollDirection direction,
    ui::ScrollGranularity granularity) {
  // Do not update the parent on behalf of inactive RenderFrameHost.
  if (IsInactiveAndDisallowReactivation())
    return;

  RenderFrameProxyHost* proxy =
      frame_tree_node()->render_manager()->GetProxyToParent();
  if (!proxy) {
    // Only frames with an out-of-process parent frame should be sending this
    // message.
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }

  proxy->GetAssociatedRemoteFrame()->BubbleLogicalScroll(direction,
                                                         granularity);
}

void RenderFrameHostImpl::RenderFallbackContentInParentProcess() {
  bool is_object_type =
      frame_tree_node()->current_replication_state().frame_owner_element_type ==
      blink::mojom::FrameOwnerElementType::kObject;
  if (!is_object_type) {
    // Only object elements are expected to render their own fallback content
    // and since the owner type is set at the creation time of the
    // FrameTreeNode, this received IPC makes no sense here.
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CANNOT_RENDER_FALLBACK_CONTENT);
    return;
  }

  // The ContentFrame() of the owner element in parent process could be either
  // a frame or a proxy. When navigating cross-site from a frame which is same-
  // site with its parent, the frame is still local (e.g., about:blank).
  // However, navigating from an origin which is cross-site with parent, the
  // frame of the owner is a proxy.
  auto* rfh = frame_tree_node()->current_frame_host();
  if (rfh->GetSiteInstance() == rfh->GetParent()->GetSiteInstance()) {
    rfh->GetAssociatedLocalFrame()->RenderFallbackContent();
  } else if (auto* proxy =
                 frame_tree_node()->render_manager()->GetProxyToParent()) {
    proxy->GetAssociatedRemoteFrame()->RenderFallbackContent();
  }
}

void RenderFrameHostImpl::ShowPopupMenu(
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int32_t item_height,
    double font_size,
    int32_t selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  if (delegate()->ShowPopupMenu(this, &popup_client, bounds, item_height,
                                font_size, selected_item, &menu_items,
                                right_aligned, allow_multiple_selection)) {
    return;
  }

  auto* view = render_view_host()->delegate_->GetDelegateView();
  if (!view)
    return;

  gfx::Point original_point(bounds.x(), bounds.y());
  gfx::Point transformed_point =
      static_cast<RenderWidgetHostViewBase*>(GetView())
          ->TransformPointToRootCoordSpace(original_point);
  gfx::Rect transformed_bounds(transformed_point.x(), transformed_point.y(),
                               bounds.width(), bounds.height());
  view->ShowPopupMenu(this, std::move(popup_client), transformed_bounds,
                      item_height, font_size, selected_item,
                      std::move(menu_items), right_aligned,
                      allow_multiple_selection);
#endif
}

void RenderFrameHostImpl::DidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& http_method,
    const std::string& mime_type,
    network::mojom::RequestDestination request_destination) {
  delegate_->DidLoadResourceFromMemoryCache(this, url, http_method, mime_type,
                                            request_destination);
}

void RenderFrameHostImpl::DidChangeFrameOwnerProperties(
    const base::UnguessableToken& child_frame_token,
    blink::mojom::FrameOwnerPropertiesPtr properties) {
  auto* child =
      FindAndVerifyChild(child_frame_token, bad_message::RFH_OWNER_PROPERTY);
  if (!child)
    return;

  bool has_display_none_property_changed =
      properties->is_display_none !=
      child->frame_tree_node()->frame_owner_properties().is_display_none;

  child->frame_tree_node()->set_frame_owner_properties(*properties);

  child->frame_tree_node()->render_manager()->OnDidUpdateFrameOwnerProperties(
      *properties);
  if (has_display_none_property_changed) {
    delegate_->DidChangeDisplayState(
        child, properties->is_display_none /* is_display_none */);
  }
}

void RenderFrameHostImpl::DidChangeOpener(
    const base::Optional<base::UnguessableToken>& opener_frame_token) {
  frame_tree_node_->render_manager()->DidChangeOpener(opener_frame_token,
                                                      GetSiteInstance());
}

void RenderFrameHostImpl::DidChangeCSPAttribute(
    const base::UnguessableToken& child_frame_token,
    network::mojom::ContentSecurityPolicyPtr parsed_csp_attribute) {
  auto* child =
      FindAndVerifyChild(child_frame_token, bad_message::RFH_CSP_ATTRIBUTE);
  if (!child)
    return;

  child->frame_tree_node()->set_csp_attribute(std::move(parsed_csp_attribute));
}

void RenderFrameHostImpl::DidChangeFramePolicy(
    const base::UnguessableToken& child_frame_token,
    const blink::FramePolicy& frame_policy) {
  // Ensure that a frame can only update sandbox flags or feature policy for its
  // immediate children.  If this is not the case, the renderer is considered
  // malicious and is killed.
  RenderFrameHostImpl* child = FindAndVerifyChild(
      // TODO(iclelland): Rename this message
      child_frame_token, bad_message::RFH_SANDBOX_FLAGS);
  if (!child)
    return;

  child->frame_tree_node()->SetPendingFramePolicy(frame_policy);

  // Notify the RenderFrame if it lives in a different process from its parent.
  // The frame's proxies in other processes also need to learn about the updated
  // flags and policy, but these notifications are sent later in
  // RenderFrameHostManager::CommitPendingFramePolicy(), when the frame
  // navigates and the new policies take effect.
  if (child->GetSiteInstance() != GetSiteInstance()) {
    child->GetAssociatedLocalFrame()->DidUpdateFramePolicy(frame_policy);
  }
}

void RenderFrameHostImpl::CapturePaintPreviewOfSubframe(
    const gfx::Rect& clip_rect,
    const base::UnguessableToken& guid) {
  if (IsInactiveAndDisallowReactivation())
    return;
  // This should only be called on a subframe.
  if (is_main_frame()) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_SUBFRAME_CAPTURE_ON_MAIN_FRAME);
    return;
  }

  delegate()->CapturePaintPreviewOfCrossProcessSubframe(clip_rect, guid, this);
}

void RenderFrameHostImpl::BindInterfaceProviderReceiver(
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
        interface_provider_receiver) {
  DCHECK(!document_scoped_interface_provider_receiver_.is_bound());
  DCHECK(interface_provider_receiver.is_valid());
  document_scoped_interface_provider_receiver_.Bind(
      std::move(interface_provider_receiver));
  document_scoped_interface_provider_receiver_.SetFilter(
      std::make_unique<ActiveURLMessageFilter>(this));
}

void RenderFrameHostImpl::BindBrowserInterfaceBrokerReceiver(
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
  DCHECK(receiver.is_valid());
  broker_receiver_.Bind(std::move(receiver));
  broker_receiver_.SetFilter(std::make_unique<ActiveURLMessageFilter>(this));
}

void RenderFrameHostImpl::BindDomOperationControllerHostReceiver(
    mojo::PendingAssociatedReceiver<mojom::DomAutomationControllerHost>
        receiver) {
  DCHECK(receiver.is_valid());
  dom_automation_controller_receiver_.Bind(std::move(receiver));
  dom_automation_controller_receiver_.SetFilter(
      CreateMessageFilterForAssociatedReceiver(
          mojom::DomAutomationControllerHost::Name_));
}

void RenderFrameHostImpl::SetKeepAliveTimeoutForTesting(
    base::TimeDelta timeout) {
  keep_alive_timeout_ = timeout;
  if (keep_alive_handle_factory_)
    keep_alive_handle_factory_->SetTimeout(keep_alive_timeout_);
}

void RenderFrameHostImpl::ShowCreatedWindow(int pending_widget_routing_id,
                                            WindowOpenDisposition disposition,
                                            const gfx::Rect& initial_rect,
                                            bool user_gesture) {
  delegate_->ShowCreatedWindow(this, pending_widget_routing_id, disposition,
                               initial_rect, user_gesture);
}

void RenderFrameHostImpl::UpdateState(const PageState& state) {
  // TODO(creis): Verify the state's ISN matches the last committed FNE.

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!CanAccessFilesOfPageState(state)) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CAN_ACCESS_FILES_OF_PAGE_STATE);
    return;
  }

  delegate_->UpdateStateForFrame(this, state);
}

void RenderFrameHostImpl::OpenURL(mojom::OpenURLParamsPtr params) {
  // Verify and unpack the Mojo payload.
  GURL validated_url;
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (!VerifyOpenURLParams(GetSiteInstance(), params, &validated_url,
                           &blob_url_loader_factory)) {
    return;
  }

  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OpenURL", "url",
               validated_url.possibly_invalid_spec());

  frame_tree_node_->navigator().RequestOpenURL(
      this, validated_url,
      GlobalFrameRoutingId(GetProcess()->GetID(), params->initiator_routing_id),
      params->initiator_origin, params->post_body, params->extra_headers,
      params->referrer.To<content::Referrer>(), params->disposition,
      params->should_replace_current_entry, params->user_gesture,
      params->triggering_event_info, params->href_translate,
      std::move(blob_url_loader_factory), params->impression);
}

void RenderFrameHostImpl::DidStopLoading() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::DidStopLoading",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  // This method should never be called when the frame is not loading.
  // Unfortunately, it can happen if a history navigation happens during a
  // BeforeUnload or Unload event.
  // TODO(fdegans): Change this to a DCHECK after LoadEventProgress has been
  // refactored in Blink. See crbug.com/466089
  if (!is_loading_)
    return;

  was_discarded_ = false;
  is_loading_ = false;

  // If we have a PeakGpuMemoryTrack, close it as loading as stopped. It will
  // asynchronously receive the statistics from the GPU process, and update
  // UMA stats.
  if (loading_mem_tracker_)
    loading_mem_tracker_.reset();

  // Only inform the FrameTreeNode of a change in load state if the load state
  // of this RenderFrameHost is being tracked.
  if (!IsPendingDeletion())
    frame_tree_node_->DidStopLoading();
}

void RenderFrameHostImpl::GetSavableResourceLinksCallback(
    blink::mojom::GetSavableResourceLinksReplyPtr reply) {
  if (!reply) {
    delegate_->SavableResourceLinksError(this);
    return;
  }

  delegate_->SavableResourceLinksResponse(this, reply->resources_list,
                                          std::move(reply->referrer),
                                          reply->subframes);
}

void RenderFrameHostImpl::DomOperationResponse(const std::string& json_string) {
  delegate_->DomOperationResponse(json_string);
}

std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
RenderFrameHostImpl::CreateCrossOriginPrefetchLoaderFactoryBundle() {
  network::mojom::URLLoaderFactoryParamsPtr factory_params =
      URLLoaderFactoryParamsHelper::CreateForPrefetch(
          this, mojo::Clone(last_committed_client_security_state_));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  bool bypass_redirect_checks = false;
  // Passing an empty IsolationInfo ensures the factory is not initialized with
  // a IsolationInfo. This is necessary for a cross-origin prefetch factory
  // because the factory must use the value provided by requests going through
  // it.
  bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
      std::move(factory_params),
      // This is for renderer prefetches, so it's associated with this page, not
      // pending navigation.
      base::UkmSourceId::FromInt64(GetPageUkmSourceId()),
      pending_default_factory.InitWithNewPipeAndPassReceiver());

  return std::make_unique<blink::PendingURLLoaderFactoryBundle>(
      std::move(pending_default_factory),
      blink::PendingURLLoaderFactoryBundle::SchemeMap(),
      CreateURLLoaderFactoriesForIsolatedWorlds(
          FindLatestNavigationRequestThatIsStillCommitting(),
          isolated_worlds_requiring_separate_url_loader_factory_),
      bypass_redirect_checks);
}

base::WeakPtr<RenderFrameHostImpl> RenderFrameHostImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void RenderFrameHostImpl::CreateNewWindow(
    mojom::CreateNewWindowParamsPtr params,
    CreateNewWindowCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::CreateNewWindow",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(), "url",
               params->target_url.possibly_invalid_spec());

  bool no_javascript_access = false;

  // Filter out URLs to which navigation is disallowed from this context.
  //
  // Note that currently, "javascript:" URLs and empty strings (both of which
  // are legal arguments to window.open) make it here; FilterURL rewrites them
  // to "about:blank" -- they shouldn't be cancelled.
  GetProcess()->FilterURL(false, &params->target_url);

  bool effective_transient_activation_state =
      params->allow_popup || frame_tree_node_->HasTransientUserActivation();

  // Ignore window creation when sent from a frame that's not current or
  // created.
  bool can_create_window =
      IsCurrent() && render_frame_created_ &&
      GetContentClient()->browser()->CanCreateWindow(
          this, GetLastCommittedURL(), GetMainFrame()->GetLastCommittedURL(),
          last_committed_origin_, params->window_container_type,
          params->target_url, params->referrer.To<Referrer>(),
          params->frame_name, params->disposition, *params->features,
          effective_transient_activation_state, params->opener_suppressed,
          &no_javascript_access);

  bool was_consumed = false;
  if (can_create_window) {
    // Consume activation even w/o User Activation v2, to sync other renderers
    // with calling renderer.
    was_consumed = frame_tree_node_->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kConsumeTransientActivation,
        blink::mojom::UserActivationNotificationType::kNone);
  } else {
    std::move(callback).Run(mojom::CreateNewWindowStatus::kIgnore, nullptr);
    return;
  }

  // For Android WebView, we support a pop-up like behavior for window.open()
  // even if the embedding app doesn't support multiple windows. In this case,
  // window.open() will return "window" and navigate it to whatever URL was
  // passed.
  if (!GetOrCreateWebPreferences().supports_multiple_windows) {
    // See crbug.com/1083819, we should ignore if the URL is javascript: scheme,
    // previously we already filtered out javascript: scheme and replace the
    // URL with |kBlockedURL|, so we check against |kBlockedURL| here.
    if (params->target_url == GURL(kBlockedURL)) {
      std::move(callback).Run(mojom::CreateNewWindowStatus::kIgnore, nullptr);
    } else {
      std::move(callback).Run(mojom::CreateNewWindowStatus::kReuse, nullptr);
    }
    return;
  }

  // This will clone the sessionStorage for namespace_id_to_clone.
  StoragePartition* storage_partition = BrowserContext::GetStoragePartition(
      GetSiteInstance()->GetBrowserContext(), GetSiteInstance());
  DOMStorageContextWrapper* dom_storage_context =
      static_cast<DOMStorageContextWrapper*>(
          storage_partition->GetDOMStorageContext());

  scoped_refptr<SessionStorageNamespaceImpl> cloned_namespace;
  if (!params->clone_from_session_storage_namespace_id.empty()) {
    cloned_namespace = SessionStorageNamespaceImpl::CloneFrom(
        dom_storage_context, params->session_storage_namespace_id,
        params->clone_from_session_storage_namespace_id);
  } else {
    cloned_namespace = SessionStorageNamespaceImpl::Create(
        dom_storage_context, params->session_storage_namespace_id);
  }

  network::CrossOriginOpenerPolicy popup_coop;
  network::CrossOriginEmbedderPolicy popup_coep;
  // On popup creation, if the opener and the openers's top-level document
  // are same origin, then the popup's initial empty document inherits its
  // COOP policy from the opener's top-level document. See
  // https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e#model
  RenderFrameHostImpl* top_level_opener = GetMainFrame();
  // Verify that they are same origin.
  if (top_level_opener->GetLastCommittedOrigin().IsSameOriginWith(
          GetLastCommittedOrigin())) {
    popup_coop = top_level_opener->cross_origin_opener_policy();
  } else {
    // The documents are cross origin, leave COOP of the popup to the default
    // unsafe-none.
    // Then set the popup to noopener if the top level COOP is same origin.
    if (top_level_opener->cross_origin_opener_policy().value ==
        network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin) {
      DCHECK(base::FeatureList::IsEnabled(
          network::features::kCrossOriginOpenerPolicy));
      params->opener_suppressed = true;
      // The frame name should not be forwarded to a noopener popup.
      // TODO(https://crbug.com/1060691) This should be applied to all
      // popups opened with noopener.
      params->frame_name.clear();
    }
  }

  // The initial empty document in the popup inherits the COEP of its opener (if
  // any).
  if (!params->opener_suppressed)
    popup_coep = cross_origin_embedder_policy();

  int popup_virtual_browsing_context_group =
      params->opener_suppressed
          ? CrossOriginOpenerPolicyReporter::NextVirtualBrowsingContextGroup()
          : top_level_opener->virtual_browsing_context_group();

  // If the opener is suppressed or script access is disallowed, we should
  // open the window in a new BrowsingInstance, and thus a new process. That
  // means the current renderer process will not be able to route messages to
  // it. Because of this, we will immediately show and navigate the window
  // in OnCreateNewWindowOnUI, using the params provided here.
  bool is_new_browsing_instance =
      params->opener_suppressed || no_javascript_access;

  DCHECK(IsRenderFrameLive());

  // The non-owning pointer |new_window| is valid in this stack frame since
  // nothing can delete it until this thread is freed up again.
  RenderFrameHostDelegate* new_window =
      delegate_->CreateNewWindow(this, *params, is_new_browsing_instance,
                                 was_consumed, cloned_namespace.get());

  if (is_new_browsing_instance || !new_window) {
    // Opener suppressed, Javascript access disabled, or delegate did not
    // provide a handle to any windows it created. In these cases, never tell
    // the renderer about the new window.
    std::move(callback).Run(mojom::CreateNewWindowStatus::kIgnore, nullptr);
    return;
  }

  RenderFrameHostImpl* main_frame = new_window->GetMainFrame();

  // When the popup is created, it hasn't committed any navigation yet - its
  // initial empty document should inherit the origin of its opener (the origin
  // may change after the first commit). See also https://crbug.com/932067.
  //
  // Note that that origin of the new frame might depend on sandbox flags.
  // Checking sandbox flags of the new frame should be safe at this point,
  // because the flags should be already inherited by the CreateNewWindow call
  // above.
  main_frame->SetOriginDependentStateOfNewFrame(GetLastCommittedOrigin());
  main_frame->cross_origin_opener_policy_ = popup_coop;
  main_frame->cross_origin_embedder_policy_ = popup_coep;
  main_frame->virtual_browsing_context_group_ =
      popup_virtual_browsing_context_group;

  // If inheriting coop (checking this via |opener_suppressed|) and the original
  // coop page has a reporter we make sure the the newly created popup also has
  // a reporter.
  if (!params->opener_suppressed && GetMainFrame()->coop_reporter()) {
    main_frame->set_coop_reporter(
        std::make_unique<CrossOriginOpenerPolicyReporter>(
            GetProcess()->GetStoragePartition(), GetLastCommittedURL(),
            params->referrer->url, popup_coop));
  }

  if (main_frame->waiting_for_init_) {
    // Need to check |waiting_for_init_| as some paths inside CreateNewWindow
    // call above (eg if WebContentsDelegate::IsWebContentsCreationOverridden()
    // returns true) will resume requests by calling RenderFrameHostImpl::Init.
    main_frame->frame_->BlockRequests();
  }

  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      main_frame_interface_provider_info;
  main_frame->BindInterfaceProviderReceiver(
      main_frame_interface_provider_info.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker;
  main_frame->BindBrowserInterfaceBrokerReceiver(
      browser_interface_broker.InitWithNewPipeAndPassReceiver());

  mojo::PendingAssociatedRemote<blink::mojom::FrameWidget> blink_frame_widget;
  mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
      blink_frame_widget_receiver =
          blink_frame_widget.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>
      blink_frame_widget_host;
  mojo::PendingAssociatedReceiver<blink::mojom::FrameWidgetHost>
      blink_frame_widget_host_receiver =
          blink_frame_widget_host.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget;
  mojo::PendingAssociatedReceiver<blink::mojom::Widget> blink_widget_receiver =
      blink_widget.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<blink::mojom::WidgetHost> blink_widget_host;
  mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
      blink_widget_host_receiver =
          blink_widget_host.InitWithNewEndpointAndPassReceiver();

  // With this path, RenderViewHostImpl::CreateRenderView is never called
  // because RenderView is already created on the renderer side. Thus we need to
  // establish the connection here.
  mojo::PendingAssociatedRemote<blink::mojom::PageBroadcast> page_broadcast;
  mojo::PendingAssociatedReceiver<blink::mojom::PageBroadcast>
      page_broadcast_receiver =
          page_broadcast.InitWithNewEndpointAndPassReceiver();

  // TODO(danakj): The main frame's RenderWidgetHost has no RenderWidgetHostView
  // yet here. It seems like it should though? In the meantime we send some
  // nonsense with a semi-valid but incorrect ScreenInfo (it needs a
  // RenderWidgetHostView to be correct). An updates VisualProperties will get
  // to the RenderWidget eventually.
  blink::VisualProperties visual_properties;
  main_frame->GetLocalRenderWidgetHost()->GetScreenInfo(
      &visual_properties.screen_info);
  main_frame->GetLocalRenderWidgetHost()->BindFrameWidgetInterfaces(
      std::move(blink_frame_widget_host_receiver),
      std::move(blink_frame_widget));
  main_frame->GetLocalRenderWidgetHost()->BindWidgetInterfaces(
      std::move(blink_widget_host_receiver), std::move(blink_widget));
  main_frame->render_view_host()->BindPageBroadcast(std::move(page_broadcast));

  bool wait_for_debugger =
      devtools_instrumentation::ShouldWaitForDebuggerInWindowOpen();
  mojom::CreateNewWindowReplyPtr reply = mojom::CreateNewWindowReply::New(
      main_frame->GetRenderViewHost()->GetRoutingID(),
      main_frame->GetRoutingID(), main_frame->GetFrameToken(),
      main_frame->GetLocalRenderWidgetHost()->GetRoutingID(), visual_properties,
      std::move(blink_frame_widget_host),
      std::move(blink_frame_widget_receiver), std::move(blink_widget_host),
      std::move(blink_widget_receiver), std::move(page_broadcast_receiver),
      mojom::DocumentScopedInterfaceBundle::New(
          std::move(main_frame_interface_provider_info),
          std::move(browser_interface_broker)),
      cloned_namespace->id(), main_frame->GetDevToolsFrameToken(),
      wait_for_debugger);

  std::move(callback).Run(mojom::CreateNewWindowStatus::kSuccess,
                          std::move(reply));
}

void RenderFrameHostImpl::CreatePortal(
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> pending_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
    CreatePortalCallback callback) {
  if (!Portal::IsEnabled()) {
    mojo::ReportBadMessage(
        "blink.mojom.Portal can only be used if the Portals feature is "
        "enabled.");
    frame_host_associated_receiver_.reset();
    return;
  }

  // We don't support attaching a portal inside a nested browsing context.
  if (!is_main_frame()) {
    mojo::ReportBadMessage(
        "RFHI::CreatePortal called in a nested browsing context");
    frame_host_associated_receiver_.reset();
    return;
  }

  // TODO(crbug.com/1051639): We need to find a long term solution to when/how
  // portals should work in sandboxed documents.
  if (active_sandbox_flags_ != network::mojom::WebSandboxFlags::kNone) {
    mojo::ReportBadMessage(
        "RFHI::CreatePortal called in a sandboxed browsing context");
    frame_host_associated_receiver_.reset();
    return;
  }

  // Note that we don't check |GetLastCommittedOrigin|, since that is inherited
  // by the initial empty document of a new frame.
  // TODO(1008989): Once issue 1008989 is fixed we could move this check into
  // |Portal::Create|.
  if (!GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    mojo::ReportBadMessage("Portal creation is restricted to the HTTP family.");
    frame_host_associated_receiver_.reset();
    return;
  }

  auto portal = std::make_unique<Portal>(this);
  portal->Bind(std::move(pending_receiver), std::move(client));
  auto it = portals_.insert(std::move(portal)).first;

  RenderFrameProxyHost* proxy_host = (*it)->CreateProxyAndAttachPortal();

  // Since the portal is newly created and has yet to commit a navigation, this
  // state is trivial.
  const FrameReplicationState& initial_replicated_state =
      proxy_host->frame_tree_node()->current_replication_state();
  DCHECK(initial_replicated_state.origin.opaque());

  std::move(callback).Run(proxy_host->GetRoutingID(), initial_replicated_state,
                          (*it)->portal_token(), proxy_host->GetFrameToken(),
                          (*it)->GetDevToolsFrameToken());
}

void RenderFrameHostImpl::AdoptPortal(const blink::PortalToken& portal_token,
                                      AdoptPortalCallback callback) {
  Portal* portal = FindPortalByToken(portal_token);
  if (!portal) {
    mojo::ReportBadMessage("Unknown portal_token when adopting portal.");
    frame_host_associated_receiver_.reset();
    return;
  }
  DCHECK_EQ(portal->owner_render_frame_host(), this);
  RenderFrameProxyHost* proxy_host = portal->CreateProxyAndAttachPortal();
  std::move(callback).Run(
      proxy_host->GetRoutingID(),
      static_cast<RenderWidgetHostViewBase*>(proxy_host->frame_tree_node()
                                                 ->render_manager()
                                                 ->GetRenderWidgetHostView())
          ->GetFrameSinkId(),
      proxy_host->frame_tree_node()->current_replication_state(),
      proxy_host->GetFrameToken(), portal->GetDevToolsFrameToken());
}

void RenderFrameHostImpl::CreateNewWidget(
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget,
    CreateNewWidgetCallback callback) {
  int32_t widget_route_id = GetProcess()->GetNextRoutingID();
  std::move(callback).Run(widget_route_id);
  delegate_->CreateNewWidget(agent_scheduling_group_, widget_route_id,
                             std::move(blink_widget_host),
                             std::move(blink_widget));
}

void RenderFrameHostImpl::CreateNewFullscreenWidget(
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget,
    CreateNewFullscreenWidgetCallback callback) {
  int32_t widget_route_id = GetProcess()->GetNextRoutingID();
  std::move(callback).Run(widget_route_id);
  delegate_->CreateNewFullscreenWidget(agent_scheduling_group_, widget_route_id,
                                       std::move(blink_widget_host),
                                       std::move(blink_widget));
}

void RenderFrameHostImpl::IssueKeepAliveHandle(
    mojo::PendingReceiver<mojom::KeepAliveHandle> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (GetProcess()->IsKeepAliveRefCountDisabled())
    return;

  if (base::FeatureList::IsEnabled(network::features::kDisableKeepaliveFetch)) {
    return;
  }

  if (!keep_alive_handle_factory_) {
    keep_alive_handle_factory_ =
        std::make_unique<KeepAliveHandleFactory>(GetProcess());
    keep_alive_handle_factory_->SetTimeout(keep_alive_timeout_);
  }
  keep_alive_handle_factory_->Create(std::move(receiver));
}

// TODO(ahemery): Move checks to mojo bad message reporting.
void RenderFrameHostImpl::BeginNavigation(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
    mojo::PendingRemote<blink::mojom::NavigationInitiator>
        navigation_initiator) {
  if (frame_tree_node_->render_manager()->is_attaching_inner_delegate()) {
    // Avoid starting any new navigations since this frame is in the process of
    // attaching an inner delegate.
    return;
  }

  if (IsInactiveAndDisallowReactivation())
    return;

  TRACE_EVENT2("navigation", "RenderFrameHostImpl::BeginNavigation",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(), "url",
               common_params->url.possibly_invalid_spec());

  DCHECK(navigation_client.is_valid());

  mojom::CommonNavigationParamsPtr validated_params = common_params.Clone();
  if (!VerifyBeginNavigationCommonParams(GetSiteInstance(), &*validated_params))
    return;

  // If the request is bearing Trust Tokens parameters:
  // - it must not be a main-frame navigation, and
  // - for certain Trust Tokens operations, the frame's parent needs the
  // trust-token-redemption Feature Policy feature.
  if (begin_params->trust_token_params && !GetParent()) {
    mojo::ReportBadMessage("RFHI: Trust Token params in main frame nav");
    return;
  }
  if (begin_params->trust_token_params &&
      ParentNeedsTrustTokenFeaturePolicy(*begin_params)) {
    RenderFrameHostImpl* parent = GetParent();
    if (!parent->IsFeatureEnabled(
            blink::mojom::FeaturePolicyFeature::kTrustTokenRedemption)) {
      mojo::ReportBadMessage(
          "RFHI: Mandatory Trust Tokens Feature Policy feature is absent");
      return;
    }
  }

  GetProcess()->FilterURL(true, &begin_params->searchable_form_url);

  // If the request was for a blob URL, but the validated URL is no longer a
  // blob URL, reset the blob_url_token to prevent hitting the ReportBadMessage
  // below, and to make sure we don't incorrectly try to use the blob_url_token.
  if (common_params->url.SchemeIsBlob() &&
      !validated_params->url.SchemeIsBlob()) {
    blob_url_token = mojo::NullRemote();
  }

  if (blob_url_token && !validated_params->url.SchemeIsBlob()) {
    mojo::ReportBadMessage("Blob URL Token, but not a blob: URL");
    return;
  }
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (blob_url_token) {
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            GetStoragePartition(), std::move(blob_url_token));
  }

  // If the request was for a blob URL, but no blob_url_token was passed in this
  // is not necessarily an error. Renderer initiated reloads for example are one
  // situation where a renderer currently doesn't have an easy way of resolving
  // the blob URL. For those situations resolve the blob URL here, as we don't
  // care about ordering with other blob URL manipulation anyway.
  if (validated_params->url.SchemeIsBlob() && !blob_url_loader_factory) {
    blob_url_loader_factory = ChromeBlobStorageContext::URLLoaderFactoryForUrl(
        GetStoragePartition(), validated_params->url);
  }

  if (waiting_for_init_) {
    pending_navigate_ = std::make_unique<PendingNavigation>(
        std::move(validated_params), std::move(begin_params),
        std::move(blob_url_loader_factory), std::move(navigation_client),
        std::move(navigation_initiator));
    return;
  }

  frame_tree_node()->navigator().OnBeginNavigation(
      frame_tree_node(), std::move(validated_params), std::move(begin_params),
      std::move(blob_url_loader_factory), std::move(navigation_client),
      std::move(navigation_initiator), EnsurePrefetchedSignedExchangeCache(),
      MaybeCreateWebBundleHandleTracker());
}

void RenderFrameHostImpl::SubresourceResponseStarted(
    const GURL& url,
    net::CertStatus cert_status) {
  delegate_->SubresourceResponseStarted(url, cert_status);
}

void RenderFrameHostImpl::ResourceLoadComplete(
    blink::mojom::ResourceLoadInfoPtr resource_load_info) {
  GlobalRequestID global_request_id;
  const bool is_frame_request =
      blink::IsRequestDestinationFrame(resource_load_info->request_destination);
  if (main_frame_request_ids_.first == resource_load_info->request_id) {
    global_request_id = main_frame_request_ids_.second;
  } else if (is_frame_request) {
    // The load complete message for the main resource arrived before
    // |DidCommitProvisionalLoad()|. We save the load info so
    // |ResourceLoadComplete()| can be called later in
    // |DidCommitProvisionalLoad()| when we can map to the global request ID.
    deferred_main_frame_load_info_ = std::move(resource_load_info);
    return;
  }
  delegate_->ResourceLoadComplete(this, global_request_id,
                                  std::move(resource_load_info));
}

void RenderFrameHostImpl::HandleAXEvents(
    const std::vector<ui::AXTreeUpdate>& updates,
    const std::vector<ui::AXEvent>& events,
    int32_t reset_token,
    HandleAXEventsCallback callback) {
  // Don't process this IPC if either we're waiting on a reset and this IPC
  // doesn't have the matching token ID, or if we're not waiting on a reset but
  // this message includes a reset token.
  if (accessibility_reset_token_ != reset_token) {
    std::move(callback).Run();
    return;
  }
  accessibility_reset_token_ = 0;

  RenderWidgetHostViewBase* view = GetViewForAccessibility();
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.is_mode_off() || !view ||
      IsInactiveAndDisallowReactivation()) {
    std::move(callback).Run();
    return;
  }

  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs))
    GetOrCreateBrowserAccessibilityManager();

  AXEventNotificationDetails details;
  details.ax_tree_id = GetAXTreeID();
  details.events = events;

  details.updates.resize(updates.size());
  for (size_t i = 0; i < updates.size(); ++i) {
    details.updates[i] = updates[i];
    if (updates[i].has_tree_data) {
      ax_tree_data_ = updates[i].tree_data;
      details.updates[i].tree_data = GetAXTreeData();
    }
  }

  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs))
    SendAccessibilityEventsToManager(details);

  delegate_->AccessibilityEventReceived(details);

  // For testing only.
  if (!accessibility_testing_callback_.is_null()) {
    if (details.events.empty()) {
      // Objects were marked dirty but no events were provided.
      // The callback must still run, otherwise dump event tests can hang.
      accessibility_testing_callback_.Run(this, ax::mojom::Event::kNone, 0);
    } else {
      // Call testing callback functions for each event to fire.
      for (auto& event : details.events) {
        if (static_cast<int>(event.event_type) < 0)
          continue;

        accessibility_testing_callback_.Run(this, event.event_type, event.id);
      }
    }
  }

  // Always send an ACK or the renderer can be in a bad state.
  std::move(callback).Run();
}

void RenderFrameHostImpl::HandleAXLocationChanges(
    std::vector<mojom::LocationChangesPtr> changes) {
  if (accessibility_reset_token_ || IsInactiveAndDisallowReactivation())
    return;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view) {
    ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
    if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs)) {
      BrowserAccessibilityManager* manager =
          GetOrCreateBrowserAccessibilityManager();
      if (manager)
        manager->OnLocationChanges(changes);
    }

    // Send the updates to the automation extension API.
    std::vector<AXLocationChangeNotificationDetails> details;
    details.reserve(changes.size());
    for (auto& change : changes) {
      AXLocationChangeNotificationDetails detail;
      detail.id = change->id;
      detail.ax_tree_id = GetAXTreeID();
      detail.new_location = change->new_location;
      details.push_back(detail);
    }
    delegate_->AccessibilityLocationChangesReceived(details);
  }
}

media::MediaMetricsProvider::RecordAggregateWatchTimeCallback
RenderFrameHostImpl::GetRecordAggregateWatchTimeCallback() {
  return delegate_->GetRecordAggregateWatchTimeCallback();
}

void RenderFrameHostImpl::ResetWaitingState() {
  // We don't allow resetting waiting state when the RenderFrameHost is either
  // in BackForwardCache or in pending deletion state, as we don't allow
  // navigations from either of these two states.
  DCHECK(!IsInBackForwardCache());
  DCHECK(!IsPendingDeletion());

  // Whenever we reset the RFH state, we should not be waiting for beforeunload
  // or close acks.  We clear them here to be safe, since they can cause
  // navigations to be ignored in DidCommitProvisionalLoad.
  if (is_waiting_for_beforeunload_completion_) {
    is_waiting_for_beforeunload_completion_ = false;
    if (beforeunload_timeout_)
      beforeunload_timeout_->Stop();
    has_shown_beforeunload_dialog_ = false;
    beforeunload_pending_replies_.clear();
  }
  send_before_unload_start_time_ = base::TimeTicks();
  render_view_host_->is_waiting_for_page_close_completion_ = false;
}

CanCommitStatus RenderFrameHostImpl::CanCommitOriginAndUrl(
    const url::Origin& origin,
    const GURL& url) {
  // If the --disable-web-security flag is specified, all bets are off and the
  // renderer process can send any origin it wishes.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity)) {
    return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
  }

  // Renderer-debug URLs can never be committed.
  if (IsRendererDebugURL(url)) {
    LogCanCommitOriginAndUrlFailureReason("is_renderer_debug_url");
    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  // Verify that if this RenderFrameHost is for a WebUI it is not committing a
  // URL which is not allowed in a WebUI process. As we are at the commit stage,
  // set |origin_requests_isolation| = false.
  if (!Navigator::CheckWebUIRendererDoesNotDisplayNormalURL(
          this, UrlInfo(url, false /* origin_requests_isolation */),
          /* is_renderer_initiated_check */ true)) {
    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  // MHTML subframes can supply URLs at commit time that do not match the
  // process lock. For example, it can be either "cid:..." or arbitrary URL at
  // which the frame was at the time of generating the MHTML
  // (e.g. "http://localhost"). In such cases, don't verify the URL, but require
  // the URL to commit in the process of the main frame.
  if (!frame_tree_node()->IsMainFrame()) {
    RenderFrameHostImpl* main_frame = GetMainFrame();
    if (main_frame->is_mhtml_document()) {
      if (IsSameSiteInstance(main_frame))
        return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;

      // If an MHTML subframe commits in a different process (even one that
      // appears correct for the subframe's URL), then we aren't correctly
      // loading it from the archive and should kill the renderer.
      base::debug::SetCrashKeyString(
          base::debug::AllocateCrashKeyString(
              "oopif_in_mhtml_page", base::debug::CrashKeySize::Size32),
          is_mhtml_document() ? "is_mhtml_doc" : "not_mhtml_doc");
      LogCanCommitOriginAndUrlFailureReason("oopif_in_mhtml_page");
      return CanCommitStatus::CANNOT_COMMIT_URL;
    }
  }

  // Give the client a chance to disallow URLs from committing.
  if (!GetContentClient()->browser()->CanCommitURL(GetProcess(), url)) {
    LogCanCommitOriginAndUrlFailureReason(
        "content_client_disallowed_commit_url");
    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  const CanCommitStatus can_commit_status = policy->CanCommitOriginAndUrl(
      GetProcess()->GetID(), GetSiteInstance()->GetIsolationContext(), origin,
      UrlInfo(url, false /* origin_requests_isolation */),
      GetSiteInstance()->IsCoopCoepCrossOriginIsolated(),
      GetSiteInstance()->CoopCoepCrossOriginIsolatedOrigin());
  if (can_commit_status != CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL) {
    LogCanCommitOriginAndUrlFailureReason("cpspi_disallowed_commit");
    return can_commit_status;
  }

  const auto origin_tuple_or_precursor_tuple =
      origin.GetTupleOrPrecursorTupleIfOpaque();
  if (origin_tuple_or_precursor_tuple.IsValid()) {
    // Verify that the origin/precursor is allowed to commit in this process.
    // Note: This also handles non-standard cases for |url|, such as
    // about:blank, data, and blob URLs.

    // Renderer-debug URLs can never be committed.
    if (IsRendererDebugURL(origin_tuple_or_precursor_tuple.GetURL())) {
      LogCanCommitOriginAndUrlFailureReason(
          "origin_or_precursor_is_renderer_debug_url");
      return CanCommitStatus::CANNOT_COMMIT_ORIGIN;
    }

    // Give the client a chance to disallow origin URLs from committing.
    // TODO(acolwell): Fix this code to work with opaque origins. Currently
    // some opaque origin precursors, like chrome-extension schemes, can trigger
    // the commit to fail. These need to be investigated.
    if (!origin.opaque() && !GetContentClient()->browser()->CanCommitURL(
                                GetProcess(), origin.GetURL())) {
      LogCanCommitOriginAndUrlFailureReason(
          "content_client_disallowed_commit_origin");
      return CanCommitStatus::CANNOT_COMMIT_ORIGIN;
    }
  }

  return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
}

void RenderFrameHostImpl::Stop() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::Stop", "frame_tree_node",
               frame_tree_node_->frame_tree_node_id());
  GetAssociatedLocalFrame()->StopLoading();
}

void RenderFrameHostImpl::DispatchBeforeUnload(BeforeUnloadType type,
                                               bool is_reload) {
  bool for_navigation =
      type == BeforeUnloadType::BROWSER_INITIATED_NAVIGATION ||
      type == BeforeUnloadType::RENDERER_INITIATED_NAVIGATION;
  bool for_inner_delegate_attach =
      type == BeforeUnloadType::INNER_DELEGATE_ATTACH;
  DCHECK(for_navigation || for_inner_delegate_attach || !is_reload);

  // TAB_CLOSE and DISCARD should only dispatch beforeunload on main frames.
  DCHECK(type == BeforeUnloadType::BROWSER_INITIATED_NAVIGATION ||
         type == BeforeUnloadType::RENDERER_INITIATED_NAVIGATION ||
         type == BeforeUnloadType::INNER_DELEGATE_ATTACH ||
         frame_tree_node_->IsMainFrame());

  if (!for_navigation) {
    // Cancel any pending navigations, to avoid their navigation commit/fail
    // event from wiping out the is_waiting_for_beforeunload_completion_ state.
    if (frame_tree_node_->navigation_request() &&
        frame_tree_node_->navigation_request()->IsNavigationStarted()) {
      frame_tree_node_->navigation_request()->set_net_error(net::ERR_ABORTED);
    }
    frame_tree_node_->ResetNavigationRequest(false);
  }

  // In renderer-initiated navigations, don't check for beforeunload in the
  // navigating frame, as it has already run beforeunload before it sent the
  // BeginNavigation IPC.
  bool check_subframes_only =
      type == BeforeUnloadType::RENDERER_INITIATED_NAVIGATION;
  if (!ShouldDispatchBeforeUnload(check_subframes_only)) {
    // When running beforeunload for navigations, ShouldDispatchBeforeUnload()
    // is checked earlier and we would only get here if it had already returned
    // true.
    DCHECK(!for_navigation);

    // Dispatch the ACK to prevent re-entrancy.
    base::OnceClosure task = base::BindOnce(
        [](base::WeakPtr<RenderFrameHostImpl> self) {
          if (!self)
            return;
          self->frame_tree_node_->render_manager()->BeforeUnloadCompleted(
              true, base::TimeTicks::Now());
        },
        weak_ptr_factory_.GetWeakPtr());
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(task));
    return;
  }
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "navigation", "RenderFrameHostImpl BeforeUnload", TRACE_ID_LOCAL(this),
      "&RenderFrameHostImpl", static_cast<void*>(this));

  // This may be called more than once (if the user clicks the tab close button
  // several times, or if they click the tab close button then the browser close
  // button), and we only send the message once.
  if (is_waiting_for_beforeunload_completion_) {
    // Some of our close messages could be for the tab, others for cross-site
    // transitions. We always want to think it's for closing the tab if any
    // of the messages were, since otherwise it might be impossible to close
    // (if there was a cross-site "close" request pending when the user clicked
    // the close button). We want to keep the "for cross site" flag only if
    // both the old and the new ones are also for cross site.
    unload_ack_is_for_navigation_ =
        unload_ack_is_for_navigation_ && for_navigation;
  } else {
    // Start the hang monitor in case the renderer hangs in the beforeunload
    // handler.
    is_waiting_for_beforeunload_completion_ = true;
    beforeunload_dialog_request_cancels_unload_ = false;
    unload_ack_is_for_navigation_ = for_navigation;
    send_before_unload_start_time_ = base::TimeTicks::Now();
    if (render_view_host_->GetDelegate()->IsJavaScriptDialogShowing()) {
      // If there is a JavaScript dialog up, don't bother sending the renderer
      // the unload event because it is known unresponsive, waiting for the
      // reply from the dialog. If this incoming request is for a DISCARD be
      // sure to reply with |proceed = false|, because the presence of a dialog
      // indicates that the page can't be discarded.
      SimulateBeforeUnloadCompleted(type != BeforeUnloadType::DISCARD);
    } else {
      // Start a timer that will be shared by all frames that need to run
      // beforeunload in the current frame's subtree.
      if (beforeunload_timeout_)
        beforeunload_timeout_->Start(beforeunload_timeout_delay_);

      beforeunload_pending_replies_.clear();
      beforeunload_dialog_request_cancels_unload_ =
          (type == BeforeUnloadType::DISCARD);

      // Run beforeunload in this frame and its cross-process descendant
      // frames, in parallel.
      CheckOrDispatchBeforeUnloadForSubtree(check_subframes_only,
                                            true /* send_ipc */, is_reload);
    }
  }
}

bool RenderFrameHostImpl::CheckOrDispatchBeforeUnloadForSubtree(
    bool subframes_only,
    bool send_ipc,
    bool is_reload) {
  bool found_beforeunload = false;
  for (FrameTreeNode* node : frame_tree()->SubtreeNodes(frame_tree_node_)) {
    RenderFrameHostImpl* rfh = node->current_frame_host();

    // If |subframes_only| is true, skip this frame and its same-site
    // descendants.  This happens for renderer-initiated navigations, where
    // these frames have already run beforeunload.
    if (subframes_only && rfh->GetSiteInstance() == GetSiteInstance())
      continue;

    // No need to run beforeunload if the RenderFrame isn't live.
    if (!rfh->IsRenderFrameLive())
      continue;

    // Only run beforeunload in frames that have registered a beforeunload
    // handler.
    bool should_run_beforeunload = rfh->has_before_unload_handler_;
    // TODO(alexmos): Many tests, as well as some DevTools cases, currently
    // assume that beforeunload for a navigating/closing frame is always sent
    // to the renderer. For now, keep this assumption by checking |rfh ==
    // this|. In the future, this condition should be removed, and beforeunload
    // should only be sent when a handler is registered.  For subframes of a
    // navigating/closing frame, this assumption was never present, so
    // subframes are included only if they have registered a beforeunload
    // handler.
    if (rfh == this)
      should_run_beforeunload = true;

    if (!should_run_beforeunload)
      continue;

    // If we're only checking whether there's at least one frame with
    // beforeunload, then we've just found one, so we can return now.
    found_beforeunload = true;
    if (!send_ipc)
      return true;

    // Otherwise, figure out whether we need to send the IPC, or whether this
    // beforeunload was already triggered by an ancestor frame's IPC.

    // Only send beforeunload to local roots, and let Blink handle any
    // same-site frames under them. That is, if a frame has a beforeunload
    // handler, ask its local root to run it. If we've already sent the message
    // to that local root, skip this frame. For example, in A1(A2,A3), if A2
    // and A3 contain beforeunload handlers, and all three frames are
    // same-site, we ask A1 to run beforeunload for all three frames, and only
    // ask it once.
    while (!rfh->is_local_root() && rfh != this)
      rfh = rfh->GetParent();
    if (base::Contains(beforeunload_pending_replies_, rfh))
      continue;

    // For a case like A(B(A)), it's not necessary to send an IPC for the
    // innermost frame, as Blink will walk all same-site (local)
    // descendants. Detect cases like this and skip them.
    bool has_same_site_ancestor = false;
    for (auto* added_rfh : beforeunload_pending_replies_) {
      if (rfh->IsDescendantOf(added_rfh) &&
          rfh->GetSiteInstance() == added_rfh->GetSiteInstance()) {
        has_same_site_ancestor = true;
        break;
      }
    }
    if (has_same_site_ancestor)
      continue;

    // Add |rfh| to the list of frames that need to receive beforeunload
    // ACKs.
    beforeunload_pending_replies_.insert(rfh);

    SendBeforeUnload(is_reload, rfh->GetWeakPtr());
  }

  return found_beforeunload;
}

void RenderFrameHostImpl::SimulateBeforeUnloadCompleted(bool proceed) {
  DCHECK(is_waiting_for_beforeunload_completion_);
  base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;

  // Dispatch the ACK to prevent re-entrancy.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderFrameHostImpl::ProcessBeforeUnloadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), proceed,
                     true /* treat_as_final_completion_callback */,
                     approx_renderer_start_time, base::TimeTicks::Now()));
}

bool RenderFrameHostImpl::ShouldDispatchBeforeUnload(
    bool check_subframes_only) {
  return CheckOrDispatchBeforeUnloadForSubtree(
      check_subframes_only, false /* send_ipc */, false /* is_reload */);
}

void RenderFrameHostImpl::SetBeforeUnloadTimeoutDelayForTesting(
    const base::TimeDelta& timeout) {
  beforeunload_timeout_delay_ = timeout;
}

void RenderFrameHostImpl::StartPendingDeletionOnSubtree() {
  ResetNavigationsForPendingDeletion();

  DCHECK(IsPendingDeletion());
  for (std::unique_ptr<FrameTreeNode>& child_frame : children_) {
    for (FrameTreeNode* node : frame_tree()->SubtreeNodes(child_frame.get())) {
      RenderFrameHostImpl* child = node->current_frame_host();
      if (child->IsPendingDeletion())
        continue;

      // Blink handles deletion of all same-process descendants, running their
      // unload handler if necessary. So delegate sending IPC on the topmost
      // ancestor using the same process.
      RenderFrameHostImpl* local_ancestor = child;
      for (auto* rfh = child->parent_; rfh != parent_; rfh = rfh->parent_) {
        if (rfh->GetSiteInstance() == child->GetSiteInstance())
          local_ancestor = rfh;
      }

      local_ancestor->DeleteRenderFrame(FrameDeleteIntention::kNotMainFrame);
      if (local_ancestor != child) {
        LifecycleState child_lifecycle_state =
            child->has_unload_handlers()
                ? LifecycleState::kRunningUnloadHandlers
                : LifecycleState::kReadyToBeDeleted;
        child->SetLifecycleState(child_lifecycle_state);
      }

      node->frame_tree()->FrameUnloading(node);
    }
  }
}

void RenderFrameHostImpl::PendingDeletionCheckCompleted() {
  if (lifecycle_state_ == LifecycleState::kReadyToBeDeleted &&
      children_.empty()) {
    if (is_waiting_for_unload_ack_)
      OnUnloaded();
    else
      parent_->RemoveChild(frame_tree_node_);
  }
}

void RenderFrameHostImpl::PendingDeletionCheckCompletedOnSubtree() {
  if (children_.empty()) {
    PendingDeletionCheckCompleted();
    return;
  }

  // Collect children first before calling PendingDeletionCheckCompleted() on
  // them, because it may delete them.
  std::vector<RenderFrameHostImpl*> children_rfh;
  for (std::unique_ptr<FrameTreeNode>& child : children_)
    children_rfh.push_back(child->current_frame_host());

  for (RenderFrameHostImpl* child_rfh : children_rfh)
    child_rfh->PendingDeletionCheckCompletedOnSubtree();
}

void RenderFrameHostImpl::ResetNavigationsForPendingDeletion() {
  for (auto& child : children_)
    child->current_frame_host()->ResetNavigationsForPendingDeletion();
  ResetNavigationRequests();
  frame_tree_node_->ResetNavigationRequest(false);
  frame_tree_node_->render_manager()->CleanUpNavigation();
}

void RenderFrameHostImpl::OnUnloadTimeout() {
  DCHECK(IsPendingDeletion());
  parent_->RemoveChild(frame_tree_node_);
}

void RenderFrameHostImpl::UpdateOpener() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::UpdateOpener",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  // This frame (the frame whose opener is being updated) might not have had
  // proxies for the new opener chain in its SiteInstance.  Make sure they
  // exist.
  if (frame_tree_node_->opener()) {
    frame_tree_node_->opener()->render_manager()->CreateOpenerProxies(
        GetSiteInstance(), frame_tree_node_);
  }

  auto opener_frame_token =
      frame_tree_node_->render_manager()->GetOpenerFrameToken(
          GetSiteInstance());
  GetAssociatedLocalFrame()->UpdateOpener(opener_frame_token);
}

void RenderFrameHostImpl::SetFocusedFrame() {
  GetAssociatedLocalFrame()->Focus();
}

void RenderFrameHostImpl::AdvanceFocus(blink::mojom::FocusType type,
                                       RenderFrameProxyHost* source_proxy) {
  DCHECK(!source_proxy ||
         (source_proxy->GetProcess()->GetID() == GetProcess()->GetID()));

  base::Optional<base::UnguessableToken> frame_token;
  if (source_proxy)
    frame_token = source_proxy->GetFrameToken();

  GetAssociatedLocalFrame()->AdvanceFocusInFrame(type, frame_token);
}

void RenderFrameHostImpl::JavaScriptDialogClosed(
    JavaScriptDialogCallback dialog_closed_callback,
    bool success,
    const base::string16& user_input) {
  GetProcess()->SetBlocked(false);
  std::move(dialog_closed_callback).Run(success, user_input);
  // If executing as part of beforeunload event handling, there may have been
  // timers stopped in this frame or a frame up in the frame hierarchy. Restart
  // any timers that were stopped in OnRunBeforeUnloadConfirm().
  for (RenderFrameHostImpl* frame = this; frame; frame = frame->GetParent()) {
    if (frame->is_waiting_for_beforeunload_completion_ &&
        frame->beforeunload_timeout_) {
      frame->beforeunload_timeout_->Start(beforeunload_timeout_delay_);
    }
  }
}

bool RenderFrameHostImpl::ShouldDispatchPagehideAndVisibilitychangeDuringCommit(
    RenderFrameHostImpl* old_frame_host,
    const UrlInfo& dest_url_info) {
  // Only return true if this is a same-site navigation and we did a proactive
  // BrowsingInstance swap but we're reusing the old page's renderer process.
  DCHECK(old_frame_host);
  if (old_frame_host->GetSiteInstance()->IsRelatedSiteInstance(
          GetSiteInstance())) {
    return false;
  }
  if (old_frame_host->GetProcess() != GetProcess()) {
    return false;
  }
  if (!old_frame_host->IsNavigationSameSite(
          dest_url_info, GetSiteInstance()->IsCoopCoepCrossOriginIsolated(),
          GetSiteInstance()->CoopCoepCrossOriginIsolatedOrigin())) {
    return false;
  }
  DCHECK(frame_tree_node_->IsMainFrame());
  DCHECK_NE(old_frame_host, this);
  DCHECK_NE(old_frame_host->GetSiteInstance(), GetSiteInstance());
  return true;
}

void RenderFrameHostImpl::CommitNavigation(
    NavigationRequest* navigation_request,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    bool is_view_source,
    base::Optional<SubresourceLoaderParams> subresource_loader_params,
    base::Optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    const base::UnguessableToken& devtools_navigation_token,
    std::unique_ptr<WebBundleHandle> web_bundle_handle) {
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::CommitNavigation",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(), "url",
               common_params->url.possibly_invalid_spec());
  DCHECK(!IsRendererDebugURL(common_params->url));
  DCHECK(navigation_request);

  bool is_same_document =
      NavigationTypeUtils::IsSameDocument(common_params->navigation_type);
  bool is_mhtml_iframe = navigation_request->IsForMhtmlSubframe();

  // A |response| and a |url_loader_client_endpoints| must always be provided,
  // except for edge cases, where another way to load the document exist.
  DCHECK((response_head && url_loader_client_endpoints) ||
         common_params->url.SchemeIs(url::kDataScheme) || is_same_document ||
         !IsURLHandledByNetworkStack(common_params->url) || is_mhtml_iframe);

  // All children of MHTML documents must be MHTML documents.
  // As a defensive measure, crash the browser if something went wrong.
  if (!frame_tree_node()->IsMainFrame()) {
    RenderFrameHostImpl* root = GetMainFrame();
    if (root->is_mhtml_document_ &&
        !common_params->url.SchemeIs(url::kDataScheme)) {
      bool loaded_from_outside_the_archive =
          response_head || url_loader_client_endpoints;
      CHECK(!loaded_from_outside_the_archive);
      CHECK(is_mhtml_iframe);
      CHECK_EQ(GetSiteInstance(), root->GetSiteInstance());
      CHECK_EQ(GetProcess(), root->GetProcess());
    } else {
      DCHECK(!is_mhtml_iframe);
    }
  }

  bool is_srcdoc = common_params->url.IsAboutSrcdoc();
  if (is_srcdoc) {
    // Main frame srcdoc navigation are meaningless. They are blocked whenever a
    // navigation attempt is made. It shouldn't reach CommitNavigation.
    CHECK(!frame_tree_node_->IsMainFrame());

    // An about:srcdoc document is always same SiteInstance with its parent.
    // Otherwise, it won't be able to load. The parent's document contains the
    // iframe and its srcdoc attribute.
    CHECK_EQ(GetSiteInstance(), parent_->GetSiteInstance());
  }

  // TODO(https://crbug.com/888079): Compute the Origin to commit here.

  // If this is an attempt to commit a URL in an incompatible process, capture a
  // crash dump to diagnose why it is occurring.
  // TODO(creis): Remove this check after we've gathered enough information to
  // debug issues with browser-side security checks. https://crbug.com/931895.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  const ProcessLock process_lock = GetSiteInstance()->GetProcessLock();
  if (process_lock != ProcessLock::CreateForErrorPage() &&
      common_params->url.IsStandard() &&
      !policy->CanAccessDataForOrigin(GetProcess()->GetID(),
                                      common_params->url) &&
      !is_mhtml_iframe) {
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("lock_url",
                                            base::debug::CrashKeySize::Size64),
        process_lock.ToString());
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("commit_origin",
                                            base::debug::CrashKeySize::Size64),
        common_params->url.GetOrigin().spec());
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_main_frame",
                                            base::debug::CrashKeySize::Size32),
        frame_tree_node_->IsMainFrame() ? "true" : "false");
    NOTREACHED() << "Commiting in incompatible process for URL: "
                 << process_lock.lock_url() << " lock vs "
                 << common_params->url.GetOrigin();
    base::debug::DumpWithoutCrashing();
  }

  const bool is_first_navigation = !has_committed_any_navigation_;
  has_committed_any_navigation_ = true;

  // If this is NOT for same-document navigation, existing |web_bundle_handle_|
  // should be reset to the new one. Otherwise the existing one should be kept
  // around so that the subresource requests keep being served from the
  // WebBundleURLLoaderFactory held by the handle.
  if (!is_same_document)
    web_bundle_handle_ = std::move(web_bundle_handle);

  UpdatePermissionsForNavigation(*common_params, *commit_params);

  // Get back to a clean state, in case we start a new navigation without
  // completing an unload handler.
  ResetWaitingState();

  // The renderer can exit view source mode when any error or cancellation
  // happen. When reusing the same renderer, overwrite to recover the mode.
  if (is_view_source && IsCurrent()) {
    DCHECK(!GetParent());
    GetAssociatedLocalFrame()->EnableViewSourceMode();
  }

  // TODO(lfg): The renderer is not able to handle a null response, so the
  // browser provides an empty response instead. See the DCHECK in the beginning
  // of this method for the edge cases where a response isn't provided.
  network::mojom::URLResponseHeadPtr head =
      response_head ? std::move(response_head)
                    : network::mojom::URLResponseHead::New();

  // TODO(crbug.com/979296): Consider changing this code to copy an origin
  // instead of creating one from a URL which lacks opacity information.
  url::Origin main_world_origin_for_url_loader_factory =
      navigation_request->GetOriginForURLLoaderFactory();

  // IsolationInfo should be filled before the URLLoaderFactory for
  // sub-resources is created. Only update for cross document navigations since
  // for opaque origin same document navigations, a new origin should not be
  // created as that would be different from the original.
  if (!is_same_document) {
    isolation_info_ = ComputeIsolationInfoInternal(
        main_world_origin_for_url_loader_factory,
        net::IsolationInfo::RedirectMode::kUpdateNothing);
  }
  DCHECK(!isolation_info_.IsEmpty());

  if (navigation_request->appcache_handle()) {
    // AppCache may create a subresource URLLoaderFactory later, so make sure it
    // has the correct origin to use when calling
    // ContentBrowserClient::WillCreateURLLoaderFactory().
    navigation_request->appcache_handle()
        ->host()
        ->set_origin_for_url_loader_factory(
            main_world_origin_for_url_loader_factory);
  }
  // This needs to ask |navigation_request| for the UKM ID since the commit is
  // in progress but not yet done.
  base::UkmSourceId next_page_ukm_source_id = base::UkmSourceId::FromInt64(
      navigation_request->GetNextPageUkmSourceId());
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories;
  if ((!is_same_document || is_first_navigation) && !is_srcdoc) {
    recreate_default_url_loader_factory_after_network_service_crash_ = false;
    subresource_loader_factories =
        std::make_unique<blink::PendingURLLoaderFactoryBundle>();
    BrowserContext* browser_context = GetSiteInstance()->GetBrowserContext();

    // NOTE: On Network Service navigations, we want to ensure that a frame is
    // given everything it will need to load any accessible subresources. We
    // however only do this for cross-document navigations, because the
    // alternative would be redundant effort.
    if (subresource_loader_params &&
        subresource_loader_params->pending_appcache_loader_factory.is_valid()) {
      // If the caller has supplied a factory for AppCache, use it.
      mojo::Remote<network::mojom::URLLoaderFactory> appcache_remote(std::move(
          subresource_loader_params->pending_appcache_loader_factory));

      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          appcache_proxied_remote;
      auto appcache_proxied_receiver =
          appcache_proxied_remote.InitWithNewPipeAndPassReceiver();
      bool use_proxy =
          GetContentClient()->browser()->WillCreateURLLoaderFactory(
              browser_context, this, GetProcess()->GetID(),
              ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
              main_world_origin_for_url_loader_factory,
              base::nullopt /* navigation_id */, next_page_ukm_source_id,
              &appcache_proxied_receiver, nullptr /* header_client */,
              nullptr /* bypass_redirect_checks */,
              nullptr /* disable_secure_dns */, nullptr /* factory_override */);
      if (use_proxy) {
        appcache_remote->Clone(std::move(appcache_proxied_receiver));
        appcache_remote.reset();
        appcache_remote.Bind(std::move(appcache_proxied_remote));
      }

      subresource_loader_factories->pending_appcache_factory() =
          appcache_remote.Unbind();
    }

    non_network_uniquely_owned_factories_.clear();
    ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;

    // Set up the default factory.
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_default_factory;

    // See if this is for WebUI.
    std::string scheme = common_params->url.scheme();
    const auto& webui_schemes = URLDataManagerBackend::GetWebUISchemes();
    if (base::Contains(webui_schemes, scheme)) {
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_for_webui;
      auto factory_receiver =
          factory_for_webui.InitWithNewPipeAndPassReceiver();
      GetContentClient()->browser()->WillCreateURLLoaderFactory(
          browser_context, this, GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
          main_world_origin_for_url_loader_factory,
          base::nullopt /* navigation_id */, next_page_ukm_source_id,
          &factory_receiver, nullptr /* header_client */,
          nullptr /* bypass_redirect_checks */,
          nullptr /* disable_secure_dns */, nullptr /* factory_override */);
      mojo::Remote<network::mojom::URLLoaderFactory> direct_factory_for_webui(
          CreateWebUIURLLoaderFactory(this, scheme, {}));
      direct_factory_for_webui->Clone(std::move(factory_receiver));

      // If the renderer has webui bindings, then don't give it access to
      // network loader for security reasons.
      // http://crbug.com/829412: make an exception for a small whitelist
      // of WebUIs that need to be fixed to not make network requests in JS.
      if ((enabled_bindings_ & kWebUIBindingsPolicyMask) &&
          !GetContentClient()->browser()->IsWebUIAllowedToMakeNetworkRequests(
              url::Origin::Create(common_params->url.GetOrigin()))) {
        pending_default_factory = std::move(factory_for_webui);
        // WebUIURLLoaderFactory will kill the renderer if it sees a request
        // with a non-chrome scheme. Register a URLLoaderFactory for the about
        // scheme so about:blank doesn't kill the renderer.
        non_network_factories[url::kAboutScheme] =
            AboutURLLoaderFactory::Create();
      } else {
        // This is a webui scheme that doesn't have webui bindings. Give it
        // access to the network loader as it might require it.
        subresource_loader_factories->pending_scheme_specific_factories()
            .emplace(scheme, std::move(factory_for_webui));
      }
    }

    if (!pending_default_factory) {
      // Otherwise default to a Network Service-backed loader from the
      // appropriate NetworkContext.
      recreate_default_url_loader_factory_after_network_service_crash_ = true;

      // Otherwise default to a Network Service-backed loader from the
      // appropriate NetworkContext.
      bool bypass_redirect_checks =
          CreateNetworkServiceDefaultFactoryAndObserve(
              CreateURLLoaderFactoryParamsForMainWorld(
                  navigation_request, "RFHI::CommitNavigation"),
              next_page_ukm_source_id,
              pending_default_factory.InitWithNewPipeAndPassReceiver());
      subresource_loader_factories->set_bypass_redirect_checks(
          bypass_redirect_checks);
    }

    bool navigation_to_web_bundle = false;

    if (web_bundle_handle_ && web_bundle_handle_->IsReadyForLoading()) {
      navigation_to_web_bundle = true;
      mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory(
          std::move(pending_default_factory));
      web_bundle_handle_->CreateURLLoaderFactory(
          pending_default_factory.InitWithNewPipeAndPassReceiver(),
          std::move(fallback_factory));
      DCHECK(web_bundle_handle_->navigation_info());
      commit_params->web_bundle_physical_url =
          web_bundle_handle_->navigation_info()->source().url();
      if (web_bundle_handle_->claimed_url().is_valid()) {
        commit_params->web_bundle_claimed_url =
            web_bundle_handle_->claimed_url();
      }
    }

    DCHECK(pending_default_factory);
    subresource_loader_factories->pending_default_factory() =
        std::move(pending_default_factory);

    // Only file resources and about:blank with an initiator that can load files
    // can load file subresources.
    //
    // Other URLs like about:srcdoc might be able load files, but only because
    // they will inherit loaders from their parents instead of the ones
    // provided by the browser process here.
    // TODO(crbug.com/949510): Make about:srcdoc also use this path instead of
    // inheriting loaders from the parent.
    //
    // For loading Web Bundle files, we don't set FileURLLoaderFactory.
    // Because loading local files from a Web Bundle file is prohibited.
    //
    // TODO(crbug.com/888079): In the future, use
    // GetOriginForURLLoaderFactory/GetOriginToCommit.
    if ((common_params->url.SchemeIsFile() ||
         (common_params->url.IsAboutBlank() &&
          common_params->initiator_origin &&
          common_params->initiator_origin->scheme() == url::kFileScheme)) &&
        !navigation_to_web_bundle) {
      // USER_BLOCKING because this scenario is exactly one of the examples
      // given by the doc comment for USER_BLOCKING: Loading and rendering a web
      // page after the user clicks a link.
      base::TaskPriority file_factory_priority =
          base::TaskPriority::USER_BLOCKING;
      non_network_factories.emplace(
          url::kFileScheme,
          FileURLLoaderFactory::Create(
              browser_context->GetPath(),
              browser_context->GetSharedCorsOriginAccessList(),
              file_factory_priority));
    }

#if defined(OS_ANDROID)
    if (common_params->url.SchemeIs(url::kContentScheme)) {
      // Only content:// URLs can load content:// subresources
      non_network_factories.emplace(url::kContentScheme,
                                    ContentURLLoaderFactory::Create());
    }
#endif

    auto* partition =
        static_cast<StoragePartitionImpl*>(BrowserContext::GetStoragePartition(
            browser_context, GetSiteInstance()));
    non_network_factories.emplace(
        url::kFileSystemScheme, CreateFileSystemURLLoaderFactory(
                                    GetProcess()->GetID(), GetFrameTreeNodeId(),
                                    partition->GetFileSystemContext(),
                                    partition->GetPartitionDomain()));

    non_network_factories.emplace(url::kDataScheme,
                                  DataURLLoaderFactory::Create());

    GetContentClient()
        ->browser()
        ->RegisterNonNetworkSubresourceURLLoaderFactories(
            GetProcess()->GetID(), routing_id_,
            &non_network_uniquely_owned_factories_, &non_network_factories);

    for (auto& factory : non_network_uniquely_owned_factories_) {
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_factory_proxy;
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver =
          pending_factory_proxy.InitWithNewPipeAndPassReceiver();
      WillCreateURLLoaderFactory(main_world_origin_for_url_loader_factory,
                                 &factory_receiver, next_page_ukm_source_id);
      factory.second->Clone(std::move(factory_receiver));
      subresource_loader_factories->pending_scheme_specific_factories().emplace(
          factory.first, std::move(pending_factory_proxy));
    }

    for (auto& factory : non_network_factories) {
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_factory_proxy;
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver =
          pending_factory_proxy.InitWithNewPipeAndPassReceiver();
      WillCreateURLLoaderFactory(main_world_origin_for_url_loader_factory,
                                 &factory_receiver, next_page_ukm_source_id);
      mojo::Remote<network::mojom::URLLoaderFactory> remote(
          std::move(factory.second));
      remote->Clone(std::move(factory_receiver));
      subresource_loader_factories->pending_scheme_specific_factories().emplace(
          factory.first, std::move(pending_factory_proxy));
    }

    subresource_loader_factories->pending_isolated_world_factories() =
        CreateURLLoaderFactoriesForIsolatedWorlds(
            navigation_request,
            isolated_worlds_requiring_separate_url_loader_factory_);
  }

  // It is imperative that cross-document navigations always provide a set of
  // subresource ULFs.
  DCHECK(is_same_document || !is_first_navigation || is_srcdoc ||
         subresource_loader_factories);

  if (is_same_document) {
    DCHECK_EQ(frame_tree_node()->current_frame_host(), this);
    DCHECK(same_document_navigation_request_);
    bool should_replace_current_entry =
        common_params->should_replace_current_entry;
    GetNavigationControl()->CommitSameDocumentNavigation(
        std::move(common_params), std::move(commit_params),
        base::BindOnce(&RenderFrameHostImpl::OnSameDocumentCommitProcessed,
                       base::Unretained(this),
                       same_document_navigation_request_->GetNavigationId(),
                       should_replace_current_entry));
  } else {
    // Pass the controller service worker info if we have one.
    blink::mojom::ControllerServiceWorkerInfoPtr controller;
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerObject>
        remote_object;
    blink::mojom::ServiceWorkerState sent_state;
    if (subresource_loader_params &&
        subresource_loader_params->controller_service_worker_info) {
      controller =
          std::move(subresource_loader_params->controller_service_worker_info);
      if (controller->object_info) {
        controller->object_info->receiver =
            remote_object.InitWithNewEndpointAndPassReceiver();
        sent_state = controller->object_info->state;
      }
    }

    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        factory_bundle_for_prefetch;
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory;
    if (subresource_loader_factories) {
      // Clone the factory bundle for prefetch.
      auto bundle = base::MakeRefCounted<blink::URLLoaderFactoryBundle>(
          std::move(subresource_loader_factories));
      subresource_loader_factories = CloneFactoryBundle(bundle);
      factory_bundle_for_prefetch = CloneFactoryBundle(bundle);
    }

    if (factory_bundle_for_prefetch) {
      if (prefetched_signed_exchange_cache_) {
        prefetched_signed_exchange_cache_->RecordHistograms();
        // Reset |prefetched_signed_exchange_cache_|, not to reuse the cached
        // signed exchange which was prefetched in the previous page.
        prefetched_signed_exchange_cache_.reset();
      }

      // Also set-up URLLoaderFactory for prefetch using the same loader
      // factories. TODO(kinuko): Consider setting this up only when prefetch
      // is used. Currently we have this here to make sure we have non-racy
      // situation (https://crbug.com/849929).
      auto* storage_partition = static_cast<StoragePartitionImpl*>(
          BrowserContext::GetStoragePartition(
              GetSiteInstance()->GetBrowserContext(), GetSiteInstance()));
      storage_partition->GetPrefetchURLLoaderService()->GetFactory(
          prefetch_loader_factory.InitWithNewPipeAndPassReceiver(),
          frame_tree_node_->frame_tree_node_id(),
          std::move(factory_bundle_for_prefetch),
          weak_ptr_factory_.GetWeakPtr(),
          EnsurePrefetchedSignedExchangeCache());
    }

    mojom::NavigationClient* navigation_client =
        navigation_request->GetCommitNavigationClient();

    // Record the metrics about the state of the old main frame at the moment
    // when we navigate away from it as it matters for whether the page
    // is eligible for being put into back-forward cache.
    //
    // Ideally we would do this when we are just about to swap out the old
    // render frame and swap in the new one, but we can't do this for
    // same-process navigations yet as we are reusing the RenderFrameHost and
    // as the local frame navigates it overrides the values that we are
    // interested in. The cross-process navigation case is handled in
    // RenderFrameHostManager::UnloadOldFrame.
    //
    // Here we are recording the metrics for same-process navigations at the
    // point just before the navigation commits.
    // TODO(altimin, crbug.com/933147): Remove this logic after we are done with
    // implementing back-forward cache.
    if (!GetParent() && frame_tree_node_->current_frame_host() == this) {
      if (NavigationEntryImpl* last_committed_entry =
              NavigationEntryImpl::FromNavigationEntry(
                  frame_tree()->controller()->GetLastCommittedEntry())) {
        if (last_committed_entry->back_forward_cache_metrics()) {
          last_committed_entry->back_forward_cache_metrics()
              ->RecordFeatureUsage(this);
        }
      }
    }

    // about:srcdoc "inherits" loaders from its parent in the renderer process,
    // There are no need to provide new ones here.
    // TODO(arthursonzogni): What about about:blank URLs?
    // TODO(arthursonzogni): What about data-URLs?
    //
    // Note: Inheriting loaders could be done in the browser process, but we
    //       aren't confident there are enough reliable information in the
    //       browser process to always make the correct decision. "Inheriting"
    //       in the renderer process is slightly less problematic in that it
    //       guarantees the renderer won't have higher privileges than it
    //       originally had (since it will inherit loader factories it already
    //       had access to).
    if (is_srcdoc) {
      DCHECK(!subresource_loader_factories);
      DCHECK(!subresource_overrides);
      DCHECK(!prefetch_loader_factory);
    }

    // If a network request was made, update the Previews state.
    if (IsURLHandledByNetworkStack(common_params->url))
      last_navigation_previews_state_ = common_params->previews_state;

    dom_content_loaded_ = false;
    SendCommitNavigation(
        navigation_client, navigation_request, std::move(common_params),
        std::move(commit_params), std::move(head), std::move(response_body),
        std::move(url_loader_client_endpoints),
        std::move(subresource_loader_factories),
        std::move(subresource_overrides), std::move(controller),
        std::move(container_info), std::move(prefetch_loader_factory),
        devtools_navigation_token);

    // |remote_object| is an associated interface ptr, so calls can't be made on
    // it until its request endpoint is sent. Now that the request endpoint was
    // sent, it can be used, so add it to ServiceWorkerObjectHost.
    if (remote_object.is_valid()) {
      RunOrPostTaskOnThread(
          FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
          base::BindOnce(
              &ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState,
              subresource_loader_params->controller_service_worker_object_host,
              std::move(remote_object), sent_state));
    }
  }

  is_loading_ = true;
}

void RenderFrameHostImpl::FailedNavigation(
    NavigationRequest* navigation_request,
    const mojom::CommonNavigationParams& common_params,
    const mojom::CommitNavigationParams& commit_params,
    bool has_stale_copy_in_cache,
    int error_code,
    const base::Optional<std::string>& error_page_content) {
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::FailedNavigation",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(),
               "error", error_code);

  DCHECK(navigation_request);

  // Update renderer permissions even for failed commits, so that for example
  // the URL bar correctly displays privileged URLs instead of filtering them.
  UpdatePermissionsForNavigation(common_params, commit_params);

  // Get back to a clean state, in case a new navigation started without
  // completing an unload handler.
  ResetWaitingState();

  isolation_info_ = net::IsolationInfo::CreateTransient();

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> default_factory_remote;
  bool bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
      CreateURLLoaderFactoryParamsForMainWorld(navigation_request,
                                               "RFHI::FailedNavigation"),
      base::kInvalidUkmSourceId,
      default_factory_remote.InitWithNewPipeAndPassReceiver());
  subresource_loader_factories =
      std::make_unique<blink::PendingURLLoaderFactoryBundle>(
          std::move(default_factory_remote),
          blink::PendingURLLoaderFactoryBundle::SchemeMap(),
          blink::PendingURLLoaderFactoryBundle::OriginMap(),
          bypass_redirect_checks);

  mojom::NavigationClient* navigation_client =
      navigation_request->GetCommitNavigationClient();

  SendCommitFailedNavigation(
      navigation_client, navigation_request, common_params.Clone(),
      commit_params.Clone(), has_stale_copy_in_cache, error_code,
      error_page_content, std::move(subresource_loader_factories));

  // TODO(crbug/1129537): support UKM source creation for failed navigations
  // too.

  // An error page is expected to commit, hence why is_loading_ is set to true.
  is_loading_ = true;
  dom_content_loaded_ = false;
  has_committed_any_navigation_ = true;
  DCHECK(navigation_request && navigation_request->IsNavigationStarted() &&
         navigation_request->GetNetErrorCode() != net::OK);
}

void RenderFrameHostImpl::HandleRendererDebugURL(const GURL& url) {
  DCHECK(IsRendererDebugURL(url));

  // Several tests expect a load of Chrome Debug URLs to send a DidStopLoading
  // notification, so set is loading to true here to properly surface it when
  // the renderer process is done handling the URL.
  // TODO(clamy): Remove the test dependency on this behavior.
  if (!url.SchemeIs(url::kJavaScriptScheme)) {
    bool was_loading = frame_tree()->IsLoading();
    is_loading_ = true;
    frame_tree_node()->DidStartLoading(true, was_loading);
  }

  GetNavigationControl()->HandleRendererDebugURL(url);

  // Ensure that the renderer process is marked as used after processing a
  // renderer debug URL, since this process is now unsafe to be reused by sites
  // that require a dedicated process.  Usually this happens at ready-to-commit
  // (NavigationRequest::OnResponseStarted) time for regular navigations, but
  // renderer debug URLs don't go through that path.  This matters for initial
  // navigations to renderer debug URLs.  See https://crbug.com/1074108.
  GetProcess()->SetIsUsed();
}

void RenderFrameHostImpl::SetUpMojoIfNeeded() {
  if (associated_registry_.get())
    return;

  associated_registry_ = std::make_unique<blink::AssociatedInterfaceRegistry>();

  auto bind_frame_host_receiver =
      [](RenderFrameHostImpl* impl,
         mojo::PendingAssociatedReceiver<mojom::FrameHost> receiver) {
        impl->frame_host_associated_receiver_.Bind(std::move(receiver));
        impl->frame_host_associated_receiver_.SetFilter(
            impl->CreateMessageFilterForAssociatedReceiver(
                mojom::FrameHost::Name_));
      };
  associated_registry_->AddInterface(
      base::BindRepeating(bind_frame_host_receiver, base::Unretained(this)));

  associated_registry_->AddInterface(base::BindRepeating(
      [](RenderFrameHostImpl* impl,
         mojo::PendingAssociatedReceiver<
             blink::mojom::BackForwardCacheControllerHost> receiver) {
        impl->back_forward_cache_controller_host_associated_receiver_.Bind(
            std::move(receiver));
        impl->back_forward_cache_controller_host_associated_receiver_.SetFilter(
            CreateMessageFilterForAssociatedReceiverImpl(
                impl, blink::mojom::BackForwardCacheControllerHost::Name_,
                BackForwardCacheImpl::kMessagePolicyNone));
      },
      base::Unretained(this)));

  associated_registry_->AddInterface(base::BindRepeating(
      [](RenderFrameHostImpl* self,
         mojo::PendingAssociatedReceiver<blink::mojom::PortalHost> receiver) {
        Portal::BindPortalHostReceiver(self, std::move(receiver));
      },
      base::Unretained(this)));

  associated_registry_->AddInterface(base::BindRepeating(
      [](RenderFrameHostImpl* impl,
         mojo::PendingAssociatedReceiver<blink::mojom::LocalFrameHost>
             receiver) {
        impl->local_frame_host_receiver_.Bind(std::move(receiver));
        impl->local_frame_host_receiver_.SetFilter(
            impl->CreateMessageFilterForAssociatedReceiver(
                blink::mojom::LocalFrameHost::Name_));
      },
      base::Unretained(this)));

  if (frame_tree_node_->IsMainFrame()) {
    associated_registry_->AddInterface(base::BindRepeating(
        [](RenderFrameHostImpl* impl,
           mojo::PendingAssociatedReceiver<blink::mojom::LocalMainFrameHost>
               receiver) {
          impl->local_main_frame_host_receiver_.Bind(std::move(receiver));
          impl->local_main_frame_host_receiver_.SetFilter(
              impl->CreateMessageFilterForAssociatedReceiver(
                  blink::mojom::LocalMainFrameHost::Name_));
        },
        base::Unretained(this)));

    associated_registry_->AddInterface(base::BindRepeating(
        [](RenderFrameHostImpl* impl,
           mojo::PendingAssociatedReceiver<
               blink::mojom::ManifestUrlChangeObserver> receiver) {
          ManifestManagerHost::GetOrCreateForCurrentDocument(impl)
              ->BindObserver(std::move(receiver));
        },
        base::Unretained(this)));
  }

  associated_registry_->AddInterface(base::BindRepeating(
      [](RenderFrameHostImpl* impl,
         mojo::PendingAssociatedReceiver<mojom::RenderAccessibilityHost>
             receiver) {
        impl->render_accessibility_host_receiver_.Bind(std::move(receiver));
        impl->render_accessibility_host_receiver_.SetFilter(
            impl->CreateMessageFilterForAssociatedReceiver(
                mojom::RenderAccessibilityHost::Name_));
      },
      base::Unretained(this)));

  // TODO(crbug.com/1047354): How to avoid binding if the
  // BINDINGS_POLICY_DOM_AUTOMATION policy is not set?
  associated_registry_->AddInterface(base::BindRepeating(
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

  mojo::PendingRemote<mojom::FrameFactory> frame_factory;
  GetProcess()->BindReceiver(frame_factory.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::FrameFactory>(std::move(frame_factory))
      ->CreateFrame(routing_id_, frame_.BindNewPipeAndPassReceiver());

  // TODO(http://crbug.com/1014212): Change to DCHECK.
  CHECK(frame_);

  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      remote_interfaces;
  frame_->GetInterfaceProvider(
      remote_interfaces.InitWithNewPipeAndPassReceiver());
  remote_interfaces_ = std::make_unique<service_manager::InterfaceProvider>();
  remote_interfaces_->Bind(std::move(remote_interfaces));

  // Called to bind the receiver for this interface to the local frame. We need
  // to eagarly bind here because binding happens at normal priority on the main
  // thread and future calls to this interface need to be high priority.
  GetHighPriorityLocalFrame();
}

void RenderFrameHostImpl::InvalidateMojoConnection() {
  frame_.reset();
  frame_bindings_control_.reset();
  frame_host_associated_receiver_.reset();
  local_frame_.reset();
  local_main_frame_.reset();
  high_priority_local_frame_.reset();
  navigation_control_.reset();
  find_in_page_.reset();
  render_accessibility_.reset();

  // Disconnect with ImageDownloader Mojo service in Blink.
  mojo_image_downloader_.reset();

  // The geolocation service and sensor provider proxy may attempt to cancel
  // permission requests so they must be reset before the routing_id mapping is
  // removed.
  geolocation_service_.reset();
  sensor_provider_proxy_.reset();

  render_accessibility_host_receiver_.reset();
  local_frame_host_receiver_.reset();
  local_main_frame_host_receiver_.reset();
  associated_registry_.reset();
}

bool RenderFrameHostImpl::IsFocused() {
  if (!GetRenderWidgetHost()->is_focused() || !frame_tree_->GetFocusedFrame())
    return false;

  RenderFrameHostImpl* focused_rfh =
      frame_tree_->GetFocusedFrame()->current_frame_host();
  return focused_rfh == this || focused_rfh->IsDescendantOf(this);
}

bool RenderFrameHostImpl::CreateWebUI(const GURL& dest_url,
                                      int entry_bindings) {
  // Verify expectation that WebUI should not be created for error pages.
  DCHECK_NE(GetSiteInstance()->GetSiteInfo(), SiteInfo::CreateForErrorPage());

  WebUI::TypeID new_web_ui_type =
      WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          GetSiteInstance()->GetBrowserContext(), dest_url);
  CHECK_NE(new_web_ui_type, WebUI::kNoWebUI);

  // If |web_ui_| already exists, there is no need to create a new one. However,
  // it is useful to verify that its type hasn't changed. Site isolation
  // guarantees that RenderFrameHostImpl will be changed if the WebUI type
  // differs.
  if (web_ui_) {
    CHECK_EQ(new_web_ui_type, web_ui_type_);
    return false;
  }

  web_ui_ = delegate_->CreateWebUIForRenderFrameHost(this, dest_url);
  if (!web_ui_)
    return false;

  // If we have assigned (zero or more) bindings to the NavigationEntry in
  // the past, make sure we're not granting it different bindings than it
  // had before. If so, note it and don't give it any bindings, to avoid a
  // potential privilege escalation.
  if (entry_bindings != FrameNavigationEntry::kInvalidBindings &&
      web_ui_->GetBindings() != entry_bindings) {
    RecordAction(base::UserMetricsAction("ProcessSwapBindingsMismatch_RVHM"));
    ClearWebUI();
    return false;
  }

  // It is not expected for GuestView to be able to navigate to WebUI.
  DCHECK(!GetProcess()->IsForGuestsOnly());

  web_ui_type_ = new_web_ui_type;

  // WebUIs need the ability to request certain schemes.
  for (const auto& scheme : web_ui_->GetRequestableSchemes()) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestScheme(
        GetProcess()->GetID(), scheme);
  }

  // Since this is new WebUI instance, this RenderFrameHostImpl should not
  // have had any bindings. Verify that and grant the required bindings.
  DCHECK_EQ(0, GetEnabledBindings());
  AllowBindings(web_ui_->GetBindings());

  return true;
}

void RenderFrameHostImpl::ClearWebUI() {
  web_ui_type_ = WebUI::kNoWebUI;
  web_ui_.reset();
}

const mojo::Remote<blink::mojom::ImageDownloader>&
RenderFrameHostImpl::GetMojoImageDownloader() {
  if (!mojo_image_downloader_.is_bound() && GetRemoteInterfaces()) {
    GetRemoteInterfaces()->GetInterface(
        mojo_image_downloader_.BindNewPipeAndPassReceiver());
  }
  return mojo_image_downloader_;
}

const mojo::AssociatedRemote<blink::mojom::FindInPage>&
RenderFrameHostImpl::GetFindInPage() {
  if (!find_in_page_)
    GetRemoteAssociatedInterfaces()->GetInterface(&find_in_page_);
  return find_in_page_;
}

const mojo::AssociatedRemote<blink::mojom::LocalFrame>&
RenderFrameHostImpl::GetAssociatedLocalFrame() {
  if (!local_frame_)
    GetRemoteAssociatedInterfaces()->GetInterface(&local_frame_);
  return local_frame_;
}

blink::mojom::LocalMainFrame*
RenderFrameHostImpl::GetAssociatedLocalMainFrame() {
  DCHECK(frame_tree_node_->IsMainFrame());
  if (!local_main_frame_)
    GetRemoteAssociatedInterfaces()->GetInterface(&local_main_frame_);
  return local_main_frame_.get();
}

const mojo::Remote<blink::mojom::HighPriorityLocalFrame>&
RenderFrameHostImpl::GetHighPriorityLocalFrame() {
  if (!high_priority_local_frame_.is_bound()) {
    GetRemoteInterfaces()->GetInterface(
        high_priority_local_frame_.BindNewPipeAndPassReceiver());
  }
  return high_priority_local_frame_;
}

const mojo::AssociatedRemote<mojom::FrameBindingsControl>&
RenderFrameHostImpl::GetFrameBindingsControl() {
  if (!frame_bindings_control_)
    GetRemoteAssociatedInterfaces()->GetInterface(&frame_bindings_control_);
  return frame_bindings_control_;
}

void RenderFrameHostImpl::ResetLoadingState() {
  if (is_loading()) {
    // When pending deletion, just set the loading state to not loading.
    // Otherwise, DidStopLoading will take care of that, as well as sending
    // notification to the FrameTreeNode about the change in loading state.
    if (IsPendingDeletion() || IsInBackForwardCache())
      is_loading_ = false;
    else
      DidStopLoading();
  }
}

void RenderFrameHostImpl::ClearFocusedElement() {
  has_focused_editable_element_ = false;
  GetAssociatedLocalFrame()->ClearFocusedElement();
}

void RenderFrameHostImpl::BlockRequestsForFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ForEachFrame(this,
               base::BindRepeating([](RenderFrameHostImpl* render_frame_host) {
                 if (render_frame_host->frame_)
                   render_frame_host->frame_->BlockRequests();
               }));
}

void RenderFrameHostImpl::ResumeBlockedRequestsForFrame() {
  ForEachFrame(this,
               base::BindRepeating([](RenderFrameHostImpl* render_frame_host) {
                 if (render_frame_host->frame_)
                   render_frame_host->frame_->ResumeBlockedRequests();
               }));
}

void RenderFrameHostImpl::CancelBlockedRequestsForFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ForEachFrame(this,
               base::BindRepeating([](RenderFrameHostImpl* render_frame_host) {
                 if (render_frame_host->frame_)
                   render_frame_host->frame_->CancelBlockedRequests();
               }));
}

void RenderFrameHostImpl::BindDevToolsAgent(
    mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgentHost> host,
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> receiver) {
  GetNavigationControl()->BindDevToolsAgent(std::move(host),
                                            std::move(receiver));
}

bool RenderFrameHostImpl::IsSameSiteInstance(
    RenderFrameHostImpl* other_render_frame_host) {
  // As a sanity check, make sure the frame belongs to the same BrowserContext.
  CHECK_EQ(GetSiteInstance()->GetBrowserContext(),
           other_render_frame_host->GetSiteInstance()->GetBrowserContext());
  return GetSiteInstance() == other_render_frame_host->GetSiteInstance();
}

void RenderFrameHostImpl::UpdateAccessibilityMode() {
  // Don't update accessibility mode for a frame that hasn't been created yet.
  if (!IsRenderFrameCreated())
    return;

  ui::AXMode ax_mode = delegate_->GetAccessibilityMode();
  if (!ax_mode.has_mode(ui::AXMode::kWebContents)) {
    // Resetting the Remote signals the renderer to shutdown accessibility
    // in the renderer.
    render_accessibility_.reset();
    return;
  }

  if (!render_accessibility_) {
    // Render accessibility is not enabled yet, so bind the interface first.
    GetRemoteAssociatedInterfaces()->GetInterface(&render_accessibility_);
  }

  render_accessibility_->SetMode(ax_mode.mode());
}

void RenderFrameHostImpl::RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                                                ui::AXMode ax_mode) {
  // TODO(https://crbug.com/859110): Remove once frame_ can no longer be null.
  if (!IsRenderFrameLive())
    return;

  frame_->SnapshotAccessibilityTree(
      ax_mode.mode(),
      base::BindOnce(&RenderFrameHostImpl::RequestAXTreeSnapshotCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RenderFrameHostImpl::GetSavableResourceLinksFromRenderer() {
  if (!IsRenderFrameLive())
    return;

  GetAssociatedLocalFrame()->GetSavableResourceLinks(
      base::BindOnce(&RenderFrameHostImpl::GetSavableResourceLinksCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RenderFrameHostImpl::SetAccessibilityCallbackForTesting(
    const AccessibilityCallbackForTesting& callback) {
  accessibility_testing_callback_ = callback;
}

void RenderFrameHostImpl::UpdateAXTreeData() {
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.is_mode_off() || IsInactiveAndDisallowReactivation()) {
    return;
  }

  AXEventNotificationDetails detail;
  detail.ax_tree_id = GetAXTreeID();
  detail.updates.resize(1);
  detail.updates[0].has_tree_data = true;
  detail.updates[0].tree_data = GetAXTreeData();

  SendAccessibilityEventsToManager(detail);
  delegate_->AccessibilityEventReceived(detail);
}

BrowserAccessibilityManager*
RenderFrameHostImpl::GetOrCreateBrowserAccessibilityManager() {
  RenderWidgetHostViewBase* view = GetViewForAccessibility();
  if (view && !browser_accessibility_manager_ &&
      !no_create_browser_accessibility_manager_for_testing_) {
    browser_accessibility_manager_.reset(
        view->CreateBrowserAccessibilityManager(this, is_main_frame()));
  }
  return browser_accessibility_manager_.get();
}

void RenderFrameHostImpl::ActivateFindInPageResultForAccessibility(
    int request_id) {
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs)) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager)
      manager->ActivateFindInPageResult(request_id);
  }
}

void RenderFrameHostImpl::InsertVisualStateCallback(
    VisualStateCallback callback) {
  GetRenderWidgetHost()->InsertVisualStateCallback(std::move(callback));
}

bool RenderFrameHostImpl::IsRenderFrameCreated() {
  return render_frame_created_;
}

bool RenderFrameHostImpl::IsRenderFrameLive() {
  bool is_live =
      GetProcess()->IsInitializedAndNotDead() && render_frame_created_;

  // Sanity check: the RenderView should always be live if the RenderFrame is.
  DCHECK(!is_live || render_view_host_->IsRenderViewLive());

  return is_live;
}

bool RenderFrameHostImpl::IsCurrent() {
  RenderFrameHostImpl* rfh = this;
  // Check this RenderFrameHost and all its ancestors to see if they are the
  // current ones in their respective FrameTreeNodes.
  // It is important to check for all ancestors as when navigation commits a new
  // RenderFrameHost may replace one of the parents, swapping out the old with
  // its entire subtree but |this| will still be a current one in its
  // FrameTreeNode.
  while (rfh) {
    if (rfh->frame_tree_node()->current_frame_host() != rfh)
      return false;
    rfh = rfh->GetParent();
  }
  return true;
}

size_t RenderFrameHostImpl::GetProxyCount() {
  if (!IsCurrent())
    return 0;
  return frame_tree_node_->render_manager()->GetProxyCount();
}

bool RenderFrameHostImpl::HasSelection() {
  return has_selection_;
}

RenderFrameHostImpl* RenderFrameHostImpl::GetMainFrame() {
  // Iteration over the GetParent() chain is used below, because returning
  // |frame_tree()->root()->current_frame_host()| might
  // give an incorrect result after |this| has been detached from the frame
  // tree.
  RenderFrameHostImpl* main_frame = this;
  while (main_frame->GetParent())
    main_frame = main_frame->GetParent();
  return main_frame;
}

bool RenderFrameHostImpl::CanAccessFilesOfPageState(const PageState& state) {
  return ChildProcessSecurityPolicyImpl::GetInstance()->CanReadAllFiles(
      GetProcess()->GetID(), state.GetReferencedFiles());
}

void RenderFrameHostImpl::GrantFileAccessFromPageState(const PageState& state) {
  GrantFileAccess(GetProcess()->GetID(), state.GetReferencedFiles());
}

void RenderFrameHostImpl::GrantFileAccessFromResourceRequestBody(
    const network::ResourceRequestBody& body) {
  GrantFileAccess(GetProcess()->GetID(), body.GetReferencedFiles());
}

void RenderFrameHostImpl::UpdatePermissionsForNavigation(
    const mojom::CommonNavigationParams& common_params,
    const mojom::CommitNavigationParams& commit_params) {
  // Browser plugin guests are not allowed to navigate outside web-safe schemes,
  // so do not grant them the ability to commit additional URLs.
  if (!GetProcess()->IsForGuestsOnly()) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantCommitURL(
        GetProcess()->GetID(), common_params.url);
    if (NavigationRequest::IsLoadDataWithBaseURL(common_params)) {
      // When there's a base URL specified for the data URL, we also need to
      // grant access to the base URL. This allows file: and other unexpected
      // schemes to be accepted at commit time and during CORS checks (e.g., for
      // font requests).
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantCommitURL(
          GetProcess()->GetID(), common_params.base_url_for_data_url);
    }
  }

  // We may be returning to an existing NavigationEntry that had been granted
  // file access.  If this is a different process, we will need to grant the
  // access again.  Abuse is prevented, because the files listed in the page
  // state are validated earlier, when they are received from the renderer (in
  // RenderFrameHostImpl::CanAccessFilesOfPageState).
  if (commit_params.page_state.IsValid())
    GrantFileAccessFromPageState(commit_params.page_state);

  // We may be here after transferring navigation to a different renderer
  // process.  In this case, we need to ensure that the new renderer retains
  // ability to access files that the old renderer could access.  Abuse is
  // prevented, because the files listed in ResourceRequestBody are validated
  // earlier, when they are received from the renderer.
  if (common_params.post_data)
    GrantFileAccessFromResourceRequestBody(*common_params.post_data);
}

bool RenderFrameHostImpl::WindowPlacementAllowsFullscreen() {
  if (!delegate_->IsTransientAllowFullscreenActive())
    return false;
  auto* controller =
      PermissionControllerImpl::FromBrowserContext(GetBrowserContext());
  return controller &&
         controller->GetPermissionStatusForFrame(
             PermissionType::WINDOW_PLACEMENT, this, GetLastCommittedURL()) ==
             blink::mojom::PermissionStatus::GRANTED;
}

mojo::AssociatedRemote<mojom::NavigationClient>
RenderFrameHostImpl::GetNavigationClientFromInterfaceProvider() {
  mojo::AssociatedRemote<mojom::NavigationClient> navigation_client_remote;
  GetRemoteAssociatedInterfaces()->GetInterface(&navigation_client_remote);
  return navigation_client_remote;
}

void RenderFrameHostImpl::NavigationRequestCancelled(
    NavigationRequest* navigation_request) {
  OnCrossDocumentCommitProcessed(navigation_request,
                                 blink::mojom::CommitResult::Aborted);
}

NavigationRequest*
RenderFrameHostImpl::FindLatestNavigationRequestThatIsStillCommitting() {
  // Find the most recent NavigationRequest that has triggered a Commit IPC to
  // the renderer process.  Once the renderer process handles the IPC, it may
  // possibly change the origin from |last_committed_origin_| to another origin.
  NavigationRequest* found_request = nullptr;
  for (const auto& it : navigation_requests_) {
    NavigationRequest* candidate = it.first;
    DCHECK_EQ(candidate, it.second.get());

    if (candidate->state() < NavigationRequest::READY_TO_COMMIT)
      continue;
    if (candidate->state() >= NavigationRequest::DID_COMMIT)
      continue;

    if (!found_request ||
        found_request->NavigationStart() < candidate->NavigationStart()) {
      found_request = candidate;
    }
  }

  return found_request;
}

void RenderFrameHostImpl::
    ExtractFactoryParamsFromNavigationRequestOrLastCommittedNavigation(
        NavigationRequest* navigation_request,
        url::Origin* out_main_world_origin,
        network::mojom::ClientSecurityStatePtr* out_client_security_state,
        mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>*
            coep_reporter_pending_remote,
        network::mojom::TrustTokenRedemptionPolicy*
            out_trust_token_redemption_policy) {
  // |navigation_request| is optional.
  DCHECK(out_main_world_origin);
  DCHECK(out_client_security_state);
  // |coep_reporter_receiver| is optional.
  DCHECK(out_trust_token_redemption_policy);

  bool have_successful_request =
      navigation_request && navigation_request->GetNetErrorCode() == net::OK;
  bool have_failed_request =
      navigation_request && navigation_request->GetNetErrorCode() != net::OK;
  CrossOriginEmbedderPolicyReporter* coep_reporter = nullptr;

  if (have_successful_request) {
    // Return values based on the |navigation_request|.
    *out_main_world_origin = navigation_request->GetOriginForURLLoaderFactory();
    *out_client_security_state =
        mojo::Clone(navigation_request->client_security_state());
    coep_reporter = navigation_request->coep_reporter();
    *out_trust_token_redemption_policy =
        DetermineWhetherToForbidTrustTokenRedemption(
            GetParent(), navigation_request->commit_params(),
            *out_main_world_origin);
  } else if (have_failed_request) {
    // Some return values should always be the same for error pages.  Some
    // return values may be based on the |navigation_request|.

    // Error page will commit in an opaque origin.
    // TODO(lukasza): https://crbug.com/888079: Use this origin when sending the
    // commit IPC and setting |last_committed_origin_|.
    url::Origin error_page_origin = url::Origin();
    *out_main_world_origin = error_page_origin;

    *out_client_security_state =
        mojo::Clone(navigation_request->client_security_state());
    coep_reporter = nullptr;
    *out_trust_token_redemption_policy =
        network::mojom::TrustTokenRedemptionPolicy::kForbid;
  } else {
    // Use properties of the last committed navigation.
    *out_main_world_origin = last_committed_origin_;
    *out_client_security_state =
        mojo::Clone(last_committed_client_security_state_);
    coep_reporter = coep_reporter_.get();
    *out_trust_token_redemption_policy =
        DetermineAfterCommitWhetherToForbidTrustTokenRedemption(this);
  }

  if (coep_reporter && coep_reporter_pending_remote) {
    coep_reporter->Clone(
        coep_reporter_pending_remote->InitWithNewPipeAndPassReceiver());
  }
}

network::mojom::URLLoaderFactoryParamsPtr
RenderFrameHostImpl::CreateURLLoaderFactoryParamsForMainWorld(
    NavigationRequest* navigation_request,
    base::StringPiece debug_tag) {
  url::Origin main_world_origin;
  network::mojom::ClientSecurityStatePtr client_security_state;
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_remote;
  network::mojom::TrustTokenRedemptionPolicy trust_token_redemption_policy =
      network::mojom::TrustTokenRedemptionPolicy::kForbid;
  ExtractFactoryParamsFromNavigationRequestOrLastCommittedNavigation(
      navigation_request, &main_world_origin, &client_security_state,
      &coep_reporter_remote, &trust_token_redemption_policy);

  return URLLoaderFactoryParamsHelper::CreateForFrame(
      this, main_world_origin, std::move(client_security_state),
      std::move(coep_reporter_remote), GetProcess(),
      trust_token_redemption_policy, debug_tag);
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryAndObserve(
    network::mojom::URLLoaderFactoryParamsPtr params,
    base::UkmSourceId ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver) {
  bool bypass_redirect_checks = CreateNetworkServiceDefaultFactoryInternal(
      std::move(params), ukm_source_id, std::move(default_factory_receiver));

  // Add a disconnect handler when Network Service is running
  // out-of-process.
  if (IsOutOfProcessNetworkService() &&
      (!network_service_disconnect_handler_holder_ ||
       !network_service_disconnect_handler_holder_.is_connected())) {
    network_service_disconnect_handler_holder_.reset();
    StoragePartition* storage_partition = BrowserContext::GetStoragePartition(
        GetSiteInstance()->GetBrowserContext(), GetSiteInstance());
    network::mojom::URLLoaderFactoryParamsPtr monitoring_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    monitoring_factory_params->process_id = GetProcess()->GetID();
    monitoring_factory_params->debug_tag = "RFHI - monitoring_factory_params";

    // This factory should never be used to issue actual requests (i.e. it
    // should only be used to monitor for Network Service crashes).  Below is an
    // attempt to enforce that the factory cannot be used in practice.
    monitoring_factory_params->request_initiator_origin_lock =
        url::Origin::Create(
            GURL("https://monitoring.url.loader.factory.invalid"));

    storage_partition->GetNetworkContext()->CreateURLLoaderFactory(
        network_service_disconnect_handler_holder_.BindNewPipeAndPassReceiver(),
        std::move(monitoring_factory_params));
    network_service_disconnect_handler_holder_.set_disconnect_handler(
        base::BindOnce(&RenderFrameHostImpl::UpdateSubresourceLoaderFactories,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  return bypass_redirect_checks;
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryInternal(
    network::mojom::URLLoaderFactoryParamsPtr params,
    base::UkmSourceId ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver) {
  DCHECK(params->request_initiator_origin_lock.has_value());
  const url::Origin& request_initiator =
      params->request_initiator_origin_lock.value();

  bool bypass_redirect_checks = false;
  WillCreateURLLoaderFactory(
      request_initiator, &default_factory_receiver, ukm_source_id,
      &params->header_client, &bypass_redirect_checks,
      &params->disable_secure_dns, &params->factory_override);

  GetProcess()->CreateURLLoaderFactory(std::move(default_factory_receiver),
                                       std::move(params));

  return bypass_redirect_checks;
}

void RenderFrameHostImpl::WillCreateURLLoaderFactory(
    const url::Origin& request_initiator,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    base::UkmSourceId ukm_source_id,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      GetBrowserContext(), this, GetProcess()->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      request_initiator, base::nullopt /* navigation_id */, ukm_source_id,
      factory_receiver, header_client, bypass_redirect_checks,
      disable_secure_dns, factory_override);

  // Keep DevTools proxy last, i.e. closest to the network.
  devtools_instrumentation::WillCreateURLLoaderFactory(
      this, false /* is_navigation */, false /* is_download */,
      factory_receiver, factory_override);
}

bool RenderFrameHostImpl::CanExecuteJavaScript() {
  if (g_allow_injecting_javascript)
    return true;
  return !frame_tree_node_->current_url().is_valid() ||
         frame_tree_node_->current_url().SchemeIs(kChromeDevToolsScheme) ||
         ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
             GetProcess()->GetID()) ||
         // It's possible to load about:blank in a Web UI renderer.
         // See http://crbug.com/42547
         (frame_tree_node_->current_url().spec() == url::kAboutBlankURL) ||
         // InterstitialPageImpl should be the only case matching this.
         (delegate_->GetAsWebContents() == nullptr);
}

// static
int RenderFrameHost::GetFrameTreeNodeIdForRoutingId(int process_id,
                                                    int routing_id) {
  auto frame_or_proxy = LookupRenderFrameHostOrProxy(process_id, routing_id);
  if (frame_or_proxy)
    return frame_or_proxy.GetFrameTreeNode()->frame_tree_node_id();
  return kNoFrameTreeNodeId;
}

// static
RenderFrameHost* RenderFrameHost::FromPlaceholderToken(
    int render_process_id,
    const base::UnguessableToken& placeholder_frame_token) {
  RenderFrameProxyHost* rfph = RenderFrameProxyHost::FromFrameToken(
      render_process_id, placeholder_frame_token);
  FrameTreeNode* node = rfph ? rfph->frame_tree_node() : nullptr;
  return node ? node->current_frame_host() : nullptr;
}

ui::AXTreeID RenderFrameHostImpl::GetParentAXTreeID() {
  if (browser_plugin_embedder_ax_tree_id_ != ui::AXTreeIDUnknown())
    return browser_plugin_embedder_ax_tree_id_;

  if (!frame_tree_node_->IsMainFrame())
    return GetParent()->GetAXTreeID();

  return ui::AXTreeIDUnknown();
}

ui::AXTreeID RenderFrameHostImpl::GetFocusedAXTreeID() {
  // If this is not the root frame tree node, we're done.
  if (!is_main_frame())
    return ui::AXTreeIDUnknown();

  auto* focused_frame = static_cast<RenderFrameHostImpl*>(
      delegate_->GetFocusedFrameIncludingInnerWebContents());
  if (focused_frame)
    return focused_frame->GetAXTreeID();

  return ui::AXTreeIDUnknown();
}

ui::AXTreeData RenderFrameHostImpl::GetAXTreeData() {
  // Make sure to update the locally stored |ax_tree_data_| to include
  // references to the relevant AXTreeIDs before returning its value.
  ax_tree_data_.tree_id = GetAXTreeID();
  ax_tree_data_.parent_tree_id = GetParentAXTreeID();
  ax_tree_data_.focused_tree_id = GetFocusedAXTreeID();
  return ax_tree_data_;
}

void RenderFrameHostImpl::AccessibilityHitTestCallback(
    int request_id,
    ax::mojom::Event event_to_fire,
    base::OnceCallback<void(BrowserAccessibilityManager* hit_manager,
                            int hit_node_id)> opt_callback,
    mojom::HitTestResponsePtr hit_test_response) {
  if (!hit_test_response) {
    if (opt_callback)
      std::move(opt_callback).Run(nullptr, 0);
    return;
  }

  auto frame_or_proxy = LookupRenderFrameHostOrProxy(
      GetProcess()->GetID(), hit_test_response->hit_frame_token);
  RenderFrameHostImpl* hit_frame =
      frame_or_proxy.proxy
          ? frame_or_proxy.proxy->frame_tree_node()->current_frame_host()
          : frame_or_proxy.frame;

  if (!hit_frame || hit_frame->IsInactiveAndDisallowReactivation()) {
    if (opt_callback)
      std::move(opt_callback).Run(nullptr, 0);
    return;
  }

  // If the hit node's routing ID is the same frame, we're done. If a
  // callback was provided, call it with the information about the hit node.
  if (hit_frame->GetFrameToken() == frame_token_) {
    if (opt_callback) {
      std::move(opt_callback)
          .Run(hit_frame->browser_accessibility_manager(),
               hit_test_response->hit_node_id);
    }
    return;
  }

  // The hit node has a child frame. Do a hit test in that frame's renderer.
  hit_frame->AccessibilityHitTest(
      hit_test_response->hit_frame_transformed_point, event_to_fire, request_id,
      std::move(opt_callback));
}

void RenderFrameHostImpl::RequestAXTreeSnapshotCallback(
    AXTreeSnapshotCallback callback,
    const ui::AXTreeUpdate& snapshot) {
  ui::AXTreeUpdate dst_snapshot;
  dst_snapshot.root_id = snapshot.root_id;
  dst_snapshot.nodes.resize(snapshot.nodes.size());
  for (size_t i = 0; i < snapshot.nodes.size(); ++i)
    dst_snapshot.nodes[i] = snapshot.nodes[i];

  if (snapshot.has_tree_data) {
    ax_tree_data_ = snapshot.tree_data;
    dst_snapshot.tree_data = GetAXTreeData();
    dst_snapshot.has_tree_data = true;
  }
  std::move(callback).Run(dst_snapshot);
}

void RenderFrameHostImpl::CreatePaymentManager(
    mojo::PendingReceiver<payments::mojom::PaymentManager> receiver) {
  if (!IsFeatureEnabled(blink::mojom::FeaturePolicyFeature::kPayment)) {
    mojo::ReportBadMessage("Feature policy blocks Payment");
    return;
  }
  GetProcess()->CreatePaymentManagerForOrigin(GetLastCommittedOrigin(),
                                              std::move(receiver));

  // Blocklist PaymentManager from the back-forward cache as at the moment we
  // don't cancel pending payment requests when the RenderFrameHost is stored
  // in back-forward cache.
  OnSchedulerTrackedFeatureUsed(
      blink::scheduler::WebSchedulerTrackedFeature::kPaymentManager);
}

void RenderFrameHostImpl::CreateWebBluetoothService(
    mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver) {
  BackForwardCache::DisableForRenderFrameHost(this, "WebBluetooth");
  // RFHI owns |web_bluetooth_services_| and |web_bluetooth_service| owns the
  // |receiver_| which may run the error handler. |receiver_| can't run the
  // error handler after it's destroyed so it can't run after the RFHI is
  // destroyed.
  auto web_bluetooth_service =
      std::make_unique<WebBluetoothServiceImpl>(this, std::move(receiver));
  web_bluetooth_service->SetClientConnectionErrorHandler(
      base::BindOnce(&RenderFrameHostImpl::DeleteWebBluetoothService,
                     base::Unretained(this), web_bluetooth_service.get()));
  web_bluetooth_services_.push_back(std::move(web_bluetooth_service));
}

WebBluetoothServiceImpl*
RenderFrameHostImpl::GetWebBluetoothServiceForTesting() {
  DCHECK(web_bluetooth_services_.back());
  return web_bluetooth_services_.back().get();
}

void RenderFrameHostImpl::DeleteWebBluetoothService(
    WebBluetoothServiceImpl* web_bluetooth_service) {
  auto it = std::find_if(
      web_bluetooth_services_.begin(), web_bluetooth_services_.end(),
      [web_bluetooth_service](
          const std::unique_ptr<WebBluetoothServiceImpl>& service) {
        return web_bluetooth_service == service.get();
      });
  DCHECK(it != web_bluetooth_services_.end());
  web_bluetooth_services_.erase(it);
}

void RenderFrameHostImpl::CreateWebUsbService(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  BackForwardCache::DisableForRenderFrameHost(this, "WebUSB");
  GetContentClient()->browser()->CreateWebUsbService(this, std::move(receiver));
}

void RenderFrameHostImpl::ResetFeaturePolicy() {
  RenderFrameHostImpl* parent_frame_host = GetParent();
  if (!parent_frame_host && !frame_tree_node_->current_replication_state()
                                 .opener_feature_state.empty()) {
    DCHECK(base::FeatureList::IsEnabled(features::kFeaturePolicyForSandbox));
    feature_policy_ = blink::FeaturePolicy::CreateWithOpenerPolicy(
        frame_tree_node_->current_replication_state().opener_feature_state,
        last_committed_origin_);
    return;
  }
  const blink::FeaturePolicy* parent_policy =
      parent_frame_host ? parent_frame_host->feature_policy() : nullptr;
  blink::ParsedFeaturePolicy container_policy =
      frame_tree_node()->effective_frame_policy().container_policy;
  feature_policy_ = blink::FeaturePolicy::CreateFromParentPolicy(
      parent_policy, container_policy, last_committed_origin_);
}

void RenderFrameHostImpl::CreateAudioInputStreamFactory(
    mojo::PendingReceiver<blink::mojom::RendererAudioInputStreamFactory>
        receiver) {
  BrowserMainLoop* browser_main_loop = BrowserMainLoop::GetInstance();
  DCHECK(browser_main_loop);
  MediaStreamManager* msm = browser_main_loop->media_stream_manager();
  audio_service_audio_input_stream_factory_.emplace(std::move(receiver), msm,
                                                    this);
}

void RenderFrameHostImpl::CreateAudioOutputStreamFactory(
    mojo::PendingReceiver<blink::mojom::RendererAudioOutputStreamFactory>
        receiver) {
  media::AudioSystem* audio_system =
      BrowserMainLoop::GetInstance()->audio_system();
  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();
  audio_service_audio_output_stream_factory_.emplace(
      this, audio_system, media_stream_manager, std::move(receiver));
}

void RenderFrameHostImpl::GetFeatureObserver(
    mojo::PendingReceiver<blink::mojom::FeatureObserver> receiver) {
  if (!feature_observer_) {
    // Lazy initialize because tests sets the overridden content client
    // after the RFHI constructor.
    auto* client = GetContentClient()->browser()->GetFeatureObserverClient();
    if (!client)
      return;
    feature_observer_ = std::make_unique<FeatureObserver>(
        client, GlobalFrameRoutingId(GetProcess()->GetID(), routing_id_));
  }
  feature_observer_->GetFeatureObserver(std::move(receiver));
}

void RenderFrameHostImpl::BindScreenEnumerationReceiver(
    mojo::PendingReceiver<blink::mojom::ScreenEnumeration> receiver) {
  if (!screen_enumeration_impl_)
    screen_enumeration_impl_ = std::make_unique<ScreenEnumerationImpl>(this);
  screen_enumeration_impl_->Bind(std::move(receiver));
}

void RenderFrameHostImpl::BindMediaInterfaceFactoryReceiver(
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver) {
  if (!media_interface_proxy_) {
    media_interface_proxy_ = std::make_unique<MediaInterfaceProxy>(this);
  }
  media_interface_proxy_->Bind(std::move(receiver));
}

void RenderFrameHostImpl::BindMediaMetricsProviderReceiver(
    mojo::PendingReceiver<media::mojom::MediaMetricsProvider> receiver) {
  // Only save decode stats when BrowserContext provides a VideoPerfHistory.
  // Off-the-record contexts will internally use an ephemeral history DB.
  media::VideoDecodePerfHistory::SaveCallback save_stats_cb;
  if (GetSiteInstance()->GetBrowserContext()->GetVideoDecodePerfHistory()) {
    save_stats_cb = GetSiteInstance()
                        ->GetBrowserContext()
                        ->GetVideoDecodePerfHistory()
                        ->GetSaveCallback();
  }

  media::MediaMetricsProvider::Create(
      GetProcess()->GetBrowserContext()->IsOffTheRecord()
          ? media::MediaMetricsProvider::BrowsingMode::kIncognito
          : media::MediaMetricsProvider::BrowsingMode::kNormal,
      frame_tree_node_->IsMainFrame()
          ? media::MediaMetricsProvider::FrameStatus::kTopFrame
          : media::MediaMetricsProvider::FrameStatus::kNotTopFrame,
      base::BindRepeating(
          &RenderFrameHostDelegate::
              GetUkmSourceIdForLastCommittedSourceIncludingSameDocument,
          // This callback is only executed when Create() is called, during
          // which the lifetime of the |delegate_| is guaranteed.
          base::Unretained(delegate_)),
      base::BindRepeating(
          [](RenderFrameHostImpl* frame) {
            return ::media::learning::FeatureValue(
                frame->GetLastCommittedOrigin().host());
          },
          // Same as above.
          base::Unretained(this)),
      std::move(save_stats_cb),
      base::BindRepeating(
          [](base::WeakPtr<RenderFrameHostImpl> frame)
              -> media::learning::LearningSession* {
            if (!base::FeatureList::IsEnabled(media::kMediaLearningFramework) ||
                !frame) {
              return nullptr;
            }

            return frame->GetProcess()
                ->GetBrowserContext()
                ->GetLearningSession();
          },
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &RenderFrameHostImpl::GetRecordAggregateWatchTimeCallback,
          base::Unretained(this)),
      std::move(receiver));
}

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
void RenderFrameHostImpl::BindMediaRemoterFactoryReceiver(
    mojo::PendingReceiver<media::mojom::RemoterFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<RemoterFactoryImpl>(GetProcess()->GetID(), routing_id_),
      std::move(receiver));
}
#endif

void RenderFrameHostImpl::CreateWebSocketConnector(
    mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebSocketConnectorImpl>(
                                  GetProcess()->GetID(), routing_id_,
                                  last_committed_origin_, isolation_info_),
                              std::move(receiver));
}

void RenderFrameHostImpl::CreateQuicTransportConnector(
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<QuicTransportConnectorImpl>(
          GetProcess()->GetID(), weak_ptr_factory_.GetWeakPtr(),
          last_committed_origin_, isolation_info_.network_isolation_key()),
      std::move(receiver));
}

void RenderFrameHostImpl::CreateNotificationService(
    mojo::PendingReceiver<blink::mojom::NotificationService> receiver) {
  GetProcess()->CreateNotificationService(GetLastCommittedOrigin(),
                                          std::move(receiver));
}

void RenderFrameHostImpl::CreateInstalledAppProvider(
    mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver) {
  InstalledAppProviderImpl::Create(this, std::move(receiver));
}

void RenderFrameHostImpl::CreateDedicatedWorkerHostFactory(
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver) {
  // Allocate the worker in the same process as the creator.
  int worker_process_id = GetProcess()->GetID();

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  auto coep_reporter_endpoint = coep_reporter.InitWithNewPipeAndPassReceiver();
  if (coep_reporter_)
    coep_reporter_->Clone(std::move(coep_reporter_endpoint));

  // When a dedicated worker is created from the frame script, the frame is both
  // the creator and the ancestor.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DedicatedWorkerHostFactoryImpl>(
          worker_process_id,
          /*creator_render_frame_host_id=*/GetGlobalFrameRoutingId(),
          /*ancestor_render_frame_host_id=*/GetGlobalFrameRoutingId(),
          last_committed_origin_, cross_origin_embedder_policy_,
          std::move(coep_reporter)),
      std::move(receiver));
}

#if defined(OS_ANDROID)
void RenderFrameHostImpl::BindNFCReceiver(
    mojo::PendingReceiver<device::mojom::NFC> receiver) {
  delegate_->GetNFC(this, std::move(receiver));
}
#endif

#if !defined(OS_ANDROID)
void RenderFrameHostImpl::BindSerialService(
    mojo::PendingReceiver<blink::mojom::SerialService> receiver) {
  if (!IsFeatureEnabled(blink::mojom::FeaturePolicyFeature::kSerial)) {
    mojo::ReportBadMessage("Feature policy blocks access to Serial.");
    return;
  }

  SerialService::GetOrCreateForCurrentDocument(this)->Bind(std::move(receiver));
}

void RenderFrameHostImpl::BindAuthenticatorReceiver(
    mojo::PendingReceiver<blink::mojom::Authenticator> receiver) {
  if (!authenticator_impl_)
    authenticator_impl_ = std::make_unique<AuthenticatorImpl>(this);

  authenticator_impl_->Bind(std::move(receiver));
}

void RenderFrameHostImpl::GetHidService(
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  HidService::Create(this, std::move(receiver));
}
#endif

IdleManager* RenderFrameHostImpl::GetIdleManager() {
  return idle_manager_.get();
}

void RenderFrameHostImpl::BindIdleManager(
    mojo::PendingReceiver<blink::mojom::IdleManager> receiver) {
  if (!IsFeatureEnabled(blink::mojom::FeaturePolicyFeature::kIdleDetection)) {
    mojo::ReportBadMessage("Feature policy blocks access to IdleDetection.");
    return;
  }

  idle_manager_->CreateService(std::move(receiver),
                               GetMainFrame()->GetLastCommittedOrigin());
  OnSchedulerTrackedFeatureUsed(
      blink::scheduler::WebSchedulerTrackedFeature::kIdleManager);
}

void RenderFrameHostImpl::GetPresentationService(
    mojo::PendingReceiver<blink::mojom::PresentationService> receiver) {
  if (!presentation_service_)
    presentation_service_ = PresentationServiceImpl::Create(this);
  presentation_service_->Bind(std::move(receiver));
}

PresentationServiceImpl&
RenderFrameHostImpl::GetPresentationServiceForTesting() {
  DCHECK(presentation_service_);
  return *presentation_service_.get();
}

void RenderFrameHostImpl::GetSpeechSynthesis(
    mojo::PendingReceiver<blink::mojom::SpeechSynthesis> receiver) {
  if (!speech_synthesis_impl_) {
    speech_synthesis_impl_ = std::make_unique<SpeechSynthesisImpl>(
        GetProcess()->GetBrowserContext(), delegate_->GetAsWebContents());
  }
  speech_synthesis_impl_->AddReceiver(std::move(receiver));

  // Blocklist SpeechSynthesis for BackForwardCache, because currently we do not
  // handle speech synthesis after placing the page in BackForwardCache.
  // TODO(sreejakshetty): Make SpeechSynthesis compatible with BackForwardCache.
  OnSchedulerTrackedFeatureUsed(
      blink::scheduler::WebSchedulerTrackedFeature::kSpeechSynthesis);
}

void RenderFrameHostImpl::GetSensorProvider(
    mojo::PendingReceiver<device::mojom::SensorProvider> receiver) {
  if (!sensor_provider_proxy_) {
    sensor_provider_proxy_ = std::make_unique<SensorProviderProxyImpl>(
        PermissionControllerImpl::FromBrowserContext(
            GetProcess()->GetBrowserContext()),
        this);
  }
  sensor_provider_proxy_->Bind(std::move(receiver));
}

void RenderFrameHostImpl::BindCacheStorage(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_remote;
  if (coep_reporter_) {
    coep_reporter_->Clone(
        coep_reporter_remote.InitWithNewPipeAndPassReceiver());
  }
  GetProcess()->BindCacheStorage(cross_origin_embedder_policy_,
                                 std::move(coep_reporter_remote),
                                 GetLastCommittedOrigin(), std::move(receiver));
}

void RenderFrameHostImpl::BindInputInjectorReceiver(
    mojo::PendingReceiver<mojom::InputInjector> receiver) {
  InputInjectorImpl::Create(weak_ptr_factory_.GetWeakPtr(),
                            std::move(receiver));
}

void RenderFrameHostImpl::BindSmsReceiverReceiver(
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver) {
  if (GetParent() && !GetMainFrame()->GetLastCommittedOrigin().IsSameOriginWith(
                         GetLastCommittedOrigin())) {
    mojo::ReportBadMessage("Must have the same origin as the top-level frame.");
    return;
  }
  auto* fetcher = SmsFetcher::Get(GetProcess()->GetBrowserContext());
  SmsService::Create(fetcher, this, std::move(receiver));
  document_used_web_otp_ = true;
}

void RenderFrameHostImpl::BindRestrictedCookieManager(
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition())
      ->CreateRestrictedCookieManager(
          network::mojom::RestrictedCookieManagerRole::SCRIPT,
          GetLastCommittedOrigin(), isolation_info_.site_for_cookies(),
          ComputeTopFrameOrigin(GetLastCommittedOrigin()),
          /* is_service_worker = */ false, GetProcess()->GetID(), routing_id(),
          std::move(receiver), CreateCookieAccessObserver());
}

void RenderFrameHostImpl::BindHasTrustTokensAnswerer(
    mojo::PendingReceiver<network::mojom::HasTrustTokensAnswerer> receiver) {
  auto top_frame_origin = ComputeTopFrameOrigin(GetLastCommittedOrigin());

  // A check at the callsite in the renderer ensures a correctly functioning
  // renderer will only request this Mojo handle if the top-frame origin is
  // potentially trustworthy and has scheme HTTP or HTTPS.
  if ((top_frame_origin.scheme() != url::kHttpScheme &&
       top_frame_origin.scheme() != url::kHttpsScheme) ||
      !network::IsOriginPotentiallyTrustworthy(top_frame_origin)) {
    mojo::ReportBadMessage(
        "Attempted to get a HasTrustTokensAnswerer for a non-trustworthy or "
        "non-HTTP/HTTPS top-frame origin.");
    return;
  }

  // This is enforced in benign renderers by the RuntimeEnabled=TrustTokens IDL
  // attribute (the base::Feature's value is tied to the
  // RuntimeEnabledFeature's).
  if (!base::FeatureList::IsEnabled(network::features::kTrustTokens)) {
    mojo::ReportBadMessage(
        "Attempted to get a HasTrustTokensAnswerer with Trust Tokens "
        "disabled.");
    return;
  }

  // TODO(crbug.com/1145346): Document.hasTrustToken is restricted to secure
  // contexts, so we could additionally add a check verifying that the bind
  // request "is coming from a secure context"---but there's currently no
  // direct way to perform such a check in the browser.

  GetProcess()->GetStoragePartition()->CreateHasTrustTokensAnswerer(
      std::move(receiver), ComputeTopFrameOrigin(GetLastCommittedOrigin()));
}

void RenderFrameHostImpl::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // Requests are serviced on |document_scoped_interface_provider_receiver_|. It
  // is therefore safe to assume that every incoming interface request is coming
  // from the currently active document in the corresponding RenderFrame.
  // NOTE: this way of acquiring interfaces has been replaced with
  // BrowserInterfaceBroker (see https://crbug.com/718652). The code below is
  // left to support the CastWebContentsImpl use case.
  delegate_->OnInterfaceRequest(this, interface_name, &interface_pipe);
}

void RenderFrameHostImpl::CreateAppCacheBackend(
    mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(StoragePartition::IsAppCacheEnabled());
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "CreateAppCacheBackend-data", base::debug::CrashKeySize::Size64);
  std::string data = base::StringPrintf(
      "f=%d br=%d irfl=%d iiand=%d fid=%d", frame_.is_bound(),
      broker_receiver_.is_bound(), IsRenderFrameLive(),
      GetProcess()->IsInitializedAndNotDead(),
      RenderProcessHost::FromID(GetProcess()->GetID()) != nullptr);
  base::debug::ScopedCrashKeyString scoped_crash_key(crash_key, data);

  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition());
  auto* appcache_service = storage_partition_impl->GetAppCacheService();
  // CreateAppCacheBackend should only be called if AppCache is enabled
  // (which implies the service exists).
  DCHECK(appcache_service);
  appcache_service->CreateBackend(GetProcess()->GetID(), routing_id_,
                                  std::move(receiver));
}

void RenderFrameHostImpl::GetAudioContextManager(
    mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver) {
  AudioContextManagerImpl::Create(this, std::move(receiver));
}

void RenderFrameHostImpl::GetContactsManager(
    mojo::PendingReceiver<blink::mojom::ContactsManager> receiver) {
  ContactsManagerImpl::Create(this, std::move(receiver));
}

void RenderFrameHostImpl::GetFileSystemManager(
    mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver) {
  // This is safe because file_system_manager_ is deleted on the IO thread
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemManagerImpl::BindReceiver,
                                base::Unretained(file_system_manager_.get()),
                                std::move(receiver)));
}

void RenderFrameHostImpl::GetGeolocationService(
    mojo::PendingReceiver<blink::mojom::GeolocationService> receiver) {
  if (!geolocation_service_) {
    auto* geolocation_context = delegate_->GetGeolocationContext();
    if (!geolocation_context)
      return;
    geolocation_service_ =
        std::make_unique<GeolocationServiceImpl>(geolocation_context, this);
  }
  geolocation_service_->Bind(std::move(receiver));
}

void RenderFrameHostImpl::GetFontAccessManager(
    mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver) {
  static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition())
      ->GetFontAccessManager()
      ->BindReceiver(FontAccessManagerImpl::BindingContext(
                         GetLastCommittedOrigin(), GetGlobalFrameRoutingId()),
                     std::move(receiver));
}

void RenderFrameHostImpl::GetNativeFileSystemManager(
    mojo::PendingReceiver<blink::mojom::NativeFileSystemManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* storage_partition =
      static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition());
  auto* manager = storage_partition->GetNativeFileSystemManager();
  manager->BindReceiver(NativeFileSystemManagerImpl::BindingContext(
                            GetLastCommittedOrigin(), GetLastCommittedURL(),
                            GetGlobalFrameRoutingId()),
                        std::move(receiver));
}

void RenderFrameHostImpl::CreateLockManager(
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  GetProcess()->CreateLockManager(GetRoutingID(), GetLastCommittedOrigin(),
                                  std::move(receiver));
}

void RenderFrameHostImpl::CreateIDBFactory(
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  GetProcess()->BindIndexedDB(GetLastCommittedOrigin(), std::move(receiver));
}

void RenderFrameHostImpl::CreatePermissionService(
    mojo::PendingReceiver<blink::mojom::PermissionService> receiver) {
  if (!permission_service_context_)
    permission_service_context_ =
        std::make_unique<PermissionServiceContext>(this);

  permission_service_context_->CreateService(std::move(receiver));
}

void RenderFrameHostImpl::GetAuthenticator(
    mojo::PendingReceiver<blink::mojom::Authenticator> receiver) {
#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebAuth)) {
    BindAuthenticatorReceiver(std::move(receiver));
  }
#else
  GetJavaInterfaces()->GetInterface(std::move(receiver));
#endif  // !defined(OS_ANDROID)
}

void RenderFrameHostImpl::GetPushMessaging(
    mojo::PendingReceiver<blink::mojom::PushMessaging> receiver) {
  if (!push_messaging_manager_) {
    push_messaging_manager_.reset(new PushMessagingManager(
        GetProcess()->GetID(), routing_id_,
        static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition())
            ->GetServiceWorkerContext()));
  }

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&PushMessagingManager::AddPushMessagingReceiver,
                     push_messaging_manager_->AsWeakPtr(),
                     std::move(receiver)));
}

void RenderFrameHostImpl::GetVirtualAuthenticatorManager(
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
        receiver) {
#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebAuth) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebAuthDeprecatedMojoTestingApi)) {
    auto* environment_singleton = AuthenticatorEnvironmentImpl::GetInstance();
    environment_singleton->EnableVirtualAuthenticatorFor(frame_tree_node_);
    environment_singleton->AddVirtualAuthenticatorReceiver(frame_tree_node_,
                                                           std::move(receiver));
  }
#endif  // !defined(OS_ANDROID)
}

std::unique_ptr<NavigationRequest>
RenderFrameHostImpl::CreateNavigationRequestForCommit(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_same_document) {
  std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter;
  // We don't switch the COEP reporter on same-document navigations, so create
  // one only for cross-document navigations.
  if (!is_same_document) {
    coep_reporter = std::make_unique<CrossOriginEmbedderPolicyReporter>(
        GetProcess()->GetStoragePartition(), params.url,
        cross_origin_embedder_policy_.reporting_endpoint,
        cross_origin_embedder_policy_.report_only_reporting_endpoint);
  }
  std::unique_ptr<WebBundleNavigationInfo> web_bundle_navigation_info;
  if (is_same_document && web_bundle_handle_ &&
      web_bundle_handle_->navigation_info()) {
    // Need to set |web_bundle_navigation_info| of NavigationRequest. This
    // will be passed to FrameNavigationEntry, and will be used for subsequent
    // history navigations.
    web_bundle_navigation_info = web_bundle_handle_->navigation_info()->Clone();
  }
  return NavigationRequest::CreateForCommit(
      frame_tree_node_, this, params, std::move(coep_reporter),
      is_same_document, std::move(web_bundle_navigation_info));
}

bool RenderFrameHostImpl::NavigationRequestWasIntendedForPendingEntry(
    NavigationRequest* request,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool same_document) {
  NavigationEntryImpl* pending_entry = NavigationEntryImpl::FromNavigationEntry(
      frame_tree()->controller()->GetPendingEntry());
  if (!pending_entry)
    return false;
  if (request->nav_entry_id() != pending_entry->GetUniqueID())
    return false;
  if (!same_document) {
    // Make sure that the pending entry was really loaded via
    // LoadDataWithBaseURL and that it matches this handle.
    // TODO(csharrison): The pending entry's base url should equal
    // |params.base_url|. This is not the case for loads with invalid base urls.
    if (request->common_params().url != params.base_url ||
        pending_entry->GetBaseURLForDataURL().is_empty()) {
      return false;
    }
  }
  return true;
}

void RenderFrameHostImpl::BeforeUnloadTimeout() {
  if (render_view_host_->GetDelegate()->ShouldIgnoreUnresponsiveRenderer())
    return;

  SimulateBeforeUnloadCompleted(true /* proceed */);
}

void RenderFrameHostImpl::SetLastCommittedSiteInfo(const GURL& url) {
  // Since |url| has already committed, |origin_requests_isolation| below should
  // be set to false.
  SiteInfo site_info =
      url.is_empty()
          ? SiteInfo()
          : SiteInstanceImpl::ComputeSiteInfo(
                GetSiteInstance()->GetIsolationContext(),
                UrlInfo(url, false /* origin_requests_isolation */),
                GetSiteInstance()->IsCoopCoepCrossOriginIsolated(),
                GetSiteInstance()->CoopCoepCrossOriginIsolatedOrigin());

  if (last_committed_site_info_ == site_info)
    return;

  if (!last_committed_site_info_.site_url().is_empty()) {
    RenderProcessHostImpl::RemoveFrameWithSite(
        frame_tree()->controller()->GetBrowserContext(), GetProcess(),
        last_committed_site_info_);
  }

  last_committed_site_info_ = site_info;

  if (!last_committed_site_info_.site_url().is_empty()) {
    RenderProcessHostImpl::AddFrameWithSite(
        frame_tree()->controller()->GetBrowserContext(), GetProcess(),
        last_committed_site_info_);
  }
}

#if defined(OS_ANDROID)

class RenderFrameHostImpl::JavaInterfaceProvider
    : public service_manager::mojom::InterfaceProvider {
 public:
  using BindCallback =
      base::RepeatingCallback<void(const std::string&,
                                   mojo::ScopedMessagePipeHandle)>;

  JavaInterfaceProvider(
      const BindCallback& bind_callback,
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver)
      : bind_callback_(bind_callback), receiver_(this, std::move(receiver)) {}
  ~JavaInterfaceProvider() override = default;

 private:
  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle handle) override {
    bind_callback_.Run(interface_name, std::move(handle));
  }

  const BindCallback bind_callback_;
  mojo::Receiver<service_manager::mojom::InterfaceProvider> receiver_;

  DISALLOW_COPY_AND_ASSIGN(JavaInterfaceProvider);
};

base::android::ScopedJavaLocalRef<jobject>
RenderFrameHostImpl::GetJavaRenderFrameHost() {
  RenderFrameHostAndroid* render_frame_host_android =
      static_cast<RenderFrameHostAndroid*>(
          GetUserData(kRenderFrameHostAndroidKey));
  if (!render_frame_host_android) {
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        interface_provider_remote;
    java_interface_registry_ = std::make_unique<JavaInterfaceProvider>(
        base::BindRepeating(
            &RenderFrameHostImpl::ForwardGetInterfaceToRenderFrame,
            weak_ptr_factory_.GetWeakPtr()),
        interface_provider_remote.InitWithNewPipeAndPassReceiver());
    render_frame_host_android =
        new RenderFrameHostAndroid(this, std::move(interface_provider_remote));
    SetUserData(kRenderFrameHostAndroidKey,
                base::WrapUnique(render_frame_host_android));
  }
  return render_frame_host_android->GetJavaObject();
}

service_manager::InterfaceProvider* RenderFrameHostImpl::GetJavaInterfaces() {
  if (!java_interfaces_) {
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider> provider;
    BindInterfaceRegistryForRenderFrameHost(
        provider.InitWithNewPipeAndPassReceiver(), this);
    java_interfaces_.reset(new service_manager::InterfaceProvider);
    java_interfaces_->Bind(std::move(provider));
  }
  return java_interfaces_.get();
}

void RenderFrameHostImpl::ForwardGetInterfaceToRenderFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle pipe) {
  GetRemoteInterfaces()->GetInterfaceByName(interface_name, std::move(pipe));
}
#endif

void RenderFrameHostImpl::ForEachImmediateLocalRoot(
    const base::RepeatingCallback<void(RenderFrameHostImpl*)>& callback) {
  if (!frame_tree_node_->child_count())
    return;

  base::queue<FrameTreeNode*> queue;
  for (size_t index = 0; index < frame_tree_node_->child_count(); ++index)
    queue.push(frame_tree_node_->child_at(index));
  while (queue.size()) {
    FrameTreeNode* current = queue.front();
    queue.pop();
    if (current->current_frame_host()->is_local_root()) {
      callback.Run(current->current_frame_host());
    } else {
      for (size_t index = 0; index < current->child_count(); ++index)
        queue.push(current->child_at(index));
    }
  }
}

void RenderFrameHostImpl::SetVisibilityForChildViews(bool visible) {
  ForEachImmediateLocalRoot(base::BindRepeating(
      [](bool is_visible, RenderFrameHostImpl* frame_host) {
        if (auto* view = frame_host->GetView())
          return is_visible ? view->Show() : view->Hide();
      },
      visible));
}

mojom::FrameNavigationControl* RenderFrameHostImpl::GetNavigationControl() {
  if (!navigation_control_)
    GetRemoteAssociatedInterfaces()->GetInterface(&navigation_control_);
  return navigation_control_.get();
}

bool RenderFrameHostImpl::ShouldBypassSecurityChecksForErrorPage(
    NavigationRequest* navigation_request,
    bool* should_commit_unreachable_url) {
  if (should_commit_unreachable_url)
    *should_commit_unreachable_url = false;

  if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(is_main_frame())) {
    if (GetSiteInstance()->GetSiteInfo() == SiteInfo::CreateForErrorPage()) {
      if (should_commit_unreachable_url)
        *should_commit_unreachable_url = true;

      // With error page isolation, any URL can commit in an error page process.
      return true;
    }
  } else {
    // Without error page isolation, a blocked navigation is expected to
    // commit in the old renderer process.  This may be true for subframe
    // navigations even when error page isolation is enabled for main frames.
    if (navigation_request &&
        net::IsRequestBlockedError(navigation_request->GetNetErrorCode())) {
      return true;
    }
  }

  return false;
}

void RenderFrameHostImpl::SetAudioOutputDeviceIdForGlobalMediaControls(
    std::string hashed_device_id) {
  audio_service_audio_output_stream_factory_
      ->SetAuthorizedDeviceIdForGlobalMediaControls(
          std::move(hashed_device_id));
}

std::unique_ptr<mojo::MessageFilter>
RenderFrameHostImpl::CreateMessageFilterForAssociatedReceiver(
    const char* interface_name) {
  return CreateMessageFilterForAssociatedReceiverImpl(
      this, interface_name,
      BackForwardCacheImpl::GetChannelAssociatedMessageHandlingPolicy());
}

bool RenderFrameHostImpl::IsNavigationSameSite(
    const UrlInfo& dest_url_info,
    bool is_coop_coep_cross_origin_isolated,
    base::Optional<url::Origin> coop_coep_cross_origin_isolated_origin) {
  if (GetSiteInstance()->IsCoopCoepCrossOriginIsolated() !=
          is_coop_coep_cross_origin_isolated ||
      GetSiteInstance()->CoopCoepCrossOriginIsolatedOrigin() !=
          coop_coep_cross_origin_isolated_origin) {
    return false;
  }
  return GetSiteInstance()->IsNavigationSameSite(
      last_successful_url(), GetLastCommittedOrigin(),
      frame_tree_node()->IsMainFrame(), dest_url_info);
}

bool RenderFrameHostImpl::ValidateDidCommitParams(
    NavigationRequest* navigation_request,
    FrameHostMsg_DidCommitProvisionalLoad_Params* params,
    bool is_same_document_navigation) {
  RenderProcessHost* process = GetProcess();

  // Error pages may sometimes commit a URL in the wrong process, which requires
  // an exception for the CanCommitOriginAndUrl() checks.  This is ok as long
  // as the origin is opaque.
  bool should_commit_unreachable_url = false;
  bool bypass_checks_for_error_page = ShouldBypassSecurityChecksForErrorPage(
      navigation_request, &should_commit_unreachable_url);

  // Commits in the error page process must only be failures, otherwise
  // successful navigations could commit documents from origins different
  // than the chrome-error://chromewebdata/ one and violate expectations.
  if (should_commit_unreachable_url && !params->url_is_unreachable) {
    DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, params->origin);
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_ERROR_PROCESS_NON_ERROR_COMMIT);
    return false;
  }

  // Error pages must commit in a opaque origin. Terminate the renderer
  // process if this is violated.
  if (bypass_checks_for_error_page && !params->origin.opaque()) {
    DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, params->origin);
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_ERROR_PROCESS_NON_UNIQUE_ORIGIN_COMMIT);
    return false;
  }

  // file: URLs can be allowed to access any other origin, based on settings.
  bool bypass_checks_for_file_scheme = false;
  if (params->origin.scheme() == url::kFileScheme) {
    auto prefs = GetOrCreateWebPreferences();
    if (prefs.allow_universal_access_from_file_urls)
      bypass_checks_for_file_scheme = true;
  }

  // WebView's loadDataWithBaseURL API is allowed to bypass normal commit
  // checks because it is allowed to commit anything into its unlocked process
  // and its data: URL and non-opaque origin would fail the normal commit
  // checks.
  bool bypass_checks_for_webview = false;
  if ((navigation_request && NavigationRequest::IsLoadDataWithBaseURL(
                                 navigation_request->common_params())) ||
      (is_same_document_navigation && IsLoadDataWithBaseURL(*params))) {
    // Allow bypass if the process isn't locked. Otherwise run normal checks.
    bypass_checks_for_webview = !ChildProcessSecurityPolicyImpl::GetInstance()
                                     ->GetProcessLock(process->GetID())
                                     .is_locked_to_site();
  }

  if (!bypass_checks_for_error_page && !bypass_checks_for_file_scheme &&
      !bypass_checks_for_webview) {
    // Attempts to commit certain off-limits URL should be caught more strictly
    // than our FilterURL checks.  If a renderer violates this policy, it
    // should be killed.
    switch (CanCommitOriginAndUrl(params->origin, params->url)) {
      case CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL:
        // The origin and URL are safe to commit.
        break;
      case CanCommitStatus::CANNOT_COMMIT_URL:
        DLOG(ERROR) << "CANNOT_COMMIT_URL url '" << params->url << "'"
                    << " origin '" << params->origin << "'"
                    << " lock '"
                    << ChildProcessSecurityPolicyImpl::GetInstance()
                           ->GetProcessLock(process->GetID())
                           .ToString()
                    << "'";
        VLOG(1) << "Blocked URL " << params->url.spec();
        LogCannotCommitUrlCrashKeys(params->url, is_same_document_navigation,
                                    navigation_request);

        // Kills the process.
        bad_message::ReceivedBadMessage(
            process, bad_message::RFH_CAN_COMMIT_URL_BLOCKED);
        return false;
      case CanCommitStatus::CANNOT_COMMIT_ORIGIN:
        DLOG(ERROR) << "CANNOT_COMMIT_ORIGIN url '" << params->url << "'"
                    << " origin '" << params->origin << "'"
                    << " lock '"
                    << ChildProcessSecurityPolicyImpl::GetInstance()
                           ->GetProcessLock(process->GetID())
                           .ToString()
                    << "'";
        DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, params->origin);
        LogCannotCommitOriginCrashKeys(is_same_document_navigation,
                                       navigation_request);

        // Kills the process.
        bad_message::ReceivedBadMessage(
            process, bad_message::RFH_INVALID_ORIGIN_ON_COMMIT);
        return false;
    }
  }

  // Without this check, an evil renderer can trick the browser into creating
  // a navigation entry for a banned URL.  If the user clicks the back button
  // followed by the forward button (or clicks reload, or round-trips through
  // session restore, etc), we'll think that the browser commanded the
  // renderer to load the URL and grant the renderer the privileges to request
  // the URL.  To prevent this attack, we block the renderer from inserting
  // banned URLs into the navigation controller in the first place.
  process->FilterURL(false, &params->url);
  process->FilterURL(true, &params->referrer.url);
  for (auto& redirect : params->redirects) {
    process->FilterURL(false, &redirect);
  }

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!CanAccessFilesOfPageState(params->page_state)) {
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_CAN_ACCESS_FILES_OF_PAGE_STATE);
    return false;
  }

  // A cross-document navigation requires an embedding token. Navigations
  // served from the back-forward cache do not require new embedding tokens as
  // the token is already set.
  bool is_served_from_bfcache =
      navigation_request && navigation_request->IsServedFromBackForwardCache();
  if (!is_served_from_bfcache) {
    if (!is_same_document_navigation && !params->embedding_token.has_value()) {
      bad_message::ReceivedBadMessage(process,
                                      bad_message::RFH_MISSING_EMBEDDING_TOKEN);
      return false;
    } else if (is_same_document_navigation &&
               params->embedding_token.has_value()) {
      bad_message::ReceivedBadMessage(
          process, bad_message::RFH_UNEXPECTED_EMBEDDING_TOKEN);
      return false;
    }
  }

  return true;
}

void RenderFrameHostImpl::UpdateSiteURL(const GURL& url,
                                        bool url_is_unreachable) {
  if (url_is_unreachable) {
    SetLastCommittedSiteInfo(GURL());
  } else {
    SetLastCommittedSiteInfo(url);
  }
}

bool RenderFrameHostImpl::DidCommitNavigationInternal(
    std::unique_ptr<NavigationRequest> navigation_request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
    bool is_same_document_navigation) {
  // Sanity-check the page transition for frame type.
  DCHECK_EQ(ui::PageTransitionIsMainFrame(params->transition), !GetParent());

  // Check that the committing navigation token matches the navigation request.
  if (navigation_request &&
      navigation_request->commit_params().navigation_token !=
          params->navigation_token) {
    navigation_request.reset();
  }

  if (!navigation_request) {
    // A matching NavigationRequest should have been found, unless in a few very
    // specific cases. Check if this is one of those cases.
    bool is_commit_allowed_to_proceed = false;

    // 1) This was a renderer-initiated navigation to an empty document. Most
    // of the time: about:blank.
    is_commit_allowed_to_proceed |= params->url.SchemeIs(url::kAboutScheme) &&
                                    params->url != GURL(url::kAboutSrcdocURL);

    // 2) This was a same-document navigation.
    // TODO(clamy): We should enforce having a request on browser-initiated
    // same-document navigations.
    is_commit_allowed_to_proceed |= is_same_document_navigation;

    // 3) Error pages implementations in Chrome can commit twice.
    // TODO(clamy): Fix this.
    is_commit_allowed_to_proceed |= params->url_is_unreachable;

    // 4) Special case for DOMSerializerBrowsertests which are implemented
    // entirely renderer-side and unlike normal RenderView based tests load
    // file URLs instead of data URLs.
    // TODO(clamy): Rework the tests to remove this exception.
    is_commit_allowed_to_proceed |= params->url.SchemeIsFile();

    if (!is_commit_allowed_to_proceed) {
      bad_message::ReceivedBadMessage(
          GetProcess(),
          bad_message::RFH_NO_MATCHING_NAVIGATION_REQUEST_ON_COMMIT);
      return false;
    }
  }

  if (!ValidateDidCommitParams(navigation_request.get(), params.get(),
                               is_same_document_navigation)) {
    return false;
  }

  // TODO(clamy): We should stop having a special case for same-document
  // navigation and just put them in the general map of NavigationRequests.
  if (navigation_request &&
      navigation_request->common_params().url != params->url &&
      is_same_document_navigation) {
    same_document_navigation_request_ = std::move(navigation_request);
  }

  // Set is loading to true now if it has not been set yet. This happens for
  // renderer-initiated same-document navigations. It can also happen when a
  // racy DidStopLoading Mojo method resets the loading state that was set to
  // true in CommitNavigation.
  if (!is_loading()) {
    bool was_loading = frame_tree()->IsLoading();
    is_loading_ = true;
    frame_tree_node()->DidStartLoading(!is_same_document_navigation,
                                       was_loading);
  }

  if (navigation_request)
    was_discarded_ = navigation_request->commit_params().was_discarded;

  // If there is no valid NavigationRequest corresponding to this commit, create
  // one in order to properly issue DidFinishNavigation calls to
  // WebContentsObservers.
  if (!navigation_request) {
    navigation_request =
        CreateNavigationRequestForCommit(*params, is_same_document_navigation);
  }

  DCHECK(navigation_request);
  DCHECK(navigation_request->IsNavigationStarted());

  // Update the page transition. For subframe navigations, the renderer process
  // only gives the correct page transition at commit time.
  // TODO(clamy): We should get the correct page transition when starting the
  // request.
  navigation_request->set_transition(params->transition);

  navigation_request->set_has_user_gesture(params->gesture ==
                                           NavigationGestureUser);

  UpdateSiteURL(params->url, params->url_is_unreachable);
  if (!is_same_document_navigation)
    UpdateRenderProcessHostFramePriorities();

  // TODO(arthursonzogni): Updating this flag for same-document or bfcache
  // navigation might not be right. Should this be moved to
  // DidCommitNewDocument()?
  accessibility_reset_count_ = 0;

  // TODO(arthursonzogni): Updating this flag for same-document or bfcache
  // navigation isn't right. This should be moved to DidCommitNewDocument().
  appcache_handle_ = navigation_request->TakeAppCacheHandle();

  bool created_new_document =
      !is_same_document_navigation &&
      !navigation_request->IsServedFromBackForwardCache();

  // TODO(crbug.com/936696): Remove this after we have RenderDocument.
  // IsWaitingToCommit can be false inside DidCommitNavigationInternal only in
  // specific circumstances listed above, and specifically for the fake
  // initial navigations triggered by the blank window.open() and creating a
  // blank iframe. In that case we do not want to reset the per-document
  // states as we are not really creating a new Document and we want to
  // preserve the states set by WebContentsCreated delegate notification
  // (which among other things create tab helpers) or RenderFrameCreated.
  bool navigated_to_new_document =
      created_new_document && navigation_request->IsWaitingToCommit();

  if (navigated_to_new_document) {
    TRACE_EVENT1("content", "DidCommitProvisionalLoad_StateResetForNewDocument",
                 "render_frame_host", this);

    if (navigation_request->IsInMainFrame()) {
      render_view_host_->ResetPerPageState();
    }
    last_committed_cross_document_navigation_id_ =
        navigation_request->GetNavigationId();

    if (IsCurrent() && !committed_speculative_rfh_before_navigation_commit_) {
      // Clear all the user data associated with the non speculative
      // RenderFrameHost when the navigation is a cross-document navigation not
      // served from the back-forward cache. Make sure the data doesn't get
      // cleared for the cases when the RenderFrameHost commits before the
      // navigation commits. This happens when the current RenderFrameHost
      // crashes before navigating to a new URL.
      document_associated_data_.ClearAllUserData();
    }

    // Continue observing the events for the committed navigation.
    for (auto& receiver : navigation_request->TakeCookieObservers()) {
      cookie_observers_.Add(this, std::move(receiver));
    }

    // Resets when navigating to a new document. This is needed because
    // RenderFrameHost might be reused for a new document
    document_used_web_otp_ = false;

    // Get the UKM source id sent to the renderer.
    const ukm::SourceId document_ukm_source_id =
        navigation_request->commit_params().document_ukm_source_id;

    ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();

    // Associate the blink::Document source id to the URL. Only URLs on main
    // frames can be recorded.
    if (is_main_frame() && document_ukm_source_id != ukm::kInvalidSourceId)
      ukm_recorder->UpdateSourceURL(document_ukm_source_id, params->url);

    RecordDocumentCreatedUkmEvent(params->origin, document_ukm_source_id,
                                  ukm_recorder);
  }

  // If we still have a PeakGpuMemoryTracker, then the loading it was observing
  // never completed. Cancel it's callback so that we don't report partial
  // loads to UMA.
  if (loading_mem_tracker_)
    loading_mem_tracker_->Cancel();
  // Main Frames will create the tracker, which will be triggered after we
  // receive DidStopLoading.
  // TODO(arthursonzogni): Updating this flag for same-document or bfcache
  // navigation isn't right. This should be moved to DidCommitNewDocument().
  loading_mem_tracker_ = navigation_request->TakePeakGpuMemoryTracker();

  frame_tree_node()->navigator().DidNavigate(this, *params,
                                             std::move(navigation_request),
                                             is_same_document_navigation);

  // Reset back the state to false after navigation commits.
  // TODO(https://crbug.com/1072817): Undo this plumbing after removing the
  // early post-crash CommitPending() call.
  committed_speculative_rfh_before_navigation_commit_ = false;

  // TODO(arthursonzogni): Updating this flag for same-document or bfcache
  // navigation doesn't seem right. This should likely be executed in
  // DidCommitNewDocument().
  if (IsBackForwardCacheEnabled()) {
    // Store the Commit params so they can be reused if the page is ever
    // restored from the BackForwardCache.
    last_commit_params_ = std::move(params);
  }

  return true;
}

// TODO(arthursonzogni): Below, many NavigationRequest's objects are passed from
// the navigation to the new document. Consider grouping them in a single
// struct.
//
// TODO(arthursonzogni): Investigate what must be done when
// navigation_request->IsWaitingToCommit() is false here.
void RenderFrameHostImpl::DidCommitNewDocument(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    NavigationRequest* navigation_request) {
  // BackForwardCache navigations restore existing document, but never create
  // new ones.
  DCHECK(!navigation_request->IsServedFromBackForwardCache());

  // After setting the last committed origin, reset the feature policy and
  // sandbox flags in the RenderFrameHost to a blank policy based on the
  // parent frame or opener frame.
  ResetFeaturePolicy();
  active_sandbox_flags_ = frame_tree_node()->active_sandbox_flags();
  document_policy_ = blink::DocumentPolicy::CreateWithHeaderPolicy({});

  // Since we're changing documents, we should reset the event handler
  // trackers.
  has_before_unload_handler_ = false;
  has_unload_handler_ = false;
  has_pagehide_handler_ = false;
  has_visibilitychange_handler_ = false;

  DCHECK(params.embedding_token.has_value());
  SetEmbeddingToken(params.embedding_token.value());

  // TODO(arthursonzogni): Stop relying on DidCommitProvisionalLoad_Params. Use
  // the NavigationRequest instead. The browser process doesn't need to rely on
  // the renderer process.
  last_http_status_code_ = params.http_status_code;

  // TODO(arthursonzogni): Stop relying on DidCommitProvisionalLoad_Params. Use
  // the NavigationRequest instead. The browser process doesn't need to rely on
  // the renderer process.
  last_http_method_ = params.method;

  renderer_reported_scheduler_tracked_features_ = 0;
  browser_reported_scheduler_tracked_features_ = 0;

  last_committed_client_security_state_ =
      navigation_request->TakeClientSecurityState();

  cross_origin_opener_policy_ =
      navigation_request->coop_status().current_coop();
  coop_reporter_ = navigation_request->coop_status().TakeCoopReporter();
  virtual_browsing_context_group_ =
      navigation_request->coop_status().virtual_browsing_context_group();

  // Store the required CSP (it will be used by the AncestorThrottle if
  // this frame embeds a subframe when that subframe navigates).
  required_csp_ = navigation_request->TakeRequiredCSP();

  // Keep track of the sandbox policy of the document that has just committed.
  // It will be compared with the value computed from the renderer. The latter
  // is expected to be received in DidSetFramePolicyHeaders(..).
  active_sandbox_flags_control_ = navigation_request->SandboxFlagsToCommit();

  coep_reporter_ = navigation_request->TakeCoepReporter();
  if (coep_reporter_) {
    mojo::PendingRemote<blink::mojom::ReportingObserver> remote;
    mojo::PendingReceiver<blink::mojom::ReportingObserver> receiver =
        remote.InitWithNewPipeAndPassReceiver();
    coep_reporter_->BindObserver(std::move(remote));
    // As some tests override the associated frame after commit, do not
    // call GetAssociatedLocalFrame now.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&RenderFrameHostImpl::BindReportingObserver,
                       weak_ptr_factory_.GetWeakPtr(), std::move(receiver)));
  }

  // Set the state whether this navigation is to an MHTML document, since there
  // are certain security checks that we cannot apply to subframes in MHTML
  // documents. Do not trust renderer data when determining that, rather use
  // the |navigation_request|, which was generated and stays browser side.
  is_mhtml_document_ =
      navigation_request->GetMimeType() == "multipart/related" ||
      navigation_request->GetMimeType() == "message/rfc822";

  RecordCrossOriginIsolationMetrics(this);

  CrossOriginOpenerPolicyReporter::InstallAccessMonitorsIfNeeded(
      frame_tree_node_);
}

void RenderFrameHostImpl::OnSameDocumentCommitProcessed(
    int64_t navigation_id,
    bool should_replace_current_entry,
    blink::mojom::CommitResult result) {
  // If the NavigationRequest was deleted, another navigation commit started to
  // be processed. Let the latest commit go through and stop doing anything.
  if (!same_document_navigation_request_ ||
      same_document_navigation_request_->GetNavigationId() != navigation_id) {
    return;
  }

  if (result == blink::mojom::CommitResult::RestartCrossDocument) {
    // The navigation could not be committed as a same-document navigation.
    // Restart the navigation cross-document.
    frame_tree_node_->navigator().RestartNavigationAsCrossDocument(
        std::move(same_document_navigation_request_));
  }

  if (result == blink::mojom::CommitResult::Aborted) {
    // Note: if the commit was successful, navigation_request_ is reset in
    // DidCommitProvisionalLoad.
    same_document_navigation_request_.reset();
  }
}

void RenderFrameHostImpl::OnCrossDocumentCommitProcessed(
    NavigationRequest* navigation_request,
    blink::mojom::CommitResult result) {
  DCHECK_NE(blink::mojom::CommitResult::RestartCrossDocument, result);
  if (result == blink::mojom::CommitResult::Ok) {
    // The navigation will soon be committed. Move it out of the map to the
    // NavigationRequest that is about to commit.
    auto find_request = navigation_requests_.find(navigation_request);
    if (find_request != navigation_requests_.end()) {
      navigation_request_ = std::move(find_request->second);
    } else {
      // This frame might be used for attaching an inner WebContents in which
      // case |navigation_requests_| are deleted during attaching.
      // TODO(ekaramad): Add a DCHECK to ensure the FrameTreeNode is attaching
      // an inner delegate (https://crbug.com/911161).
    }
  }
  // Remove the requests from the list of NavigationRequests waiting to commit.
  navigation_requests_.erase(navigation_request);
}

std::unique_ptr<base::trace_event::TracedValue>
RenderFrameHostImpl::CommitAsTracedValue(
    FrameHostMsg_DidCommitProvisionalLoad_Params* params) const {
  auto value = std::make_unique<base::trace_event::TracedValue>();

  // TODO(nasko): Move the process lock into RenderProcessHost.
  value->SetString(
      "process lock",
      ChildProcessSecurityPolicyImpl::GetInstance()
          ->GetProcessLock(agent_scheduling_group_.GetProcess()->GetID())
          .ToString());

  value->SetInteger("nav_entry_id", params->nav_entry_id);
  value->SetInteger("item_sequence_number", params->item_sequence_number);
  value->SetInteger("document_sequence_number",
                    params->document_sequence_number);
  value->SetString("url", params->url.spec());
  if (!params->base_url.is_empty()) {
    value->SetString("base_url", params->base_url.possibly_invalid_spec());
  }
  value->SetInteger("transition", params->transition);
  value->BeginDictionary("referrer");
  value->SetString("url", params->referrer.url.spec());
  value->SetInteger("policy", static_cast<int>(params->referrer.policy));
  value->EndDictionary();
  value->SetBoolean("should_update_history", params->should_update_history);
  value->SetString("contents_mime_type", params->contents_mime_type);

  value->SetBoolean("intended_as_new_entry", params->intended_as_new_entry);
  value->SetBoolean("did_create_new_entry", params->did_create_new_entry);
  value->SetBoolean("should_replace_current_entry",
                    params->should_replace_current_entry);
  value->SetString("method", params->method);
  value->SetInteger("post_id", params->post_id);
  value->SetInteger("http_status_code", params->http_status_code);
  value->SetBoolean("url_is_unreachable", params->url_is_unreachable);
  value->SetString("original_request_url", params->original_request_url.spec());
  value->SetBoolean("is_overriding_user_agent",
                    params->is_overriding_user_agent);
  value->SetBoolean("history_list_was_cleared",
                    params->history_list_was_cleared);
  value->SetString("origin", params->origin.GetDebugString());
  value->SetBoolean("has_potentially_trustworthy_unique_origin",
                    params->has_potentially_trustworthy_unique_origin);
  value->SetInteger("request_id", params->request_id);
  value->SetString("navigation_token", params->navigation_token.ToString());
  if (params->embedding_token)
    value->SetString("embedding_token", params->embedding_token->ToString());

  return value;
}

void RenderFrameHostImpl::MaybeGenerateCrashReport(
    base::TerminationStatus status,
    int exit_code) {
  if (!last_committed_url_.SchemeIsHTTPOrHTTPS())
    return;

  // Only generate reports for local root frames.
  if (!frame_tree_node_->IsMainFrame() && !IsCrossProcessSubframe())
    return;

  // Check the termination status to see if a crash occurred (and potentially
  // determine the |reason| for the crash).
  std::string reason;
  switch (status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      if (exit_code == RESULT_CODE_HUNG)
        reason = "unresponsive";
      break;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      if (exit_code == RESULT_CODE_HUNG)
        reason = "unresponsive";
      else
        return;
      break;
    case base::TERMINATION_STATUS_OOM:
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
      reason = "oom";
      break;
    default:
      // Other termination statuses do not indicate a crash.
      return;
  }

  // Construct the crash report.
  auto body = base::DictionaryValue();
  if (!reason.empty())
    body.SetString("reason", reason);

  // Send the crash report to the Reporting API.
  GetProcess()->GetStoragePartition()->GetNetworkContext()->QueueReport(
      "crash" /* type */, "default" /* group */, last_committed_url_,
      base::nullopt, std::move(body));
}

void RenderFrameHostImpl::SendCommitNavigation(
    mojom::NavigationClient* navigation_client,
    NavigationRequest* navigation_request,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    base::Optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory,
    const base::UnguessableToken& devtools_navigation_token) {
  navigation_client->CommitNavigation(
      std::move(common_params), std::move(commit_params),
      std::move(response_head), std::move(response_body),
      std::move(url_loader_client_endpoints),
      std::move(subresource_loader_factories), std::move(subresource_overrides),
      std::move(controller), std::move(container_info),
      std::move(prefetch_loader_factory), devtools_navigation_token,
      BuildCommitNavigationCallback(navigation_request));
}

void RenderFrameHostImpl::SendCommitFailedNavigation(
    mojom::NavigationClient* navigation_client,
    NavigationRequest* navigation_request,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    bool has_stale_copy_in_cache,
    int32_t error_code,
    const base::Optional<std::string>& error_page_content,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories) {
  DCHECK(navigation_client && navigation_request);
  navigation_client->CommitFailedNavigation(
      std::move(common_params), std::move(commit_params),
      has_stale_copy_in_cache, error_code,
      navigation_request->GetResolveErrorInfo(), error_page_content,
      std::move(subresource_loader_factories),
      BuildCommitFailedNavigationCallback(navigation_request));
}

// Called when the renderer navigates. For every frame loaded, we'll get this
// notification containing parameters identifying the navigation.
void RenderFrameHostImpl::DidCommitNavigation(
    std::unique_ptr<NavigationRequest> request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  // BackForwardCacheImpl::CanStoreRenderFrameHost prevents placing the pages
  // with in-flight navigation requests in the back-forward cache and it's not
  // possible to start/commit a new one after the RenderFrameHost is in the
  // BackForwardCache (see the check IsInactiveAndDisallowReactivation in
  // RFH::DidCommitSameDocumentNavigation() and RFH::BeginNavigation()) so it
  // isn't possible to get a DidCommitNavigation IPC from the renderer in
  // kInBackForwardCache state.
  DCHECK(!IsInBackForwardCache());

  if (request && request->IsNavigationStarted()) {
    main_frame_request_ids_ = {params->request_id,
                               request->GetGlobalRequestID()};
    if (deferred_main_frame_load_info_)
      ResourceLoadComplete(std::move(deferred_main_frame_load_info_));
  }
  // DidCommitProvisionalLoad IPC should be associated with the URL being
  // committed (not with the *last* committed URL that most other IPCs are
  // associated with).
  ScopedActiveURL scoped_active_url(params->url,
                                    frame_tree()->root()->current_origin());

  ScopedCommitStateResetter commit_state_resetter(this);
  RenderProcessHost* process = GetProcess();

  TRACE_EVENT2("navigation", "RenderFrameHostImpl::DidCommitProvisionalLoad",
               "rfh", base::trace_event::ToTracedValue(this), "params",
               CommitAsTracedValue(params.get()));

  // If we're waiting for a cross-site beforeunload completion callback from
  // this renderer and we receive a Navigate message from the main frame, then
  // the renderer was navigating already and sent it before hearing the
  // blink::mojom::LocalFrame::StopLoading() message. Treat this as an implicit
  // beforeunload completion callback to allow the pending navigation to
  // continue.
  if (is_waiting_for_beforeunload_completion_ &&
      unload_ack_is_for_navigation_ && !GetParent()) {
    base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;
    ProcessBeforeUnloadCompleted(
        true /* proceed */, true /* treat_as_final_completion_callback */,
        approx_renderer_start_time, base::TimeTicks::Now());
  }

  // When a frame enters pending deletion, it waits for itself and its children
  // to properly unload. Receiving DidCommitProvisionalLoad() here while the
  // frame is not active means it comes from a navigation that reached the
  // ReadyToCommit stage just before the frame entered pending deletion.
  //
  // We should ignore this message, because we have already committed to
  // destroying this RenderFrameHost. Note that we intentionally do not ignore
  // commits that happen while the current tab is being closed - see
  // https://crbug.com/805705.
  if (IsPendingDeletion())
    return;

  // Retroactive sanity check:
  // - If this is the first real load committing in this frame, then by this
  //   time the RenderFrameHost's InterfaceProvider implementation should have
  //   already been bound to a message pipe whose client end is used to service
  //   interface requests from the initial empty document.
  // - Otherwise, the InterfaceProvider implementation should at this point be
  //   bound to an interface connection servicing interface requests coming from
  //   the document of the previously committed navigation.
  DCHECK(document_scoped_interface_provider_receiver_.is_bound());
  if (interface_params) {
    // As a general rule, expect the RenderFrame to have supplied the
    // receiver end of a new InterfaceProvider connection that will be used by
    // the new document to issue interface receivers to access RenderFrameHost
    // services.
    document_scoped_interface_provider_receiver_.reset();
    BindInterfaceProviderReceiver(
        std::move(interface_params->interface_provider_receiver));
    if (broker_receiver_.is_bound()) {
      auto broker_receiver_of_previous_document = broker_receiver_.Unbind();
      dropped_interface_request_logger_ =
          std::make_unique<DroppedInterfaceRequestLogger>(
              std::move(broker_receiver_of_previous_document));
    }
    BindBrowserInterfaceBrokerReceiver(
        std::move(interface_params->browser_interface_broker_receiver));
  } else {
    // If there had already been a real load committed in the frame, and this is
    // not a same-document navigation, then both the active document as well as
    // the global object was replaced in this browsing context. The RenderFrame
    // should have rebound its InterfaceProvider to a new pipe, but failed to do
    // so. Kill the renderer, and reset the old receiver to ensure that any
    // pending interface requests originating from the previous document, hence
    // possibly from a different security origin, will no longer be dispatched.
    if (frame_tree_node_->has_committed_real_load()) {
      document_scoped_interface_provider_receiver_.reset();
      broker_receiver_.reset();
      bad_message::ReceivedBadMessage(
          process, bad_message::RFH_INTERFACE_PROVIDER_MISSING);
      return;
    }

    // Otherwise, it is the first real load committed, for which the RenderFrame
    // is allowed to, and will re-use the existing InterfaceProvider connection
    // if the new document is same-origin with the initial empty document, and
    // therefore the global object is not replaced.
  }

  if (!DidCommitNavigationInternal(std::move(request), std::move(params),
                                   false /* is_same_document_navigation */)) {
    return;
  }

  // Since we didn't early return, it's safe to keep the commit state.
  commit_state_resetter.disable();

  // For a top-level frame, there are potential security concerns associated
  // with displaying graphics from a previously loaded page after the URL in
  // the omnibar has been changed. It is unappealing to clear the page
  // immediately, but if the renderer is taking a long time to issue any
  // compositor output (possibly because of script deliberately creating this
  // situation) then we clear it after a while anyway.
  // See https://crbug.com/497588.
  if (frame_tree_node_->IsMainFrame() && GetView()) {
    RenderWidgetHostImpl::From(GetView()->GetRenderWidgetHost())->DidNavigate();
  }

  // TODO(arthursonzogni): This can be removed when RenderDocument will be
  // implemented. See https://crbug.com/936696.
  EnsureDescendantsAreUnloading();
}

mojom::NavigationClient::CommitNavigationCallback
RenderFrameHostImpl::BuildCommitNavigationCallback(
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  return base::BindOnce(
      &RenderFrameHostImpl::DidCommitPerNavigationMojoInterfaceNavigation,
      base::Unretained(this), navigation_request);
}

mojom::NavigationClient::CommitFailedNavigationCallback
RenderFrameHostImpl::BuildCommitFailedNavigationCallback(
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  return base::BindOnce(
      &RenderFrameHostImpl::DidCommitPerNavigationMojoInterfaceNavigation,
      base::Unretained(this), navigation_request);
}

void RenderFrameHostImpl::SendBeforeUnload(
    bool is_reload,
    base::WeakPtr<RenderFrameHostImpl> rfh) {
  auto before_unload_closure = base::BindOnce(
      [](base::WeakPtr<RenderFrameHostImpl> impl, bool proceed,
         base::TimeTicks renderer_before_unload_start_time,
         base::TimeTicks renderer_before_unload_end_time) {
        if (!impl)
          return;
        impl->ProcessBeforeUnloadCompleted(
            proceed, false /* treat_as_final_completion_callback */,
            renderer_before_unload_start_time, renderer_before_unload_end_time);
      },
      rfh);
  // Experiment to run beforeunload handlers at a higher priority in the
  // renderer. See crubug.com/1042118.
  if (base::FeatureList::IsEnabled(features::kHighPriorityBeforeUnload)) {
    rfh->GetHighPriorityLocalFrame()->DispatchBeforeUnload(
        is_reload, std::move(before_unload_closure));
  } else {
    rfh->GetAssociatedLocalFrame()->BeforeUnload(
        is_reload, std::move(before_unload_closure));
  }
}

void RenderFrameHostImpl::AddServiceWorkerContainerHost(
    const std::string& uuid,
    base::WeakPtr<content::ServiceWorkerContainerHost> host) {
  if (IsInBackForwardCache()) {
    // RenderFrameHost entered BackForwardCache before adding
    // ServiceWorkerContainerHost. In this case, evict the entry from the cache.
    EvictFromBackForwardCacheWithReason(
        BackForwardCacheMetrics::NotRestoredReason::
            kEnteredBackForwardCacheBeforeServiceWorkerHostAdded);
  }
  DCHECK(!base::Contains(service_worker_container_hosts_, uuid));
  last_committed_service_worker_host_ = host;
  service_worker_container_hosts_[uuid] = std::move(host);
}

void RenderFrameHostImpl::RemoveServiceWorkerContainerHost(
    const std::string& uuid) {
  DCHECK(!service_worker_container_hosts_.empty());
  DCHECK(base::Contains(service_worker_container_hosts_, uuid));
  service_worker_container_hosts_.erase(uuid);
}

base::WeakPtr<ServiceWorkerContainerHost>
RenderFrameHostImpl::GetLastCommittedServiceWorkerHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return last_committed_service_worker_host_;
}

bool RenderFrameHostImpl::MaybeInterceptCommitCallback(
    NavigationRequest* navigation_request,
    FrameHostMsg_DidCommitProvisionalLoad_Params* params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params) {
  if (commit_callback_interceptor_) {
    return commit_callback_interceptor_->WillProcessDidCommitNavigation(
        navigation_request, params, interface_params);
  }
  return true;
}

void RenderFrameHostImpl::PostMessageEvent(
    const base::Optional<base::UnguessableToken>& source_token,
    const base::string16& source_origin,
    const base::string16& target_origin,
    blink::TransferableMessage message) {
  DCHECK(render_frame_created_);

  GetAssociatedLocalFrame()->PostMessageEvent(
      source_token, source_origin, target_origin, std::move(message));
}

bool RenderFrameHostImpl::IsTestRenderFrameHost() const {
  return false;
}

scoped_refptr<PrefetchedSignedExchangeCache>
RenderFrameHostImpl::EnsurePrefetchedSignedExchangeCache() {
  if (!prefetched_signed_exchange_cache_) {
    prefetched_signed_exchange_cache_ =
        base::MakeRefCounted<PrefetchedSignedExchangeCache>();
  }
  return prefetched_signed_exchange_cache_;
}

void RenderFrameHostImpl::ClearPrefetchedSignedExchangeCache() {
  if (prefetched_signed_exchange_cache_)
    prefetched_signed_exchange_cache_->Clear();
}

std::unique_ptr<WebBundleHandleTracker>
RenderFrameHostImpl::MaybeCreateWebBundleHandleTracker() {
  if (web_bundle_handle_)
    return web_bundle_handle_->MaybeCreateTracker();
  FrameTreeNode* frame_owner =
      frame_tree_node_->parent() ? frame_tree_node_->parent()->frame_tree_node()
                                 : frame_tree_node_->opener();
  if (!frame_owner)
    return nullptr;
  RenderFrameHostImpl* frame_owner_host = frame_owner->current_frame_host();
  if (!frame_owner_host->web_bundle_handle_)
    return nullptr;
  return frame_owner_host->web_bundle_handle_->MaybeCreateTracker();
}

RenderWidgetHostImpl* RenderFrameHostImpl::GetLocalRenderWidgetHost() const {
  if (!parent_)
    return render_view_host_->GetWidget();
  else
    return owned_render_widget_host_.get();
}

void RenderFrameHostImpl::EnsureDescendantsAreUnloading() {
  std::vector<RenderFrameHostImpl*> parents_to_be_checked = {this};
  std::vector<RenderFrameHostImpl*> rfhs_to_be_checked;
  while (!parents_to_be_checked.empty()) {
    RenderFrameHostImpl* document = parents_to_be_checked.back();
    parents_to_be_checked.pop_back();

    for (auto& subframe : document->children_) {
      RenderFrameHostImpl* child = subframe->current_frame_host();
      // Every child is expected to be pending deletion. If it isn't the
      // case, their FrameTreeNode is immediately removed from the tree.
      if (!child->IsPendingDeletion())
        rfhs_to_be_checked.push_back(child);
      else
        parents_to_be_checked.push_back(child);
    }
  }
  for (RenderFrameHostImpl* document : rfhs_to_be_checked)
    document->parent_->RemoveChild(document->frame_tree_node());
}

void RenderFrameHostImpl::AddUniqueMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  AddMessageToConsoleImpl(level, message, true /* discard_duplicates */);
}

void RenderFrameHostImpl::AddMessageToConsoleImpl(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message,
    bool discard_duplicates) {
  GetAssociatedLocalFrame()->AddMessageToConsole(level, message,
                                                 discard_duplicates);
}

void RenderFrameHostImpl::LogCannotCommitUrlCrashKeys(
    const GURL& url,
    bool is_same_document_navigation,
    NavigationRequest* navigation_request) {
  LogRendererKillCrashKeys(GetSiteInstance()->GetSiteInfo());

  // Temporary instrumentation to debug the root cause of renderer process
  // terminations. See https://crbug.com/931895.
  auto bool_to_crash_key = [](bool b) { return b ? "true" : "false"; };
  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_same_document",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(is_same_document_navigation));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_main_frame",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(frame_tree_node_->IsMainFrame()));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_cross_process_subframe",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(IsCrossProcessSubframe()));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("site_lock",
                                          base::debug::CrashKeySize::Size256),
      GetSiteInstance()->GetProcessLock().ToString());

  if (!GetSiteInstance()->IsDefaultSiteInstance()) {
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("original_url_origin",
                                            base::debug::CrashKeySize::Size256),
        GetSiteInstance()->original_url().GetOrigin().spec());
  }

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_mhtml_document",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(is_mhtml_document()));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("last_committed_url_origin",
                                          base::debug::CrashKeySize::Size256),
      GetLastCommittedURL().GetOrigin().spec());

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("last_successful_url_origin",
                                          base::debug::CrashKeySize::Size256),
      last_successful_url().GetOrigin().spec());

  if (navigation_request && navigation_request->IsNavigationStarted()) {
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_renderer_initiated",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsRendererInitiated()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_server_redirect",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->WasServerRedirect()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_form_submission",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsFormSubmission()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_error_page",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsErrorPage()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("from_begin_navigation",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->from_begin_navigation()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("net_error",
                                            base::debug::CrashKeySize::Size32),
        base::NumberToString(navigation_request->GetNetErrorCode()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("initiator_origin",
                                            base::debug::CrashKeySize::Size64),
        navigation_request->GetInitiatorOrigin()
            ? navigation_request->GetInitiatorOrigin()->GetDebugString()
            : "none");

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("starting_site_instance",
                                            base::debug::CrashKeySize::Size256),
        navigation_request->GetStartingSiteInstance()
            ->GetSiteInfo()
            .GetDebugString());

    // Recompute the target SiteInstance to see if it matches the current
    // one at commit time.
    scoped_refptr<SiteInstance> dest_instance =
        frame_tree_node_->render_manager()->GetSiteInstanceForNavigationRequest(
            navigation_request);
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString(
            "does_recomputed_site_instance_match",
            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(dest_instance == GetSiteInstance()));
  }
}

void RenderFrameHostImpl::MaybeEvictFromBackForwardCache() {
  if (!IsInBackForwardCache())
    return;

  RenderFrameHostImpl* top_document = this;
  while (RenderFrameHostImpl* parent = top_document->GetParent())
    top_document = parent;

  auto can_store =
      frame_tree()->controller()->GetBackForwardCache().CanStorePageNow(
          top_document);
  TRACE_EVENT1("navigation",
               "RenderFrameHostImpl::MaybeEvictFromBackForwardCache",
               "can_store", can_store.ToString());

  if (can_store)
    return;

  EvictFromBackForwardCacheWithReasons(can_store);
}

void RenderFrameHostImpl::LogCannotCommitOriginCrashKeys(
    bool is_same_document_navigation,
    NavigationRequest* navigation_request) {
  LogRendererKillCrashKeys(GetSiteInstance()->GetSiteInfo());

  // Temporary instrumentation to debug the root cause of
  // https://crbug.com/923144.
  auto bool_to_crash_key = [](bool b) { return b ? "true" : "false"; };
  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_same_document",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(is_same_document_navigation));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_subframe",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(!frame_tree_node_->IsMainFrame()));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("lifecycle_state",
                                          base::debug::CrashKeySize::Size32),
      LifecycleStateToString(lifecycle_state_));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_current",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(IsCurrent()));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_cross_process_subframe",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(IsCrossProcessSubframe()));

  if (navigation_request && navigation_request->IsNavigationStarted()) {
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_renderer_initiated",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsRendererInitiated()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_server_redirect",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->WasServerRedirect()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_form_submission",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsFormSubmission()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_error_page",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsErrorPage()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("initiator_origin",
                                            base::debug::CrashKeySize::Size64),
        navigation_request->GetInitiatorOrigin()
            ? navigation_request->GetInitiatorOrigin()->GetDebugString()
            : "none");

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("starting_site_instance",
                                            base::debug::CrashKeySize::Size256),
        navigation_request->GetStartingSiteInstance()
            ->GetSiteInfo()
            .GetDebugString());
  }
}

void RenderFrameHostImpl::EnableMojoJsBindings() {
  // This method should only be called on RenderFrameHost which is for a WebUI.
  DCHECK_NE(WebUI::kNoWebUI,
            WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
                GetSiteInstance()->GetBrowserContext(),
                site_instance_->GetSiteInfo().site_url()));

  GetFrameBindingsControl()->EnableMojoJsBindings();
}

BackForwardCacheMetrics* RenderFrameHostImpl::GetBackForwardCacheMetrics() {
  NavigationEntryImpl* navigation_entry =
      frame_tree()->controller()->GetEntryWithUniqueID(nav_entry_id());
  if (!navigation_entry)
    return nullptr;
  return navigation_entry->back_forward_cache_metrics();
}

bool RenderFrameHostImpl::IsBackForwardCacheDisabled() const {
  return back_forward_cache_disabled_reasons_.size();
}

bool RenderFrameHostImpl::IsDOMContentLoaded() {
  return dom_content_loaded_;
}

void RenderFrameHostImpl::UpdateAdFrameType(
    blink::mojom::AdFrameType ad_frame_type) {
  frame_tree_node_->SetAdFrameType(ad_frame_type);
}

blink::mojom::AuthenticatorStatus
RenderFrameHostImpl::PerformGetAssertionWebAuthSecurityChecks(
    const std::string& relying_party_id,
    const url::Origin& effective_origin) {
  bool is_cross_origin;
  blink::mojom::AuthenticatorStatus status =
      GetWebAuthRequestSecurityChecker()->ValidateAncestorOrigins(
          effective_origin,
          WebAuthRequestSecurityChecker::RequestType::kGetAssertion,
          &is_cross_origin);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return status;
  }

  status = GetWebAuthRequestSecurityChecker()->ValidateDomainAndRelyingPartyID(
      effective_origin, relying_party_id);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return status;
  }

  return blink::mojom::AuthenticatorStatus::SUCCESS;
}

blink::mojom::AuthenticatorStatus
RenderFrameHostImpl::PerformMakeCredentialWebAuthSecurityChecks(
    const std::string& relying_party_id,
    const url::Origin& effective_origin) {
  bool is_cross_origin;
  blink::mojom::AuthenticatorStatus status =
      GetWebAuthRequestSecurityChecker()->ValidateAncestorOrigins(
          effective_origin,
          WebAuthRequestSecurityChecker::RequestType::kMakeCredential,
          &is_cross_origin);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return status;
  }

  status = GetWebAuthRequestSecurityChecker()->ValidateDomainAndRelyingPartyID(
      effective_origin, relying_party_id);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return status;
  }

  return blink::mojom::AuthenticatorStatus::SUCCESS;
}

void RenderFrameHostImpl::IsClipboardPasteAllowed(
    const ui::ClipboardFormatType& data_type,
    const std::string& data,
    IsClipboardPasteAllowedCallback callback) {
  delegate_->IsClipboardPasteAllowed(GetLastCommittedURL(), data_type, data,
                                     std::move(callback));
}

RenderFrameHostImpl* RenderFrameHostImpl::ParentOrOuterDelegateFrame() {
  // Find the parent in the FrameTree (iframe).
  if (parent_)
    return parent_;

  // Find the parent in the WebContentsTree (GuestView or Portal).
  FrameTreeNode* frame_in_embedder =
      frame_tree_node()->render_manager()->GetOuterDelegateNode();
  if (frame_in_embedder)
    return frame_in_embedder->current_frame_host()->GetParent();

  // No parent found.
  return nullptr;
}

scoped_refptr<WebAuthRequestSecurityChecker>
RenderFrameHostImpl::GetWebAuthRequestSecurityChecker() {
  if (!webauth_request_security_checker_)
    webauth_request_security_checker_ =
        base::MakeRefCounted<WebAuthRequestSecurityChecker>(this);

  return webauth_request_security_checker_;
}

bool RenderFrameHostImpl::IsInBackForwardCache() {
  return lifecycle_state_ == LifecycleState::kInBackForwardCache;
}

bool RenderFrameHostImpl::IsPendingDeletion() {
  return lifecycle_state_ == LifecycleState::kRunningUnloadHandlers ||
         lifecycle_state_ == LifecycleState::kReadyToBeDeleted;
}

void RenderFrameHostImpl::SetLifecycleStateToActive() {
  // If the RenderFrameHost is restored from BackForwardCache, update states of
  // all the children to kActive. This is called from
  // RenderFrameHostManager::SetRenderFrameHost which happens after commit.
  if (IsInBackForwardCache()) {
    for (auto& child : children_) {
      DCHECK_EQ(child->current_frame_host()->lifecycle_state_,
                LifecycleState::kInBackForwardCache);
      child->current_frame_host()->SetLifecycleStateToActive();
    }
  }

  SetLifecycleState(LifecycleState::kActive);
}

void RenderFrameHostImpl::SetLifecycleState(LifecycleState state) {
  TRACE_EVENT2("content", "RenderFrameHostImpl::SetLifecycleState",
               "render_frame_host", base::trace_event::ToTracedValue(this),
               "new_state", LifecycleStateToString(state));
#if DCHECK_IS_ON()
  static const base::NoDestructor<StateTransitions<LifecycleState>>
      allowed_transitions(
          // For a graph of state transitions, see
          // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/render-frame-host-lifecycle-state.png
          // To update the graph, see the corresponding .gv file.

          // RenderFrameHost is only set speculative during its creation and no
          // transitions happen to this state during its lifetime.
          StateTransitions<LifecycleState>({
              {LifecycleState::kSpeculative,
               {LifecycleState::kActive, LifecycleState::kReadyToBeDeleted}},

              {LifecycleState::kActive,
               {LifecycleState::kInBackForwardCache,
                LifecycleState::kRunningUnloadHandlers,
                LifecycleState::kReadyToBeDeleted}},

              {LifecycleState::kInBackForwardCache,
               {LifecycleState::kActive, LifecycleState::kReadyToBeDeleted}},

              {LifecycleState::kRunningUnloadHandlers,
               {LifecycleState::kReadyToBeDeleted}},

              {LifecycleState::kReadyToBeDeleted, {}},
          }));
  DCHECK_STATE_TRANSITION(allowed_transitions, /*old_state=*/lifecycle_state_,
                          /*new_state*/ state);
#endif  // DCHECK_IS_ON()

  LifecycleState old_state = lifecycle_state_;
  lifecycle_state_ = state;
  // Notify the delegate about change in |lifecycle_state_|.
  delegate_->RenderFrameHostStateChanged(this, old_state, lifecycle_state_);
}

void RenderFrameHostImpl::RecordDocumentCreatedUkmEvent(
    const url::Origin& origin,
    const ukm::SourceId document_ukm_source_id,
    ukm::UkmRecorder* ukm_recorder) {
  DCHECK(ukm_recorder);
  if (document_ukm_source_id == ukm::kInvalidSourceId)
    return;

  // Compares the subframe origin with the main frame origin. In the case of
  // nested subframes such as A(B(A)), the bottom-most frame A is expected to
  // have |is_cross_origin_frame| set to false, even though this frame is cross-
  // origin from its parent frame B. This value is only used in manual analysis.
  bool is_cross_origin_frame =
      !is_main_frame() &&
      !GetMainFrame()->GetLastCommittedOrigin().IsSameOriginWith(origin);

  ukm::builders::DocumentCreated(document_ukm_source_id)
      .SetNavigationSourceId(GetPageUkmSourceId())
      .SetIsMainFrame(is_main_frame())
      .SetIsCrossOriginFrame(is_cross_origin_frame)
      .Record(ukm_recorder);
}

void RenderFrameHostImpl::BindReportingObserver(
    mojo::PendingReceiver<blink::mojom::ReportingObserver> receiver) {
  GetAssociatedLocalFrame()->BindReportingObserver(std::move(receiver));
}

mojo::PendingRemote<network::mojom::CookieAccessObserver>
RenderFrameHostImpl::CreateCookieAccessObserver() {
  mojo::PendingRemote<network::mojom::CookieAccessObserver> remote;
  cookie_observers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void RenderFrameHostImpl::Clone(
    mojo::PendingReceiver<network::mojom::CookieAccessObserver> observer) {
  cookie_observers_.Add(this, std::move(observer));
}

void RenderFrameHostImpl::OnCookiesAccessed(
    network::mojom::CookieAccessDetailsPtr details) {
  EmitSameSiteCookiesDeprecationWarning(this, details);

  CookieAccessDetails allowed;
  CookieAccessDetails blocked;
  SplitCookiesIntoAllowedAndBlocked(details, &allowed, &blocked);
  if (!allowed.cookie_list.empty())
    delegate_->OnCookiesAccessed(this, allowed);
  if (!blocked.cookie_list.empty())
    delegate_->OnCookiesAccessed(this, blocked);
}

void RenderFrameHostImpl::CheckSandboxFlags() {
  if (is_mhtml_document_)
    return;

  if (!active_sandbox_flags_control_)
    return;

  if (active_sandbox_flags_ == *active_sandbox_flags_control_)
    return;

  DCHECK(false);
}

void RenderFrameHostImpl::SetEmbeddingToken(
    const base::UnguessableToken& embedding_token) {
  // Everything in this method depends on whether the embedding token has
  // actually changed, including setting the AXTreeID (backed by the token).
  if (embedding_token_ == embedding_token)
    return;
  embedding_token_ = embedding_token;

  // The AXTreeID of a frame is backed by its embedding token, so we need to
  // update its AXTreeID, as well as the associated mapping in AXTreeIDRegistry.
  ui::AXTreeID ax_tree_id = ui::AXTreeID::FromToken(embedding_token);
  SetAXTreeID(ax_tree_id);
  ui::AXTreeIDRegistry::GetInstance()->SetFrameIDForAXTreeID(
      ui::AXTreeIDRegistry::FrameID(GetProcess()->GetID(), routing_id_),
      ax_tree_id);

  // Also important to notify the delegate so that the relevant observers can
  // adapt to the fact that the AXTreeID has changed for the main frame (e.g.
  // the WebView needs to update the ID tracking its child accessibility tree).
  if (is_main_frame())
    delegate_->AXTreeIDForMainFrameHasChanged();

  // Propagate the embedding token to the RenderFrameProxyHost representing the
  // parent frame if needed, that is, if either this is a cross-process subframe
  // or the main frame of an inner web contents (i.e. would need to send it to
  // the RenderFrameProxyHost for the outer web contents owning the inner one).
  PropagateEmbeddingTokenToParentFrame();
}

bool RenderFrameHostImpl::DocumentUsedWebOTP() {
  return document_used_web_otp_;
}

std::ostream& operator<<(std::ostream& o,
                         const RenderFrameHostImpl::LifecycleState& s) {
  return o << LifecycleStateToString(s);
}

}  // namespace content
