// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_impl.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/asan_invalid_access.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/guid.h"
#include "base/i18n/char_iterator.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/navigation_gesture.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params_mojom_traits.h"
#include "content/common/navigation_params_utils.h"
#include "content/common/page_messages.h"
#include "content/common/render_accessibility.mojom.h"
#include "content/common/renderer_host.mojom.h"
#include "content/common/unfreezable_frame_messages.h"
#include "content/common/view_messages.h"
#include "content/common/web_package/signed_exchange_utils.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/impression.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/page_state.h"
#include "content/public/common/untrustworthy_context_menu_params.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/context_menu_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/accessibility/aom_content_ax_tree.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/accessibility/render_accessibility_manager.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/content_security_policy_util.h"
#include "content/renderer/context_menu_params_builder.h"
#include "content/renderer/crash_helpers.h"
#include "content/renderer/dom_automation_controller.h"
#include "content/renderer/effective_connection_type_helper.h"
#include "content/renderer/frame_owner_properties_converter.h"
#include "content/renderer/gpu_benchmarking_extension.h"
#include "content/renderer/history_entry.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/impression_conversions.h"
#include "content/renderer/internal_document_state_data.h"
#include "content/renderer/loader/navigation_body_loader.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/tracked_child_url_loader_factory_bundle.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/loader/web_worker_fetch_context_impl.h"
#include "content/renderer/media/media_permission_dispatcher.h"
#include "content/renderer/mhtml_handle_writer.h"
#include "content/renderer/mojo/blink_interface_registry_impl.h"
#include "content/renderer/navigation_client.h"
#include "content/renderer/navigation_state.h"
#include "content/renderer/pepper/pepper_audio_controller.h"
#include "content/renderer/pepper/plugin_instance_throttler_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/service_worker_network_provider_for_frame.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "content/renderer/skia_benchmarking_extension.h"
#include "content/renderer/stats_collection_controller.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "content/renderer/web_ui_extension.h"
#include "content/renderer/web_ui_extension_data.h"
#include "content/renderer/worker/dedicated_worker_host_factory_client.h"
#include "crypto/sha2.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/data_url.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/not_implemented_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/blink/public/common/action_after_pagehide.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/loader/record_load_histograms.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/logging/logging_utils.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-shared.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/modules/media/audio/web_audio_device_factory.h"
#include "third_party/blink/public/web/modules/media/audio/web_audio_output_ipc_factory.h"
#include "third_party/blink/public/web/modules/media/webmediaplayer_util.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_context_features.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_frame_serializer.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_navigation_timings.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_document.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_savable_resources_test_support.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_searchable_form_data.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/events/base_event_utils.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"
#include "v8/include/v8.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/renderer/pepper/pepper_browser_connection.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_plugin_registry.h"
#include "content/renderer/pepper/pepper_webplugin_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#endif

#if defined(OS_WIN)
#include "base/debug/invalid_access_win.h"
#include "base/process/kill.h"
#elif defined(OS_POSIX)
#include <signal.h>
#endif

#if defined(OS_ANDROID)
#include <cpu-features.h>

#include "content/renderer/java/gin_java_bridge_dispatcher.h"
#endif

using base::Time;
using base::TimeDelta;
using blink::WebContentDecryptionModule;
using blink::WebContextMenuData;
using blink::WebData;
using blink::WebDocument;
using blink::WebDocumentLoader;
using blink::WebDOMEvent;
using blink::WebDOMMessageEvent;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFrame;
using blink::WebFrameLoadType;
using blink::WebFrameSerializer;
using blink::WebFrameSerializerClient;
using blink::WebHistoryItem;
using blink::WebHTTPBody;
using blink::WebLocalFrame;
using blink::WebMediaPlayer;
using blink::WebMediaPlayerClient;
using blink::WebMediaPlayerEncryptedMediaClient;
using blink::WebNavigationParams;
using blink::WebNavigationPolicy;
using blink::WebNavigationType;
using blink::WebNode;
using blink::WebPluginDocument;
using blink::WebPluginParams;
using blink::WebRange;
using blink::WebRect;
using blink::WebScriptSource;
using blink::WebSearchableFormData;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebSerializedScriptValue;
using blink::WebServiceWorkerProvider;
using blink::WebSettings;
using blink::WebString;
using blink::WebThreadSafeData;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::WebVector;
using blink::WebView;
using blink::mojom::SelectionMenuBehavior;
using network::mojom::ReferrerPolicy;

namespace content {

namespace {

const int kExtraCharsBeforeAndAfterSelection = 100;
const size_t kMaxURLLogChars = 1024;

const blink::PreviewsState kDisabledPreviewsBits =
    blink::PreviewsTypes::PREVIEWS_OFF |
    blink::PreviewsTypes::PREVIEWS_NO_TRANSFORM;

typedef std::map<int, RenderFrameImpl*> RoutingIDFrameMap;
static base::LazyInstance<RoutingIDFrameMap>::DestructorAtExit
    g_routing_id_frame_map = LAZY_INSTANCE_INITIALIZER;

typedef std::map<blink::WebFrame*, RenderFrameImpl*> FrameMap;
base::LazyInstance<FrameMap>::DestructorAtExit g_frame_map =
    LAZY_INSTANCE_INITIALIZER;

int64_t ExtractPostId(const WebHistoryItem& item) {
  if (item.IsNull() || item.HttpBody().IsNull())
    return -1;

  return item.HttpBody().Identifier();
}

std::string TrimURL(const std::string& url) {
  if (url.length() <= kMaxURLLogChars)
    return url;
  return url.substr(0, kMaxURLLogChars - 3) + "...";
}

// Calculates transition type based on navigation parameters. Used
// during navigation, before WebDocumentLoader is available.
ui::PageTransition GetTransitionType(ui::PageTransition default_transition,
                                     bool replaces_current_item,
                                     bool is_main_frame,
                                     WebNavigationType navigation_type) {
  if (replaces_current_item && !is_main_frame) {
    // Subframe navigations that don't add session history items must be
    // marked with AUTO_SUBFRAME. See also DidFailProvisionalLoad for how we
    // handle loading of error pages.
    return ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  }
  bool is_form_submit =
      navigation_type == blink::kWebNavigationTypeFormSubmitted ||
      navigation_type == blink::kWebNavigationTypeFormResubmitted;
  if (ui::PageTransitionCoreTypeIs(default_transition,
                                   ui::PAGE_TRANSITION_LINK) &&
      is_form_submit) {
    return ui::PAGE_TRANSITION_FORM_SUBMIT;
  }
  return default_transition;
}

// Calculates transition type for the specific document loaded using
// WebDocumentLoader. Used while loading subresources.
ui::PageTransition GetTransitionType(blink::WebDocumentLoader* document_loader,
                                     bool is_main_frame) {
  NavigationState* navigation_state =
      NavigationState::FromDocumentLoader(document_loader);
  ui::PageTransition default_transition =
      navigation_state->IsContentInitiated()
          ? ui::PAGE_TRANSITION_LINK
          : navigation_state->common_params().transition;
  if (navigation_state->WasWithinSameDocument())
    return default_transition;
  return GetTransitionType(default_transition,
                           document_loader->ReplacesCurrentHistoryItem(),
                           is_main_frame, document_loader->GetNavigationType());
}

void GetRedirectChain(WebDocumentLoader* document_loader,
                      std::vector<GURL>* result) {
  WebVector<WebURL> urls;
  document_loader->RedirectChain(urls);
  result->reserve(urls.size());
  for (size_t i = 0; i < urls.size(); ++i) {
    result->push_back(urls[i]);
  }
}

// Gets URL that should override the default getter for this data source
// (if any), storing it in |output|. Returns true if there is an override URL.
bool MaybeGetOverriddenURL(WebDocumentLoader* document_loader, GURL* output) {
  DocumentState* document_state =
      DocumentState::FromDocumentLoader(document_loader);

  // If load was from a data URL, then the saved data URL, not the history
  // URL, should be the URL of the data source.
  if (document_state->was_load_data_with_base_url_request()) {
    *output = document_state->data_url();
    return true;
  }

  // WebDocumentLoader has unreachable URL means that the frame is loaded
  // through blink::WebFrame::loadData(), and the base URL will be in the
  // redirect chain. However, we never visited the baseURL. So in this case, we
  // should use the unreachable URL as the original URL.
  if (document_loader->HasUnreachableURL()) {
    *output = document_loader->UnreachableURL();
    return true;
  }

  return false;
}

// Returns the original request url. If there is no redirect, the original
// url is the same as document loader's OriginalUrl(). If the WebDocumentLoader
// belongs to a frame was loaded by loadData, the original url will be
// it's UnreachableURL().
GURL GetOriginalRequestURL(WebDocumentLoader* document_loader) {
  GURL overriden_url;
  if (MaybeGetOverriddenURL(document_loader, &overriden_url))
    return overriden_url;

  std::vector<GURL> redirects;
  GetRedirectChain(document_loader, &redirects);
  if (!redirects.empty())
    return redirects.at(0);

  return document_loader->OriginalUrl();
}

// Returns false unless this is a top-level navigation.
bool IsTopLevelNavigation(WebFrame* frame) {
  return frame->Parent() == nullptr;
}

void FillNavigationParamsRequest(
    const mojom::CommonNavigationParams& common_params,
    const mojom::CommitNavigationParams& commit_params,
    blink::WebNavigationParams* navigation_params) {
  // Use the original navigation url to start with. We'll replay the redirects
  // afterwards and will eventually arrive to the final url.
  navigation_params->url = !commit_params.original_url.is_empty()
                               ? commit_params.original_url
                               : common_params.url;
  navigation_params->http_method = WebString::FromASCII(
      !commit_params.original_method.empty() ? commit_params.original_method
                                             : common_params.method);
  navigation_params->ip_address_space = commit_params.ip_address_space;

  if (common_params.referrer->url.is_valid()) {
    WebString referrer = WebSecurityPolicy::GenerateReferrerHeader(
        common_params.referrer->policy, common_params.url,
        WebString::FromUTF8(common_params.referrer->url.spec()));
    navigation_params->referrer = referrer;
    navigation_params->referrer_policy = common_params.referrer->policy;
  }
  if (common_params.referrer->policy !=
      network::mojom::ReferrerPolicy::kDefault) {
    navigation_params->referrer_policy = common_params.referrer->policy;
  }

  if (common_params.post_data) {
    navigation_params->http_body =
        GetWebHTTPBodyForRequestBody(*common_params.post_data);
    if (!commit_params.post_content_type.empty()) {
      navigation_params->http_content_type =
          WebString::FromASCII(commit_params.post_content_type);
    }
  }

  if (common_params.previews_state & kDisabledPreviewsBits) {
    // Sanity check disabled vs. enabled bits here before passing on.
    DCHECK(!(common_params.previews_state & ~kDisabledPreviewsBits))
        << common_params.previews_state;
  }
  navigation_params->previews_state =
      static_cast<blink::PreviewsState>(common_params.previews_state);

  // Set the request initiator origin, which is supplied by the browser
  // process. It is present in cases such as navigating a frame in a different
  // process, which is routed through RenderFrameProxy and the origin is
  // required to correctly compute the effective origin in which the
  // navigation will commit.
  if (common_params.initiator_origin) {
    navigation_params->requestor_origin =
        common_params.initiator_origin.value();
  }

  navigation_params->initiator_origin_trial_features = {
      common_params.initiator_origin_trial_features.begin(),
      common_params.initiator_origin_trial_features.end()};

  navigation_params->was_discarded = commit_params.was_discarded;
  navigation_params->document_ukm_source_id =
      commit_params.document_ukm_source_id;

  if (!commit_params.prefetched_signed_exchanges.empty()) {
    navigation_params->prefetched_signed_exchanges = WebVector<std::unique_ptr<
        blink::WebNavigationParams::PrefetchedSignedExchange>>();

    for (const auto& exchange : commit_params.prefetched_signed_exchanges) {
      blink::WebURLResponse web_response;
      WebURLLoaderImpl::PopulateURLResponse(
          exchange->inner_url, *exchange->inner_response, &web_response,
          false /* report_security_info*/, -1 /* request_id */);
      navigation_params->prefetched_signed_exchanges.emplace_back(
          std::make_unique<
              blink::WebNavigationParams::PrefetchedSignedExchange>(
              exchange->outer_url,
              WebString::FromLatin1(
                  signed_exchange_utils::CreateHeaderIntegrityHashString(
                      exchange->header_integrity)),
              exchange->inner_url, web_response,
              std::move(exchange->loader_factory_handle)));
    }
  }

  navigation_params->had_transient_activation = common_params.has_user_gesture;
  navigation_params->web_bundle_physical_url =
      commit_params.web_bundle_physical_url;
  navigation_params->web_bundle_claimed_url =
      commit_params.web_bundle_claimed_url;

  WebVector<WebString> web_origin_trials;
  web_origin_trials.reserve(commit_params.force_enabled_origin_trials.size());
  for (const auto& trial : commit_params.force_enabled_origin_trials)
    web_origin_trials.emplace_back(WebString::FromASCII(trial));
  navigation_params->force_enabled_origin_trials = web_origin_trials;

  WebVector<WebString> forced_content_security_policies;
  forced_content_security_policies.reserve(
      commit_params.forced_content_security_policies.size());
  for (const auto& csp : commit_params.forced_content_security_policies)
    forced_content_security_policies.emplace_back(WebString::FromASCII(csp));
  navigation_params->forced_content_security_policies =
      forced_content_security_policies;
}

mojom::CommonNavigationParamsPtr MakeCommonNavigationParams(
    const WebSecurityOrigin& current_origin,
    std::unique_ptr<blink::WebNavigationInfo> info,
    int load_flags,
    bool has_download_sandbox_flag,
    bool from_ad,
    bool is_history_navigation_in_new_child_frame) {
  // A valid RequestorOrigin is always expected to be present.
  DCHECK(!info->url_request.RequestorOrigin().IsNull());

  blink::mojom::ReferrerPtr referrer = blink::mojom::Referrer::New(
      blink::WebStringToGURL(info->url_request.ReferrerString()),
      info->url_request.GetReferrerPolicy());

  // No history-navigation is expected to happen.
  DCHECK(info->navigation_type != blink::kWebNavigationTypeBackForward);

  // Determine the navigation type. No same-document navigation is expected
  // because it is loaded immediately by the FrameLoader.
  mojom::NavigationType navigation_type =
      mojom::NavigationType::DIFFERENT_DOCUMENT;
  if (info->navigation_type == blink::kWebNavigationTypeReload) {
    if (load_flags & net::LOAD_BYPASS_CACHE)
      navigation_type = mojom::NavigationType::RELOAD_BYPASSING_CACHE;
    else
      navigation_type = mojom::NavigationType::RELOAD;
  }

  auto source_location = network::mojom::SourceLocation::New(
      info->source_location.url.Latin1(), info->source_location.line_number,
      info->source_location.column_number);

  const RequestExtraData* extra_data =
      static_cast<RequestExtraData*>(info->url_request.GetExtraData().get());
  DCHECK(extra_data);

  // Convert from WebVector<int> to std::vector<int>.
  std::vector<int> initiator_origin_trial_features(
      info->initiator_origin_trial_features.begin(),
      info->initiator_origin_trial_features.end());

  NavigationDownloadPolicy download_policy;
  RenderFrameImpl::MaybeSetDownloadFramePolicy(
      info->is_opener_navigation, info->url_request, current_origin,
      has_download_sandbox_flag, info->blocking_downloads_in_sandbox_enabled,
      from_ad, &download_policy);

  auto initiator_csp_info = mojom::InitiatorCSPInfo::New();
  initiator_csp_info->should_check_main_world_csp =
      info->should_check_main_world_content_security_policy;
  initiator_csp_info->initiator_self_source =
      BuildCSPSource(info->initiator_self_source);
  for (const auto& policy : info->initiator_csp) {
    initiator_csp_info->initiator_csp.push_back(
        BuildContentSecurityPolicy(policy));
  }

  return mojom::CommonNavigationParams::New(
      info->url_request.Url(), info->url_request.RequestorOrigin(),
      std::move(referrer), extra_data->transition_type(), navigation_type,
      download_policy,
      info->frame_load_type == WebFrameLoadType::kReplaceCurrentItem, GURL(),
      GURL(),
      static_cast<blink::PreviewsState>(info->url_request.GetPreviewsState()),
      base::TimeTicks::Now(), info->url_request.HttpMethod().Latin1(),
      GetRequestBodyForWebURLRequest(info->url_request),
      std::move(source_location), false /* started_from_context_menu */,
      info->url_request.HasUserGesture(),
      info->url_request.HasTextFragmentToken(), std::move(initiator_csp_info),
      initiator_origin_trial_features, info->href_translate.Latin1(),
      is_history_navigation_in_new_child_frame, info->input_start);
}

WebFrameLoadType NavigationTypeToLoadType(mojom::NavigationType navigation_type,
                                          bool should_replace_current_entry,
                                          bool has_valid_page_state) {
  switch (navigation_type) {
    case mojom::NavigationType::RELOAD:
    case mojom::NavigationType::RELOAD_ORIGINAL_REQUEST_URL:
      return WebFrameLoadType::kReload;

    case mojom::NavigationType::RELOAD_BYPASSING_CACHE:
      return WebFrameLoadType::kReloadBypassingCache;

    case mojom::NavigationType::HISTORY_SAME_DOCUMENT:
    case mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT:
      return WebFrameLoadType::kBackForward;

    case mojom::NavigationType::RESTORE:
    case mojom::NavigationType::RESTORE_WITH_POST:
      if (has_valid_page_state)
        return WebFrameLoadType::kBackForward;
      // If there is no valid page state, fall through to the default case.
      FALLTHROUGH;

    case mojom::NavigationType::SAME_DOCUMENT:
    case mojom::NavigationType::DIFFERENT_DOCUMENT:
      return should_replace_current_entry
                 ? WebFrameLoadType::kReplaceCurrentItem
                 : WebFrameLoadType::kStandard;

    default:
      NOTREACHED();
      return WebFrameLoadType::kStandard;
  }
}

RenderFrameImpl::CreateRenderFrameImplFunction g_create_render_frame_impl =
    nullptr;

WebString ConvertRelativePathToHtmlAttribute(const base::FilePath& path) {
  DCHECK(!path.IsAbsolute());
  return WebString::FromUTF8(
      std::string("./") +
      path.NormalizePathSeparatorsTo(FILE_PATH_LITERAL('/')).AsUTF8Unsafe());
}

class RenderFrameWebFrameSerializerClient
    : public blink::WebFrameSerializerClient {
 public:
  explicit RenderFrameWebFrameSerializerClient(
      mojo::PendingRemote<mojom::FrameHTMLSerializerHandler> handler_remote)
      : handler_remote_(std::move(handler_remote)) {}

  // WebFrameSerializerClient implementation:
  void DidSerializeDataForFrame(
      const WebVector<char>& data,
      WebFrameSerializerClient::FrameSerializationStatus status) override {
    DCHECK(handler_remote_.is_bound());
    handler_remote_->DidReceiveData(std::string(data.Data(), data.size()));

    // Make sure to report Done() to the browser process when receiving the last
    // chunk of data, and reset the mojo remote so that the DCHECK above ensures
    // this method won't be called anymore after this point.
    if (status == WebFrameSerializerClient::kCurrentFrameIsFinished) {
      handler_remote_->Done();
      handler_remote_.reset();
    }
  }

 private:
  mojo::Remote<mojom::FrameHTMLSerializerHandler> handler_remote_;
};

// Implementation of WebFrameSerializer::LinkRewritingDelegate that responds
// based on the payload of mojom::Frame::GetSerializedHtmlWithLocalLinks().
class LinkRewritingDelegate : public WebFrameSerializer::LinkRewritingDelegate {
 public:
  LinkRewritingDelegate(
      const base::flat_map<GURL, base::FilePath>& url_to_local_path,
      const base::flat_map<base::UnguessableToken, base::FilePath>&
          frame_token_to_local_path)
      : url_to_local_path_(url_to_local_path),
        frame_token_to_local_path_(frame_token_to_local_path) {}

  bool RewriteFrameSource(WebFrame* frame, WebString* rewritten_link) override {
    const base::Optional<base::UnguessableToken>& frame_token =
        frame->GetFrameToken();

    DCHECK(frame_token.has_value());
    auto it = frame_token_to_local_path_.find(frame_token.value());
    if (it == frame_token_to_local_path_.end())
      return false;  // This can happen because of https://crbug.com/541354.

    const base::FilePath& local_path = it->second;
    *rewritten_link = ConvertRelativePathToHtmlAttribute(local_path);
    return true;
  }

  bool RewriteLink(const WebURL& url, WebString* rewritten_link) override {
    auto it = url_to_local_path_.find(GURL(url));
    if (it == url_to_local_path_.end())
      return false;

    const base::FilePath& local_path = it->second;
    *rewritten_link = ConvertRelativePathToHtmlAttribute(local_path);
    return true;
  }

 private:
  const base::flat_map<GURL, base::FilePath>& url_to_local_path_;
  const base::flat_map<base::UnguessableToken, base::FilePath>&
      frame_token_to_local_path_;
};

// Implementation of WebFrameSerializer::MHTMLPartsGenerationDelegate that
// 1. Bases shouldSkipResource and getContentID responses on contents of
//    SerializeAsMHTMLParams.
// 2. Stores digests of urls of serialized resources (i.e. urls reported via
//    shouldSkipResource) into |serialized_resources_uri_digests| passed
//    to the constructor.
class MHTMLPartsGenerationDelegate
    : public WebFrameSerializer::MHTMLPartsGenerationDelegate {
 public:
  MHTMLPartsGenerationDelegate(
      const mojom::SerializeAsMHTMLParams& params,
      std::unordered_set<std::string>* serialized_resources_uri_digests)
      : params_(params),
        serialized_resources_uri_digests_(serialized_resources_uri_digests) {
    DCHECK(serialized_resources_uri_digests_);
    // Digests must be sorted for binary search.
    DCHECK(std::is_sorted(params_.digests_of_uris_to_skip.begin(),
                          params_.digests_of_uris_to_skip.end()));
    // URLs are not duplicated.
    DCHECK(std::adjacent_find(params_.digests_of_uris_to_skip.begin(),
                              params_.digests_of_uris_to_skip.end()) ==
           params_.digests_of_uris_to_skip.end());
  }

  bool ShouldSkipResource(const WebURL& url) override {
    std::string digest =
        crypto::SHA256HashString(params_.salt + GURL(url).spec());

    // Skip if the |url| already covered by serialization of an *earlier* frame.
    if (std::binary_search(params_.digests_of_uris_to_skip.begin(),
                           params_.digests_of_uris_to_skip.end(), digest))
      return true;

    // Let's record |url| as being serialized for the *current* frame.
    auto pair = serialized_resources_uri_digests_->insert(digest);
    bool insertion_took_place = pair.second;
    DCHECK(insertion_took_place);  // Blink should dedupe within a frame.

    return false;
  }

  bool UseBinaryEncoding() override { return params_.mhtml_binary_encoding; }

  bool RemovePopupOverlay() override {
    return params_.mhtml_popup_overlay_removal;
  }

  bool UsePageProblemDetectors() override {
    return params_.mhtml_problem_detection;
  }

 private:
  const mojom::SerializeAsMHTMLParams& params_;
  std::unordered_set<std::string>* serialized_resources_uri_digests_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLPartsGenerationDelegate);
};

bool IsHttpPost(const blink::WebURLRequest& request) {
  return request.HttpMethod().Utf8() == "POST";
}

// Delegate responsible for determining the handle writing implementation by
// instantiating an MHTMLHandleWriter on the heap respective to the passed in
// MHTMLSerializationParams. This transfers ownership of the handle to the
// new MHTMLHandleWriter.
class MHTMLHandleWriterDelegate {
 public:
  MHTMLHandleWriterDelegate(
      const mojom::SerializeAsMHTMLParams& params,
      MHTMLHandleWriter::MHTMLWriteCompleteCallback callback,
      scoped_refptr<base::TaskRunner> main_thread_task_runner) {
    // Handle must be instantiated.
    DCHECK(params.output_handle);

    if (params.output_handle->is_file_handle()) {
      handle_ = new MHTMLFileHandleWriter(
          std::move(main_thread_task_runner), std::move(callback),
          std::move(params.output_handle->get_file_handle()));
    } else {
      handle_ = new MHTMLProducerHandleWriter(
          std::move(main_thread_task_runner), std::move(callback),
          std::move(params.output_handle->get_producer_handle()));
    }
  }

  void WriteContents(std::vector<WebThreadSafeData> mhtml_contents) {
    // Using base::Unretained is safe, as calls to WriteContents() always
    // deletes |handle| upon Finish().
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&MHTMLHandleWriter::WriteContents,
                       base::Unretained(handle_), std::move(mhtml_contents)));
  }

  // Within the context of the delegate, only for premature write finish.
  void Finish(mojom::MhtmlSaveStatus save_status) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&MHTMLHandleWriter::Finish, base::Unretained(handle_),
                       save_status));
  }

 private:
  MHTMLHandleWriter* handle_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLHandleWriterDelegate);
};

// Use this for histograms with dynamically generated names, which otherwise
// can't use the UMA_HISTOGRAM_MEMORY_MB macro without code duplication.
void RecordSuffixedMemoryMBHistogram(base::StringPiece name,
                                     base::StringPiece suffix,
                                     int sample_mb) {
  base::UmaHistogramMemoryMB(base::StrCat({name, suffix}), sample_mb);
}

void RecordSuffixedRendererMemoryMetrics(
    const RenderThreadImpl::RendererMemoryMetrics& memory_metrics,
    base::StringPiece suffix) {
  RecordSuffixedMemoryMBHistogram("Memory.Experimental.Renderer.PartitionAlloc",
                                  suffix,
                                  memory_metrics.partition_alloc_kb / 1024);
  RecordSuffixedMemoryMBHistogram("Memory.Experimental.Renderer.BlinkGC",
                                  suffix, memory_metrics.blink_gc_kb / 1024);
  RecordSuffixedMemoryMBHistogram("Memory.Experimental.Renderer.Malloc", suffix,
                                  memory_metrics.malloc_mb);
  RecordSuffixedMemoryMBHistogram("Memory.Experimental.Renderer.Discardable",
                                  suffix, memory_metrics.discardable_kb / 1024);
  RecordSuffixedMemoryMBHistogram(
      "Memory.Experimental.Renderer.V8MainThreadIsolate", suffix,
      memory_metrics.v8_main_thread_isolate_mb);
  RecordSuffixedMemoryMBHistogram("Memory.Experimental.Renderer.TotalAllocated",
                                  suffix, memory_metrics.total_allocated_mb);
  RecordSuffixedMemoryMBHistogram(
      "Memory.Experimental.Renderer.NonDiscardableTotalAllocated", suffix,
      memory_metrics.non_discardable_total_allocated_mb);
  RecordSuffixedMemoryMBHistogram(
      "Memory.Experimental.Renderer.TotalAllocatedPerRenderView", suffix,
      memory_metrics.total_allocated_per_render_view_mb);
}

mojo::PendingRemote<blink::mojom::BlobURLToken> CloneBlobURLToken(
    blink::CrossVariantMojoRemote<blink::mojom::BlobURLTokenInterfaceBase>&
        blob_url_token) {
  if (!blob_url_token)
    return mojo::NullRemote();
  mojo::PendingRemote<blink::mojom::BlobURLToken> cloned_token;
  mojo::Remote<blink::mojom::BlobURLToken> token(std::move(blob_url_token));
  token->Clone(cloned_token.InitWithNewPipeAndPassReceiver());
  blob_url_token = token.Unbind();
  return cloned_token;
}

// Creates a fully functional DocumentState in the case where we do not have
// navigation parameters available.
std::unique_ptr<DocumentState> BuildDocumentState() {
  std::unique_ptr<DocumentState> document_state =
      std::make_unique<DocumentState>();
  InternalDocumentStateData::FromDocumentState(document_state.get())
      ->set_navigation_state(NavigationState::CreateContentInitiated());
  return document_state;
}

// Creates a fully functional DocumentState in the case where we have
// navigation parameters available in the RenderFrameImpl.
std::unique_ptr<DocumentState> BuildDocumentStateFromParams(
    const mojom::CommonNavigationParams& common_params,
    const mojom::CommitNavigationParams& commit_params,
    mojom::NavigationClient::CommitNavigationCallback commit_callback,
    std::unique_ptr<NavigationClient> navigation_client,
    int request_id,
    bool was_initiated_in_this_frame) {
  std::unique_ptr<DocumentState> document_state(new DocumentState());
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state.get());

  DCHECK(!common_params.navigation_start.is_null());
  DCHECK(!common_params.url.SchemeIs(url::kJavaScriptScheme));

  internal_data->set_is_overriding_user_agent(
      commit_params.is_overriding_user_agent);
  internal_data->set_must_reset_scroll_and_scale_state(
      common_params.navigation_type ==
      mojom::NavigationType::RELOAD_ORIGINAL_REQUEST_URL);
  internal_data->set_request_id(request_id);

  bool load_data = !common_params.base_url_for_data_url.is_empty() &&
                   !common_params.history_url_for_data_url.is_empty() &&
                   common_params.url.SchemeIs(url::kDataScheme);
  document_state->set_was_load_data_with_base_url_request(load_data);
  if (load_data)
    document_state->set_data_url(common_params.url);

  InternalDocumentStateData::FromDocumentState(document_state.get())
      ->set_navigation_state(NavigationState::CreateBrowserInitiated(
          common_params.Clone(), commit_params.Clone(),
          std::move(commit_callback), std::move(navigation_client),
          was_initiated_in_this_frame));
  return document_state;
}

void ApplyFilePathAlias(blink::WebURLRequest* request) {
  const base::CommandLine::StringType file_url_path_alias =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kFileUrlPathAlias);
  if (file_url_path_alias.empty())
    return;

  const auto alias_mapping =
      base::SplitString(file_url_path_alias, FILE_PATH_LITERAL("="),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (alias_mapping.size() != 2) {
    LOG(ERROR) << "Invalid file path alias format.";
    return;
  }

#if defined(OS_WIN)
  base::string16 path = request->Url().GetString().Utf16();
  const base::string16 file_prefix =
      base::ASCIIToUTF16(url::kFileScheme) +
      base::ASCIIToUTF16(url::kStandardSchemeSeparator);
#else
  std::string path = request->Url().GetString().Utf8();
  const std::string file_prefix =
      std::string(url::kFileScheme) + url::kStandardSchemeSeparator;
#endif
  if (!base::StartsWith(path, file_prefix + alias_mapping[0],
                        base::CompareCase::SENSITIVE)) {
    return;
  }

  base::ReplaceFirstSubstringAfterOffset(&path, 0, alias_mapping[0],
                                         alias_mapping[1]);
  request->SetUrl(blink::WebURL(GURL(path)));
}

// Packs all navigation timings sent by the browser to a blink understandable
// format, blink::WebNavigationTimings.
blink::WebNavigationTimings BuildNavigationTimings(
    base::TimeTicks navigation_start,
    const mojom::NavigationTiming& browser_navigation_timings,
    base::TimeTicks input_start) {
  blink::WebNavigationTimings renderer_navigation_timings;

  // Sanitizes the navigation_start timestamp for browser-initiated navigations,
  // where the browser possibly has a better notion of start time than the
  // renderer. In the case of cross-process navigations, this carries over the
  // time of finishing the onbeforeunload handler of the previous page.
  // TimeTicks is sometimes not monotonic across processes, and because
  // |browser_navigation_start| is likely before this process existed,
  // InterProcessTimeTicksConverter won't help. The timestamp is sanitized by
  // clamping it to now.
  DCHECK(!navigation_start.is_null());
  renderer_navigation_timings.navigation_start =
      std::min(navigation_start, base::TimeTicks::Now());

  renderer_navigation_timings.redirect_start =
      browser_navigation_timings.redirect_start;
  renderer_navigation_timings.redirect_end =
      browser_navigation_timings.redirect_end;
  renderer_navigation_timings.fetch_start =
      browser_navigation_timings.fetch_start;

  renderer_navigation_timings.input_start = input_start;

  return renderer_navigation_timings;
}

// Fills navigation data sent by the browser to a blink understandable
// format, blink::WebNavigationParams.
void FillMiscNavigationParams(
    const mojom::CommonNavigationParams& common_params,
    const mojom::CommitNavigationParams& commit_params,
    blink::WebNavigationParams* navigation_params) {
  navigation_params->navigation_timings = BuildNavigationTimings(
      common_params.navigation_start, *commit_params.navigation_timing,
      common_params.input_start);

  navigation_params->is_user_activated =
      commit_params.was_activated == mojom::WasActivatedOption::kYes;

  navigation_params->has_text_fragment_token =
      common_params.text_fragment_token;

  navigation_params->is_browser_initiated = commit_params.is_browser_initiated;

  navigation_params->is_cross_browsing_context_group_navigation =
      commit_params.is_cross_browsing_instance;

#if defined(OS_ANDROID)
  // Only android webview uses this.
  navigation_params->grant_load_local_resources =
      commit_params.can_load_local_resources;
#else
  DCHECK(!commit_params.can_load_local_resources);
#endif

  if (commit_params.origin_to_commit) {
    navigation_params->origin_to_commit =
        commit_params.origin_to_commit.value();
  }
  navigation_params->appcache_host_id =
      commit_params.appcache_host_id.value_or(base::UnguessableToken());

  navigation_params->frame_policy = commit_params.frame_policy;

  if (common_params.navigation_type == mojom::NavigationType::RESTORE) {
    // We're doing a load of a page that was restored from the last session.
    // By default this prefers the cache over loading
    // (LOAD_SKIP_CACHE_VALIDATION) which can result in stale data for pages
    // that are set to expire. We explicitly override that by setting the
    // policy here so that as necessary we load from the network.
    //
    // TODO(davidben): Remove this in favor of passing a cache policy to the
    // loadHistoryItem call in OnNavigate. That requires not overloading
    // UseProtocolCachePolicy to mean both "normal load" and "determine cache
    // policy based on load type, etc".
    navigation_params->force_fetch_cache_mode =
        blink::mojom::FetchCacheMode::kDefault;
  }

  navigation_params->origin_isolated = commit_params.origin_isolated;

  navigation_params->enabled_client_hints.reserve(
      commit_params.enabled_client_hints.size());
  for (auto enabled_hint : commit_params.enabled_client_hints)
    navigation_params->enabled_client_hints.emplace_back(enabled_hint);
}

// Fills in the origin policy associated with this response, if any is present.
// Converts it into a format that blink understands: WebOriginPolicy.
void FillNavigationParamsOriginPolicy(
    const network::mojom::URLResponseHead& head,
    blink::WebNavigationParams* navigation_params) {
  if (head.origin_policy.has_value() && head.origin_policy.value().contents) {
    navigation_params->origin_policy = blink::WebOriginPolicy();

    for (const auto& id : head.origin_policy.value().contents->ids) {
      navigation_params->origin_policy->ids.emplace_back(
          WebString::FromUTF8(id));
    }

    const base::Optional<std::string>& feature_policy =
        head.origin_policy.value().contents->feature_policy;
    if (feature_policy) {
      navigation_params->origin_policy->feature_policy =
          WebString::FromUTF8(*feature_policy);
    }

    for (const auto& csp :
         head.origin_policy.value().contents->content_security_policies) {
      navigation_params->origin_policy->content_security_policies.emplace_back(
          WebString::FromUTF8(csp));
    }

    for (const auto& csp_report_only :
         head.origin_policy.value()
             .contents->content_security_policies_report_only) {
      navigation_params->origin_policy->content_security_policies_report_only
          .emplace_back(WebString::FromUTF8(csp_report_only));
    }
  }
}

// Asks RenderProcessHostImpl::CreateURLLoaderFactoryForRendererProcess in the
// browser process for a URLLoaderFactory.
//
// AVOID: see the comment on CreateDefaultURLLoaderFactoryBundle below.
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateDefaultURLLoaderFactory() {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  ChildThread::Get()->BindHostReceiver(
      factory_remote.InitWithNewPipeAndPassReceiver());
  return factory_remote;
}

// Returns a non-null pointer to a URLLoaderFactory bundle that is not
// associated with any specific origin, frame or worker.
//
// AVOID: prefer to use an origin/frame/worker-specific factory (e.g. via
// content::RenderFrameImpl::FrameURLLoaderFactory::CreateURLLoader).  See
// also https://crbug.com/1114822.
//
// It is invalid to call this in an incomplete env where
// RenderThreadImpl::current() returns nullptr (e.g. in some tests).
scoped_refptr<ChildURLLoaderFactoryBundle>
CreateDefaultURLLoaderFactoryBundle() {
  return base::MakeRefCounted<ChildURLLoaderFactoryBundle>(
      base::BindOnce(&CreateDefaultURLLoaderFactory));
}

v8::MaybeLocal<v8::Value> GetProperty(v8::Local<v8::Context> context,
                                      v8::Local<v8::Value> object,
                                      const base::string16& name) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::String> name_str =
      gin::ConvertToV8(isolate, name).As<v8::String>();
  v8::Local<v8::Object> object_obj;
  if (!object->ToObject(context).ToLocal(&object_obj)) {
    return v8::MaybeLocal<v8::Value>();
  }
  return object_obj->Get(context, name_str);
}

v8::MaybeLocal<v8::Value> CallMethodOnFrame(blink::WebNavigationControl* frame,
                                            const base::string16& object_name,
                                            const base::string16& method_name,
                                            base::Value arguments) {
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();

  v8::Context::Scope context_scope(context);
  std::vector<v8::Local<v8::Value>> args;
  V8ValueConverterImpl converter;
  converter.SetDateAllowed(true);
  converter.SetRegExpAllowed(true);
  for (auto const& argument : arguments.GetList()) {
    args.push_back(converter.ToV8Value(&argument, context));
  }

  v8::Local<v8::Value> object;
  v8::Local<v8::Value> method;
  if (!GetProperty(context, context->Global(), object_name).ToLocal(&object) ||
      !GetProperty(context, object, method_name).ToLocal(&method)) {
    return v8::MaybeLocal<v8::Value>();
  }

  return frame->ExecuteMethodAndReturnValue(
      v8::Local<v8::Function>::Cast(method), object,
      static_cast<int>(args.size()), args.data());
}

}  // namespace

RenderFrameImpl::AssertNavigationCommits::AssertNavigationCommits(
    RenderFrameImpl* frame)
    : AssertNavigationCommits(frame, false) {}

RenderFrameImpl::AssertNavigationCommits::AssertNavigationCommits(
    RenderFrameImpl* frame,
    MayReplaceInitialEmptyDocumentTag)
    : AssertNavigationCommits(frame, true) {}

RenderFrameImpl::AssertNavigationCommits::~AssertNavigationCommits() {
  // Frame might have been synchronously detached when dispatching JS events.
  if (frame_) {
    CHECK_EQ(NavigationCommitState::kDidCommit,
             frame_->navigation_commit_state_);
    frame_->navigation_commit_state_ = NavigationCommitState::kNone;
  }
}

RenderFrameImpl::AssertNavigationCommits::AssertNavigationCommits(
    RenderFrameImpl* frame,
    bool allow_transition_from_initial_empty_document)
    : frame_(frame->weak_factory_.GetWeakPtr()) {
  if (NavigationCommitState::kNone != frame->navigation_commit_state_) {
    CHECK(allow_transition_from_initial_empty_document);
    CHECK_EQ(NavigationCommitState::kInitialEmptyDocument,
             frame->navigation_commit_state_);
  }
  frame->navigation_commit_state_ = NavigationCommitState::kWillCommit;
}

// This class uses existing WebNavigationBodyLoader to read the whole response
// body into in-memory buffer, and then creates another body loader with static
// data so that we can parse mhtml archive synchronously. This is a workaround
// for the fact that we need the whole archive to determine the document's mime
// type and construct a right document instance.
class RenderFrameImpl::MHTMLBodyLoaderClient
    : public blink::WebNavigationBodyLoader::Client {
 public:
  // Once the body is read, fills |navigation_params| with the new body loader
  // and calls |done_callbcak|.
  MHTMLBodyLoaderClient(
      RenderFrameImpl* frame,
      std::unique_ptr<blink::WebNavigationParams> navigation_params,
      base::OnceCallback<void(std::unique_ptr<blink::WebNavigationParams>)>
          done_callback)
      : frame_(frame),
        navigation_params_(std::move(navigation_params)),
        body_loader_(std::move(navigation_params_->body_loader)),
        done_callback_(std::move(done_callback)) {
    body_loader_->StartLoadingBody(this, false /* use_isolated_code_cache */);
  }

  ~MHTMLBodyLoaderClient() override {
    // MHTMLBodyLoaderClient is reset in several different places. Either:
    CHECK(
        // - the body load finished and the result is being committed, so
        //   |BodyLoadingFinished| (see below) should still be on the stack, or
        committing_ ||
        // - MHTMLBodyLoaderClient is abandoned, either because:
        //   - a new renderer-initiated navigation began, which explicitly
        //     detaches any existing MHTMLBodyLoaderClient
        //   - this renderer began the navigation and cancelled it with
        //     |AbortClientNavigation|, e.g. JS called window.stop(), which
        //     explicitly detaches any existing MHTMLBodyLoaderClient
        //   - the frame is detached and self-deleted, which also explicitly
        //     detaches any existing MHTMLBodyLoaderClient or,
        !frame_ ||
        //   - the browser requested a different navigation be committed in this
        //     frame, i.e. the navigation commit state should be |kWillCommit|
        NavigationCommitState::kWillCommit == frame_->navigation_commit_state_);
  }

  // Marks |this|'s pending load as abandoned. There are a number of reasons
  // this can happen; see the destructor for more information.
  void Detach() {
    // Note that the MHTMLBodyLoaderClient might be associated with a
    // provisional frame, so this does not assert that `frame_->in_frame_tree_`
    // is true.
    frame_ = nullptr;
  }

  // blink::WebNavigationBodyLoader::Client overrides:
  void BodyCodeCacheReceived(mojo_base::BigBuffer data) override {}

  void BodyDataReceived(base::span<const char> data) override {
    data_.Append(data.data(), data.size());
  }

  void BodyLoadingFinished(base::TimeTicks completion_time,
                           int64_t total_encoded_data_length,
                           int64_t total_encoded_body_length,
                           int64_t total_decoded_body_length,
                           bool should_report_corb_blocking,
                           const base::Optional<WebURLError>& error) override {
    committing_ = true;
    AssertNavigationCommits assert_navigation_commits(frame_);
    if (!error.has_value()) {
      WebNavigationParams::FillBodyLoader(navigation_params_.get(), data_);
      // Clear |is_static_data| flag to avoid the special behavior it triggers,
      // e.g. skipping content disposition check. We want this load to be
      // regular, just like with an original body loader.
      navigation_params_->is_static_data = false;
    }
    std::move(done_callback_).Run(std::move(navigation_params_));
  }

 private:
  // |RenderFrameImpl| owns |this|, so |frame_| is guaranteed to outlive |this|.
  // Will be nulled if |Detach()| has been called.
  RenderFrameImpl* frame_;
  bool committing_ = false;
  WebData data_;
  std::unique_ptr<blink::WebNavigationParams> navigation_params_;
  std::unique_ptr<blink::WebNavigationBodyLoader> body_loader_;
  base::OnceCallback<void(std::unique_ptr<blink::WebNavigationParams>)>
      done_callback_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLBodyLoaderClient);
};

class RenderFrameImpl::FrameURLLoaderFactory
    : public blink::WebURLLoaderFactory {
 public:
  explicit FrameURLLoaderFactory(base::WeakPtr<RenderFrameImpl> frame)
      : frame_(std::move(frame)) {}

  ~FrameURLLoaderFactory() override = default;

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const WebURLRequest& request,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override {
    // This should not be called if the frame is detached.
    DCHECK(frame_);

    mojo::PendingRemote<mojom::KeepAliveHandle> keep_alive_handle;
    if (request.GetKeepalive()) {
      frame_->GetFrameHost()->IssueKeepAliveHandle(
          keep_alive_handle.InitWithNewPipeAndPassReceiver());
    }
    return std::make_unique<WebURLLoaderImpl>(
        RenderThreadImpl::current()->resource_dispatcher(),
        std::move(task_runner_handle), frame_->GetLoaderFactoryBundle(),
        std::move(keep_alive_handle));
  }

 private:
  base::WeakPtr<RenderFrameImpl> frame_;

  DISALLOW_COPY_AND_ASSIGN(FrameURLLoaderFactory);
};

// The following methods are outside of the anonymous namespace to ensure that
// the corresponding symbols get emmitted even on symbol_level 1.
NOINLINE void ExhaustMemory() {
  volatile void* ptr = nullptr;
  do {
    ptr = malloc(0x10000000);
    base::debug::Alias(&ptr);
  } while (ptr);
}

#if defined(ADDRESS_SANITIZER)
NOINLINE void MaybeTriggerAsanError(const GURL& url) {
  // NOTE(rogerm): We intentionally perform an invalid heap access here in
  //     order to trigger an Address Sanitizer (ASAN) error report.
  if (url == kChromeUICrashHeapOverflowURL) {
    LOG(ERROR) << "Intentionally causing ASAN heap overflow"
               << " because user navigated to " << url.spec();
    base::debug::AsanHeapOverflow();
  } else if (url == kChromeUICrashHeapUnderflowURL) {
    LOG(ERROR) << "Intentionally causing ASAN heap underflow"
               << " because user navigated to " << url.spec();
    base::debug::AsanHeapUnderflow();
  } else if (url == kChromeUICrashUseAfterFreeURL) {
    LOG(ERROR) << "Intentionally causing ASAN heap use-after-free"
               << " because user navigated to " << url.spec();
    base::debug::AsanHeapUseAfterFree();
#if defined(OS_WIN)
  } else if (url == kChromeUICrashCorruptHeapBlockURL) {
    LOG(ERROR) << "Intentionally causing ASAN corrupt heap block"
               << " because user navigated to " << url.spec();
    base::debug::AsanCorruptHeapBlock();
  } else if (url == kChromeUICrashCorruptHeapURL) {
    LOG(ERROR) << "Intentionally causing ASAN corrupt heap"
               << " because user navigated to " << url.spec();
    base::debug::AsanCorruptHeap();
#endif  // OS_WIN
  }
}
#endif  // ADDRESS_SANITIZER

// Returns true if the URL is a debug URL, false otherwise. These URLs do not
// commit, though they are intentionally left in the address bar above the
// effect they cause (e.g., a sad tab).
void HandleChromeDebugURL(const GURL& url) {
  DCHECK(IsRendererDebugURL(url) && !url.SchemeIs(url::kJavaScriptScheme));
  if (url == kChromeUIBadCastCrashURL) {
    LOG(ERROR) << "Intentionally crashing (with bad cast)"
               << " because user navigated to " << url.spec();
    internal::BadCastCrashIntentionally();
  } else if (url == kChromeUICrashURL) {
    LOG(ERROR) << "Intentionally crashing (with null pointer dereference)"
               << " because user navigated to " << url.spec();
    internal::CrashIntentionally();
  } else if (url == kChromeUIDumpURL) {
    // This URL will only correctly create a crash dump file if content is
    // hosted in a process that has correctly called
    // base::debug::SetDumpWithoutCrashingFunction.  Refer to the documentation
    // of base::debug::DumpWithoutCrashing for more details.
    base::debug::DumpWithoutCrashing();
#if defined(OS_WIN) || defined(OS_POSIX)
  } else if (url == kChromeUIKillURL) {
    LOG(ERROR) << "Intentionally terminating current process because user"
                  " navigated to "
               << url.spec();
    // Simulate termination such that the base::GetTerminationStatus() API will
    // return TERMINATION_STATUS_PROCESS_WAS_KILLED.
#if defined(OS_WIN)
    base::Process::TerminateCurrentProcessImmediately(
        base::win::kProcessKilledExitCode);
#elif defined(OS_POSIX)
    PCHECK(kill(base::Process::Current().Pid(), SIGTERM) == 0);
#endif
#endif  // defined(OS_WIN) || defined(OS_POSIX)
  } else if (url == kChromeUIHangURL) {
    LOG(ERROR) << "Intentionally hanging ourselves with sleep infinite loop"
               << " because user navigated to " << url.spec();
    for (;;) {
      base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
    }
  } else if (url == kChromeUIShorthangURL) {
    LOG(ERROR) << "Intentionally sleeping renderer for 20 seconds"
               << " because user navigated to " << url.spec();
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(20));
  } else if (url == kChromeUIMemoryExhaustURL) {
    LOG(ERROR)
        << "Intentionally exhausting renderer memory because user navigated to "
        << url.spec();
    ExhaustMemory();
  } else if (url == kChromeUICheckCrashURL) {
    LOG(ERROR) << "Intentionally causing CHECK because user navigated to "
               << url.spec();
    CHECK(false);
  }

#if defined(OS_WIN)
  if (url == kChromeUIHeapCorruptionCrashURL) {
    LOG(ERROR)
        << "Intentionally causing heap corruption because user navigated to "
        << url.spec();
    base::debug::win::TerminateWithHeapCorruption();
  }
#endif

#if DCHECK_IS_ON()
  if (url == kChromeUICrashDcheckURL) {
    LOG(ERROR) << "Intentionally causing DCHECK because user navigated to "
               << url.spec();

    DCHECK(false) << "Intentional DCHECK.";
  }
#endif

#if defined(ADDRESS_SANITIZER)
  MaybeTriggerAsanError(url);
#endif  // ADDRESS_SANITIZER
}

std::string UniqueNameForWebFrame(blink::WebFrame* frame) {
  return frame->IsWebLocalFrame()
             ? RenderFrameImpl::FromWebFrame(frame)->unique_name()
             : RenderFrameProxy::FromWebFrame(frame->ToWebRemoteFrame())
                   ->unique_name();
}

RenderFrameImpl::UniqueNameFrameAdapter::UniqueNameFrameAdapter(
    RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {}

RenderFrameImpl::UniqueNameFrameAdapter::~UniqueNameFrameAdapter() {}

bool RenderFrameImpl::UniqueNameFrameAdapter::IsMainFrame() const {
  return render_frame_->IsMainFrame();
}

bool RenderFrameImpl::UniqueNameFrameAdapter::IsCandidateUnique(
    base::StringPiece name) const {
  // This method is currently O(N), where N = number of frames in the tree.
  DCHECK(!name.empty());

  for (blink::WebFrame* frame = GetWebFrame()->Top(); frame;
       frame = frame->TraverseNext()) {
    if (UniqueNameForWebFrame(frame) == name)
      return false;
  }

  return true;
}

int RenderFrameImpl::UniqueNameFrameAdapter::GetSiblingCount() const {
  int sibling_count = 0;
  for (blink::WebFrame* frame = GetWebFrame()->Parent()->FirstChild(); frame;
       frame = frame->NextSibling()) {
    if (frame == GetWebFrame())
      continue;
    ++sibling_count;
  }
  return sibling_count;
}

int RenderFrameImpl::UniqueNameFrameAdapter::GetChildCount() const {
  int child_count = 0;
  for (blink::WebFrame* frame = GetWebFrame()->FirstChild(); frame;
       frame = frame->NextSibling()) {
    ++child_count;
  }
  return child_count;
}

std::vector<std::string>
RenderFrameImpl::UniqueNameFrameAdapter::CollectAncestorNames(
    BeginPoint begin_point,
    bool (*should_stop)(base::StringPiece)) const {
  std::vector<std::string> result;
  for (blink::WebFrame* frame = begin_point == BeginPoint::kParentFrame
                                    ? GetWebFrame()->Parent()
                                    : GetWebFrame();
       frame; frame = frame->Parent()) {
    result.push_back(UniqueNameForWebFrame(frame));
    if (should_stop(result.back()))
      break;
  }
  return result;
}

std::vector<int> RenderFrameImpl::UniqueNameFrameAdapter::GetFramePosition(
    BeginPoint begin_point) const {
  std::vector<int> result;
  blink::WebFrame* parent = begin_point == BeginPoint::kParentFrame
                                ? GetWebFrame()->Parent()
                                : GetWebFrame();
  blink::WebFrame* child =
      begin_point == BeginPoint::kParentFrame ? GetWebFrame() : nullptr;
  while (parent) {
    int position_in_parent = 0;
    blink::WebFrame* sibling = parent->FirstChild();
    while (sibling != child) {
      sibling = sibling->NextSibling();
      ++position_in_parent;
    }
    result.push_back(position_in_parent);

    child = parent;
    parent = parent->Parent();
  }
  return result;
}

blink::WebLocalFrame* RenderFrameImpl::UniqueNameFrameAdapter::GetWebFrame()
    const {
  return render_frame_->frame_;
}

// static
RenderFrameImpl* RenderFrameImpl::Create(
    AgentSchedulingGroup& agent_scheduling_group,
    RenderViewImpl* render_view,
    int32_t routing_id,
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        interface_provider,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker,
    const base::UnguessableToken& devtools_frame_token) {
  DCHECK(routing_id != MSG_ROUTING_NONE);
  CreateParams params(agent_scheduling_group, render_view, routing_id,
                      std::move(interface_provider),
                      std::move(browser_interface_broker),
                      devtools_frame_token);

  if (g_create_render_frame_impl)
    return g_create_render_frame_impl(std::move(params));
  else
    return new RenderFrameImpl(std::move(params));
}

// static
RenderFrame* RenderFrame::FromRoutingID(int routing_id) {
  return RenderFrameImpl::FromRoutingID(routing_id);
}

// static
RenderFrameImpl* RenderFrameImpl::FromRoutingID(int routing_id) {
  DCHECK(RenderThread::IsMainThread());
  auto iter = g_routing_id_frame_map.Get().find(routing_id);
  if (iter != g_routing_id_frame_map.Get().end())
    return iter->second;
  return nullptr;
}

// static
RenderFrameImpl* RenderFrameImpl::CreateMainFrame(
    AgentSchedulingGroup& agent_scheduling_group,
    RenderViewImpl* render_view,
    CompositorDependencies* compositor_deps,
    blink::WebFrame* opener,
    mojom::CreateViewParamsPtr* params_ptr,
    RenderWidget::ShowCallback show_callback) {
  mojom::CreateViewParamsPtr& params = *params_ptr;

  // A main frame RenderFrame must have a RenderWidget.
  DCHECK_NE(MSG_ROUTING_NONE, params->main_frame_widget_routing_id);

  CHECK(params->main_frame_interface_bundle);
  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      main_frame_interface_provider(
          std::move(params->main_frame_interface_bundle->interface_provider));

  RenderFrameImpl* render_frame = RenderFrameImpl::Create(
      agent_scheduling_group, render_view, params->main_frame_routing_id,
      std::move(main_frame_interface_provider),
      std::move(params->main_frame_interface_bundle->browser_interface_broker),
      params->devtools_main_frame_token);
  render_frame->InitializeBlameContext(nullptr);

  WebLocalFrame* web_frame = WebLocalFrame::CreateMainFrame(
      render_view->GetWebView(), render_frame,
      render_frame->blink_interface_registry_.get(),
      params->main_frame_frame_token, opener,
      // This conversion is a little sad, as this often comes from a
      // WebString...
      WebString::FromUTF8(params->replicated_frame_state.name),
      params->replicated_frame_state.frame_policy.sandbox_flags,
      params->replicated_frame_state.opener_feature_state);
  if (params->has_committed_real_load)
    render_frame->frame_->SetCommittedFirstRealLoad();

  std::unique_ptr<RenderWidget> render_widget = RenderWidget::CreateForFrame(
      agent_scheduling_group, params->main_frame_widget_routing_id,
      compositor_deps);

  // Non-owning pointer that is self-referencing and destroyed by calling
  // Close(). The RenderViewImpl has a RenderWidget already, but not a
  // WebFrameWidget, which is now attached here.
  auto* web_frame_widget = blink::WebFrameWidget::CreateForMainFrame(
      render_widget.get(), web_frame, std::move(params->frame_widget_host),
      std::move(params->frame_widget), std::move(params->widget_host),
      std::move(params->widget),
      /*is_for_nested_main_frame=*/params->type !=
          mojom::ViewWidgetType::kTopLevel,
      /*hidden=*/true, render_view->widgets_never_composited());

  render_widget->InitForMainFrame(std::move(show_callback), web_frame_widget,
                                  params->visual_properties.screen_info,
                                  *render_view);

  // The WebFrame created here was already attached to the Page as its main
  // frame, and the WebFrameWidget has been initialized, so we can call
  // WebView's DidAttachLocalMainFrame().
  render_view->GetWebView()->DidAttachLocalMainFrame();

  // The RenderWidget should start with valid VisualProperties, including a
  // non-zero size. While RenderWidget would not normally receive IPCs and
  // thus would not get VisualProperty updates while the frame is provisional,
  // we need at least one update to them in order to meet expectations in the
  // renderer, and that update comes as part of the CreateFrame message.
  // TODO(crbug.com/419087): This could become part of RenderWidget Init.
  render_widget->GetWebWidget()->ApplyVisualProperties(
      params->visual_properties);

  render_frame->render_widget_ = render_widget.get();
  render_frame->owned_render_widget_ = std::move(render_widget);
  render_frame->in_frame_tree_ = true;
  render_frame->Initialize(nullptr);

  return render_frame;
}

// static
void RenderFrameImpl::CreateFrame(
    AgentSchedulingGroup& agent_scheduling_group,
    int routing_id,
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        interface_provider,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker,
    int previous_routing_id,
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int parent_routing_id,
    int previous_sibling_routing_id,
    const base::UnguessableToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const FrameReplicationState& replicated_state,
    CompositorDependencies* compositor_deps,
    mojom::CreateFrameWidgetParamsPtr widget_params,
    blink::mojom::FrameOwnerPropertiesPtr frame_owner_properties,
    bool has_committed_real_load) {
  // TODO(danakj): Split this method into two pieces. The first block makes a
  // WebLocalFrame and collects the RenderView and RenderFrame for it. The
  // second block uses that to make a RenderWidget, if needed.
  RenderViewImpl* render_view = nullptr;
  RenderFrameImpl* render_frame = nullptr;
  blink::WebLocalFrame* web_frame = nullptr;
  if (previous_routing_id == MSG_ROUTING_NONE) {
    // TODO(alexmos): This path is currently used only:
    // 1) When recreating a RenderFrame after a crash.
    // 2) In tests that issue this IPC directly.
    // These two cases should be cleaned up to also pass a previous_routing_id,
    // which would allow removing this branch altogether.  See
    // https://crbug.com/756790.

    CHECK_NE(parent_routing_id, MSG_ROUTING_NONE);
    RenderFrameProxy* parent_proxy =
        RenderFrameProxy::FromRoutingID(parent_routing_id);
    // If the browser is sending a valid parent routing id, it should already
    // be created and registered.
    CHECK(parent_proxy);
    blink::WebRemoteFrame* parent_web_frame = parent_proxy->web_frame();

    blink::WebFrame* previous_sibling_web_frame = nullptr;
    RenderFrameProxy* previous_sibling_proxy =
        RenderFrameProxy::FromRoutingID(previous_sibling_routing_id);
    if (previous_sibling_proxy)
      previous_sibling_web_frame = previous_sibling_proxy->web_frame();

    render_view = parent_proxy->render_view();
    // Create the RenderFrame and WebLocalFrame, linking the two.
    render_frame = RenderFrameImpl::Create(
        agent_scheduling_group, parent_proxy->render_view(), routing_id,
        std::move(interface_provider), std::move(browser_interface_broker),
        devtools_frame_token);
    render_frame->InitializeBlameContext(FromRoutingID(parent_routing_id));
    render_frame->unique_name_helper_.set_propagated_name(
        replicated_state.unique_name);
    WebFrame* opener = nullptr;
    if (opener_frame_token)
      opener = WebFrame::FromFrameToken(opener_frame_token.value());
    web_frame = parent_web_frame->CreateLocalChild(
        replicated_state.scope, WebString::FromUTF8(replicated_state.name),
        replicated_state.frame_policy, render_frame,
        render_frame->blink_interface_registry_.get(),
        previous_sibling_web_frame,
        frame_owner_properties->To<blink::WebFrameOwnerProperties>(),
        replicated_state.frame_owner_element_type, frame_token, opener);

    // The RenderFrame is created and inserted into the frame tree in the above
    // call to createLocalChild.
    render_frame->in_frame_tree_ = true;
  } else {
    RenderFrameProxy* previous_proxy =
        RenderFrameProxy::FromRoutingID(previous_routing_id);
    RenderFrameImpl* previous_frame =
        RenderFrameImpl::FromRoutingID(previous_routing_id);
    WebFrame* previous_web_frame = ResolveWebFrame(previous_routing_id);
    // Only one can be found. Either the |previous_proxy| for a remote-to-local
    // navigation, or the |previous_frame| for a local-to-local navigation.
    DCHECK(!(previous_proxy && previous_frame));
    // The previous frame could've been detached while the navigation was being
    // initiated in the browser process. Drop the navigation and don't create
    // the frame in that case.
    // See https://crbug.com/526304.
    if (!previous_proxy && !previous_frame)
      return;

    render_view = previous_frame ? previous_frame->render_view()
                                 : previous_proxy->render_view();
    // This path is creating a local frame. It may or may not be a local root,
    // depending if the frame's parent is local or remote. It may also be the
    // main frame, as in the case where a navigation to the current process'
    render_frame = RenderFrameImpl::Create(
        agent_scheduling_group, render_view, routing_id,
        std::move(interface_provider), std::move(browser_interface_broker),
        devtools_frame_token);
    render_frame->InitializeBlameContext(nullptr);
    render_frame->previous_routing_id_ = previous_routing_id;
    if (previous_proxy)
      previous_proxy->set_provisional_frame_routing_id(routing_id);
    web_frame = blink::WebLocalFrame::CreateProvisional(
        render_frame, render_frame->blink_interface_registry_.get(),
        frame_token, previous_web_frame, replicated_state.frame_policy,
        WebString::FromUTF8(replicated_state.name));
    // The new |web_frame| is a main frame iff the proxy's frame was.
    DCHECK_EQ(!previous_web_frame->Parent(), !web_frame->Parent());
  }

  CHECK(render_view);
  CHECK(render_frame);
  CHECK(web_frame);

  bool is_main_frame = !web_frame->Parent();

  // Child frames require there to be a |parent_routing_id| present, for the
  // remote parent frame. Though it is only used if the |previous_routing_id|
  // is not given, which happens in some corner cases.
  if (!is_main_frame)
    DCHECK_NE(parent_routing_id, MSG_ROUTING_NONE);

  // We now have a WebLocalFrame for the new frame. The next step is to make
  // a RenderWidget (aka WebWidgetClient) for it, if it is a local root.
  if (is_main_frame) {
    // Main frames are always local roots, so they should always have a
    // |widget_params| (and it always comes with a routing id).
    DCHECK(widget_params);
    DCHECK_NE(widget_params->routing_id, MSG_ROUTING_NONE);

    // TODO(crbug.com/419087): Can we merge this code with
    // RenderFrameImpl::CreateMainFrame()?

    std::unique_ptr<RenderWidget> render_widget = RenderWidget::CreateForFrame(
        agent_scheduling_group, widget_params->routing_id, compositor_deps);

    // Non-owning pointer that is self-referencing and destroyed by calling
    // Close(). The RenderViewImpl has a RenderWidget already, but not a
    // WebFrameWidget, which is now attached here.
    auto* web_frame_widget = blink::WebFrameWidget::CreateForMainFrame(
        render_widget.get(), web_frame,
        std::move(widget_params->frame_widget_host),
        std::move(widget_params->frame_widget),
        std::move(widget_params->widget_host), std::move(widget_params->widget),
        /*is_for_nested_main_frame=*/false,
        /*hidden=*/true, render_view->widgets_never_composited());

    render_widget->InitForMainFrame(
        RenderWidget::ShowCallback(), web_frame_widget,
        widget_params->visual_properties.screen_info, *render_view);
    // The RenderWidget should start with valid VisualProperties, including a
    // non-zero size. While RenderWidget would not normally receive IPCs and
    // thus would not get VisualProperty updates while the frame is provisional,
    // we need at least one update to them in order to meet expectations in the
    // renderer, and that update comes as part of the CreateFrame message.
    // TODO(crbug.com/419087): This could become part of RenderWidget Init.
    render_widget->GetWebWidget()->ApplyVisualProperties(
        widget_params->visual_properties);

    // Note that we do *not* call WebView's DidAttachLocalMainFrame() here yet
    // because this frame is provisional and not attached to the Page yet. We
    // will tell WebViewImpl about it once it is swapped in.

    render_frame->render_widget_ = render_widget.get();
    render_frame->owned_render_widget_ = std::move(render_widget);
  } else if (widget_params) {
    DCHECK(widget_params->routing_id != MSG_ROUTING_NONE);
    // This frame is a child local root, so we require a separate RenderWidget
    // for it from any other frames in the frame tree. Each local root defines
    // a separate context/coordinate space/world for compositing, painting,
    // input, etc. And each local root has a RenderWidget which provides
    // such services independent from other RenderWidgets.
    // Notably, we do not attempt to reuse the main frame's RenderWidget (if the
    // main frame in this frame tree is local) as that RenderWidget is
    // functioning in a different local root. Because this is a child local
    // root, it implies there is some remote frame ancestor between this frame
    // and the main frame, thus its coordinate space etc is not known relative
    // to the main frame.

    // Makes a new RenderWidget for the child local root. It provides the
    // local root with a new compositing, painting, and input coordinate
    // space/context.
    std::unique_ptr<RenderWidget> render_widget = RenderWidget::CreateForFrame(
        agent_scheduling_group, widget_params->routing_id, compositor_deps);

    // Non-owning pointer that is self-referencing and destroyed by calling
    // Close(). We use the new RenderWidget as the client for this
    // WebFrameWidget, *not* the RenderWidget of the MainFrame, which is
    // accessible from the RenderViewImpl.
    auto* web_frame_widget = blink::WebFrameWidget::CreateForChildLocalRoot(
        render_widget.get(), web_frame,
        std::move(widget_params->frame_widget_host),
        std::move(widget_params->frame_widget),
        std::move(widget_params->widget_host), std::move(widget_params->widget),
        /*hidden=*/true, render_view->widgets_never_composited());

    // Adds a reference on RenderWidget, making it self-referencing. So it
    // will not be destroyed by scoped_refptr unless Close() has been called
    // and run.
    render_widget->InitForChildLocalRoot(
        web_frame_widget, widget_params->visual_properties.screen_info);
    // The RenderWidget should start with valid VisualProperties, including a
    // non-zero size. While RenderWidget would not normally receive IPCs and
    // thus would not get VisualProperty updates while the frame is provisional,
    // we need at least one update to them in order to meet expectations in the
    // renderer, and that update comes as part of the CreateFrame message.
    // TODO(crbug.com/419087): This could become part of RenderWidget Init.
    render_widget->GetWebWidget()->ApplyVisualProperties(
        widget_params->visual_properties);

    render_frame->render_widget_ = render_widget.get();
    render_frame->owned_render_widget_ = std::move(render_widget);
  }

  if (has_committed_real_load)
    render_frame->frame_->SetCommittedFirstRealLoad();

  render_frame->Initialize(web_frame->Parent());
}

// static
RenderFrame* RenderFrame::FromWebFrame(blink::WebLocalFrame* web_frame) {
  return RenderFrameImpl::FromWebFrame(web_frame);
}

// static
void RenderFrame::ForEach(RenderFrameVisitor* visitor) {
  DCHECK(RenderThread::IsMainThread());
  FrameMap* frames = g_frame_map.Pointer();
  for (auto it = frames->begin(); it != frames->end(); ++it) {
    if (!visitor->Visit(it->second))
      return;
  }
}

// static
int RenderFrame::GetRoutingIdForWebFrame(blink::WebFrame* web_frame) {
  if (!web_frame)
    return MSG_ROUTING_NONE;
  if (web_frame->IsWebRemoteFrame()) {
    return RenderFrameProxy::FromWebFrame(web_frame->ToWebRemoteFrame())
        ->routing_id();
  }
  return RenderFrameImpl::FromWebFrame(web_frame)->GetRoutingID();
}

// static
RenderFrameImpl* RenderFrameImpl::FromWebFrame(blink::WebFrame* web_frame) {
  DCHECK(RenderThread::IsMainThread());
  auto iter = g_frame_map.Get().find(web_frame);
  if (iter != g_frame_map.Get().end())
    return iter->second;
  return nullptr;
}

// static
void RenderFrameImpl::InstallCreateHook(
    CreateRenderFrameImplFunction create_frame) {
  DCHECK(!g_create_render_frame_impl);
  g_create_render_frame_impl = create_frame;
}

// static
blink::WebFrame* RenderFrameImpl::ResolveWebFrame(int frame_routing_id) {
  if (frame_routing_id == MSG_ROUTING_NONE)
    return nullptr;

  // Opener routing ID could refer to either a RenderFrameProxy or a
  // RenderFrame, so need to check both.
  RenderFrameProxy* opener_proxy =
      RenderFrameProxy::FromRoutingID(frame_routing_id);
  if (opener_proxy)
    return opener_proxy->web_frame();

  RenderFrameImpl* opener_frame =
      RenderFrameImpl::FromRoutingID(frame_routing_id);
  if (opener_frame)
    return opener_frame->GetWebFrame();

  return nullptr;
}

// static
void RenderFrameImpl::MaybeSetDownloadFramePolicy(
    bool is_opener_navigation,
    const blink::WebURLRequest& request,
    const blink::WebSecurityOrigin& current_origin,
    bool has_download_sandbox_flag,
    bool blocking_downloads_in_sandbox_enabled,
    bool from_ad,
    NavigationDownloadPolicy* download_policy) {
  bool has_gesture = request.HasUserGesture();
  if (!has_gesture) {
    download_policy->SetAllowed(NavigationDownloadType::kNoGesture);
  }

  // Disallow downloads on an opener if the requestor is cross origin.
  // See crbug.com/632514.
  if (is_opener_navigation &&
      !request.RequestorOrigin().CanAccess(current_origin)) {
    download_policy->SetDisallowed(NavigationDownloadType::kOpenerCrossOrigin);
  }

  if (has_download_sandbox_flag) {
    if (blocking_downloads_in_sandbox_enabled) {
      download_policy->SetDisallowed(NavigationDownloadType::kSandbox);
    } else {
      download_policy->SetAllowed(NavigationDownloadType::kSandbox);
    }
  }

  if (from_ad) {
    download_policy->SetAllowed(NavigationDownloadType::kAdFrame);
    if (!has_gesture) {
      if (base::FeatureList::IsEnabled(
              blink::features::
                  kBlockingDownloadsInAdFrameWithoutUserActivation)) {
        download_policy->SetDisallowed(
            NavigationDownloadType::kAdFrameNoGesture);
      } else {
        download_policy->SetAllowed(NavigationDownloadType::kAdFrameNoGesture);
      }
    }
  }

  download_policy->blocking_downloads_in_sandbox_enabled =
      blocking_downloads_in_sandbox_enabled;
}

blink::WebURL RenderFrameImpl::OverrideFlashEmbedWithHTML(
    const blink::WebURL& url) {
  return GetContentClient()->renderer()->OverrideFlashEmbedWithHTML(url);
}

// RenderFrameImpl::CreateParams --------------------------------------------

RenderFrameImpl::CreateParams::CreateParams(
    AgentSchedulingGroup& agent_scheduling_group,
    RenderViewImpl* render_view,
    int32_t routing_id,
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        interface_provider,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker,
    const base::UnguessableToken& devtools_frame_token)
    : agent_scheduling_group(&agent_scheduling_group),
      render_view(render_view),
      routing_id(routing_id),
      interface_provider(std::move(interface_provider)),
      browser_interface_broker(std::move(browser_interface_broker)),
      devtools_frame_token(devtools_frame_token) {}
RenderFrameImpl::CreateParams::~CreateParams() = default;
RenderFrameImpl::CreateParams::CreateParams(CreateParams&&) = default;
RenderFrameImpl::CreateParams& RenderFrameImpl::CreateParams::operator=(
    CreateParams&&) = default;

// RenderFrameImpl ----------------------------------------------------------
RenderFrameImpl::RenderFrameImpl(CreateParams params)
    : agent_scheduling_group_(*params.agent_scheduling_group),
      is_main_frame_(true),
      unique_name_frame_adapter_(this),
      unique_name_helper_(&unique_name_frame_adapter_),
      in_frame_tree_(false),
      render_view_(params.render_view),
      routing_id_(params.routing_id),
      previous_routing_id_(MSG_ROUTING_NONE),
#if BUILDFLAG(ENABLE_PLUGINS)
      plugin_power_saver_helper_(nullptr),
#endif
      selection_text_offset_(0),
      selection_range_(gfx::Range::InvalidRange()),
      render_accessibility_manager_(
          std::make_unique<RenderAccessibilityManager>(this)),
      blame_context_(nullptr),
#if BUILDFLAG(ENABLE_PLUGINS)
      focused_pepper_plugin_(nullptr),
#endif
      navigation_client_impl_(nullptr),
      media_factory_(
          this,
          base::BindRepeating(&RenderFrameImpl::RequestOverlayRoutingToken,
                              base::Unretained(this))),
      devtools_frame_token_(params.devtools_frame_token) {
  DCHECK(RenderThread::IsMainThread());
  // The InterfaceProvider to access Mojo services exposed by the RFHI must be
  // provided at construction time. See: https://crbug.com/729021/.
  CHECK(params.interface_provider.is_valid());
  remote_interfaces_.Bind(std::move(params.interface_provider));
  blink_interface_registry_.reset(new BlinkInterfaceRegistryImpl(
      registry_.GetWeakPtr(), associated_interfaces_.GetWeakPtr()));

  CHECK(params.browser_interface_broker.is_valid());
  browser_interface_broker_proxy_.Bind(
      std::move(params.browser_interface_broker));

  // Must call after binding our own remote interfaces.
  media_factory_.SetupMojo();

  std::pair<RoutingIDFrameMap::iterator, bool> result =
      g_routing_id_frame_map.Get().insert(std::make_pair(routing_id_, this));
  CHECK(result.second) << "Inserting a duplicate item.";

  // Everything below subclasses RenderFrameObserver and is automatically
  // deleted when the RenderFrame gets deleted.
#if defined(OS_ANDROID)
  new GinJavaBridgeDispatcher(this);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  // Manages its own lifetime.
  plugin_power_saver_helper_ = new PluginPowerSaverHelper(this);
#endif
}

mojom::FrameHost* RenderFrameImpl::GetFrameHost() {
  if (!frame_host_remote_.is_bound())
    GetRemoteAssociatedInterfaces()->GetInterface(&frame_host_remote_);
  return frame_host_remote_.get();
}

RenderFrameImpl::~RenderFrameImpl() {
  for (auto& observer : observers_)
    observer.RenderFrameGone();
  for (auto& observer : observers_)
    observer.OnDestruct();

  web_media_stream_device_observer_.reset();

  base::trace_event::TraceLog::GetInstance()->RemoveProcessLabel(routing_id_);

  if (is_main_frame_) {
    // Ensure the RenderView doesn't point to this object, once it is destroyed.
    // TODO(nasko): Add a check that the |main_render_frame_| of |render_view_|
    // is |this|, once the object is no longer leaked.
    // See https://crbug.com/464764.
    render_view_->main_render_frame_ = nullptr;
  }

  g_routing_id_frame_map.Get().erase(routing_id_);
  agent_scheduling_group_.RemoveRoute(routing_id_);
}

void RenderFrameImpl::Initialize(blink::WebFrame* parent) {
  is_main_frame_ = !parent;

  WebFrame* local_root = frame_;
  if (parent && parent->IsWebLocalFrame()) {
    local_root = parent->ToWebLocalFrame()->LocalRoot();
  }
  bool is_tracing_rail = false;
  bool is_tracing_navigation = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("navigation", &is_tracing_navigation);
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("rail", &is_tracing_rail);
  if (is_tracing_rail || is_tracing_navigation) {
    int parent_id = RenderFrame::GetRoutingIdForWebFrame(parent);
    TRACE_EVENT2("navigation,rail", "RenderFrameImpl::Initialize", "id",
                 routing_id_, "parent", parent_id);
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  new PepperBrowserConnection(this);
#endif

  RegisterMojoInterfaces();

  // We delay calling this until we have the WebFrame so that any observer or
  // embedder can call GetWebFrame on any RenderFrame.
  GetContentClient()->renderer()->RenderFrameCreated(this);

  // blink::WebAudioOutputIPCFactory::io_task_runner_ may be null in tests.
  auto& factory = blink::WebAudioOutputIPCFactory::GetInstance();
  if (factory.io_task_runner()) {
    factory.RegisterRemoteFactory(GetWebFrame()->GetLocalFrameToken(),
                                  GetBrowserInterfaceBroker());
  }

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDomAutomationController))
    enabled_bindings_ |= BINDINGS_POLICY_DOM_AUTOMATION;
  if (command_line.HasSwitch(switches::kStatsCollectionController))
    enabled_bindings_ |= BINDINGS_POLICY_STATS_COLLECTION;
  frame_request_blocker_ = blink::WebFrameRequestBlocker::Create();

  // Bind this frame and the message router. This must be called after |frame_|
  // is set since binding requires a per-frame task runner.
  agent_scheduling_group_.AddRoute(routing_id_, this);
}

void RenderFrameImpl::InitializeBlameContext(RenderFrameImpl* parent_frame) {
  DCHECK(!blame_context_);
  blame_context_ = std::make_unique<FrameBlameContext>(this, parent_frame);
  blame_context_->Initialize();
}

void RenderFrameImpl::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (registry_.TryBindInterface(interface_name, &interface_pipe))
    return;

  for (auto& observer : observers_) {
    observer.OnInterfaceRequestForFrame(interface_name, &interface_pipe);
    if (!interface_pipe.is_valid())
      return;
  }
}

RenderWidget* RenderFrameImpl::GetLocalRootRenderWidget() {
  return GetLocalRoot()->render_widget_;
}

blink::WebFrameWidget* RenderFrameImpl::GetLocalRootWebFrameWidget() {
  // This is the same as GetLocalRootRenderWidget()->GetWebWidget() but
  // avoids casting for type safety, woo.
  return frame_->LocalRoot()->FrameWidget();
}

RenderWidget* RenderFrameImpl::GetMainFrameRenderWidget() {
  RenderFrameImpl* local_main_frame = render_view()->GetMainRenderFrame();
  if (!local_main_frame)
    return nullptr;
  return local_main_frame->render_widget_;
}

#if BUILDFLAG(ENABLE_PLUGINS)
void RenderFrameImpl::PepperPluginCreated(RendererPpapiHost* host) {
  for (auto& observer : observers_)
    observer.DidCreatePepperPlugin(host);
}

void RenderFrameImpl::PepperTextInputTypeChanged(
    PepperPluginInstanceImpl* instance) {
  if (instance != focused_pepper_plugin_)
    return;

  GetLocalRootRenderWidget()->UpdateTextInputState();
}

void RenderFrameImpl::PepperCaretPositionChanged(
    PepperPluginInstanceImpl* instance) {
  if (instance != focused_pepper_plugin_)
    return;
  GetLocalRootRenderWidget()->UpdateSelectionBounds();
}

void RenderFrameImpl::PepperCancelComposition(
    PepperPluginInstanceImpl* instance) {
  if (instance != focused_pepper_plugin_)
    return;
  GetLocalRootRenderWidget()->GetWebWidget()->CancelCompositionForPepper();
}

void RenderFrameImpl::PepperSelectionChanged(
    PepperPluginInstanceImpl* instance) {
  if (instance != focused_pepper_plugin_)
    return;
  SyncSelectionIfRequired();
}

RenderWidgetFullscreenPepper* RenderFrameImpl::CreatePepperFullscreenContainer(
    PepperPluginInstanceImpl* plugin) {
  // Get the URL of the main frame if possible.
  blink::WebURL main_frame_url;
  WebFrame* main_frame = render_view()->GetWebView()->MainFrame();
  if (main_frame->IsWebLocalFrame())
    main_frame_url = main_frame->ToWebLocalFrame()->GetDocument().Url();

  mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget;
  mojo::PendingAssociatedReceiver<blink::mojom::Widget> blink_widget_receiver =
      blink_widget.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<blink::mojom::WidgetHost> blink_widget_host;
  mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
      blink_widget_host_receiver =
          blink_widget_host.InitWithNewEndpointAndPassReceiver();

  // Synchronous IPC to obtain a routing id for the fullscreen widget.
  int32_t fullscreen_widget_routing_id = MSG_ROUTING_NONE;
  if (!GetFrameHost()->CreateNewFullscreenWidget(
          std::move(blink_widget_host_receiver), std::move(blink_widget),
          &fullscreen_widget_routing_id)) {
    return nullptr;
  }
  RenderWidget::ShowCallback show_callback =
      base::BindOnce(&RenderViewImpl::ShowCreatedFullscreenWidget,
                     render_view()->GetWeakPtr());

  RenderWidgetFullscreenPepper* widget = RenderWidgetFullscreenPepper::Create(
      agent_scheduling_group_, fullscreen_widget_routing_id,
      std::move(show_callback), GetLocalRootRenderWidget()->compositor_deps(),
      GetLocalRootRenderWidget()->GetWebWidget()->GetOriginalScreenInfo(),
      plugin, std::move(main_frame_url), std::move(blink_widget_host),
      std::move(blink_widget_receiver));
  // TODO(nick): The show() handshake seems like unnecessary complexity here,
  // since there's no real delay between CreateFullscreenWidget and
  // ShowCreatedFullscreenWidget. Would it be simpler to have the
  // CreateFullscreenWidget mojo method implicitly show the window, and skip the
  // subsequent step?
  widget->Show(blink::kWebNavigationPolicyCurrentTab);
  return widget;
}

bool RenderFrameImpl::IsPepperAcceptingCompositionEvents() const {
  if (!focused_pepper_plugin_)
    return false;
  return focused_pepper_plugin_->IsPluginAcceptingCompositionEvents();
}

void RenderFrameImpl::PluginCrashed(const base::FilePath& plugin_path,
                                    base::ProcessId plugin_pid) {
  // TODO(jam): dispatch this IPC in RenderFrameHost and switch to use
  // routing_id_ as a result.
  Send(new FrameHostMsg_PluginCrashed(routing_id_, plugin_path, plugin_pid));
}

void RenderFrameImpl::SimulateImeSetComposition(
    const base::string16& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    int selection_start,
    int selection_end) {
  if (!GetMainFrameRenderWidget()->ShouldDispatchImeEventsToPepper())
    return;
  GetMainFrameRenderWidget()->ImeSetCompositionForPepper(
      WebString::FromUTF16(text), ime_text_spans, gfx::Range::InvalidRange(),
      selection_start, selection_end);
}

void RenderFrameImpl::SimulateImeCommitText(
    const base::string16& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range) {
  if (!GetMainFrameRenderWidget()->ShouldDispatchImeEventsToPepper())
    return;
  GetMainFrameRenderWidget()->ImeCommitTextForPepper(
      WebString::FromUTF16(text), ime_text_spans, replacement_range, 0);
}

void RenderFrameImpl::OnImeSetComposition(
    const base::string16& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    int selection_start,
    int selection_end) {
  // When a PPAPI plugin has focus, we bypass WebKit.
  if (!IsPepperAcceptingCompositionEvents()) {
    pepper_composition_text_ = text;
  } else {
    // TODO(kinaba) currently all composition events are sent directly to
    // plugins. Use DOM event mechanism after WebKit is made aware about
    // plugins that support composition.
    // The code below mimics the behavior of WebCore::Editor::setComposition.

    // Empty -> nonempty: composition started.
    if (pepper_composition_text_.empty() && !text.empty()) {
      focused_pepper_plugin_->HandleCompositionStart(base::string16());
    }
    // Nonempty -> empty: composition canceled.
    if (!pepper_composition_text_.empty() && text.empty()) {
      focused_pepper_plugin_->HandleCompositionEnd(base::string16());
    }
    pepper_composition_text_ = text;
    // Nonempty: composition is ongoing.
    if (!pepper_composition_text_.empty()) {
      focused_pepper_plugin_->HandleCompositionUpdate(
          pepper_composition_text_, ime_text_spans, selection_start,
          selection_end);
    }
  }
}

void RenderFrameImpl::OnImeCommitText(const base::string16& text,
                                      const gfx::Range& replacement_range,
                                      int relative_cursor_pos) {
  HandlePepperImeCommit(text);
}

void RenderFrameImpl::OnImeFinishComposingText(bool keep_selection) {
  const base::string16& text = pepper_composition_text_;
  HandlePepperImeCommit(text);
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

void RenderFrameImpl::ScriptedPrint(bool user_initiated) {
  for (auto& observer : observers_)
    observer.ScriptedPrint(user_initiated);
}

bool RenderFrameImpl::Send(IPC::Message* message) {
  return agent_scheduling_group_.Send(message);
}

bool RenderFrameImpl::OnMessageReceived(const IPC::Message& msg) {
  // Page IPCs are routed via the main frame (both local and remote) and then
  // forwarded to the RenderView. See comment in
  // RenderFrameHostManager::SendPageMessage() for more information.
  if ((IPC_MESSAGE_CLASS(msg) == PageMsgStart)) {
    if (render_view())
      return render_view()->OnMessageReceived(msg);

    return false;
  }

  // We may get here while detaching, when the WebFrame has been deleted.  Do
  // not process any messages in this state.
  if (!frame_)
    return false;

  DCHECK(!frame_->GetDocument().IsNull());

  GetContentClient()->SetActiveURL(
      frame_->GetDocument().Url(),
      frame_->Top()->GetSecurityOrigin().ToString().Utf8());

  for (auto& observer : observers_) {
    if (observer.OnMessageReceived(msg))
      return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameImpl, msg)
    IPC_MESSAGE_HANDLER(UnfreezableFrameMsg_Unload, OnUnload)
    IPC_MESSAGE_HANDLER(FrameMsg_ContextMenuClosed, OnContextMenuClosed)
    IPC_MESSAGE_HANDLER(FrameMsg_CustomContextMenuAction,
                        OnCustomContextMenuAction)
#if BUILDFLAG(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER(FrameMsg_SetPepperVolume, OnSetPepperVolume)
#endif
    IPC_MESSAGE_HANDLER(FrameMsg_MixedContentFound, OnMixedContentFound)
    IPC_MESSAGE_HANDLER(UnfreezableFrameMsg_Delete, OnDeleteFrame)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderFrameImpl::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (!associated_interfaces_.TryBindInterface(interface_name, &handle)) {
    for (auto& observer : observers_) {
      if (observer.OnAssociatedInterfaceRequestForFrame(interface_name,
                                                        &handle)) {
        return;
      }
    }
  }
}

void RenderFrameImpl::SetUpSharedMemoryForSmoothness(
    base::ReadOnlySharedMemoryRegion shared_memory) {
  DCHECK(shared_memory.IsValid());
  for (auto& observer : observers_) {
    DCHECK(shared_memory.IsValid());
    if (observer.SetUpSmoothnessReporting(shared_memory))
      break;
  }
}

void RenderFrameImpl::BindFullscreen(
    mojo::PendingAssociatedReceiver<mojom::FullscreenVideoElementHandler>
        receiver) {
  fullscreen_receiver_.Bind(
      std::move(receiver),
      GetTaskRunner(blink::TaskType::kInternalNavigationAssociated));
}

void RenderFrameImpl::BindAutoplayConfiguration(
    mojo::PendingAssociatedReceiver<blink::mojom::AutoplayConfigurationClient>
        receiver) {
  autoplay_configuration_receiver_.Bind(
      std::move(receiver),
      GetTaskRunner(blink::TaskType::kInternalNavigationAssociated));
}

void RenderFrameImpl::BindFrame(mojo::PendingReceiver<mojom::Frame> receiver) {
  // It's not unfreezable at the moment because Frame::SetLifecycleState
  // has to run for the frozen frames.
  // TODO(altimin): Move SetLifecycleState to a dedicated scheduling interface.
  frame_receiver_.Bind(
      std::move(receiver),
      GetTaskRunner(blink::TaskType::kInternalFrameLifecycleControl));
}

void RenderFrameImpl::BindFrameBindingsControl(
    mojo::PendingAssociatedReceiver<mojom::FrameBindingsControl> receiver) {
  frame_bindings_control_receiver_.Bind(
      std::move(receiver),
      GetTaskRunner(blink::TaskType::kInternalNavigationAssociated));
}

void RenderFrameImpl::BindFrameNavigationControl(
    mojo::PendingAssociatedReceiver<mojom::FrameNavigationControl> receiver) {
  frame_navigation_control_receiver_.Bind(
      std::move(receiver),
      GetTaskRunner(blink::TaskType::kInternalNavigationAssociated));
}

void RenderFrameImpl::BindNavigationClient(
    mojo::PendingAssociatedReceiver<mojom::NavigationClient> receiver) {
  navigation_client_impl_ = std::make_unique<NavigationClient>(this);
  navigation_client_impl_->Bind(std::move(receiver));
}

// Unload this RenderFrame so the frame can navigate to a document rendered by
// a different process. We also allow this process to exit if there are no other
// active RenderFrames in it.
// This executes the unload handlers on this frame and its local descendants.
void RenderFrameImpl::OnUnload(
    int proxy_routing_id,
    bool is_loading,
    const FrameReplicationState& replicated_frame_state,
    const base::UnguessableToken& frame_token) {
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::OnUnload", "id",
               routing_id_);
  DCHECK(!base::RunLoop::IsNestedOnCurrentThread());

  // Send an UpdateState message before we get deleted.
  SendUpdateState();

  // There should always be a proxy to replace this RenderFrame. Create it now
  // so its routing id is registered for receiving IPC messages.
  CHECK_NE(proxy_routing_id, MSG_ROUTING_NONE);
  RenderFrameProxy* proxy = RenderFrameProxy::CreateProxyToReplaceFrame(
      agent_scheduling_group_, this, proxy_routing_id,
      replicated_frame_state.scope, frame_token);

  RenderViewImpl* render_view = render_view_;
  bool is_main_frame = is_main_frame_;
  int routing_id = GetRoutingID();
  auto& agent_scheduling_group = agent_scheduling_group_;

  // Before |this| is destroyed, grab the TaskRunner to be used for sending the
  // FrameHostMsg_Unload_ACK.  This will be used to schedule
  // FrameHostMsg_Unload_ACK to be sent after any postMessage IPCs scheduled
  // from the unload event above.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetTaskRunner(blink::TaskType::kPostedMessage);

  // Now that all of the cleanup is complete and the browser side is notified,
  // start using the RenderFrameProxy.
  //
  // The swap call deletes this RenderFrame via FrameDetached.  Do not access
  // any members after this call.
  //
  // TODO(creis): WebFrame::swap() can return false.  Most of those cases
  // should be due to the frame being detached during unload (in which case
  // the necessary cleanup has happened anyway), but it might be possible for
  // it to return false without detaching.  Catch any cases that the
  // RenderView's main_render_frame_ isn't cleared below (whether swap returns
  // false or not).
  //
  // This executes the unload handlers on this frame and its local descendants.
  bool success = frame_->Swap(proxy->web_frame());

  // WARNING: Do not access 'this' past this point!

  if (is_main_frame) {
    // Main frames should always swap successfully because there is no parent
    // frame to cause them to become detached.
    DCHECK(success);
    // For main frames, the swap should have cleared the RenderView's pointer to
    // this frame.
    CHECK(!render_view->main_render_frame_);

    // The RenderFrameProxy being swapped in here has now been attached to the
    // Page as its main frame and properly initialized by the WebFrame::Swap()
    // call, so we can call WebView's DidAttachRemoteMainFrame().
    render_view->GetWebView()->DidAttachRemoteMainFrame();
  }

  if (!success) {
    // The swap can fail when the frame is detached during swap (this can
    // happen while running the unload handlers). When that happens, delete
    // the proxy.
    proxy->FrameDetached(blink::WebRemoteFrameClient::DetachType::kSwap);
    return;
  }

  if (is_loading)
    proxy->DidStartLoading();

  // Initialize the WebRemoteFrame with the replication state passed by the
  // process that is now rendering the frame.
  proxy->SetReplicatedState(replicated_frame_state);

  // Notify the browser that this frame was unloaded. Use the cached
  // `AgentSchedulingGroup` because |this| is deleted. Post a task to send the
  // ACK, so that any postMessage IPCs scheduled from the unload handler are
  // sent before the ACK (see https://crbug.com/857274).
  auto send_unload_ack = base::BindOnce(
      [](AgentSchedulingGroup* agent_scheduling_group, int routing_id,
         bool is_main_frame) {
        auto* msg = new FrameHostMsg_Unload_ACK(routing_id);
        agent_scheduling_group->Send(msg);
      },
      &agent_scheduling_group, routing_id, is_main_frame);
  task_runner->PostTask(FROM_HERE, std::move(send_unload_ack));
}

void RenderFrameImpl::OnDeleteFrame(FrameDeleteIntention intent) {
  // The main frame (when not provisional) is owned by the renderer's frame tree
  // via WebViewImpl. When a provisional main frame is swapped in, the ownership
  // moves from the browser to the renderer, but this happens in the renderer
  // process and is then the browser is informed.
  // If the provisional main frame is swapped in while the browser is destroying
  // it, the browser may request to delete |this|, thinking it has ownership
  // of it, but the renderer has already taken ownership via SwapIn().
  switch (intent) {
    case FrameDeleteIntention::kNotMainFrame:
      // The frame was not a main frame, so the browser should always have
      // ownership of it and we can just proceed with deleting it on
      // request.
      DCHECK(!is_main_frame_);
      break;
    case FrameDeleteIntention::kSpeculativeMainFrameForShutdown:
      // In this case the renderer has taken ownership of the provisional main
      // frame but the browser did not know yet and is shutting down. We can
      // ignore this request as the frame will be destroyed when the RenderView
      // is. This handles the shutdown case of https://crbug.com/957858.
      DCHECK(is_main_frame_);
      if (in_frame_tree_)
        return;
      break;
    case FrameDeleteIntention::kSpeculativeMainFrameForNavigationCancelled:
      // In this case the browser was navigating and cancelled the speculative
      // navigation. The renderer *should* undo the SwapIn() but the old state
      // has already been destroyed. Both ignoring the message or handling it
      // would leave the renderer in an inconsistent state now. If we ignore it
      // then the browser thinks the RenderView has a remote main frame, but it
      // is incorrect. If we handle it, then we are deleting a local main frame
      // out from under the RenderView and we will have bad pointers in the
      // renderer. So all we can do is crash. We should instead prevent this
      // scenario by blocking the browser from dropping the speculative main
      // frame when a commit (and ownership transfer) is imminent.
      // TODO(dcheng): This is the case of https://crbug.com/838348.
      DCHECK(is_main_frame_);
#if !defined(OS_ANDROID)
      // This check is not enabled on Android, since it seems like it's much
      // easier to trigger data races there.
      CHECK(!in_frame_tree_);
#endif  // !defined(OS_ANDROID)
      break;
  }

  // This will result in a call to RenderFrameImpl::FrameDetached, which
  // deletes the object. Do not access |this| after detach.
  frame_->Detach();
}

void RenderFrameImpl::OnContextMenuClosed(
    const CustomContextMenuContext& custom_context) {
  if (custom_context.request_id) {
    // External request, should be in our map.
    ContextMenuClient* client =
        pending_context_menus_.Lookup(custom_context.request_id);
    if (client) {
      client->OnMenuClosed(custom_context.request_id);
      pending_context_menus_.Remove(custom_context.request_id);
    }
  } else {
    if (custom_context.link_followed.is_valid())
      frame_->SendPings(custom_context.link_followed);
  }

  render_view()->GetWebView()->DidCloseContextMenu();
}

void RenderFrameImpl::OnCustomContextMenuAction(
    const CustomContextMenuContext& custom_context,
    unsigned action) {
  if (custom_context.request_id) {
    // External context menu request, look in our map.
    ContextMenuClient* client =
        pending_context_menus_.Lookup(custom_context.request_id);
    if (client)
      client->OnMenuAction(custom_context.request_id, action);
  } else {
    // Internal request, forward to WebKit.
    render_view_->GetWebView()->PerformCustomContextMenuAction(action);
  }
}

void RenderFrameImpl::JavaScriptMethodExecuteRequest(
    const base::string16& object_name,
    const base::string16& method_name,
    base::Value arguments,
    bool wants_result,
    JavaScriptMethodExecuteRequestCallback callback) {
  TRACE_EVENT_INSTANT0("test_tracing", "JavaScriptMethodExecuteRequest",
                       TRACE_EVENT_SCOPE_THREAD);

  // Note that CallMethodOnFrame may end up killing this object.
  base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> result;
  if (!CallMethodOnFrame(frame_, object_name, method_name, std::move(arguments))
           .ToLocal(&result)) {
    std::move(callback).Run({});
    return;
  }

  if (!weak_this)
    return;

  if (wants_result)
    std::move(callback).Run(GetJavaScriptExecutionResult(result));
  else
    std::move(callback).Run({});
}

void RenderFrameImpl::JavaScriptExecuteRequest(
    const base::string16& javascript,
    bool wants_result,
    JavaScriptExecuteRequestCallback callback) {
  TRACE_EVENT_INSTANT0("test_tracing", "JavaScriptExecuteRequest",
                       TRACE_EVENT_SCOPE_THREAD);

  // Note that ExecuteScriptAndReturnValue may end up killing this object.
  base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> result = frame_->ExecuteScriptAndReturnValue(
      WebScriptSource(WebString::FromUTF16(javascript)));

  if (!weak_this)
    return;

  if (wants_result)
    std::move(callback).Run(GetJavaScriptExecutionResult(result));
  else
    std::move(callback).Run({});
}

void RenderFrameImpl::JavaScriptExecuteRequestForTests(
    const base::string16& javascript,
    bool wants_result,
    bool has_user_gesture,
    int32_t world_id,
    JavaScriptExecuteRequestForTestsCallback callback) {
  TRACE_EVENT_INSTANT0("test_tracing", "JavaScriptExecuteRequestForTests",
                       TRACE_EVENT_SCOPE_THREAD);

  // Note that ExecuteScriptAndReturnValue may end up killing this object.
  base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();

  // A bunch of tests expect to run code in the context of a user gesture, which
  // can grant additional privileges (e.g. the ability to create popups).
  if (has_user_gesture)
    frame_->NotifyUserActivation(
        blink::mojom::UserActivationNotificationType::kTest);

  v8::HandleScope handle_scope(blink::MainThreadIsolate());
  v8::Local<v8::Value> result;
  if (world_id == ISOLATED_WORLD_ID_GLOBAL) {
    result = frame_->ExecuteScriptAndReturnValue(
        WebScriptSource(WebString::FromUTF16(javascript)));
  } else {
    result = frame_->ExecuteScriptInIsolatedWorldAndReturnValue(
        world_id, WebScriptSource(WebString::FromUTF16(javascript)));
  }

  if (!weak_this)
    return;

  if (wants_result)
    std::move(callback).Run(GetJavaScriptExecutionResult(result));
  else
    std::move(callback).Run({});
}

void RenderFrameImpl::JavaScriptExecuteRequestInIsolatedWorld(
    const base::string16& javascript,
    bool wants_result,
    int32_t world_id,
    JavaScriptExecuteRequestInIsolatedWorldCallback callback) {
  TRACE_EVENT_INSTANT0("test_tracing",
                       "JavaScriptExecuteRequestInIsolatedWorld",
                       TRACE_EVENT_SCOPE_THREAD);

  if (world_id <= ISOLATED_WORLD_ID_GLOBAL ||
      world_id > ISOLATED_WORLD_ID_MAX) {
    // Return if the world_id is not valid. world_id is passed as a plain int
    // over IPC and needs to be verified here, in the IPC endpoint.
    NOTREACHED();
    std::move(callback).Run(base::Value());
    return;
  }

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebScriptSource script = WebScriptSource(WebString::FromUTF16(javascript));
  JavaScriptIsolatedWorldRequest* request = new JavaScriptIsolatedWorldRequest(
      weak_factory_.GetWeakPtr(), wants_result, std::move(callback));
  frame_->RequestExecuteScriptInIsolatedWorld(
      world_id, &script, 1, false, WebLocalFrame::kSynchronous, request);
}

RenderFrameImpl::JavaScriptIsolatedWorldRequest::JavaScriptIsolatedWorldRequest(
    base::WeakPtr<RenderFrameImpl> render_frame_impl,
    bool wants_result,
    JavaScriptExecuteRequestInIsolatedWorldCallback callback)
    : render_frame_impl_(render_frame_impl),
      wants_result_(wants_result),
      callback_(std::move(callback)) {}

RenderFrameImpl::JavaScriptIsolatedWorldRequest::
    ~JavaScriptIsolatedWorldRequest() = default;

void RenderFrameImpl::JavaScriptIsolatedWorldRequest::Completed(
    const blink::WebVector<v8::Local<v8::Value>>& result) {
  if (!render_frame_impl_) {
    // If the frame is gone, there's nothing that can be safely done; bail.
    delete this;
    return;
  }

  base::Value value;
  if (!result.empty() && wants_result_) {
    // It's safe to always use the main world context when converting
    // here. V8ValueConverterImpl shouldn't actually care about the
    // context scope, and it switches to v8::Object's creation context
    // when encountered. (from extensions/renderer/script_injection.cc)
    v8::Local<v8::Context> context =
        render_frame_impl_.get()->frame_->MainWorldScriptContext();
    v8::Context::Scope context_scope(context);
    V8ValueConverterImpl converter;
    converter.SetDateAllowed(true);
    converter.SetRegExpAllowed(true);
    std::unique_ptr<base::Value> new_value =
        converter.FromV8Value(*result.begin(), context);
    if (new_value)
      value = base::Value::FromUniquePtrValue(std::move(new_value));
  }

  std::move(callback_).Run(std::move(value));

  delete this;
}

base::Value RenderFrameImpl::GetJavaScriptExecutionResult(
    v8::Local<v8::Value> result) {
  if (!result.IsEmpty()) {
    v8::Local<v8::Context> context = frame_->MainWorldScriptContext();
    v8::Context::Scope context_scope(context);
    V8ValueConverterImpl converter;
    converter.SetDateAllowed(true);
    converter.SetRegExpAllowed(true);
    std::unique_ptr<base::Value> new_value =
        converter.FromV8Value(result, context);
    if (new_value)
      return std::move(*new_value);
  }
  return base::Value();
}

void RenderFrameImpl::SwapIn() {
  SwapInInternal();
}

void RenderFrameImpl::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  TRACE_EVENT2("renderer", "RenderFrameImpl::UpdateBrowserControlsState",
               "Constraint", static_cast<int>(constraints), "Current",
               static_cast<int>(current));
  TRACE_EVENT_INSTANT1("renderer", "is_animated", TRACE_EVENT_SCOPE_THREAD,
                       "animated", animate);

  DCHECK(is_main_frame_);

  // Check content::BrowserControlsState, and cc::BrowserControlsState
  // are kept in sync.
  static_assert(static_cast<int>(BROWSER_CONTROLS_STATE_SHOWN) ==
                    static_cast<int>(cc::BrowserControlsState::kShown),
                "mismatching enums: SHOWN");
  static_assert(static_cast<int>(BROWSER_CONTROLS_STATE_HIDDEN) ==
                    static_cast<int>(cc::BrowserControlsState::kHidden),
                "mismatching enums: HIDDEN");
  static_assert(static_cast<int>(BROWSER_CONTROLS_STATE_BOTH) ==
                    static_cast<int>(cc::BrowserControlsState::kBoth),
                "mismatching enums: BOTH");

  cc::LayerTreeHost* host = render_widget_->layer_tree_host();
  host->UpdateBrowserControlsState(
      static_cast<cc::BrowserControlsState>(constraints),
      static_cast<cc::BrowserControlsState>(current), animate);
}

void RenderFrameImpl::SnapshotAccessibilityTree(
    uint32_t ax_mode,
    SnapshotAccessibilityTreeCallback callback) {
  ui::AXTreeUpdate response;
  RenderAccessibilityImpl::SnapshotAccessibilityTree(this, &response,
                                                     ui::AXMode(ax_mode));
  std::move(callback).Run(response);
}

void RenderFrameImpl::GetSerializedHtmlWithLocalLinks(
    const base::flat_map<GURL, base::FilePath>& url_map,
    const base::flat_map<base::UnguessableToken, base::FilePath>&
        frame_token_map,
    bool save_with_empty_url,
    mojo::PendingRemote<mojom::FrameHTMLSerializerHandler> handler_remote) {
  // Convert input to the canonical way of passing a map into a Blink API.
  LinkRewritingDelegate delegate(url_map, frame_token_map);
  RenderFrameWebFrameSerializerClient client(std::move(handler_remote));

  // Serialize the frame (without recursing into subframes).
  WebFrameSerializer::Serialize(GetWebFrame(), &client, &delegate,
                                save_with_empty_url);
}

#if defined(OS_ANDROID)
void RenderFrameImpl::ExtractSmartClipData(
    const gfx::Rect& rect,
    ExtractSmartClipDataCallback callback) {
  blink::WebString clip_text;
  blink::WebString clip_html;
  blink::WebRect clip_rect;
  GetWebFrame()->ExtractSmartClipData(rect, clip_text, clip_html, clip_rect);
  std::move(callback).Run(clip_text.Utf16(), clip_html.Utf16(), clip_rect);
}
#endif  // defined(OS_ANDROID)

void RenderFrameImpl::NotifyObserversOfFailedProvisionalLoad() {
  for (auto& observer : observers_)
    observer.DidFailProvisionalLoad();
}

void RenderFrameImpl::DidMeaningfulLayout(
    blink::WebMeaningfulLayout layout_type) {
  for (auto& observer : observers_)
    observer.DidMeaningfulLayout(layout_type);
}

void RenderFrameImpl::DidCommitAndDrawCompositorFrame() {
#if BUILDFLAG(ENABLE_PLUGINS)
  // Notify all instances that we painted.  The same caveats apply as for
  // ViewFlushedPaint regarding instances closing themselves, so we take
  // similar precautions.
  PepperPluginSet plugins = active_pepper_instances_;
  for (auto* plugin : plugins) {
    if (active_pepper_instances_.find(plugin) != active_pepper_instances_.end())
      plugin->ViewInitiatedPaint();
  }
#endif
}

RenderView* RenderFrameImpl::GetRenderView() {
  return render_view_;
}

RenderAccessibility* RenderFrameImpl::GetRenderAccessibility() {
  return render_accessibility_manager_->GetRenderAccessibilityImpl();
}

std::unique_ptr<AXTreeSnapshotter> RenderFrameImpl::CreateAXTreeSnapshotter() {
  return std::make_unique<AXTreeSnapshotterImpl>(this);
}

int RenderFrameImpl::GetRoutingID() {
  return routing_id_;
}

blink::WebLocalFrame* RenderFrameImpl::GetWebFrame() {
  DCHECK(frame_);
  return frame_;
}

const blink::web_pref::WebPreferences& RenderFrameImpl::GetBlinkPreferences() {
  return render_view_->GetBlinkPreferences();
}

const blink::mojom::RendererPreferences&
RenderFrameImpl::GetRendererPreferences() const {
  return render_view_->renderer_preferences();
}

int RenderFrameImpl::ShowContextMenu(
    ContextMenuClient* client,
    const UntrustworthyContextMenuParams& params) {
  DCHECK(client);  // A null client means "internal" when we issue callbacks.
  UntrustworthyContextMenuParams our_params(params);

  blink::WebRect position_in_window(params.x, params.y, 0, 0);
  GetLocalRootRenderWidget()->ConvertViewportToWindow(&position_in_window);
  our_params.x = position_in_window.x;
  our_params.y = position_in_window.y;

  our_params.custom_context.request_id = pending_context_menus_.Add(client);
  Send(new FrameHostMsg_ContextMenu(routing_id_, our_params));
  return our_params.custom_context.request_id;
}

void RenderFrameImpl::CancelContextMenu(int request_id) {
  DCHECK(pending_context_menus_.Lookup(request_id));
  pending_context_menus_.Remove(request_id);
}

void RenderFrameImpl::ShowVirtualKeyboard() {
  GetLocalRootRenderWidget()->GetWebWidget()->ShowVirtualKeyboard();
}

blink::WebPlugin* RenderFrameImpl::CreatePlugin(
    const WebPluginInfo& info,
    const blink::WebPluginParams& params,
    std::unique_ptr<content::PluginInstanceThrottler> throttler) {
#if BUILDFLAG(ENABLE_PLUGINS)
  base::Optional<url::Origin> origin_lock;
  if (GetContentClient()->renderer()->IsOriginIsolatedPepperPlugin(info.path)) {
    origin_lock = url::Origin::Create(GURL(params.url));
  }

  bool pepper_plugin_was_registered = false;
  scoped_refptr<PluginModule> pepper_module(PluginModule::Create(
      this, info, origin_lock, &pepper_plugin_was_registered,
      GetTaskRunner(blink::TaskType::kNetworking)));
  if (pepper_plugin_was_registered) {
    if (pepper_module.get()) {
      return new PepperWebPluginImpl(
          pepper_module.get(), params, this,
          base::WrapUnique(
              static_cast<PluginInstanceThrottlerImpl*>(throttler.release())));
    }
  }
#if defined(OS_CHROMEOS)
  LOG(WARNING) << "Pepper module/plugin creation failed.";
#endif
#endif  // BUILDFLAG(ENABLE_PLUGINS)
  return nullptr;
}

void RenderFrameImpl::ExecuteJavaScript(const base::string16& javascript) {
  JavaScriptExecuteRequest(javascript, false, base::DoNothing());
}

void RenderFrameImpl::BindLocalInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  GetInterface(interface_name, std::move(interface_pipe));
}

service_manager::InterfaceProvider* RenderFrameImpl::GetRemoteInterfaces() {
  return &remote_interfaces_;
}

blink::AssociatedInterfaceRegistry*
RenderFrameImpl::GetAssociatedInterfaceRegistry() {
  return &associated_interfaces_;
}

blink::AssociatedInterfaceProvider*
RenderFrameImpl::GetRemoteAssociatedInterfaces() {
  if (!remote_associated_interfaces_) {
    mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
        remote_interfaces;
    agent_scheduling_group_.GetRemoteRouteProvider()->GetRoute(
        routing_id_, remote_interfaces.InitWithNewEndpointAndPassReceiver());
    remote_associated_interfaces_ =
        std::make_unique<blink::AssociatedInterfaceProvider>(
            std::move(remote_interfaces),
            GetTaskRunner(blink::TaskType::kInternalNavigationAssociated));
  }
  return remote_associated_interfaces_.get();
}

#if BUILDFLAG(ENABLE_PLUGINS)
void RenderFrameImpl::RegisterPeripheralPlugin(
    const url::Origin& content_origin,
    base::OnceClosure unthrottle_callback) {
  return plugin_power_saver_helper_->RegisterPeripheralPlugin(
      content_origin, std::move(unthrottle_callback));
}

RenderFrame::PeripheralContentStatus
RenderFrameImpl::GetPeripheralContentStatus(
    const url::Origin& main_frame_origin,
    const url::Origin& content_origin,
    const gfx::Size& unobscured_size,
    RecordPeripheralDecision record_decision) {
  return plugin_power_saver_helper_->GetPeripheralContentStatus(
      main_frame_origin, content_origin, unobscured_size, record_decision);
}

void RenderFrameImpl::AllowlistContentOrigin(
    const url::Origin& content_origin) {
  return plugin_power_saver_helper_->AllowlistContentOrigin(content_origin);
}

void RenderFrameImpl::PluginDidStartLoading() {
  DidStartLoading();
}

void RenderFrameImpl::PluginDidStopLoading() {
  DidStopLoading();
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

bool RenderFrameImpl::IsFTPDirectoryListing() {
  return frame_->GetDocumentLoader()->IsListingFtpDirectory();
}

void RenderFrameImpl::SetSelectedText(const base::string16& selection_text,
                                      size_t offset,
                                      const gfx::Range& range) {
  GetWebFrame()->TextSelectionChanged(WebString::FromUTF16(selection_text),
                                      static_cast<uint32_t>(offset), range);
}

void RenderFrameImpl::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  AddMessageToConsoleImpl(level, message, false /* discard_duplicates */);
}

blink::PreviewsState RenderFrameImpl::GetPreviewsState() {
  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();
  return document_loader ? document_loader->GetPreviewsState()
                         : blink::PreviewsTypes::PREVIEWS_UNSPECIFIED;
}

bool RenderFrameImpl::IsPasting() {
  return GetLocalRootRenderWidget()->GetFrameWidget()->IsPasting();
}

// blink::mojom::FullscreenVideoElementHandler implementation ------------------
void RenderFrameImpl::RequestFullscreenVideoElement() {
  WebElement video_element =
      frame_->GetDocument().GetElementsByHTMLTagName("video").FirstItem();

  if (!video_element.IsNull()) {
    // This is always initiated from browser side (which should require the user
    // interacting with ui) which suffices for a user gesture even though there
    // will have been no input to the frame at this point.
    frame_->NotifyUserActivation(
        blink::mojom::UserActivationNotificationType::kInteraction);

    video_element.RequestFullscreen();
  }
}

// blink::mojom::AutoplayConfigurationClient implementation
// --------------------------

void RenderFrameImpl::AddAutoplayFlags(const url::Origin& origin,
                                       const int32_t flags) {
  // If the origin is the same as the previously stored flags then we should
  // merge the two sets of flags together.
  if (autoplay_flags_.first == origin) {
    autoplay_flags_.second |= flags;
  } else {
    autoplay_flags_ = std::make_pair(origin, flags);
  }
}

// blink::mojom::ResourceLoadInfoNotifier implementation
// --------------------------

void RenderFrameImpl::NotifyResourceRedirectReceived(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_response) {}

void RenderFrameImpl::NotifyResourceResponseReceived(
    blink::mojom::ResourceLoadInfoPtr resource_load_info,
    network::mojom::URLResponseHeadPtr response_head,
    int32_t previews_state) {
  DCHECK_NE(resource_load_info->request_destination,
            network::mojom::RequestDestination::kDocument);
  if (response_head->network_accessed) {
    UMA_HISTOGRAM_ENUMERATION("Net.ConnectionInfo.SubResource",
                              response_head->connection_info,
                              net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS);
  }
}

void RenderFrameImpl::NotifyResourceTransferSizeUpdated(
    int32_t request_id,
    int32_t transfer_size_diff) {
  DidReceiveTransferSizeUpdate(request_id, transfer_size_diff);
}

void RenderFrameImpl::NotifyResourceLoadCompleted(
    blink::mojom::ResourceLoadInfoPtr resource_load_info,
    const network::URLLoaderCompletionStatus& status) {
  blink::RecordLoadHistograms(
      url::Origin::Create(resource_load_info->final_url),
      resource_load_info->request_destination, status.error_code);
  DidCompleteResponse(resource_load_info->request_id, status);
  GetFrameHost()->ResourceLoadComplete(std::move(resource_load_info));
}

void RenderFrameImpl::NotifyResourceLoadCanceled(int32_t request_id) {
  DidCancelResponse(request_id);
}

void RenderFrameImpl::Clone(
    mojo::PendingReceiver<blink::mojom::ResourceLoadInfoNotifier>
        pending_resource_load_info_notifier) {
  resource_load_info_notifier_receivers_.Add(
      this, std::move(pending_resource_load_info_notifier));
}

// mojom::Frame implementation -------------------------------------------------

void RenderFrameImpl::GetInterfaceProvider(
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver) {
  interface_provider_receivers_.Add(this, std::move(receiver));
}

void RenderFrameImpl::GetCanonicalUrlForSharing(
    GetCanonicalUrlForSharingCallback callback) {
  WebURL canonical_url = GetWebFrame()->GetDocument().CanonicalUrlForSharing();
  std::move(callback).Run(canonical_url.IsNull()
                              ? base::nullopt
                              : base::make_optional(GURL(canonical_url)));
}

void RenderFrameImpl::BlockRequests() {
  frame_request_blocker_->Block();
}

void RenderFrameImpl::ResumeBlockedRequests() {
  frame_request_blocker_->Resume();
}

void RenderFrameImpl::CancelBlockedRequests() {
  frame_request_blocker_->Cancel();
}

void RenderFrameImpl::AllowBindings(int32_t enabled_bindings_flags) {
  enabled_bindings_ |= enabled_bindings_flags;
}

void RenderFrameImpl::EnableMojoJsBindings() {
  enable_mojo_js_bindings_ = true;
}

void RenderFrameImpl::BindWebUI(mojo::PendingReceiver<mojom::WebUI> receiver,
                                mojo::PendingRemote<mojom::WebUIHost> remote) {
  DCHECK(enabled_bindings_ & BINDINGS_POLICY_WEB_UI);
  WebUIExtensionData::Create(this, std::move(receiver), std::move(remote));
}

void RenderFrameImpl::SetOldPageLifecycleStateFromNewPageCommitIfNeeded(
    const mojom::OldPageInfo* old_page_info) {
  if (!old_page_info)
    return;
  RenderFrameImpl* old_main_render_frame = RenderFrameImpl::FromRoutingID(
      old_page_info->routing_id_for_old_main_frame);
  if (!old_main_render_frame) {
    // Even if we sent a valid |routing_id_for_old_main_frame|, it might have
    // already been destroyed by the time we try to get the RenderFrame, so
    // we should check if it still exists.
    return;
  }
  CHECK(IsMainFrame());
  CHECK(old_main_render_frame->IsMainFrame());
  DCHECK_EQ(old_page_info->new_lifecycle_state_for_old_page->visibility,
            PageVisibilityState::kHidden);
  DCHECK_NE(old_page_info->new_lifecycle_state_for_old_page->pagehide_dispatch,
            blink::mojom::PagehideDispatch::kNotDispatched);
  WebFrame* old_main_web_frame = old_main_render_frame->GetWebFrame();
  old_main_web_frame->View()->SetPageLifecycleStateFromNewPageCommit(
      old_page_info->new_lifecycle_state_for_old_page->visibility,
      old_page_info->new_lifecycle_state_for_old_page->pagehide_dispatch);
}

void RenderFrameImpl::CommitNavigation(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    base::Optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory,
    const base::UnguessableToken& devtools_navigation_token,
    mojom::NavigationClient::CommitNavigationCallback commit_callback) {
  DCHECK(navigation_client_impl_);
  DCHECK(!IsRendererDebugURL(common_params->url));
  DCHECK(!NavigationTypeUtils::IsSameDocument(common_params->navigation_type));

  AssertNavigationCommits assert_navigation_commits(
      this, kMayReplaceInitialEmptyDocument);

  if (ShouldIgnoreCommitNavigation(*commit_params)) {
    browser_side_navigation_pending_url_ = GURL();
    AbortCommitNavigation();

    // Disarm AssertNavigationCommits and pretend that the commit actually
    // happened. Signalling that the load stopped /should/ signal the browser to
    // tear down the speculative RFH associated with |this|...
    navigation_commit_state_ = NavigationCommitState::kDidCommit;

    return;
  }

  SetOldPageLifecycleStateFromNewPageCommitIfNeeded(
      commit_params->old_page_info.get());

  bool was_initiated_in_this_frame =
      navigation_client_impl_ &&
      navigation_client_impl_->was_initiated_in_this_frame();

  // Sanity check that the browser always sends us new loader factories on
  // cross-document navigations.
  DCHECK(common_params->url.SchemeIs(url::kJavaScriptScheme) ||
         common_params->url.IsAboutSrcdoc() || subresource_loader_factories);

  int request_id = ResourceDispatcher::MakeRequestID();
  std::unique_ptr<DocumentState> document_state = BuildDocumentStateFromParams(
      *common_params, *commit_params, std::move(commit_callback),
      std::move(navigation_client_impl_), request_id,
      was_initiated_in_this_frame);

  // Check if the navigation being committed originated as a client redirect.
  bool is_client_redirect =
      !!(common_params->transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  auto navigation_params =
      std::make_unique<WebNavigationParams>(devtools_navigation_token);
  navigation_params->is_client_redirect = is_client_redirect;
  FillMiscNavigationParams(*common_params, *commit_params,
                           navigation_params.get());

  auto commit_with_params = base::BindOnce(
      &RenderFrameImpl::CommitNavigationWithParams, weak_factory_.GetWeakPtr(),
      common_params.Clone(), commit_params.Clone(),
      std::move(subresource_loader_factories), std::move(subresource_overrides),
      std::move(controller_service_worker_info), std::move(container_info),
      std::move(prefetch_loader_factory), std::move(document_state));

  // Perform a navigation to a data url if needed (for main frames).
  // Note: the base URL might be invalid, so also check the data URL string.
  bool should_load_data_url = !common_params->base_url_for_data_url.is_empty();
#if defined(OS_ANDROID)
  should_load_data_url |= !commit_params->data_url_as_string.empty();
#endif
  if (is_main_frame_ && should_load_data_url) {
    std::string mime_type, charset, data;
    GURL base_url;
    DecodeDataURL(*common_params, *commit_params, &mime_type, &charset, &data,
                  &base_url);
    navigation_params->url = base_url;
    WebNavigationParams::FillStaticResponse(navigation_params.get(),
                                            WebString::FromUTF8(mime_type),
                                            WebString::FromUTF8(charset), data);
    // Needed so that history-url-only changes don't become reloads.
    navigation_params->unreachable_url =
        common_params->history_url_for_data_url;
    std::move(commit_with_params).Run(std::move(navigation_params));
    return;
  }

  FillNavigationParamsRequest(*common_params, *commit_params,
                              navigation_params.get());
  if (!url_loader_client_endpoints &&
      common_params->url.SchemeIs(url::kDataScheme)) {
    // Normally, data urls will have |url_loader_client_endpoints| set.
    // However, tests and interstitial pages pass data urls directly,
    // without creating url loader.
    std::string mime_type, charset, data;
    if (!net::DataURL::Parse(common_params->url, &mime_type, &charset, &data)) {
      CHECK(false) << "Invalid URL passed: "
                   << common_params->url.possibly_invalid_spec();
      return;
    }
    WebNavigationParams::FillStaticResponse(navigation_params.get(),
                                            WebString::FromUTF8(mime_type),
                                            WebString::FromUTF8(charset), data);
  } else {
    NavigationBodyLoader::FillNavigationParamsResponseAndBodyLoader(
        std::move(common_params), std::move(commit_params), request_id,
        response_head.Clone(), std::move(response_body),
        std::move(url_loader_client_endpoints),
        GetTaskRunner(blink::TaskType::kInternalLoading), GetRoutingID(),
        !frame_->Parent(), navigation_params.get());
  }

  FillNavigationParamsOriginPolicy(*response_head, navigation_params.get());

  // The MHTML mime type should be same as the one we check in the browser
  // process's download_utils::MustDownload.
  bool is_mhtml_archive =
      base::LowerCaseEqualsASCII(response_head->mime_type,
                                 "multipart/related") ||
      base::LowerCaseEqualsASCII(response_head->mime_type, "message/rfc822");
  if (is_mhtml_archive && navigation_params->body_loader) {
    // Load full mhtml archive before committing navigation.
    // We need this to retrieve the document mime type prior to committing.
    mhtml_body_loader_client_ =
        std::make_unique<RenderFrameImpl::MHTMLBodyLoaderClient>(
            this, std::move(navigation_params), std::move(commit_with_params));
    // The navigation didn't really commit, but lie about it anyway. Why? MHTML
    // is a bit special: the renderer process is responsible for parsing the
    // archive, but at this point, the response body isn't fully loaded yet.
    // Instead, MHTMLBodyLoaderClient will read the entire response body and
    // parse the archive to extract the main resource to be committed.
    //
    // There are two possibilities from this point:
    // - |MHTMLBodyLoaderClient::BodyLoadingFinished()| is called. At that
    //   point, the main resource can be extracted, and the navigation will be
    //   synchronously committed. If |this| is a provisional frame, it will be
    //   swapped in and committed. A separate |AssertNavigationCommits| is
    //   instantiated in |MHTMLBodyLoaderClient::BodyLoadingFinished()| to
    //   assert the commit actually happens. ✔️
    // - Alternatively, the pending archive load may be cancelled. This can only
    //   happen if the renderer initiates a new navigation, or if the browser
    //   requests that this frame commit a different navigation.
    //   - If |this| is already swapped in, the reason for cancelling the
    //     pending archive load does not matter. There will be no state skew
    //     between the browser and the renderer, since |this| has already
    //     committed a navigation. ✔️
    //   - If |this| is provisional, the pending archive load may only be
    //     cancelled by the browser requesting |this| to commit a different
    //     navigation. AssertNavigationCommits ensures the new request ends up
    //     committing and swapping in |this|, so this is OK. ✔️
    navigation_commit_state_ = NavigationCommitState::kDidCommit;
    return;
  }

  // Common case - fill navigation params from provided information and commit.
  std::move(commit_with_params).Run(std::move(navigation_params));
}

bool RenderFrameImpl::ShouldIgnoreCommitNavigation(
    const mojom::CommitNavigationParams& commit_params) {
  // We can ignore renderer-initiated navigations (nav_entry_id == 0) which
  // have been canceled in the renderer, but browser was not aware yet at the
  // moment of issuing a CommitNavigation call.
  if (!browser_side_navigation_pending_ &&
      !browser_side_navigation_pending_url_.is_empty() &&
      browser_side_navigation_pending_url_ == commit_params.original_url &&
      commit_params.nav_entry_id == 0) {
    // If a navigation has been cancelled in the renderer, the renderer must
    // have already committed and swapped this frame into the frame tree:
    // - a provisional frame cannot run JS, so it cannot self-cancel its own
    //   navigation.
    // - a provisional frame is only partially linked into the frame tree, so
    //   other frames should not be able to reach into it to cancel the
    //   navigation either.
    CHECK(in_frame_tree_);
    return true;
  }
  return false;
}

void RenderFrameImpl::CommitNavigationWithParams(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    base::Optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory,
    std::unique_ptr<DocumentState> document_state,
    std::unique_ptr<WebNavigationParams> navigation_params) {
  if (ShouldIgnoreCommitNavigation(*commit_params)) {
    browser_side_navigation_pending_url_ = GURL();
    return;
  }

  // TODO(738611): This is temporary switch to have chrome WebUI use the old web
  // APIs. After completion of the migration, we should remove this.
  if (GetContentClient()->renderer()->RequiresWebComponentsV0(
          common_params->url)) {
    blink::WebRuntimeFeatures::EnableShadowDOMV0(true);
    blink::WebRuntimeFeatures::EnableCustomElementsV0(true);
    blink::WebRuntimeFeatures::EnableHTMLImports(true);
  }

  // Here, creator means either the parent frame or the window opener.
  bool inherit_loaders_from_creator =
      // Iframe with the about:srcdoc URL inherits subresource loaders from
      // its parent. If its parent is able to use the FileUrlLoader, then its
      // about:srcdoc iframe can use it too.
      // TODO(arthursonzogni): Ideally, this decision should be made by the
      // browser process. However, giving an iframe the FileUrlLoader mistakenly
      // could have terrible consequences (e.g. give access to user's file from
      // an unknown website). Inheriting from the parent in the renderer process
      // is more conservative and feels more cautious for now.
      // TODO(arthursonzogni): Something similar needs to be done for
      // about:blank.
      common_params->url.IsAboutSrcdoc();

  // TODO(lukasza): https://crbug.com/936696: No need to postpone setting the
  // |new_loader_factories| once we start swapping RenderFrame^H^H^H
  // RenderDocument on every cross-document navigation.
  scoped_refptr<ChildURLLoaderFactoryBundle> new_loader_factories;
  if (inherit_loaders_from_creator) {
    // The browser process didn't provide any way to fetch subresources, it
    // expects this document to inherit loaders from its parent.
    DCHECK(!subresource_loader_factories);
    DCHECK(!subresource_overrides);
    DCHECK(!prefetch_loader_factory);

    new_loader_factories = GetLoaderFactoryBundleFromCreator();
  } else {
    new_loader_factories = CreateLoaderFactoryBundle(
        std::move(subresource_loader_factories),
        std::move(subresource_overrides), std::move(prefetch_loader_factory));
  }
  DCHECK(new_loader_factories);
  DCHECK(new_loader_factories->HasBoundDefaultFactory());

  // If the navigation is for "view source", the WebLocalFrame needs to be put
  // in a special mode.
  if (commit_params->is_view_source)
    frame_->EnableViewSourceMode(true);

  // Note: this intentionally does not call |Detach()| before |reset()|. If
  // there is an active |MHTMLBodyLoaderClient|, the browser-side navigation
  // code is explicitly replacing it with a new navigation commit request.
  // The check for |kWillCommit| in |~MHTMLBodyLoaderClient| covers this case.
  mhtml_body_loader_client_.reset();

  PrepareFrameForCommit(common_params->url, *commit_params);

  blink::WebFrameLoadType load_type =
      NavigationTypeToLoadType(common_params->navigation_type,
                               common_params->should_replace_current_entry,
                               commit_params->page_state.IsValid());

  WebHistoryItem item_for_history_navigation;
  blink::mojom::CommitResult commit_status = blink::mojom::CommitResult::Ok;

  if (load_type == WebFrameLoadType::kBackForward) {
    // We must know the nav entry ID of the page we are navigating back to,
    // which should be the case because history navigations are routed via the
    // browser.
    DCHECK_NE(0, commit_params->nav_entry_id);

    // Check that the history navigation can commit.
    commit_status = PrepareForHistoryNavigationCommit(
        *common_params, *commit_params, &item_for_history_navigation,
        &load_type);
  }

  if (commit_status != blink::mojom::CommitResult::Ok) {
    // The browser expects the frame to be loading this navigation. Inform it
    // that the load stopped if needed.
    if (frame_ && !frame_->IsLoading())
      GetFrameHost()->DidStopLoading();
    return;
  }

  navigation_params->frame_load_type = load_type;
  navigation_params->history_item = item_for_history_navigation;

  if (!container_info) {
    // An empty network provider will always be created since it is expected in
    // a certain number of places.
    navigation_params->service_worker_network_provider =
        ServiceWorkerNetworkProviderForFrame::CreateInvalidInstance();
  } else {
    navigation_params->service_worker_network_provider =
        ServiceWorkerNetworkProviderForFrame::Create(
            this, std::move(container_info),
            std::move(controller_service_worker_info),
            network::SharedURLLoaderFactory::Create(
                new_loader_factories->CloneWithoutAppCacheFactory()));
  }

  DCHECK(!pending_loader_factories_);
  pending_loader_factories_ = std::move(new_loader_factories);

  base::WeakPtr<RenderFrameImpl> weak_self = weak_factory_.GetWeakPtr();
  frame_->CommitNavigation(std::move(navigation_params),
                           std::move(document_state));
  // The commit can result in this frame being removed.
  if (!weak_self)
    return;

  pending_loader_factories_ = nullptr;
}

void RenderFrameImpl::CommitFailedNavigation(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    bool has_stale_copy_in_cache,
    int error_code,
    net::ResolveErrorInfo resolve_error_info,
    const base::Optional<std::string>& error_page_content,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    mojom::NavigationClient::CommitFailedNavigationCallback callback) {
  TRACE_EVENT1("navigation,benchmark,rail",
               "RenderFrameImpl::CommitFailedNavigation", "id", routing_id_);
  DCHECK(navigation_client_impl_);
  DCHECK(!NavigationTypeUtils::IsSameDocument(common_params->navigation_type));

  AssertNavigationCommits assert_navigation_commits(
      this, kMayReplaceInitialEmptyDocument);

  RenderFrameImpl::PrepareRenderViewForNavigation(common_params->url,
                                                  *commit_params);
  // Note: this intentionally does not call |Detach()| before |reset()|. If
  // there is an active |MHTMLBodyLoaderClient|, the browser-side navigation
  // code is explicitly replacing it with a new navigation commit request.
  // The check for |kWillCommit| in |~MHTMLBodyLoaderClient| covers this case.
  mhtml_body_loader_client_.reset();

  GetContentClient()->SetActiveURL(
      common_params->url, frame_->Top()->GetSecurityOrigin().ToString().Utf8());

  // TODO(lukasza): https://crbug.com/936696: No need to postpone setting the
  // |new_loader_factories| once we start swapping RenderFrame^H^H^H
  // RenderDocument on every cross-document navigation.
  scoped_refptr<ChildURLLoaderFactoryBundle> new_loader_factories =
      CreateLoaderFactoryBundle(
          std::move(subresource_loader_factories),
          base::nullopt /* subresource_overrides */,
          mojo::NullRemote() /* prefetch_loader_factory */);
  DCHECK(new_loader_factories->HasBoundDefaultFactory());

  // Send the provisional load failure.
  WebURLError error(
      error_code, 0, resolve_error_info,
      has_stale_copy_in_cache ? WebURLError::HasCopyInCache::kTrue
                              : WebURLError::HasCopyInCache::kFalse,
      WebURLError::IsWebSecurityViolation::kFalse, common_params->url);

  auto navigation_params = std::make_unique<WebNavigationParams>();
  FillNavigationParamsRequest(*common_params, *commit_params,
                              navigation_params.get());
  navigation_params->url = GURL(kUnreachableWebDataURL);
  // FillNavigationParamsRequest() sets the |navigation_params->http_method| to
  // the original method of the request. In successful page loads,
  // |navigation_params->redirects| also gets populated and the redirects are
  // later replayed to update the method. However, in the case of an error page
  // load, the redirects are neither populated nor replayed. Hence |http_method|
  // needs to be manually set to the final method.
  navigation_params->http_method = WebString::FromASCII(common_params->method);
  navigation_params->error_code = error_code;

  // This is already checked in `NavigationRequest::OnRequestFailedInternal` and
  // `NavigationRequest::OnFailureChecksCompleted` on the browser side, so the
  // renderer should never see this.
  CHECK_NE(net::ERR_ABORTED, error_code);

  // On load failure, a frame can ask its owner to render fallback content.
  // When that happens, don't load an error page.
  base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();
  blink::WebNavigationControl::FallbackContentResult fallback_result =
      frame_->MaybeRenderFallbackContent(error);

  // The rendering fallback content can result in this frame being removed.
  // Use a WeakPtr as an easy way to detect whether this has occurred. If so,
  // this method should return immediately and not touch any part of the object,
  // otherwise it will result in a use-after-free bug.
  if (!weak_this)
    return;

  if (commit_params->nav_entry_id == 0) {
    // For renderer initiated navigations, we send out a
    // DidFailProvisionalLoad() notification.
    NotifyObserversOfFailedProvisionalLoad();
  }

  if (fallback_result != blink::WebNavigationControl::NoFallbackContent) {
    AbortCommitNavigation();
    if (fallback_result == blink::WebNavigationControl::NoLoadInProgress) {
      // If the frame wasn't loading but was fallback-eligible, the fallback
      // content won't be shown. However, showing an error page isn't right
      // either, as the frame has already been populated with something
      // unrelated to this navigation failure. In that case, just send a stop
      // IPC to the browser to unwind its state, and leave the frame as-is.
      GetFrameHost()->DidStopLoading();
    }
    browser_side_navigation_pending_ = false;
    browser_side_navigation_pending_url_ = GURL();

    // Disarm AssertNavigationCommits and pretend that the commit actually
    // happened. Hopefully, either:
    // - the browser process is signalled to stop loading, which /should/ signal
    //   the browser to tear down the speculative RFH associated with |this| or
    // - fallback content is triggered, which /should/ eventually discard |this|
    //   as well.
    navigation_commit_state_ = NavigationCommitState::kDidCommit;
    return;
  }

  // Replace the current history entry in reloads, and loads of the same url.
  // This corresponds to Blink's notion of a standard commit.
  // Also replace the current history entry if the browser asked for it
  // specifically.
  // TODO(clamy): see if initial commits in subframes should be handled
  // separately.
  bool is_reload_or_history =
      NavigationTypeUtils::IsReload(common_params->navigation_type) ||
      NavigationTypeUtils::IsHistory(common_params->navigation_type);
  bool replace = is_reload_or_history ||
                 common_params->url == GetLoadingUrl() ||
                 common_params->should_replace_current_entry;
  std::unique_ptr<HistoryEntry> history_entry;
  if (commit_params->page_state.IsValid())
    history_entry = PageStateToHistoryEntry(commit_params->page_state);

  std::string error_html;
  std::string* error_html_ptr = &error_html;
  if (error_page_content) {
    error_html = error_page_content.value();
    error_html_ptr = nullptr;
  }
  GetContentClient()->renderer()->PrepareErrorPage(
      this, error, navigation_params->http_method.Ascii(), error_html_ptr);

  // Make sure we never show errors in view source mode.
  frame_->EnableViewSourceMode(false);

  if (history_entry) {
    navigation_params->frame_load_type = WebFrameLoadType::kBackForward;
    navigation_params->history_item = history_entry->root();
  } else if (replace) {
    navigation_params->frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
  }
  navigation_params->service_worker_network_provider =
      ServiceWorkerNetworkProviderForFrame::CreateInvalidInstance();
  FillMiscNavigationParams(*common_params, *commit_params,
                           navigation_params.get());
  WebNavigationParams::FillStaticResponse(navigation_params.get(), "text/html",
                                          "UTF-8", error_html);
  navigation_params->unreachable_url = error.url();

  // The error page load (not to confuse with a failed load of original page)
  // was not initiated through BeginNavigation, therefore
  // |was_initiated_in_this_frame| is false.
  std::unique_ptr<DocumentState> document_state = BuildDocumentStateFromParams(
      *common_params, *commit_params, std::move(callback),
      std::move(navigation_client_impl_), ResourceDispatcher::MakeRequestID(),
      false /* was_initiated_in_this_frame */);

  DCHECK(!pending_loader_factories_);
  pending_loader_factories_ = std::move(new_loader_factories);

  // The load of the error page can result in this frame being removed.
  // Use a WeakPtr as an easy way to detect whether this has occurred. If so,
  // this method should return immediately and not touch any part of the object,
  // otherwise it will result in a use-after-free bug.
  frame_->CommitNavigation(std::move(navigation_params),
                           std::move(document_state));
  if (!weak_this)
    return;

  pending_loader_factories_ = nullptr;
  browser_side_navigation_pending_ = false;
  browser_side_navigation_pending_url_ = GURL();
}

// mojom::FrameNavigationControl implementation --------------------------------

void RenderFrameImpl::CommitSameDocumentNavigation(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    CommitSameDocumentNavigationCallback callback) {
  DCHECK(!IsRendererDebugURL(common_params->url));
  DCHECK(!NavigationTypeUtils::IsReload(common_params->navigation_type));
  DCHECK(!commit_params->is_view_source);
  DCHECK(NavigationTypeUtils::IsSameDocument(common_params->navigation_type));

  // TODO(danakj): Disabled for M87, but underlying problem exists.
  // CHECK(in_frame_tree_);

  // Unlike a cross-document navigation commit, detach the MHTMLBodyLoaderClient
  // before resetting it. In the case of a cross-document navigation, it's
  // important to ensure *something* commits, even if the original commit
  // request was replaced by a commit request. However, in the case of a
  // same-document navigation commit request, |this| must already be committed.
  //
  // Note that this means a same-document navigation might cancel a
  // cross-document navigation, which is a bit strange. In the future, explore
  // the idea of allowing the cross-document navigation to continue.
  if (mhtml_body_loader_client_) {
    mhtml_body_loader_client_->Detach();
    mhtml_body_loader_client_.reset();
  }

  PrepareFrameForCommit(common_params->url, *commit_params);

  blink::WebFrameLoadType load_type =
      NavigationTypeToLoadType(common_params->navigation_type,
                               common_params->should_replace_current_entry,
                               commit_params->page_state.IsValid());

  blink::mojom::CommitResult commit_status = blink::mojom::CommitResult::Ok;
  WebHistoryItem item_for_history_navigation;

  if (common_params->navigation_type ==
      mojom::NavigationType::HISTORY_SAME_DOCUMENT) {
    DCHECK(commit_params->page_state.IsValid());
    // We must know the nav entry ID of the page we are navigating back to,
    // which should be the case because history navigations are routed via the
    // browser.
    DCHECK_NE(0, commit_params->nav_entry_id);
    DCHECK(!common_params->is_history_navigation_in_new_child_frame);
    commit_status = PrepareForHistoryNavigationCommit(
        *common_params, *commit_params, &item_for_history_navigation,
        &load_type);
  }

  if (commit_status == blink::mojom::CommitResult::Ok) {
    base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();
    // Same-document navigations on data URLs loaded with a valid base URL
    // should keep the base URL as document URL.
    bool use_base_url_for_data_url =
        !common_params->base_url_for_data_url.is_empty();
#if defined(OS_ANDROID)
    use_base_url_for_data_url |= !commit_params->data_url_as_string.empty();
#endif

    GURL url;
    if (is_main_frame_ && use_base_url_for_data_url) {
      url = common_params->base_url_for_data_url;
    } else {
      url = common_params->url;
    }
    bool is_client_redirect =
        !!(common_params->transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT);
    DocumentState* original_document_state =
        DocumentState::FromDocumentLoader(frame_->GetDocumentLoader());
    std::unique_ptr<DocumentState> document_state =
        original_document_state->Clone();
    InternalDocumentStateData* internal_data =
        InternalDocumentStateData::FromDocumentState(document_state.get());
    internal_data->CopyFrom(
        InternalDocumentStateData::FromDocumentState(original_document_state));
    // This is a browser-initiated same-document navigation (as opposed to a
    // fragment link click), therefore |was_initiated_in_this_frame| is false.
    internal_data->set_navigation_state(NavigationState::CreateBrowserInitiated(
        std::move(common_params), std::move(commit_params),
        mojom::NavigationClient::CommitNavigationCallback(), nullptr,
        false /* was_initiated_in_this_frame */));

    // Load the request.
    commit_status = frame_->CommitSameDocumentNavigation(
        url, load_type, item_for_history_navigation, is_client_redirect,
        std::move(document_state));

    // The load of the URL can result in this frame being removed. Use a
    // WeakPtr as an easy way to detect whether this has occured. If so, this
    // method should return immediately and not touch any part of the object,
    // otherwise it will result in a use-after-free bug.
    if (!weak_this)
      return;
  }

  std::move(callback).Run(commit_status);

  // The browser expects the frame to be loading this navigation. Inform it
  // that the load stopped if needed.
  if (frame_ && !frame_->IsLoading() &&
      commit_status != blink::mojom::CommitResult::Ok) {
    GetFrameHost()->DidStopLoading();
  }
}

void RenderFrameImpl::HandleRendererDebugURL(const GURL& url) {
  DCHECK(IsRendererDebugURL(url));
  base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    // Javascript URLs should be sent to Blink for handling.
    frame_->LoadJavaScriptURL(url);
  } else {
    // This is a Chrome Debug URL. Handle it.
    HandleChromeDebugURL(url);
  }

  // The browser sets its status as loading before calling this IPC. Inform it
  // that the load stopped if needed, while leaving the debug URL visible in the
  // address bar.
  if (weak_this && frame_ && !frame_->IsLoading())
    GetFrameHost()->DidStopLoading();
}

void RenderFrameImpl::UpdateSubresourceLoaderFactories(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories) {
  // TODO(lukasza): https://crbug.com/1013254: Avoid checking
  // |loader_factories_| for null below - they should be guaranteed to be
  // non-null after a frame commits (and UpdateSubresourceLoaderFactories should
  // only be called after a commit).  The check below is just a temporary
  // workaround to paper-over the crash in https://crbug.com/1013254.
  if (!loader_factories_)
    loader_factories_ = GetLoaderFactoryBundleFromCreator();
  if (!loader_factories_)
    loader_factories_ = GetLoaderFactoryBundleFallback();

  if (loader_factories_->IsHostChildURLLoaderFactoryBundle()) {
    static_cast<HostChildURLLoaderFactoryBundle*>(loader_factories_.get())
        ->UpdateThisAndAllClones(std::move(subresource_loader_factories));
  } else {
#if DCHECK_IS_ON()
    // This situation should happen only if the frame hosts a document that
    // isn't related to a real navigation (i.e. if the frame should "inherit"
    // the factories from its opener/parent - for example for about:blank or
    // about:srcdoc or about:blank#someHref frames, or for frames with no URL
    // - like the initial frame opened by window('', 'popup')).
    WebURL url = GetWebFrame()->GetDocument().Url();
    if (url.IsValid() && !url.IsEmpty())
      DCHECK(url.ProtocolIs(url::kAboutScheme));
#endif
    auto partial_bundle = base::MakeRefCounted<ChildURLLoaderFactoryBundle>();
    static_cast<blink::URLLoaderFactoryBundle*>(partial_bundle.get())
        ->Update(std::move(subresource_loader_factories));
    loader_factories_->Update(partial_bundle->PassInterface());
  }
}

void RenderFrameImpl::BindDevToolsAgent(
    mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgentHost> host,
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> receiver) {
  frame_->BindDevToolsAgent(std::move(host), std::move(receiver));
}

// blink::WebLocalFrameClient implementation
// ----------------------------------------
blink::BrowserInterfaceBrokerProxy*
RenderFrameImpl::GetBrowserInterfaceBroker() {
  return &browser_interface_broker_proxy_;
}

bool RenderFrameImpl::IsPluginHandledExternally(
    const blink::WebElement& plugin_element,
    const blink::WebURL& url,
    const blink::WebString& suggested_mime_type) {
#if BUILDFLAG(ENABLE_PLUGINS)
  return GetContentClient()->renderer()->IsPluginHandledExternally(
      this, plugin_element, GURL(url), suggested_mime_type.Utf8());
#else
  return false;
#endif
}

v8::Local<v8::Object> RenderFrameImpl::GetScriptableObject(
    const blink::WebElement& plugin_element,
    v8::Isolate* isolate) {
#if BUILDFLAG(ENABLE_PLUGINS)

  return GetContentClient()->renderer()->GetScriptableObject(plugin_element,
                                                             isolate);
#else
  return v8::Local<v8::Object>();
#endif
}

void RenderFrameImpl::UpdateSubresourceFactory(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> info) {
  auto child_info =
      std::make_unique<ChildPendingURLLoaderFactoryBundle>(std::move(info));
  GetLoaderFactoryBundle()->Update(std::move(child_info));
}

void RenderFrameImpl::BindToFrame(blink::WebNavigationControl* frame) {
  DCHECK(!frame_);

  std::pair<FrameMap::iterator, bool> result =
      g_frame_map.Get().emplace(frame, this);
  CHECK(result.second) << "Inserting a duplicate item.";

  frame_ = frame;
}

blink::WebPlugin* RenderFrameImpl::CreatePlugin(
    const blink::WebPluginParams& params) {
  blink::WebPlugin* plugin = nullptr;
  if (GetContentClient()->renderer()->OverrideCreatePlugin(this, params,
                                                           &plugin)) {
    return plugin;
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  WebPluginInfo info;
  std::string mime_type;
  bool found = false;
  Send(new FrameHostMsg_GetPluginInfo(
      routing_id_, params.url, frame_->Top()->GetSecurityOrigin(),
      params.mime_type.Utf8(), &found, &info, &mime_type));
  if (!found)
    return nullptr;

  WebPluginParams params_to_use = params;
  params_to_use.mime_type = WebString::FromUTF8(mime_type);
  return CreatePlugin(info, params_to_use, nullptr /* throttler */);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_PLUGINS)
}

blink::WebMediaPlayer* RenderFrameImpl::CreateMediaPlayer(
    const blink::WebMediaPlayerSource& source,
    WebMediaPlayerClient* client,
    blink::MediaInspectorContext* inspector_context,
    WebMediaPlayerEncryptedMediaClient* encrypted_client,
    WebContentDecryptionModule* initial_cdm,
    const blink::WebString& sink_id) {
  return media_factory_.CreateMediaPlayer(
      source, client, inspector_context, encrypted_client, initial_cdm, sink_id,
      GetLocalRootRenderWidget()->GetFrameSinkId(),
      GetLocalRootRenderWidget()->layer_tree_host()->GetSettings());
}

std::unique_ptr<blink::WebContentSettingsClient>
RenderFrameImpl::CreateWorkerContentSettingsClient() {
  if (!frame_ || !frame_->View())
    return nullptr;
  return GetContentClient()->renderer()->CreateWorkerContentSettingsClient(
      this);
}

#if !defined(OS_ANDROID)
std::unique_ptr<media::SpeechRecognitionClient>
RenderFrameImpl::CreateSpeechRecognitionClient(
    media::SpeechRecognitionClient::OnReadyCallback callback) {
  if (!frame_ || !frame_->View())
    return nullptr;
  return GetContentClient()->renderer()->CreateSpeechRecognitionClient(
      this, std::move(callback));
}
#endif

scoped_refptr<blink::WebWorkerFetchContext>
RenderFrameImpl::CreateWorkerFetchContext() {
  ServiceWorkerNetworkProviderForFrame* provider =
      static_cast<ServiceWorkerNetworkProviderForFrame*>(
          frame_->GetDocumentLoader()->GetServiceWorkerNetworkProvider());
  DCHECK(provider);

  mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher;
  mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
      watcher_receiver = watcher.InitWithNewPipeAndPassReceiver();
  render_view()->RegisterRendererPreferenceWatcher(std::move(watcher));

  // |pending_subresource_loader_updater| and
  // |pending_resource_load_info_notifier| are not used for
  // non-PlzDedicatedWorker and worklets.
  scoped_refptr<WebWorkerFetchContextImpl> worker_fetch_context =
      WebWorkerFetchContextImpl::Create(
          provider->context(), render_view_->renderer_preferences(),
          std::move(watcher_receiver), GetLoaderFactoryBundle()->Clone(),
          GetLoaderFactoryBundle()->CloneWithoutAppCacheFactory(),
          /*pending_subresource_loader_updater=*/mojo::NullReceiver(),
          RenderThreadImpl::current()
              ->resource_dispatcher()
              ->cors_exempt_header_list(),
          /*pending_resource_load_info_notifier=*/mojo::NullRemote());

  worker_fetch_context->set_ancestor_frame_id(routing_id_);
  worker_fetch_context->set_frame_request_blocker(frame_request_blocker_);
  worker_fetch_context->set_site_for_cookies(
      frame_->GetDocument().SiteForCookies());
  worker_fetch_context->set_top_frame_origin(
      frame_->GetDocument().TopFrameOrigin());

  for (auto& observer : observers_)
    observer.WillCreateWorkerFetchContext(worker_fetch_context.get());
  return worker_fetch_context;
}

scoped_refptr<blink::WebWorkerFetchContext>
RenderFrameImpl::CreateWorkerFetchContextForPlzDedicatedWorker(
    blink::WebDedicatedWorkerHostFactoryClient* factory_client) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  DCHECK(factory_client);

  mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher;
  mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
      watcher_receiver = watcher.InitWithNewPipeAndPassReceiver();
  render_view()->RegisterRendererPreferenceWatcher(std::move(watcher));

  mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
      pending_resource_load_info_notifier;
  resource_load_info_notifier_receivers_.Add(
      this,
      pending_resource_load_info_notifier.InitWithNewPipeAndPassReceiver());

  scoped_refptr<WebWorkerFetchContextImpl> worker_fetch_context =
      static_cast<DedicatedWorkerHostFactoryClient*>(factory_client)
          ->CreateWorkerFetchContext(
              render_view_->renderer_preferences(), std::move(watcher_receiver),
              std::move(pending_resource_load_info_notifier));

  worker_fetch_context->set_ancestor_frame_id(routing_id_);
  worker_fetch_context->set_frame_request_blocker(frame_request_blocker_);
  worker_fetch_context->set_site_for_cookies(
      frame_->GetDocument().SiteForCookies());
  worker_fetch_context->set_top_frame_origin(
      frame_->GetDocument().TopFrameOrigin());

  for (auto& observer : observers_)
    observer.WillCreateWorkerFetchContext(worker_fetch_context.get());
  return worker_fetch_context;
}

std::unique_ptr<blink::WebPrescientNetworking>
RenderFrameImpl::CreatePrescientNetworking() {
  return GetContentClient()->renderer()->CreatePrescientNetworking(this);
}

blink::BlameContext* RenderFrameImpl::GetFrameBlameContext() {
  DCHECK(blame_context_);
  return blame_context_.get();
}

std::unique_ptr<blink::WebServiceWorkerProvider>
RenderFrameImpl::CreateServiceWorkerProvider() {
  // Bail-out if we are about to be navigated away.
  // We check that DocumentLoader is attached since:
  // - This serves as the signal since the DocumentLoader is detached in
  //   FrameLoader::PrepareForCommit().
  // - Creating ServiceWorkerProvider in
  //   RenderFrameImpl::CreateServiceWorkerProvider() assumes that there is a
  //   DocumentLoader attached to the frame.
  if (!frame_->GetDocumentLoader())
    return nullptr;

  // At this point we should have non-null data source.
  if (!ChildThreadImpl::current())
    return nullptr;  // May be null in some tests.
  ServiceWorkerNetworkProviderForFrame* provider =
      static_cast<ServiceWorkerNetworkProviderForFrame*>(
          frame_->GetDocumentLoader()->GetServiceWorkerNetworkProvider());
  if (!provider->context()) {
    // The context can be null when the frame is sandboxed.
    return nullptr;
  }
  return std::make_unique<WebServiceWorkerProviderImpl>(provider->context());
}

blink::AssociatedInterfaceProvider*
RenderFrameImpl::GetRemoteNavigationAssociatedInterfaces() {
  return GetRemoteAssociatedInterfaces();
}

blink::WebLocalFrame* RenderFrameImpl::CreateChildFrame(
    blink::WebLocalFrame* parent,
    blink::mojom::TreeScopeType scope,
    const blink::WebString& name,
    const blink::WebString& fallback_name,
    const blink::FramePolicy& frame_policy,
    const blink::WebFrameOwnerProperties& frame_owner_properties,
    blink::mojom::FrameOwnerElementType frame_owner_element_type) {
  DCHECK_EQ(frame_, parent);

  // Synchronously notify the browser of a child frame creation to get the
  // routing_id for the RenderFrame.
  FrameHostMsg_CreateChildFrame_Params params;
  params.parent_routing_id = routing_id_;
  params.scope = scope;
  params.frame_name = name.Utf8();

  FrameHostMsg_CreateChildFrame_Params_Reply params_reply;

  // The unique name generation logic was moved out of Blink, so for historical
  // reasons, unique name generation needs to take something called the
  // |fallback_name| into account. Normally, unique names are generated based on
  // the browing context name. For new frames, the initial browsing context name
  // comes from the name attribute of the browsing context container element.
  //
  // However, when the browsing context name is null, Blink instead uses the
  // "fallback name" to derive the unique name. The exact contents of the
  // "fallback name" are unspecified, but may contain the value of the
  // 'subresource attribute' of the browsing context container element.
  //
  // Note that Blink can't be changed to just pass |fallback_name| as |name| in
  // the case |name| is empty: |fallback_name| should never affect the actual
  // browsing context name, only unique name generation.
  params.is_created_by_script =
      v8::Isolate::GetCurrent() && v8::Isolate::GetCurrent()->InContext();
  params.frame_unique_name = unique_name_helper_.GenerateNameForNewChildFrame(
      params.frame_name.empty() ? fallback_name.Utf8() : params.frame_name,
      params.is_created_by_script);
  params.frame_policy = frame_policy;
  params.frame_owner_properties =
      *(blink::mojom::FrameOwnerProperties::From(frame_owner_properties).get());
  params.frame_owner_element_type = frame_owner_element_type;
  if (!Send(new FrameHostMsg_CreateChildFrame(params, &params_reply))) {
    // Allocation of routing id failed, so we can't create a child frame. This
    // can happen if the synchronous IPC message above has failed.  This can
    // legitimately happen when the browser process has already destroyed
    // RenderProcessHost, but the renderer process hasn't quit yet.
    return nullptr;
  }

  DCHECK(params_reply.new_interface_provider.is_valid());
  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      child_interface_provider(
          mojo::ScopedMessagePipeHandle(params_reply.new_interface_provider),
          0u);

  DCHECK(params_reply.browser_interface_broker_handle.is_valid());

  // This method is always called by local frames, never remote frames.

  // Tracing analysis uses this to find main frames when this value is
  // MSG_ROUTING_NONE, and build the frame tree otherwise.
  TRACE_EVENT2("navigation,rail", "RenderFrameImpl::createChildFrame", "id",
               routing_id_, "child", params_reply.child_routing_id);

  // Create the RenderFrame and WebLocalFrame, linking the two.
  RenderFrameImpl* child_render_frame = RenderFrameImpl::Create(
      agent_scheduling_group_, render_view_, params_reply.child_routing_id,
      std::move(child_interface_provider),
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>(
          mojo::ScopedMessagePipeHandle(
              params_reply.browser_interface_broker_handle),
          blink::mojom::BrowserInterfaceBroker::Version_),
      params_reply.devtools_frame_token);
  child_render_frame->unique_name_helper_.set_propagated_name(
      params.frame_unique_name);
  if (params.is_created_by_script)
    child_render_frame->unique_name_helper_.Freeze();
  child_render_frame->InitializeBlameContext(this);
  blink::WebLocalFrame* web_frame = parent->CreateLocalChild(
      scope, child_render_frame,
      child_render_frame->blink_interface_registry_.get(),
      params_reply.frame_token);

  child_render_frame->in_frame_tree_ = true;
  child_render_frame->Initialize(parent);

  return web_frame;
}

std::pair<blink::WebRemoteFrame*, blink::PortalToken>
RenderFrameImpl::CreatePortal(
    blink::CrossVariantMojoAssociatedReceiver<blink::mojom::PortalInterfaceBase>
        portal_endpoint,
    blink::CrossVariantMojoAssociatedRemote<
        blink::mojom::PortalClientInterfaceBase> client_endpoint,
    const blink::WebElement& portal_element) {
  int proxy_routing_id = MSG_ROUTING_NONE;
  FrameReplicationState initial_replicated_state;
  blink::PortalToken portal_token;
  base::UnguessableToken frame_token;
  base::UnguessableToken devtools_frame_token;
  GetFrameHost()->CreatePortal(std::move(portal_endpoint),
                               std::move(client_endpoint), &proxy_routing_id,
                               &initial_replicated_state, &portal_token,
                               &frame_token, &devtools_frame_token);
  RenderFrameProxy* proxy = RenderFrameProxy::CreateProxyForPortal(
      agent_scheduling_group_, this, proxy_routing_id, frame_token,
      devtools_frame_token, portal_element);
  proxy->SetReplicatedState(initial_replicated_state);
  return std::make_pair(proxy->web_frame(), portal_token);
}

blink::WebRemoteFrame* RenderFrameImpl::AdoptPortal(
    const blink::PortalToken& portal_token,
    const blink::WebElement& portal_element) {
  int proxy_routing_id = MSG_ROUTING_NONE;
  base::UnguessableToken frame_token;
  base::UnguessableToken devtools_frame_token;
  FrameReplicationState replicated_state;
  viz::FrameSinkId frame_sink_id;
  GetFrameHost()->AdoptPortal(portal_token, &proxy_routing_id, &frame_sink_id,
                              &replicated_state, &frame_token,
                              &devtools_frame_token);
  RenderFrameProxy* proxy = RenderFrameProxy::CreateProxyForPortal(
      agent_scheduling_group_, this, proxy_routing_id, frame_token,
      devtools_frame_token, portal_element);
  proxy->FrameSinkIdChanged(frame_sink_id);
  proxy->SetReplicatedState(replicated_state);
  return proxy->web_frame();
}

blink::WebFrame* RenderFrameImpl::FindFrame(const blink::WebString& name) {
  if (render_view_->renderer_wide_named_frame_lookup()) {
    for (const auto& it : g_routing_id_frame_map.Get()) {
      WebLocalFrame* frame = it.second->GetWebFrame();
      if (frame->AssignedName() == name)
        return frame;
    }
  }

  return GetContentClient()->renderer()->FindFrame(this->GetWebFrame(),
                                                   name.Utf8());
}

void RenderFrameImpl::WillDetach() {
  for (auto& observer : observers_)
    observer.WillDetach();

  // blink::WebAudioOutputIPCFactory::io_task_runner_ may be null in tests.
  auto& factory = blink::WebAudioOutputIPCFactory::GetInstance();
  if (factory.io_task_runner())
    factory.MaybeDeregisterRemoteFactory(GetWebFrame()->GetLocalFrameToken());

  // Send a state update before the frame is detached.
  SendUpdateState();
}

void RenderFrameImpl::FrameDetached() {
  // Close/delete the RenderWidget if this frame was a local root.
  if (render_widget_) {
    DCHECK(owned_render_widget_);
    render_widget_->CloseForFrame(std::move(owned_render_widget_));
  }

  // We need to clean up subframes by removing them from the map and deleting
  // the RenderFrameImpl.  In contrast, the main frame is owned by its
  // containing RenderViewHost (so that they have the same lifetime), so only
  // removal from the map is needed and no deletion.
  auto it = g_frame_map.Get().find(frame_);
  CHECK(it != g_frame_map.Get().end());
  CHECK_EQ(it->second, this);
  g_frame_map.Get().erase(it);

  // RenderAccessibilityManager keeps a reference to the RenderFrame that owns
  // it, so we need to clear the pointer to prevent invalid access after the
  // frame gets closed and deleted.
  render_accessibility_manager_.reset();

  // |frame_| may not be referenced after this, so clear the pointer since
  // the actual WebLocalFrame may not be deleted immediately and other methods
  // may try to access it.
  frame_->Close();
  frame_ = nullptr;

  // If this was a provisional frame with an associated frame to replace, tell
  // the previous_proxy it is no longer associated with this frame.
  if (previous_routing_id_ != MSG_ROUTING_NONE) {
    RenderFrameProxy* proxy =
        RenderFrameProxy::FromRoutingID(previous_routing_id_);

    // |proxy| should always exist. Detaching the proxy would've also
    // detached this provisional frame. The proxy should also not be
    // associated with another provisional frame at this point.
    //
    // RenderDocument: The |previous_routing_id_| represents a RenderFrame
    // instead of a RenderFrameProxy when a local<->local RenderFrame
    // navigation happens. In this case |proxy| is null.
    if (proxy) {
      CHECK_EQ(routing_id_, proxy->provisional_frame_routing_id());
      proxy->set_provisional_frame_routing_id(MSG_ROUTING_NONE);
    } else
      CHECK(ShouldCreateNewHostForSameSiteSubframe());
  }

  if (mhtml_body_loader_client_) {
    mhtml_body_loader_client_->Detach();
    mhtml_body_loader_client_.reset();
  }

  delete this;
  // Object is invalid after this point.
}

void RenderFrameImpl::DidChangeName(const blink::WebString& name) {
  if (current_history_item_.IsNull()) {
    // Once a navigation has committed, the unique name must no longer change to
    // avoid breaking back/forward navigations: https://crbug.com/607205
    unique_name_helper_.UpdateName(name.Utf8());
  }
  GetFrameHost()->DidChangeName(name.Utf8(), unique_name_helper_.value());
}

void RenderFrameImpl::DidSetFramePolicyHeaders(
    network::mojom::WebSandboxFlags flags,
    const blink::ParsedFeaturePolicy& fp_header,
    const blink::DocumentPolicyFeatureState& dp_header) {
  // If any of Feature Policy or Sandbox Flags or Document Policy are different
  // from the default (empty) values, then send them to the browser.
  if (!dp_header.empty() || !fp_header.empty() ||
      flags != network::mojom::WebSandboxFlags::kNone) {
    GetFrameHost()->DidSetFramePolicyHeaders(flags, fp_header, dp_header);
  }
}

void RenderFrameImpl::DidMatchCSS(
    const blink::WebVector<blink::WebString>& newly_matching_selectors,
    const blink::WebVector<blink::WebString>& stopped_matching_selectors) {
  for (auto& observer : observers_)
    observer.DidMatchCSS(newly_matching_selectors, stopped_matching_selectors);
}

bool RenderFrameImpl::ShouldReportDetailedMessageForSource(
    const blink::WebString& source) {
  return GetContentClient()->renderer()->ShouldReportDetailedMessageForSource(
      source.Utf16());
}

void RenderFrameImpl::DidAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  if (ShouldReportDetailedMessageForSource(source_name)) {
    for (auto& observer : observers_) {
      observer.DetailedConsoleMessageAdded(
          message.text.Utf16(), source_name.Utf16(), stack_trace.Utf16(),
          source_line, blink::ConsoleMessageLevelToLogSeverity(message.level));
    }
  }

  GetFrameHost()->DidAddMessageToConsole(message.level, message.text.Utf16(),
                                         static_cast<int32_t>(source_line),
                                         source_name.Utf16());
}

void RenderFrameImpl::WillSendSubmitEvent(const blink::WebFormElement& form) {
  for (auto& observer : observers_)
    observer.WillSendSubmitEvent(form);
}

void RenderFrameImpl::DidCreateDocumentLoader(
    blink::WebDocumentLoader* document_loader) {
  DocumentState* document_state =
      DocumentState::FromDocumentLoader(document_loader);
  if (!document_state) {
    // This must be an initial empty document.
    document_loader->SetExtraData(BuildDocumentState());
    document_loader->SetServiceWorkerNetworkProvider(
        ServiceWorkerNetworkProviderForFrame::CreateInvalidInstance());
  }
}

void RenderFrameImpl::DidCommitNavigation(
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type,
    bool should_reset_browser_interface_broker) {
  CHECK_EQ(NavigationCommitState::kWillCommit, navigation_commit_state_);
  navigation_commit_state_ = NavigationCommitState::kDidCommit;

  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentLoader(document_loader);
  NavigationState* navigation_state = internal_data->navigation_state();
  DCHECK(!navigation_state->WasWithinSameDocument());

  TRACE_EVENT2("navigation,benchmark,rail",
               "RenderFrameImpl::didStartProvisionalLoad", "id", routing_id_,
               "url", document_loader->GetUrl().GetString().Utf8());

  // Install factories as early as possible - it needs to happen before the
  // newly committed document starts any subresource fetches.  In particular,
  // this needs to happen before invoking
  // RenderFrameObserver::ReadyToCommitNavigation below.
  if (pending_loader_factories_) {
    // Commits triggered by the browser process should always provide
    // |pending_loader_factories_|.
    loader_factories_ = std::move(pending_loader_factories_);
  } else if (!loader_factories_) {
    // When committing an initial empty document synchronously (e,g, in response
    // to |window.open('', '_blank')|) we won't get |pending_loader_factories_|
    // from the browser.  In such cases we expect to always have a local parent
    // or opener - we should eagerly inherit the factories from them (so that
    // even if the opener gets closed, our factories will be correctly
    // initialized).
    loader_factories_ = GetLoaderFactoryBundleFromCreator();
  }
  DCHECK(loader_factories_);
  DCHECK(loader_factories_->HasBoundDefaultFactory());

  // TODO(dgozman): call DidStartNavigation in various places where we call
  // CommitNavigation() on the frame.
  if (!navigation_state->was_initiated_in_this_frame()) {
    // Navigation initiated in this frame has been already reported in
    // BeginNavigation.
    for (auto& observer : observers_)
      observer.DidStartNavigation(document_loader->GetUrl(), base::nullopt);
  }

  for (auto& observer : observers_)
    observer.ReadyToCommitNavigation(document_loader);

  for (auto& observer : observers_)
    observer.DidCreateNewDocument();

  DVLOG(1) << "Committed provisional load: "
           << TrimURL(GetLoadingUrl().possibly_invalid_spec());
  TRACE_EVENT2("navigation,rail", "RenderFrameImpl::didCommitProvisionalLoad",
               "id", routing_id_, "url",
               GetLoadingUrl().possibly_invalid_spec());

  bool is_provisional_frame = previous_routing_id_ != MSG_ROUTING_NONE;
  if (is_provisional_frame) {
    // If this is a provisional frame associated with a proxy (i.e., a frame
    // created for a remote-to-local navigation), swap it into the frame tree
    // now.
    if (!SwapInInternal())
      return;
  }

  // Generate a new embedding token on each document change.
  GetWebFrame()->SetEmbeddingToken(base::UnguessableToken::Create());

  // Navigations that change the document represent a new content source.  Keep
  // track of that on the widget to help the browser process detect when stale
  // compositor frames are being shown after a commit.
  if (is_main_frame_) {
    GetLocalRootRenderWidget()->DidNavigate(
        frame_->GetDocument().GetUkmSourceId(), GetLoadingUrl());
  }

  mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
      remote_interface_provider_receiver;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker_receiver;

  // blink passes true when the new pipe needs to be bound.
  if (should_reset_browser_interface_broker) {
    // If we're navigating to a new document, bind |remote_interfaces_| to a new
    // message pipe. The receiver end of the new InterfaceProvider interface
    // will be sent over as part of DidCommitProvisionalLoad. After the RFHI
    // receives the commit confirmation, it will immediately close the old
    // message pipe to avoid GetInterface calls racing with navigation commit,
    // and bind the receiver end of the message pipe created here.
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        interfaces_provider;
    remote_interface_provider_receiver =
        interfaces_provider.InitWithNewPipeAndPassReceiver();

    // Must initialize |remote_interfaces_| with a new working pipe *before*
    // observers receive DidCommitProvisionalLoad, so they can already request
    // remote interfaces. The interface requests will be serviced once the
    // InterfaceProvider interface request is bound by the RenderFrameHostImpl.
    remote_interfaces_.Close();
    remote_interfaces_.Bind(std::move(interfaces_provider));

    // If we're navigating to a new document, bind
    // |browser_interface_broker_proxy_| to a new browser interface broker. The
    // request end of the new BrowserInterfaceBroker interface will be sent over
    // as part of DidCommitProvisionalLoad. After the RFHI receives the commit
    // confirmation, it will immediately close the old message pipe to avoid
    // GetInterface() calls racing with navigation commit, and bind the request
    // end of the message pipe created here. Must initialize
    // |browser_interface_broker_proxy_| with a new working pipe *before*
    // observers receive DidCommitProvisionalLoad, so they can already request
    // remote interfaces. The interface requests will be serviced once the
    // BrowserInterfaceBroker interface request is bound by the
    // RenderFrameHostImpl.
    browser_interface_broker_receiver = browser_interface_broker_proxy_.Reset();

    // blink::WebAudioOutputIPCFactory::io_task_runner_ may be null in tests.
    auto& factory = blink::WebAudioOutputIPCFactory::GetInstance();
    if (factory.io_task_runner()) {
      // The RendererAudioOutputStreamFactory must be readily accessible on the
      // IO thread when it's needed, because the main thread may block while
      // waiting for the factory call to finish on the IO thread, so if we tried
      // to lazily initialize it, we could deadlock.
      //
      // TODO(https://crbug.com/668275): Still, it is odd for one specific
      // factory to be registered here, make this a RenderFrameObserver.
      // code.
      factory.MaybeDeregisterRemoteFactory(GetWebFrame()->GetLocalFrameToken());
      factory.RegisterRemoteFactory(GetWebFrame()->GetLocalFrameToken(),
                                    GetBrowserInterfaceBroker());
    }

    // If the request for |audio_input_stream_factory_| is in flight when
    // |remote_interfaces_| is reset, it will be silently dropped. We reset
    // |audio_input_stream_factory_| to force a new mojo request to be sent
    // the next time it's used. See https://crbug.com/795258 for implementing a
    // nicer solution.
    audio_input_stream_factory_.reset();
  }

  // Notify the MediaPermissionDispatcher that its connection will be closed
  // due to a navigation to a different document.
  if (media_permission_dispatcher_)
    media_permission_dispatcher_->OnNavigation();

  ui::PageTransition transition =
      GetTransitionType(frame_->GetDocumentLoader(), IsMainFrame());

  DidCommitNavigationInternal(
      item, commit_type, false /* was_within_same_document */, transition,
      should_reset_browser_interface_broker
          ? mojom::DidCommitProvisionalLoadInterfaceParams::New(
                std::move(remote_interface_provider_receiver),
                std::move(browser_interface_broker_receiver))
          : nullptr,
      GetWebFrame()->GetEmbeddingToken());

  // If we end up reusing this WebRequest (for example, due to a #ref click),
  // we don't want the transition type to persist.  Just clear it.
  navigation_state->set_transition_type(ui::PAGE_TRANSITION_LINK);

  // Check whether we have new encoding name.
  UpdateEncoding(frame_, frame_->View()->PageEncoding().Utf8());

  NotifyObserversOfNavigationCommit(transition);
}

void RenderFrameImpl::DidCreateInitialEmptyDocument() {
  for (auto& observer : observers_)
    observer.DidCreateNewDocument();
}

void RenderFrameImpl::DidCommitDocumentReplacementNavigation(
    blink::WebDocumentLoader* document_loader) {
  // TODO(https://crbug.com/855189): figure out which of the following observer
  // calls are necessary, if any.
  for (auto& observer : observers_)
    observer.DidStartNavigation(document_loader->GetUrl(), base::nullopt);
  for (auto& observer : observers_)
    observer.ReadyToCommitNavigation(document_loader);
  for (auto& observer : observers_)
    observer.DidCreateNewDocument();
  ui::PageTransition transition =
      GetTransitionType(document_loader, IsMainFrame());
  NotifyObserversOfNavigationCommit(transition);
}

void RenderFrameImpl::DidClearWindowObject() {
  if (enabled_bindings_ & BINDINGS_POLICY_WEB_UI)
    WebUIExtension::Install(frame_);

  if (enabled_bindings_ & BINDINGS_POLICY_DOM_AUTOMATION)
    DomAutomationController::Install(this, frame_);

  if (enabled_bindings_ & BINDINGS_POLICY_STATS_COLLECTION)
    StatsCollectionController::Install(frame_);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(cc::switches::kEnableGpuBenchmarking))
    GpuBenchmarking::Install(weak_factory_.GetWeakPtr());

  if (command_line.HasSwitch(switches::kEnableSkiaBenchmarking))
    SkiaBenchmarking::Install(frame_);

  for (auto& observer : observers_)
    observer.DidClearWindowObject();
}

void RenderFrameImpl::DidCreateDocumentElement() {
  for (auto& observer : observers_)
    observer.DidCreateDocumentElement();
}

void RenderFrameImpl::RunScriptsAtDocumentElementAvailable() {
  GetContentClient()->renderer()->RunScriptsAtDocumentStart(this);
  // Do not use |this|! ContentClient might have deleted them by now!
}

void RenderFrameImpl::DidReceiveTitle(const blink::WebString& title) {
  // Ignore all but top level navigations.
  if (!frame_->Parent()) {
    base::trace_event::TraceLog::GetInstance()->UpdateProcessLabel(
        routing_id_, title.Utf8());
  } else {
    // Set process title for sub-frames in traces.
    GURL loading_url = GetLoadingUrl();
    if (!loading_url.host().empty() &&
        loading_url.scheme() != url::kFileScheme) {
      std::string subframe_title = "Subframe: " + loading_url.scheme() +
                                   url::kStandardSchemeSeparator +
                                   loading_url.host();
      base::trace_event::TraceLog::GetInstance()->UpdateProcessLabel(
          routing_id_, subframe_title);
    }
  }

  // Also check whether we have new encoding name.
  UpdateEncoding(frame_, frame_->View()->PageEncoding().Utf8());
}

void RenderFrameImpl::DidFinishDocumentLoad() {
  TRACE_EVENT1("navigation,benchmark,rail",
               "RenderFrameImpl::didFinishDocumentLoad", "id", routing_id_);
  for (auto& observer : observers_)
    observer.DidFinishDocumentLoad();

  // Check whether we have new encoding name.
  UpdateEncoding(frame_, frame_->View()->PageEncoding().Utf8());
}

void RenderFrameImpl::RunScriptsAtDocumentReady(bool document_is_empty) {
  base::WeakPtr<RenderFrameImpl> weak_self = weak_factory_.GetWeakPtr();

  GetContentClient()->renderer()->RunScriptsAtDocumentEnd(this);

  // ContentClient might have deleted |frame_| and |this| by now!
  if (!weak_self.get())
    return;

  // At this point, the entire data stream for the document is loaded. The
  // below logic displays an error page to the user if:
  // - this is a main frame navigation,
  // - the stream contained no data,
  // - and the HTTP status code indicates an error.
  if (!document_is_empty || !IsMainFrame())
    return;

  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();
  int http_status_code = document_loader->GetResponse().HttpStatusCode();
  if (!GetContentClient()->renderer()->HasErrorPage(http_status_code))
    return;

  // Rather than display an unexplainable blank page to the user, commit a
  // custom error page with some basic information.
  WebURL unreachable_url = frame_->GetDocument().Url();
  std::string error_html;
  GetContentClient()->renderer()->PrepareErrorPageForHttpStatusError(
      this, unreachable_url, document_loader->HttpMethod().Ascii(),
      http_status_code, &error_html);
  // Make sure we never show errors in view source mode.
  frame_->EnableViewSourceMode(false);

  auto navigation_params = WebNavigationParams::CreateForErrorPage(
      document_loader, error_html, GURL(kUnreachableWebDataURL),
      unreachable_url, net::ERR_FAILED);
  navigation_params->frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
  navigation_params->service_worker_network_provider =
      ServiceWorkerNetworkProviderForFrame::CreateInvalidInstance();

  // TODO(dcheng): Remove this strange case. Typically, loading finishes
  // asynchronously, so this will not be called while `CommitNavigation()`
  // is on the stack. However, completion for media files is synchronously
  // signalled in `blink::DocumentLoader::StartLoadingResponse()`. To prevent
  // the CHECK in `AssertNavigationCommits` from tripping, temporarily reset the
  // state and restore it after the reentrant `CommitNavigation()` completes.
  bool reentrantly_committing =
      (NavigationCommitState::kNone != navigation_commit_state_);
  if (reentrantly_committing) {
    CHECK_EQ(NavigationCommitState::kDidCommit, navigation_commit_state_);
    navigation_commit_state_ = NavigationCommitState::kNone;
  }
  {
    AssertNavigationCommits assert_navigation_commits(this);
    frame_->CommitNavigation(std::move(navigation_params),
                             BuildDocumentState());
  }
  // WARNING: The previous call may have have deleted |this|.
  // Do not use |this| or |frame_| here without checking |weak_self|.
  if (weak_self && reentrantly_committing)
    navigation_commit_state_ = NavigationCommitState::kDidCommit;
}

void RenderFrameImpl::RunScriptsAtDocumentIdle() {
  GetContentClient()->renderer()->RunScriptsAtDocumentIdle(this);
  // ContentClient might have deleted |this| by now!
}

void RenderFrameImpl::DidHandleOnloadEvents() {
  for (auto& observer : observers_)
    observer.DidHandleOnloadEvents();
}

void RenderFrameImpl::DidFinishLoad() {
  TRACE_EVENT1("navigation,benchmark,rail", "RenderFrameImpl::didFinishLoad",
               "id", routing_id_);
  if (!frame_->Parent()) {
    TRACE_EVENT_INSTANT0("WebCore,benchmark,rail", "LoadFinished",
                         TRACE_EVENT_SCOPE_PROCESS);
  }

  for (auto& observer : observers_)
    observer.DidFinishLoad();

  if (!RenderThreadImpl::current())
    return;
  RenderThreadImpl::RendererMemoryMetrics memory_metrics;
  if (!RenderThreadImpl::current()->GetRendererMemoryMetrics(&memory_metrics))
    return;
  RecordSuffixedRendererMemoryMetrics(memory_metrics, ".DidFinishLoad");
  if (!IsMainFrame())
    return;
  RecordSuffixedRendererMemoryMetrics(memory_metrics,
                                      ".MainFrameDidFinishLoad");
}

void RenderFrameImpl::DidFinishSameDocumentNavigation(
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type,
    bool content_initiated) {
  TRACE_EVENT1("navigation,rail",
               "RenderFrameImpl::didFinishSameDocumentNavigation", "id",
               routing_id_);
  InternalDocumentStateData* data =
      InternalDocumentStateData::FromDocumentLoader(
          frame_->GetDocumentLoader());
  if (content_initiated)
    data->set_navigation_state(NavigationState::CreateContentInitiated());
  data->navigation_state()->set_was_within_same_document(true);

  ui::PageTransition transition =
      GetTransitionType(frame_->GetDocumentLoader(), IsMainFrame());
  DidCommitNavigationInternal(item, commit_type,
                              // was_within_same_document
                              true, transition,
                              // interface_params
                              nullptr,
                              /*embedding_token=*/base::nullopt);

  // If we end up reusing this WebRequest (for example, due to a #ref click),
  // we don't want the transition type to persist.  Just clear it.
  data->navigation_state()->set_transition_type(ui::PAGE_TRANSITION_LINK);

  for (auto& observer : observers_)
    observer.DidFinishSameDocumentNavigation();
}

void RenderFrameImpl::DidUpdateCurrentHistoryItem() {
  render_view_->StartNavStateSyncTimerIfNecessary(this);
}

base::UnguessableToken RenderFrameImpl::GetDevToolsFrameToken() {
  return devtools_frame_token_;
}

void RenderFrameImpl::AbortClientNavigation() {
  CHECK(in_frame_tree_);
  browser_side_navigation_pending_ = false;
  if (mhtml_body_loader_client_) {
    mhtml_body_loader_client_->Detach();
    mhtml_body_loader_client_.reset();
  }
  NotifyObserversOfFailedProvisionalLoad();
  navigation_client_impl_.reset();
}

void RenderFrameImpl::DidChangeSelection(bool is_empty_selection) {
  if (!GetLocalRootRenderWidget()->GetWebWidget()->HandlingInputEvent() &&
      !GetLocalRootRenderWidget()->GetFrameWidget()->HandlingSelectRange())
    return;

  if (is_empty_selection)
    selection_text_.clear();

  // UpdateTextInputState should be called before SyncSelectionIfRequired.
  // UpdateTextInputState may send TextInputStateChanged to notify the focus
  // was changed, and SyncSelectionIfRequired may send SelectionChanged
  // to notify the selection was changed.  Focus change should be notified
  // before selection change.
  GetLocalRootRenderWidget()->UpdateTextInputState();
  SyncSelectionIfRequired();
}

void RenderFrameImpl::ShowContextMenu(
    const blink::WebContextMenuData& data,
    const base::Optional<gfx::Point>& host_context_menu_location) {
  UntrustworthyContextMenuParams params = ContextMenuParamsBuilder::Build(data);
  if (host_context_menu_location.has_value()) {
    // If the context menu request came from the browser, it came with a
    // position that was stored on blink::WebFrameWidgetBase and is relative to
    // the WindowScreenRect.
    params.x = host_context_menu_location.value().x();
    params.y = host_context_menu_location.value().y();
  } else {
    // If the context menu request came from the renderer, the position in
    // |params| is real, but they come in blink viewport coordiates, which
    // include the device scale factor, but not emulation scale. Here we convert
    // them to DIP coordiates relative to the WindowScreenRect.
    blink::WebRect position_in_window(params.x, params.y, 0, 0);
    GetLocalRootRenderWidget()->ConvertViewportToWindow(&position_in_window);
    const float scale =
        GetLocalRootRenderWidget()->GetWebWidget()->GetEmulatorScale();
    params.x = position_in_window.x * scale;
    params.y = position_in_window.y * scale;
  }

  // Serializing a GURL longer than kMaxURLChars will fail, so don't do
  // it.  We replace it with an empty GURL so the appropriate items are disabled
  // in the context menu.
  // TODO(jcivelli): http://crbug.com/45160 This prevents us from saving large
  //                 data encoded images.  We should have a way to save them.
  if (params.src_url.spec().size() > url::kMaxURLChars)
    params.src_url = GURL();

  blink::WebRect selection_in_window(data.selection_rect);
  GetLocalRootRenderWidget()->ConvertViewportToWindow(&selection_in_window);
  params.selection_rect = selection_in_window;

#if defined(OS_ANDROID)
  // The Samsung Email app relies on the context menu being shown after the
  // javascript onselectionchanged is triggered.
  // See crbug.com/729488
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&RenderFrameImpl::ShowDeferredContextMenu,
                                weak_factory_.GetWeakPtr(), params));
#else
  ShowDeferredContextMenu(params);
#endif
}

void RenderFrameImpl::ShowDeferredContextMenu(
    const UntrustworthyContextMenuParams& params) {
  Send(new FrameHostMsg_ContextMenu(routing_id_, params));
}

void RenderFrameImpl::FrameRectsChanged(const blink::WebRect& frame_rect) {
  // To limit the number of IPCs, only notify the browser when the rect's size
  // changes, not when the position changes. The size needs to be replicated if
  // the iframe goes out-of-process.
  gfx::Size frame_size(frame_rect.width, frame_rect.height);
  if (!frame_size_ || *frame_size_ != frame_size) {
    frame_size_ = frame_size;
    GetFrameHost()->FrameSizeChanged(frame_size);
  }
}

void RenderFrameImpl::OnMainFrameIntersectionChanged(
    const blink::WebRect& mainframe_intersection_rect) {
  if (!mainframe_intersection_rect_ ||
      mainframe_intersection_rect != mainframe_intersection_rect_) {
    mainframe_intersection_rect_ = mainframe_intersection_rect;
    for (auto& observer : observers_) {
      observer.OnMainFrameIntersectionChanged(mainframe_intersection_rect);
    }
  }
}

void RenderFrameImpl::WillSendRequest(blink::WebURLRequest& request,
                                      ForRedirect for_redirect) {
  // This method is called for subresources, while transition type is
  // a navigation concept. We pass ui::PAGE_TRANSITION_LINK as default one.
  WillSendRequestInternal(request, /*for_main_frame=*/false,
                          ui::PAGE_TRANSITION_LINK, for_redirect);
}

void RenderFrameImpl::WillSendRequestInternal(
    blink::WebURLRequest& request,
    bool for_main_frame,
    ui::PageTransition transition_type,
    ForRedirect for_redirect) {
  if (render_view_->renderer_preferences_.enable_do_not_track) {
    request.SetHttpHeaderField(blink::WebString::FromUTF8(kDoNotTrackHeader),
                               "1");
  }

  ApplyFilePathAlias(&request);
  GURL new_url;
  bool force_ignore_site_for_cookies = false;
  base::Optional<url::Origin> initiator_origin =
      request.RequestorOrigin().IsNull()
          ? base::Optional<url::Origin>()
          : base::Optional<url::Origin>(request.RequestorOrigin());
  GetContentClient()->renderer()->WillSendRequest(
      frame_, transition_type, request.Url(), request.SiteForCookies(),
      base::OptionalOrNullptr(initiator_origin), &new_url,
      &force_ignore_site_for_cookies);
  if (!new_url.is_empty())
    request.SetUrl(WebURL(new_url));

  // The request's extra data may indicate that we should set a custom user
  // agent. This needs to be done here, after WebKit is through with setting the
  // user agent on its own.
  WebString custom_user_agent;
  if (request.GetExtraData()) {
    RequestExtraData* old_extra_data =
        static_cast<RequestExtraData*>(request.GetExtraData().get());

    custom_user_agent = old_extra_data->custom_user_agent();
    if (!custom_user_agent.IsNull()) {
      if (custom_user_agent.IsEmpty())
        request.ClearHttpHeaderField("User-Agent");
      else
        request.SetHttpHeaderField("User-Agent", custom_user_agent);
    }
  }

  WebDocument frame_document = frame_->GetDocument();
  if (!request.GetExtraData())
    request.SetExtraData(base::MakeRefCounted<RequestExtraData>());
  auto* extra_data =
      static_cast<RequestExtraData*>(request.GetExtraData().get());
  extra_data->set_custom_user_agent(custom_user_agent);
  extra_data->set_render_frame_id(routing_id_);
  extra_data->set_is_main_frame(IsMainFrame());
  extra_data->set_transition_type(transition_type);
  bool is_for_no_state_prefetch =
      GetContentClient()->renderer()->IsPrefetchOnly(this, request);
  extra_data->set_is_for_no_state_prefetch(is_for_no_state_prefetch);
  extra_data->set_force_ignore_site_for_cookies(force_ignore_site_for_cookies);
  extra_data->set_frame_request_blocker(frame_request_blocker_);
  extra_data->set_allow_cross_origin_auth_prompt(
      render_view_->renderer_preferences().allow_cross_origin_auth_prompt);

  request.SetDownloadToNetworkCacheOnly(is_for_no_state_prefetch &&
                                        !for_main_frame);

  // The RenderThreadImpl or its URLLoaderThrottleProvider member may not be
  // valid in some tests.
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (!for_redirect && render_thread &&
      render_thread->url_loader_throttle_provider()) {
    extra_data->set_url_loader_throttles(
        render_thread->url_loader_throttle_provider()->CreateThrottles(
            routing_id_, request));
  }

  // This is an instance where we embed a copy of the routing id
  // into the data portion of the message. This can cause problems if we
  // don't register this id on the browser side, since the download manager
  // expects to find a RenderViewHost based off the id.
  request.SetRequestorID(render_view_->GetRoutingID());
  request.SetHasUserGesture(frame_->HasTransientUserActivation());

  if (!render_view_->renderer_preferences_.enable_referrers) {
    request.SetReferrerString(WebString());
    request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  }
}

void RenderFrameImpl::DidLoadResourceFromMemoryCache(
    const blink::WebURLRequest& request,
    const blink::WebURLResponse& response) {
  for (auto& observer : observers_) {
    observer.DidLoadResourceFromMemoryCache(
        request.Url(), response.RequestId(), response.EncodedBodyLength(),
        response.MimeType().Utf8(), response.FromArchive());
  }
}

void RenderFrameImpl::DidStartResponse(
    const GURL& response_url,
    int request_id,
    network::mojom::URLResponseHeadPtr response_head,
    network::mojom::RequestDestination request_destination,
    blink::PreviewsState previews_state) {
  for (auto& observer : observers_) {
    observer.DidStartResponse(response_url, request_id, *response_head,
                              request_destination, previews_state);
  }
}

void RenderFrameImpl::DidCompleteResponse(
    int request_id,
    const network::URLLoaderCompletionStatus& status) {
  for (auto& observer : observers_)
    observer.DidCompleteResponse(request_id, status);
}

void RenderFrameImpl::DidCancelResponse(int request_id) {
  for (auto& observer : observers_)
    observer.DidCancelResponse(request_id);
}

void RenderFrameImpl::DidReceiveTransferSizeUpdate(int resource_id,
                                                   int received_data_length) {
  for (auto& observer : observers_) {
    observer.DidReceiveTransferSizeUpdate(resource_id, received_data_length);
  }
}

void RenderFrameImpl::DidChangePerformanceTiming() {
  for (auto& observer : observers_)
    observer.DidChangePerformanceTiming();
}

void RenderFrameImpl::DidObserveInputDelay(base::TimeDelta input_delay) {
  for (auto& observer : observers_)
    observer.DidObserveInputDelay(input_delay);
}

void RenderFrameImpl::DidChangeCpuTiming(base::TimeDelta time) {
  for (auto& observer : observers_)
    observer.DidChangeCpuTiming(time);
}

void RenderFrameImpl::DidObserveLoadingBehavior(
    blink::LoadingBehaviorFlag behavior) {
  for (auto& observer : observers_)
    observer.DidObserveLoadingBehavior(behavior);
}

void RenderFrameImpl::DidObserveNewFeatureUsage(
    blink::mojom::WebFeature feature) {
  for (auto& observer : observers_)
    observer.DidObserveNewFeatureUsage(feature);
}

void RenderFrameImpl::DidObserveNewCssPropertyUsage(
    blink::mojom::CSSSampleId css_property,
    bool is_animated) {
  for (auto& observer : observers_)
    observer.DidObserveNewCssPropertyUsage(css_property, is_animated);
}

void RenderFrameImpl::DidObserveLayoutShift(double score,
                                            bool after_input_or_scroll) {
  for (auto& observer : observers_)
    observer.DidObserveLayoutShift(score, after_input_or_scroll);
}

void RenderFrameImpl::DidObserveLayoutNg(uint32_t all_block_count,
                                         uint32_t ng_block_count,
                                         uint32_t all_call_count,
                                         uint32_t ng_call_count) {
  for (auto& observer : observers_)
    observer.DidObserveLayoutNg(all_block_count, ng_block_count, all_call_count,
                                ng_call_count);
}

void RenderFrameImpl::DidObserveLazyLoadBehavior(
    WebLocalFrameClient::LazyLoadBehavior lazy_load_behavior) {
  for (auto& observer : observers_)
    observer.DidObserveLazyLoadBehavior(lazy_load_behavior);
}

bool RenderFrameImpl::ShouldTrackUseCounter(const blink::WebURL& url) {
  return GetContentClient()->renderer()->ShouldTrackUseCounter(url);
}

void RenderFrameImpl::DidCreateScriptContext(v8::Local<v8::Context> context,
                                             int world_id) {
  if (((enabled_bindings_ & BINDINGS_POLICY_MOJO_WEB_UI) ||
       enable_mojo_js_bindings_) &&
      IsMainFrame() && world_id == ISOLATED_WORLD_ID_GLOBAL) {
    // We only allow these bindings to be installed when creating the main
    // world context of the main frame.
    blink::WebContextFeatures::EnableMojoJS(context, true);
  }

  for (auto& observer : observers_)
    observer.DidCreateScriptContext(context, world_id);
}

void RenderFrameImpl::WillReleaseScriptContext(v8::Local<v8::Context> context,
                                               int world_id) {
  for (auto& observer : observers_)
    observer.WillReleaseScriptContext(context, world_id);
}

void RenderFrameImpl::DidChangeScrollOffset() {
  render_view_->StartNavStateSyncTimerIfNecessary(this);

  for (auto& observer : observers_)
    observer.DidChangeScrollOffset();
}

blink::WebMediaStreamDeviceObserver*
RenderFrameImpl::MediaStreamDeviceObserver() {
  if (!web_media_stream_device_observer_)
    InitializeMediaStreamDeviceObserver();
  return web_media_stream_device_observer_.get();
}

bool RenderFrameImpl::AllowRTCLegacyTLSProtocols() {
  return GetRendererPreferences().webrtc_allow_legacy_tls_protocols;
}

blink::WebEncryptedMediaClient* RenderFrameImpl::EncryptedMediaClient() {
  return media_factory_.EncryptedMediaClient();
}

blink::WebString RenderFrameImpl::UserAgentOverride() {
  if (ShouldUseUserAgentOverride()) {
    return WebString::FromUTF8(render_view_->renderer_preferences_
                                   .user_agent_override.ua_string_override);
  }
  return blink::WebString();
}

base::Optional<blink::UserAgentMetadata>
RenderFrameImpl::UserAgentMetadataOverride() {
  if (ShouldUseUserAgentOverride()) {
    return render_view_->renderer_preferences_.user_agent_override
        .ua_metadata_override;
  }
  return base::nullopt;
}

bool RenderFrameImpl::ShouldUseUserAgentOverride() const {
  if (!render_view_->GetWebView() || !render_view_->GetWebView()->MainFrame() ||
      render_view_->renderer_preferences_.user_agent_override.ua_string_override
          .empty()) {
    return false;
  }

  // TODO(nasko): When the top-level frame is remote, there is no
  // WebDocumentLoader associated with it, so the checks below are not valid.
  // Temporarily return early and fix properly as part of
  // https://crbug.com/426555.
  if (render_view_->GetWebView()->MainFrame()->IsWebRemoteFrame())
    return false;
  WebLocalFrame* main_frame =
      render_view_->GetWebView()->MainFrame()->ToWebLocalFrame();

  WebDocumentLoader* document_loader = main_frame->GetDocumentLoader();
  InternalDocumentStateData* internal_data =
      document_loader
          ? InternalDocumentStateData::FromDocumentLoader(document_loader)
          : nullptr;
  return internal_data && internal_data->is_overriding_user_agent();
}

blink::WebString RenderFrameImpl::DoNotTrackValue() {
  if (render_view_->renderer_preferences_.enable_do_not_track)
    return WebString::FromUTF8("1");
  return WebString();
}

blink::mojom::RendererAudioInputStreamFactory*
RenderFrameImpl::GetAudioInputStreamFactory() {
  if (!audio_input_stream_factory_)
    GetBrowserInterfaceBroker()->GetInterface(
        audio_input_stream_factory_.BindNewPipeAndPassReceiver());
  return audio_input_stream_factory_.get();
}

bool RenderFrameImpl::AllowContentInitiatedDataUrlNavigations(
    const blink::WebURL& url) {
  // Error pages can navigate to data URLs.
  return url.GetString() == kUnreachableWebDataURL;
}

void RenderFrameImpl::PostAccessibilityEvent(const ui::AXEvent& event) {
  if (!IsAccessibilityEnabled())
    return;

  render_accessibility_manager_->GetRenderAccessibilityImpl()
      ->HandleWebAccessibilityEvent(event);
}

void RenderFrameImpl::MarkWebAXObjectDirty(const blink::WebAXObject& obj,
                                           bool subtree) {
  if (!IsAccessibilityEnabled())
    return;

  render_accessibility_manager_->GetRenderAccessibilityImpl()
      ->MarkWebAXObjectDirty(obj, subtree);
}

void RenderFrameImpl::AddObserver(RenderFrameObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderFrameImpl::RemoveObserver(RenderFrameObserver* observer) {
  observer->RenderFrameGone();
  observers_.RemoveObserver(observer);
}

void RenderFrameImpl::OnDroppedNavigation() {
  browser_side_navigation_pending_ = false;
  browser_side_navigation_pending_url_ = GURL();
  frame_->DidDropNavigation();
}

void RenderFrameImpl::WasHidden() {
  frame_->WasHidden();
  for (auto& observer : observers_)
    observer.WasHidden();

#if BUILDFLAG(ENABLE_PLUGINS)
  for (auto* plugin : active_pepper_instances_)
    plugin->PageVisibilityChanged(false);
#endif  // ENABLE_PLUGINS
}

void RenderFrameImpl::WasShown() {
  frame_->WasShown();
  for (auto& observer : observers_)
    observer.WasShown();

#if BUILDFLAG(ENABLE_PLUGINS)
  for (auto* plugin : active_pepper_instances_)
    plugin->PageVisibilityChanged(true);
#endif  // ENABLE_PLUGINS
}

bool RenderFrameImpl::IsMainFrame() {
  return is_main_frame_;
}

bool RenderFrameImpl::IsHidden() {
  return GetLocalRootRenderWidget()->GetWebWidget()->IsHidden();
}

bool RenderFrameImpl::IsLocalRoot() const {
  bool is_local_root = static_cast<bool>(render_widget_);
  DCHECK_EQ(is_local_root,
            !(frame_->Parent() && frame_->Parent()->IsWebLocalFrame()));
  return is_local_root;
}

const RenderFrameImpl* RenderFrameImpl::GetLocalRoot() const {
  return IsLocalRoot() ? this
                       : RenderFrameImpl::FromWebFrame(frame_->LocalRoot());
}

std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
RenderFrameImpl::MakeDidCommitProvisionalLoadParams(
    blink::WebHistoryCommitType commit_type,
    ui::PageTransition transition,
    const base::Optional<base::UnguessableToken>& embedding_token) {
  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();
  const WebURLResponse& response = document_loader->GetResponse();

  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentLoader(
          frame_->GetDocumentLoader());
  NavigationState* navigation_state = internal_data->navigation_state();

  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params =
      std::make_unique<FrameHostMsg_DidCommitProvisionalLoad_Params>();
  params->http_status_code = response.HttpStatusCode();
  params->url_is_unreachable = document_loader->HasUnreachableURL();
  params->method = "GET";
  params->intended_as_new_entry =
      navigation_state->commit_params().intended_as_new_entry;
  params->should_replace_current_entry =
      document_loader->ReplacesCurrentHistoryItem();
  params->post_id = -1;
  params->nav_entry_id = navigation_state->commit_params().nav_entry_id;
  params->embedding_token = embedding_token;

  // Pass the navigation token back to the browser process, or generate a new
  // one if this navigation is committing without the browser process asking for
  // it.
  // TODO(clamy): We should add checks on navigations that commit without having
  // been asked to commit by the browser process.
  params->navigation_token = navigation_state->commit_params().navigation_token;
  if (params->navigation_token.is_empty())
    params->navigation_token = base::UnguessableToken::Create();

  // "Standard" commits from Blink create new NavigationEntries. We also treat
  // main frame "inert" commits as creating new NavigationEntries if they
  // replace the current entry on a cross-document navigation (e.g., client
  // redirects, location.replace, navigation to same URL), since this will
  // replace all the subframes and could go cross-origin. We don't want to rely
  // on updating the existing NavigationEntry in this case, since it could leave
  // stale state around.
  params->did_create_new_entry =
      (commit_type == blink::kWebStandardCommit) ||
      (commit_type == blink::kWebHistoryInertCommit && !frame_->Parent() &&
       params->should_replace_current_entry &&
       !navigation_state->WasWithinSameDocument());

  WebDocument frame_document = frame_->GetDocument();
  // Set the origin of the frame.  This will be replicated to the corresponding
  // RenderFrameProxies in other processes.
  WebSecurityOrigin frame_origin = frame_document.GetSecurityOrigin();
  params->origin = frame_origin;

  params->insecure_request_policy = frame_->GetInsecureRequestPolicy();
  params->insecure_navigations_set =
      frame_->GetInsecureRequestToUpgrade().ReleaseVector();

  params->has_potentially_trustworthy_unique_origin =
      frame_origin.IsOpaque() && frame_origin.IsPotentiallyTrustworthy();

  // Set the URL to be displayed in the browser UI to the user.
  params->url = GetLoadingUrl();
  if (GURL(frame_document.BaseURL()) != params->url)
    params->base_url = frame_document.BaseURL();

  GetRedirectChain(document_loader, &params->redirects);
  params->should_update_history =
      !document_loader->HasUnreachableURL() && response.HttpStatusCode() != 404;

  params->gesture = document_loader->HadUserGesture() ? NavigationGestureUser
                                                      : NavigationGestureAuto;

  // Make navigation state a part of the DidCommitProvisionalLoad message so
  // that committed entry has it at all times.  Send a single HistoryItem for
  // this frame, rather than the whole tree.  It will be stored in the
  // corresponding FrameNavigationEntry.
  params->page_state = SingleHistoryItemToPageState(current_history_item_);

  params->method = document_loader->HttpMethod().Latin1();
  if (params->method == "POST")
    params->post_id = ExtractPostId(current_history_item_);

  params->item_sequence_number = current_history_item_.ItemSequenceNumber();
  params->document_sequence_number =
      current_history_item_.DocumentSequenceNumber();

  // If the page contained a client redirect (meta refresh, document.loc...),
  // set the referrer appropriately.
  if (document_loader->IsClientRedirect()) {
    params->referrer =
        Referrer(params->redirects[0], document_loader->GetReferrerPolicy());
  } else {
    params->referrer =
        Referrer(blink::WebStringToGURL(document_loader->Referrer()),
                 document_loader->GetReferrerPolicy());
  }

  if (!frame_->Parent()) {
    // Top-level navigation.

    // Update contents MIME type for main frame.
    params->contents_mime_type =
        document_loader->GetResponse().MimeType().Utf8();

    params->transition = transition;
    DCHECK(ui::PageTransitionIsMainFrame(params->transition));

    // If the page contained a client redirect (meta refresh, document.loc...),
    // set the transition appropriately.
    if (document_loader->IsClientRedirect()) {
      params->transition = ui::PageTransitionFromInt(
          params->transition | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
    }

    // Send the user agent override back.
    params->is_overriding_user_agent =
        internal_data->is_overriding_user_agent();

    // Track the URL of the original request.  We use the first entry of the
    // redirect chain if it exists because the chain may have started in another
    // process.
    params->original_request_url = GetOriginalRequestURL(document_loader);

    params->history_list_was_cleared =
        navigation_state->commit_params().should_clear_history_list;
  } else {
    // Subframe navigation: the type depends on whether this navigation
    // generated a new session history entry. When they do generate a session
    // history entry, it means the user initiated the navigation and we should
    // mark it as such.
    if (commit_type == blink::kWebStandardCommit)
      params->transition = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
    else
      params->transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;

    DCHECK(!navigation_state->commit_params().should_clear_history_list);
    params->history_list_was_cleared = false;
  }

  // Standard URLs must match the reported origin, when it is not unique.
  // This check is very similar to RenderFrameHostImpl::CanCommitOrigin, but
  // adapted to the renderer process side.
  if (!params->origin.opaque() && params->url.IsStandard() &&
      render_view_->GetBlinkPreferences().web_security_enabled) {
    // Exclude file: URLs when settings allow them access any origin.
    if (params->origin.scheme() != url::kFileScheme ||
        !render_view_->GetBlinkPreferences()
             .allow_universal_access_from_file_urls) {
      if (!params->origin.IsSameOriginWith(url::Origin::Create(params->url))) {
        base::debug::CrashKeyString* url = base::debug::AllocateCrashKeyString(
            "mismatched_url", base::debug::CrashKeySize::Size256);
        base::debug::CrashKeyString* origin =
            base::debug::AllocateCrashKeyString(
                "mismatched_origin", base::debug::CrashKeySize::Size256);
        base::debug::ScopedCrashKeyString scoped_url(
            url, params->url.possibly_invalid_spec());
        base::debug::ScopedCrashKeyString scoped_origin(
            origin, params->origin.GetDebugString());
        CHECK(false) << " url:" << params->url << " origin:" << params->origin;
      }
    }
  }
  params->request_id = internal_data->request_id();

  return params;
}

void RenderFrameImpl::UpdateNavigationHistory(
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type) {
  NavigationState* navigation_state =
      NavigationState::FromDocumentLoader(frame_->GetDocumentLoader());
  const mojom::CommitNavigationParams& commit_params =
      navigation_state->commit_params();

  // Update the current history item for this frame.
  current_history_item_ = item;
  // Note: don't reference |item| after this point, as its value may not match
  // |current_history_item_|.
  current_history_item_.SetTarget(
      blink::WebString::FromUTF8(unique_name_helper_.value()));
  bool is_new_navigation = commit_type == blink::kWebStandardCommit;
  if (commit_params.should_clear_history_list) {
    render_view_->history_list_offset_ = 0;
    render_view_->history_list_length_ = 1;
  } else if (is_new_navigation) {
    DCHECK(!navigation_state->common_params().should_replace_current_entry ||
           render_view_->history_list_length_ > 0);
    if (!navigation_state->common_params().should_replace_current_entry) {
      // Advance our offset in session history, applying the length limit.
      // There is now no forward history.
      render_view_->history_list_offset_++;
      if (render_view_->history_list_offset_ >= kMaxSessionHistoryEntries)
        render_view_->history_list_offset_ = kMaxSessionHistoryEntries - 1;
      render_view_->history_list_length_ =
          render_view_->history_list_offset_ + 1;
    }
  } else if (commit_params.nav_entry_id != 0 &&
             !commit_params.intended_as_new_entry) {
    render_view_->history_list_offset_ =
        navigation_state->commit_params().pending_history_list_offset;
  }
}

void RenderFrameImpl::NotifyObserversOfNavigationCommit(
    ui::PageTransition transition) {
  for (auto& observer : observers_)
    observer.DidCommitProvisionalLoad(transition);
}

void RenderFrameImpl::UpdateStateForCommit(
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type,
    ui::PageTransition transition) {
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentLoader(
          frame_->GetDocumentLoader());
  NavigationState* navigation_state = internal_data->navigation_state();

  // We need to update the last committed session history entry with state for
  // the previous page. Do this before updating the current history item.
  SendUpdateState();

  UpdateNavigationHistory(item, commit_type);

  if (internal_data->must_reset_scroll_and_scale_state()) {
    render_view_->GetWebView()->ResetScrollAndScaleState();
    internal_data->set_must_reset_scroll_and_scale_state(false);
  }
  if (!frame_->Parent()) {  // Only for top frames.
    RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
    if (render_thread_impl) {  // Can be NULL in tests.
      render_thread_impl->histogram_customizer()->RenderViewNavigatedToHost(
          GURL(GetLoadingUrl()).host(), RenderView::GetRenderViewCount());
    }
  }

  if (render_widget_) {
    // This goes to WebViewImpl and sets the zoom factor which will be
    // propagated down to this RenderFrameImpl's LocalFrame in blink.
    // Non-local-roots are able to grab the value off their parents but local
    // roots can not and this is a huge action-at-a-distance to make up for that
    // flaw in how LocalFrame determines the zoom factor.
    // TODO(danakj): This should not be needed if the zoom factor/device scale
    // factor did not need to be propagated to each frame. Since they are a
    // global that should be okay?? The test that fails without this, for
    // child frames, is in content_browsertests:
    //     SitePerProcessHighDPIBrowserTest.
    //         SubframeLoadsWithCorrectDeviceScaleFactor
    // And when UseZoomForDSF is disabled, in content_browsertests:
    //     IFrameZoomBrowserTest.SubframesDontZoomIndependently (and the whole
    //     suite).
    render_view_->PropagatePageZoomToNewlyAttachedFrame(
        render_widget_->compositor_deps()->IsUseZoomForDSFEnabled(),
        render_widget_->GetWebWidget()->GetScreenInfo().device_scale_factor);
  }

  // If we are a top frame navigation to another document we should clear any
  // existing autoplay flags on the Page. This is because flags are stored at
  // the page level so subframes would only add to them.
  if (!frame_->Parent() && !navigation_state->WasWithinSameDocument()) {
    render_view_->GetWebView()->ClearAutoplayFlags();
  }

  // Set the correct autoplay flags on the Page and wipe the cached origin so
  // this will not be used incorrectly.
  if (url::Origin(frame_->GetSecurityOrigin()) == autoplay_flags_.first) {
    render_view_->GetWebView()->AddAutoplayFlags(autoplay_flags_.second);
    autoplay_flags_.first = url::Origin();
  }
}

void RenderFrameImpl::DidCommitNavigationInternal(
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type,
    bool was_within_same_document,
    ui::PageTransition transition,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params,
    const base::Optional<base::UnguessableToken>& embedding_token) {
  DCHECK(!(was_within_same_document && interface_params));
  UpdateStateForCommit(item, commit_type, transition);

  if (render_view_->renderer_wide_named_frame_lookup())
    GetWebFrame()->SetAllowsCrossBrowsingInstanceFrameLookup();

  // This invocation must precede any calls to allowScripts(), allowImages(),
  // or allowPlugins() for the new page. This ensures that when these functions
  // call chrome::ContentSettingsManager::OnContentBlocked, those calls arrive
  // after the browser process has already been informed of the provisional
  // load committing.
  auto params = MakeDidCommitProvisionalLoadParams(commit_type, transition,
                                                   embedding_token);
  if (was_within_same_document) {
    GetFrameHost()->DidCommitSameDocumentNavigation(std::move(params));
  } else {
    NavigationState* navigation_state =
        NavigationState::FromDocumentLoader(frame_->GetDocumentLoader());
    if (navigation_state->has_navigation_client()) {
      navigation_state->RunCommitNavigationCallback(
          std::move(params), std::move(interface_params));
    } else {
      GetFrameHost()->DidCommitProvisionalLoad(std::move(params),
                                               std::move(interface_params));
    }
  }

  // Ensure we will propagate frame intersections when the main frame commits
  // even if the intersection does not change across navigations.
  if (IsMainFrame()) {
    mainframe_intersection_rect_.reset();
  }
}

void RenderFrameImpl::PrepareFrameForCommit(
    const GURL& url,
    const mojom::CommitNavigationParams& commit_params) {
  browser_side_navigation_pending_ = false;
  browser_side_navigation_pending_url_ = GURL();
  GetContentClient()->SetActiveURL(
      url, frame_->Top()->GetSecurityOrigin().ToString().Utf8());

  RenderFrameImpl::PrepareRenderViewForNavigation(url, commit_params);
}

blink::mojom::CommitResult RenderFrameImpl::PrepareForHistoryNavigationCommit(
    const mojom::CommonNavigationParams& common_params,
    const mojom::CommitNavigationParams& commit_params,
    WebHistoryItem* item_for_history_navigation,
    blink::WebFrameLoadType* load_type) {
  mojom::NavigationType navigation_type = common_params.navigation_type;
  DCHECK(navigation_type == mojom::NavigationType::HISTORY_SAME_DOCUMENT ||
         navigation_type == mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT ||
         navigation_type == mojom::NavigationType::RESTORE ||
         navigation_type == mojom::NavigationType::RESTORE_WITH_POST);
  std::unique_ptr<HistoryEntry> entry =
      PageStateToHistoryEntry(commit_params.page_state);
  if (!entry)
    return blink::mojom::CommitResult::Aborted;

  // The browser process sends a single WebHistoryItem for this frame.
  // TODO(creis): Change PageState to FrameState.  In the meantime, we
  // store the relevant frame's WebHistoryItem in the root of the
  // PageState.
  *item_for_history_navigation = entry->root();
  *load_type = blink::WebFrameLoadType::kBackForward;

  // Keep track of which subframes the browser process has history items
  // for during a history navigation.
  history_subframe_unique_names_ = commit_params.subframe_unique_names;

  if (navigation_type == mojom::NavigationType::HISTORY_SAME_DOCUMENT) {
    // If this is marked as a same document load but we haven't committed
    // anything, we can't proceed with the load. The browser shouldn't let this
    // happen.
    if (current_history_item_.IsNull()) {
      NOTREACHED();
      return blink::mojom::CommitResult::RestartCrossDocument;
    }

    // Additionally, if the |current_history_item_|'s document sequence number
    // doesn't match the one sent from the browser, it is possible that this
    // renderer has committed a different document. In such case, the navigation
    // cannot be loaded as a same-document navigation.
    if (current_history_item_.DocumentSequenceNumber() !=
        item_for_history_navigation->DocumentSequenceNumber()) {
      return blink::mojom::CommitResult::RestartCrossDocument;
    }
  }

  // Note: we used to check that initial history navigation in the child frame
  // was not canceled by a client redirect before committing it. However,
  // we now destroy the NavigationClient for initial history navigation, and
  // commit does not arrive to the renderer in this case.

  return blink::mojom::CommitResult::Ok;
}

std::string RenderFrameImpl::GetPreviousFrameUniqueName() {
  DCHECK(!in_frame_tree_);

  RenderFrameProxy* previous_proxy =
      RenderFrameProxy::FromRoutingID(previous_routing_id_);
  RenderFrameImpl* previous_frame =
      RenderFrameImpl::FromRoutingID(previous_routing_id_);

  // The |previous_proxy| or the |previous_frame| should always exist.
  // If it was detached while the provisional LocalFrame was being navigated,
  // the provisional frame would've been cleaned up by
  // RenderFrameProxy::FrameDetached.
  // See https://crbug.com/526304 and https://crbug.com/568676 for context.
  CHECK_NE(!!previous_proxy, !!previous_frame);

  if (previous_proxy)
    return previous_proxy->unique_name();
  return previous_frame->unique_name();
}

bool RenderFrameImpl::SwapInInternal() {
  CHECK_NE(previous_routing_id_, MSG_ROUTING_NONE);
  CHECK(!in_frame_tree_);

  unique_name_helper_.set_propagated_name(GetPreviousFrameUniqueName());

  // Note: Calling swap() will detach and delete |previous_frame|, so do not
  // reference it after this.
  WebFrame* previous_web_frame = ResolveWebFrame(previous_routing_id_);
  if (!previous_web_frame->Swap(frame_)) {
    // Main frames should always swap successfully because there is no parent
    // frame to cause them to become detached.
    DCHECK(!is_main_frame_);
    return false;
  }

  previous_routing_id_ = MSG_ROUTING_NONE;
  in_frame_tree_ = true;

  // If this is the main frame going from a remote frame to a local frame,
  // it needs to set RenderViewImpl's pointer for the main frame to itself.
  if (is_main_frame_) {
    // TODO(https://crubg.com/936696): Implement RenderDocument on main frames.
    CHECK(!render_view_->main_render_frame_);
    render_view_->main_render_frame_ = this;

    // The WebFrame being swapped in here has now been attached to the Page as
    // its main frame, and the WebFrameWidget was previously initialized when
    // the frame was created so we can call WebView's DidAttachLocalMainFrame().
    render_view_->GetWebView()->DidAttachLocalMainFrame();
  }

  return true;
}

void RenderFrameImpl::DidStartLoading() {
  // TODO(dgozman): consider removing this callback.
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::didStartLoading", "id",
               routing_id_);
}

void RenderFrameImpl::DidStopLoading() {
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::didStopLoading", "id",
               routing_id_);

  // Any subframes created after this point won't be considered part of the
  // current history navigation (if this was one), so we don't need to track
  // this state anymore.
  history_subframe_unique_names_.clear();

  GetFrameHost()->DidStopLoading();
}

void RenderFrameImpl::NotifyAccessibilityModeChange(ui::AXMode new_mode) {
  for (auto& observer : observers_)
    observer.AccessibilityModeChanged(new_mode);
}

void RenderFrameImpl::FocusedElementChanged(const blink::WebElement& element) {
  has_scrolled_focused_editable_node_into_rect_ = false;

  for (auto& observer : observers_)
    observer.FocusedElementChanged(element);
}

void RenderFrameImpl::BeginNavigation(
    std::unique_ptr<blink::WebNavigationInfo> info) {
  // A provisional frame should never make a renderer-initiated navigation: no
  // JS should be running in |this|, and no other frame should have a reference
  // to |this|.
  CHECK(in_frame_tree_);

  // This method is only called for renderer initiated navigations, which
  // may have originated from a link-click, script, drag-n-drop operation, etc.

  // Note that we don't want to go to browser for a navigation to an empty url,
  // which happens for window.open('') call. An example would be embedder
  // deciding to fork the process for the empty url, or setting
  // |browser_handles_all_top_level_requests| preference.
  //
  // Doing a browser-side navigation might later trigger unload handlers,
  // e.g. when the dom window of the popup has already been touched
  // synchronously in this process. We should avoid that.
  //
  // See the checks for empty url in the cases below.
  // TODO(dgozman): if we rewrite empty url to about:blank earlier
  // (we currently do that in DocumentLoader), all the empty checks can be
  // removed, since they already account for an empty url.

  // Blink is asking whether to navigate to a new URL.
  // This is fine normally, except if we're showing UI from one security
  // context and they're trying to navigate to a different context.
  const GURL& url = info->url_request.Url();
  TRACE_EVENT2("navigation", "RenderFrameImpl::BeginNavigation", "url",
               url.possibly_invalid_spec(), "navigation_type",
               static_cast<int>(info->navigation_type));

  if (GetWebFrame() && GetWebFrame()->DispatchedPagehideAndStillHidden()) {
    // The navigation started after the pagehide event got dispatched. This
    // navigation will be ignored by the browser, and we need to track that it's
    // happening. Note that this problem is not unique to BackForwardCache/
    // same-site BrowsingInstance swap, as navigations started after unload in
    // normal scenarios will also be ignored by the browser.
    UMA_HISTOGRAM_ENUMERATION("BackForwardCache.SameSite.ActionAfterPagehide2",
                              blink::ActionAfterPagehide::kNavigation);
  }

  // When an MHTML Archive is present, it should be used to serve iframe
  // content instead of doing a network request. This should never be true for
  // the main frame.
  bool use_archive = (info->archive_status ==
                      blink::WebNavigationInfo::ArchiveStatus::Present) &&
                     !url.SchemeIs(url::kDataScheme);
  DCHECK(!(use_archive && IsMainFrame()));

#ifdef OS_ANDROID
  bool render_view_was_created_by_renderer =
      render_view_->was_created_by_renderer_;
  // The handlenavigation API is deprecated and will be removed once
  // crbug.com/325351 is resolved.
  if (!url.is_empty() && !use_archive && !IsURLHandledByNetworkStack(url) &&
      GetContentClient()->renderer()->HandleNavigation(
          this, true /* is_content_initiated */,
          render_view_was_created_by_renderer, frame_, info->url_request,
          info->navigation_type, info->navigation_policy,
          false /* is_redirect */)) {
    return;
  }
#endif

  // If the browser is interested, then give it a chance to look at the request.
  if (IsTopLevelNavigation(frame_) &&
      render_view_->renderer_preferences_
          .browser_handles_all_top_level_requests) {
    OpenURL(std::move(info));
    return;  // Suppress the load here.
  }

  // Back/forward navigations in newly created subframes should be sent to the
  // browser if there is a matching FrameNavigationEntry, and if it isn't just
  // staying at about:blank.  If this frame isn't in the map of unique names
  // that have history items, or if it's staying at the initial about:blank URL,
  // fall back to loading the default url.  (We remove each name as we encounter
  // it, because it will only be used once as the frame is created.)
  // Note: Skip this logic for MHTML files (|use_archive|), which should load
  // their subframes from the archive and not from history.
  bool is_history_navigation_in_new_child_frame = false;
  if (info->is_history_navigation_in_new_child_frame && frame_->Parent() &&
      !use_archive) {
    // Check whether the browser has a history item for this frame that isn't
    // just staying at the initial about:blank document.
    RenderFrameImpl* parent = RenderFrameImpl::FromWebFrame(frame_->Parent());
    auto iter = parent->history_subframe_unique_names_.find(
        unique_name_helper_.value());
    if (iter != parent->history_subframe_unique_names_.end()) {
      bool history_item_is_about_blank = iter->second;
      is_history_navigation_in_new_child_frame =
          !history_item_is_about_blank || url != url::kAboutBlankURL;
      parent->history_subframe_unique_names_.erase(iter);
    }
  }

  if (is_history_navigation_in_new_child_frame) {
    // Don't do this if |info| also says it is a client redirect, in which
    // case JavaScript on the page is trying to interrupt the history
    // navigation.
    if (info->is_client_redirect) {
      // Client redirects during an initial history load should attempt to
      // cancel the history navigation.  They will create a provisional
      // document loader, causing the history load to be ignored in
      // NavigateInternal, and this IPC will try to cancel any cross-process
      // history load.
      is_history_navigation_in_new_child_frame = false;
      GetFrameHost()->CancelInitialHistoryLoad();
    }
  }

  // Use the frame's original request's URL rather than the document's URL for
  // subsequent checks.  For a popup, the document's URL may become the opener
  // window's URL if the opener has called document.write().
  // See http://crbug.com/93517.
  GURL old_url(frame_->GetDocumentLoader()->GetUrl());

  // Detect when we're crossing a permission-based boundary (e.g. into or out of
  // an extension or app origin, leaving a WebUI page, etc). We only care about
  // top-level navigations (not iframes). But we sometimes navigate to
  // about:blank to clear a tab, and we want to still allow that.
  if (!frame_->Parent() && !url.SchemeIs(url::kAboutScheme) &&
      !url.is_empty()) {
    // All navigations to or from WebUI URLs or within WebUI-enabled
    // RenderProcesses must be handled by the browser process so that the
    // correct bindings and data sources can be registered.
    // All frames in a WebUI process must have the same enabled_bindings_, so
    // we can do a per-frame check here rather than a process-wide check.
    bool should_fork = HasWebUIScheme(url) || HasWebUIScheme(old_url) ||
                       (enabled_bindings_ & kWebUIBindingsPolicyMask);
    if (should_fork) {
      OpenURL(std::move(info));
      return;  // Suppress the load here.
    }
  }

  if (info->navigation_policy == blink::kWebNavigationPolicyCurrentTab) {
    // Execute the BeforeUnload event. If asked not to proceed or the frame is
    // destroyed, ignore the navigation.
    // Keep a WeakPtr to this RenderFrameHost to detect if executing the
    // BeforeUnload event destroyed this frame.
    base::WeakPtr<RenderFrameImpl> weak_self = weak_factory_.GetWeakPtr();

    base::TimeTicks renderer_before_unload_start = base::TimeTicks::Now();
    if (!frame_->DispatchBeforeUnloadEvent(info->navigation_type ==
                                           blink::kWebNavigationTypeReload) ||
        !weak_self) {
      return;
    }
    base::TimeTicks renderer_before_unload_end = base::TimeTicks::Now();

    if (!info->form.IsNull()) {
      for (auto& observer : observers_)
        observer.WillSubmitForm(info->form);
    }

    if (mhtml_body_loader_client_) {
      mhtml_body_loader_client_->Detach();
      mhtml_body_loader_client_.reset();
    }

    // First navigation in a frame to an empty document must be handled
    // synchronously.
    bool is_first_real_empty_document_navigation =
        WebDocumentLoader::WillLoadUrlAsEmpty(url) &&
        !frame_->HasCommittedFirstRealLoad();

    if (is_first_real_empty_document_navigation &&
        !is_history_navigation_in_new_child_frame) {
      for (auto& observer : observers_)
        observer.DidStartNavigation(url, info->navigation_type);
      CommitSyncNavigation(std::move(info));
      return;
    }

    // Everything else is handled asynchronously by the browser process through
    // BeginNavigation.
    BeginNavigationInternal(
        std::move(info), is_history_navigation_in_new_child_frame,
        renderer_before_unload_start, renderer_before_unload_end);
    return;
  }

  if (info->navigation_policy == blink::kWebNavigationPolicyDownload) {
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token =
        CloneBlobURLToken(info->blob_url_token);

    frame_->DownloadURL(info->url_request,
                        network::mojom::RedirectMode::kFollow,
                        std::move(blob_url_token));
  } else {
    OpenURL(std::move(info));
  }
}

void RenderFrameImpl::CommitSyncNavigation(
    std::unique_ptr<blink::WebNavigationInfo> info) {
  CHECK_EQ(NavigationCommitState::kInitialEmptyDocument,
           navigation_commit_state_);
  navigation_commit_state_ = NavigationCommitState::kNone;
  AssertNavigationCommits assert_navigation_commits(this);

  // TODO(dgozman): should we follow the RFI::CommitNavigation path instead?
  auto navigation_params = WebNavigationParams::CreateFromInfo(*info);
  // We need the provider to be non-null, otherwise Blink crashes, even
  // though the provider should not be used for any actual networking.
  navigation_params->service_worker_network_provider =
      ServiceWorkerNetworkProviderForFrame::CreateInvalidInstance();
  frame_->CommitNavigation(std::move(navigation_params), BuildDocumentState());
}

// mojom::MhtmlFileWriter implementation
// ----------------------------------------

void RenderFrameImpl::SerializeAsMHTML(mojom::SerializeAsMHTMLParamsPtr params,
                                       SerializeAsMHTMLCallback callback) {
  TRACE_EVENT0("page-serialization", "RenderFrameImpl::SerializeAsMHTML");
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Unpack payload.
  const WebString mhtml_boundary =
      WebString::FromUTF8(params->mhtml_boundary_marker);
  DCHECK(!mhtml_boundary.IsEmpty());

  // Holds WebThreadSafeData instances for some or all of header, contents and
  // footer.
  std::vector<WebThreadSafeData> mhtml_contents;
  std::unordered_set<std::string> serialized_resources_uri_digests;
  MHTMLPartsGenerationDelegate delegate(*params,
                                        &serialized_resources_uri_digests);

  mojom::MhtmlSaveStatus save_status = mojom::MhtmlSaveStatus::kSuccess;
  bool has_some_data = false;

  // Generate MHTML header if needed.
  if (IsMainFrame()) {
    TRACE_EVENT0("page-serialization",
                 "RenderFrameImpl::SerializeAsMHTML header");
    // The returned data can be empty if the main frame should be skipped. If
    // the main frame is skipped, then the whole archive is bad.
    mhtml_contents.emplace_back(WebFrameSerializer::GenerateMHTMLHeader(
        mhtml_boundary, GetWebFrame(), &delegate));
    has_some_data = true;
  }

  // Generate MHTML parts.  Note that if this is not the main frame, then even
  // skipping the whole parts generation step is not an error - it simply
  // results in an omitted resource in the final file.
  if (save_status == mojom::MhtmlSaveStatus::kSuccess) {
    TRACE_EVENT0("page-serialization",
                 "RenderFrameImpl::SerializeAsMHTML parts serialization");
    // The returned data can be empty if the frame should be skipped, but this
    // is OK.
    mhtml_contents.emplace_back(WebFrameSerializer::GenerateMHTMLParts(
        mhtml_boundary, GetWebFrame(), &delegate));
    has_some_data |= !mhtml_contents.back().IsEmpty();
  }

  // Note: the MHTML footer is written by the browser process, after the last
  // frame is serialized by a renderer process.

  // Note: we assume RenderFrameImpl::OnWriteMHTMLComplete and the rest of
  // this function will be fast enough to not need to be accounted for in this
  // metric.
  base::TimeDelta main_thread_use_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES(
      "PageSerialization.MhtmlGeneration.RendererMainThreadTime.SingleFrame",
      main_thread_use_time);

  MHTMLHandleWriterDelegate handle_delegate(
      *params,
      base::BindOnce(&RenderFrameImpl::OnWriteMHTMLComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(serialized_resources_uri_digests),
                     main_thread_use_time),
      GetTaskRunner(blink::TaskType::kInternalDefault));

  if (save_status == mojom::MhtmlSaveStatus::kSuccess && has_some_data) {
    handle_delegate.WriteContents(mhtml_contents);
  } else {
    handle_delegate.Finish(save_status);
  }
}

void RenderFrameImpl::OnWriteMHTMLComplete(
    SerializeAsMHTMLCallback callback,
    std::unordered_set<std::string> serialized_resources_uri_digests,
    base::TimeDelta main_thread_use_time,
    mojom::MhtmlSaveStatus save_status) {
  TRACE_EVENT1("page-serialization", "RenderFrameImpl::OnWriteMHTMLComplete",
               "frame save status", save_status);
  DCHECK(RenderThread::IsMainThread())
      << "Must run in the main renderer thread";

  // Convert the set into a vector for transport.
  std::vector<std::string> digests_of_new_parts(
      std::make_move_iterator(serialized_resources_uri_digests.begin()),
      std::make_move_iterator(serialized_resources_uri_digests.end()));

  // Notify the browser process about completion using the callback.
  // Note: we assume this method is fast enough to not need to be accounted for
  // in PageSerialization.MhtmlGeneration.RendererMainThreadTime.SingleFrame.
  std::move(callback).Run(save_status, std::move(digests_of_new_parts),
                          main_thread_use_time);
}

#ifndef STATIC_ASSERT_ENUM
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)
#undef STATIC_ASSERT_ENUM
#endif

void RenderFrameImpl::OnMixedContentFound(
    const FrameMsg_MixedContentFound_Params& params) {
  blink::WebSourceLocation source_location;
  source_location.url = WebString::FromLatin1(params.source_location.url);
  source_location.line_number = params.source_location.line;
  source_location.column_number = params.source_location.column;
  auto request_context = static_cast<blink::mojom::RequestContextType>(
      params.request_context_type);
  frame_->MixedContentFound(params.main_resource_url, params.mixed_content_url,
                            request_context, params.was_allowed,
                            params.url_before_redirects, params.had_redirect,
                            source_location);
}

void RenderFrameImpl::RequestOverlayRoutingToken(
    media::RoutingTokenCallback callback) {
  std::move(callback).Run(GetWebFrame()->GetFrameToken());
}

void RenderFrameImpl::OpenURL(std::unique_ptr<blink::WebNavigationInfo> info) {
  // A valid RequestorOrigin is always expected to be present.
  DCHECK(!info->url_request.RequestorOrigin().IsNull());

  WebNavigationPolicy policy = info->navigation_policy;
  auto params = mojom::OpenURLParams::New();
  params->url = info->url_request.Url();
  params->initiator_origin = info->url_request.RequestorOrigin();
  params->post_body = GetRequestBodyForWebURLRequest(info->url_request);
  DCHECK_EQ(!!params->post_body, IsHttpPost(info->url_request));
  params->extra_headers = GetWebURLRequestHeadersAsString(info->url_request);

  params->referrer = blink::mojom::Referrer::New(
      blink::WebStringToGURL(info->url_request.ReferrerString()),
      info->url_request.GetReferrerPolicy());
  params->disposition = RenderViewImpl::NavigationPolicyToDisposition(policy);
  params->triggering_event_info = info->triggering_event_info;
  params->blob_url_token = CloneBlobURLToken(info->blob_url_token).PassPipe();
  params->should_replace_current_entry =
      info->frame_load_type == WebFrameLoadType::kReplaceCurrentItem &&
      render_view_->history_list_length_;
  params->user_gesture = info->has_transient_user_activation;

  RenderFrameImpl* initiator_render_frame =
      RenderFrameImpl::FromWebFrame(info->initiator_frame);
  params->initiator_routing_id = initiator_render_frame
                                     ? initiator_render_frame->GetRoutingID()
                                     : MSG_ROUTING_NONE;

  if (info->impression)
    params->impression = ConvertWebImpressionToImpression(*info->impression);

  if (GetContentClient()->renderer()->AllowPopup())
    params->user_gesture = true;

  // A main frame navigation should already have consumed an activation in
  // FrameLoader::StartNavigation.
  DCHECK(!is_main_frame_ || !frame_->HasTransientUserActivation());
  if (!is_main_frame_ &&
      (policy == blink::kWebNavigationPolicyNewBackgroundTab ||
       policy == blink::kWebNavigationPolicyNewForegroundTab ||
       policy == blink::kWebNavigationPolicyNewWindow ||
       policy == blink::kWebNavigationPolicyNewPopup)) {
    frame_->ConsumeTransientUserActivation();
  }

  params->href_translate = info->href_translate.Latin1();

  bool current_frame_has_download_sandbox_flag = !frame_->IsAllowedToDownload();
  bool has_download_sandbox_flag =
      info->initiator_frame_has_download_sandbox_flag ||
      current_frame_has_download_sandbox_flag;
  bool from_ad = info->initiator_frame_is_ad || frame_->IsAdSubframe();

  MaybeSetDownloadFramePolicy(info->is_opener_navigation, info->url_request,
                              frame_->GetSecurityOrigin(),
                              has_download_sandbox_flag,
                              info->blocking_downloads_in_sandbox_enabled,
                              from_ad, &params->download_policy);
  GetFrameHost()->OpenURL(std::move(params));
}

ChildURLLoaderFactoryBundle* RenderFrameImpl::GetLoaderFactoryBundle() {
  if (!loader_factories_)
    loader_factories_ = GetLoaderFactoryBundleFromCreator();
  if (!loader_factories_)
    loader_factories_ = GetLoaderFactoryBundleFallback();
  return loader_factories_.get();
}

scoped_refptr<ChildURLLoaderFactoryBundle>
RenderFrameImpl::GetLoaderFactoryBundleFallback() {
  return CreateLoaderFactoryBundle(
      nullptr, base::nullopt /* subresource_overrides */,
      mojo::NullRemote() /* prefetch_loader_factory */);
}

scoped_refptr<ChildURLLoaderFactoryBundle>
RenderFrameImpl::GetLoaderFactoryBundleFromCreator() {
  RenderFrameImpl* creator = RenderFrameImpl::FromWebFrame(
      frame_->Parent() ? frame_->Parent() : frame_->Opener());
  if (creator) {
    auto bundle_info = base::WrapUnique(
        static_cast<TrackedChildPendingURLLoaderFactoryBundle*>(
            creator->GetLoaderFactoryBundle()->Clone().release()));
    return base::MakeRefCounted<TrackedChildURLLoaderFactoryBundle>(
        std::move(bundle_info));
  }
  return nullptr;
}

scoped_refptr<ChildURLLoaderFactoryBundle>
RenderFrameImpl::CreateLoaderFactoryBundle(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> info,
    base::Optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory) {
  scoped_refptr<ChildURLLoaderFactoryBundle> loader_factories =
      base::MakeRefCounted<HostChildURLLoaderFactoryBundle>(
          GetTaskRunner(blink::TaskType::kInternalLoading));

  // CreateDefaultURLLoaderFactoryBundle can't be called if (as in some tests)
  // RenderThreadImpl::current is null.
  if (RenderThreadImpl::current() && !info) {
    // This should only happen for a placeholder document or an initial empty
    // document cases.
    DCHECK(GetLoadingUrl().is_empty() ||
           GetLoadingUrl().spec() == url::kAboutBlankURL);
    loader_factories->Update(
        CreateDefaultURLLoaderFactoryBundle()->PassInterface());
    loader_factories->MarkAsDeprecatedProcessWideFactory();
  }

  if (info) {
    loader_factories->Update(
        std::make_unique<ChildPendingURLLoaderFactoryBundle>(std::move(info)));
  }
  if (subresource_overrides) {
    loader_factories->UpdateSubresourceOverrides(&*subresource_overrides);
  }
  if (prefetch_loader_factory) {
    loader_factories->SetPrefetchLoaderFactory(
        std::move(prefetch_loader_factory));
  }

  return loader_factories;
}

void RenderFrameImpl::UpdateEncoding(WebFrame* frame,
                                     const std::string& encoding_name) {
  // Only update main frame's encoding_name.
  if (!frame->Parent())
    GetFrameHost()->UpdateEncoding(encoding_name);
}

void RenderFrameImpl::SyncSelectionIfRequired() {
  base::string16 text;
  size_t offset;
  gfx::Range range;
#if BUILDFLAG(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->GetSurroundingText(&text, &range);
    offset = 0;  // Pepper API does not support offset reporting.
    // TODO(kinaba): cut as needed.
  } else
#endif
  {
    WebRange selection =
        frame_->GetInputMethodController()->GetSelectionOffsets();
    if (selection.IsNull())
      return;

    range = gfx::Range(selection.StartOffset(), selection.EndOffset());

    if (frame_->GetInputMethodController()->TextInputType() !=
        blink::kWebTextInputTypeNone) {
      // If current focused element is editable, we will send 100 more chars
      // before and after selection. It is for input method surrounding text
      // feature.
      if (selection.StartOffset() > kExtraCharsBeforeAndAfterSelection)
        offset = selection.StartOffset() - kExtraCharsBeforeAndAfterSelection;
      else
        offset = 0;
      size_t length =
          selection.EndOffset() - offset + kExtraCharsBeforeAndAfterSelection;
      text = frame_->RangeAsText(WebRange(offset, length)).Utf16();
    } else {
      offset = selection.StartOffset();
      text = frame_->SelectionAsText().Utf16();
      // http://crbug.com/101435
      // In some case, frame->selectionAsText() returned text's length is not
      // equal to the length returned from frame_->GetSelectionOffsets(). So we
      // have to set the range according to text.length().
      range.set_end(range.start() + text.length());
    }
  }

  // TODO(dglazkov): Investigate if and why this would be happening,
  // and resolve this. We shouldn't be carrying selection text here.
  // http://crbug.com/632920.
  // Sometimes we get repeated didChangeSelection calls from webkit when
  // the selection hasn't actually changed. We don't want to report these
  // because it will cause us to continually claim the X clipboard.
  if (selection_text_offset_ != offset || selection_range_ != range ||
      selection_text_ != text) {
    selection_text_ = text;
    selection_text_offset_ = offset;
    selection_range_ = range;
    SetSelectedText(text, offset, range);
  }
  GetLocalRootRenderWidget()->UpdateSelectionBounds();
}

void RenderFrameImpl::ScrollFocusedEditableElementIntoRect(
    const gfx::Rect& rect) {
  // TODO(ekaramad): Perhaps we should remove |rect| since all it seems to be
  // doing is helping verify if scrolling animation for a given focused editable
  // element has finished.
  blink::WebAutofillClient* autofill_client = frame_->AutofillClient();
  if (has_scrolled_focused_editable_node_into_rect_ &&
      rect == rect_for_scrolled_focused_editable_node_ && autofill_client) {
    autofill_client->DidCompleteFocusChangeInFrame();
    return;
  }

  if (!frame_->LocalRoot()
           ->FrameWidget()
           ->ScrollFocusedEditableElementIntoView()) {
    return;
  }

  rect_for_scrolled_focused_editable_node_ = rect;
  has_scrolled_focused_editable_node_into_rect_ = true;
  if (!GetLocalRootRenderWidget()
           ->layer_tree_host()
           ->HasPendingPageScaleAnimation() &&
      autofill_client) {
    autofill_client->DidCompleteFocusChangeInFrame();
  }
}

void RenderFrameImpl::ResetHasScrolledFocusedEditableIntoView() {
  has_scrolled_focused_editable_node_into_rect_ = false;
}

void RenderFrameImpl::CreateAudioInputStream(
    blink::CrossVariantMojoRemote<
        blink::mojom::RendererAudioInputStreamFactoryClientInterfaceBase>
        client,
    const base::UnguessableToken& session_id,
    const media::AudioParameters& params,
    bool automatic_gain_control,
    uint32_t shared_memory_count) {
  GetAudioInputStreamFactory()->CreateStream(std::move(client), session_id,
                                             params, automatic_gain_control,
                                             shared_memory_count);
}

void RenderFrameImpl::AssociateInputAndOutputForAec(
    const base::UnguessableToken& input_stream_id,
    const std::string& output_device_id) {
  GetAudioInputStreamFactory()->AssociateInputAndOutputForAec(input_stream_id,
                                                              output_device_id);
}

void RenderFrameImpl::InitializeMediaStreamDeviceObserver() {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (!render_thread)  // Will be NULL during unit tests.
    return;

  DCHECK(!web_media_stream_device_observer_);
  web_media_stream_device_observer_ =
      std::make_unique<blink::WebMediaStreamDeviceObserver>(GetWebFrame());
}

void RenderFrameImpl::PrepareRenderViewForNavigation(
    const GURL& url,
    const mojom::CommitNavigationParams& commit_params) {
  DCHECK(render_view_->GetWebView());

  render_view_->history_list_offset_ =
      commit_params.current_history_list_offset;
  render_view_->history_list_length_ =
      commit_params.current_history_list_length;
}

void RenderFrameImpl::BeginNavigationInternal(
    std::unique_ptr<blink::WebNavigationInfo> info,
    bool is_history_navigation_in_new_child_frame,
    base::TimeTicks renderer_before_unload_start,
    base::TimeTicks renderer_before_unload_end) {
  if (!frame_->WillStartNavigation(*info))
    return;

  for (auto& observer : observers_)
    observer.DidStartNavigation(info->url_request.Url(), info->navigation_type);
  browser_side_navigation_pending_ = true;
  browser_side_navigation_pending_url_ = info->url_request.Url();

  blink::WebURLRequest& request = info->url_request;

  // Set SiteForCookies.
  WebDocument frame_document = frame_->GetDocument();
  if (info->frame_type == blink::mojom::RequestContextFrameType::kTopLevel)
    request.SetSiteForCookies(net::SiteForCookies::FromUrl(request.Url()));
  else
    request.SetSiteForCookies(frame_document.SiteForCookies());

  ui::PageTransition transition_type = GetTransitionType(
      ui::PAGE_TRANSITION_LINK,
      info->frame_load_type == WebFrameLoadType::kReplaceCurrentItem,
      IsMainFrame(), info->navigation_type);
  if (info->is_client_redirect) {
    transition_type = ui::PageTransitionFromInt(
        transition_type | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  }

  // Note: At this stage, the goal is to apply all the modifications the
  // renderer wants to make to the request, and then send it to the browser, so
  // that the actual network request can be started. Ideally, all such
  // modifications should take place in WillSendRequestInternal, and in the
  // implementation of willSendRequest for the various InspectorAgents
  // (devtools).
  //
  // TODO(clamy): Apply devtools override.
  // TODO(clamy): Make sure that navigation requests are not modified somewhere
  // else in blink.
  bool for_main_frame = !frame_->Parent();
  WillSendRequestInternal(request, for_main_frame, transition_type,
                          ForRedirect(false));

  if (!info->url_request.GetExtraData())
    info->url_request.SetExtraData(base::MakeRefCounted<RequestExtraData>());

  // TODO(clamy): Same-document navigations should not be sent back to the
  // browser.
  // TODO(clamy): Data urls should not be sent back to the browser either.
  // These values are assumed on the browser side for navigations. These checks
  // ensure the renderer has the correct values.
  DCHECK_EQ(network::mojom::RequestMode::kNavigate,
            info->url_request.GetMode());
  DCHECK_EQ(network::mojom::CredentialsMode::kInclude,
            info->url_request.GetCredentialsMode());
  DCHECK_EQ(network::mojom::RedirectMode::kManual,
            info->url_request.GetRedirectMode());
  DCHECK(frame_->Parent() ||
         info->frame_type == blink::mojom::RequestContextFrameType::kTopLevel);
  DCHECK(!frame_->Parent() ||
         info->frame_type == blink::mojom::RequestContextFrameType::kNested);

  bool is_form_submission =
      info->navigation_type == blink::kWebNavigationTypeFormSubmitted ||
      info->navigation_type == blink::kWebNavigationTypeFormResubmitted;

  bool was_initiated_by_link_click =
      info->navigation_type == blink::kWebNavigationTypeLinkClicked;

  GURL searchable_form_url;
  std::string searchable_form_encoding;
  if (!info->form.IsNull()) {
    WebSearchableFormData web_searchable_form_data(info->form);
    searchable_form_url = web_searchable_form_data.Url();
    searchable_form_encoding = web_searchable_form_data.Encoding().Utf8();
  }

  GURL client_side_redirect_url;
  if (info->is_client_redirect)
    client_side_redirect_url = frame_->GetDocument().Url();

  mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token(
      CloneBlobURLToken(info->blob_url_token));

  int load_flags = info->url_request.GetLoadFlagsForWebUrlRequest();
  std::unique_ptr<base::DictionaryValue> initiator;
  if (!info->devtools_initiator_info.IsNull()) {
    initiator = base::DictionaryValue::From(
        base::JSONReader::ReadDeprecated(info->devtools_initiator_info.Utf8()));
  }

  RenderFrameImpl* initiator_render_frame =
      RenderFrameImpl::FromWebFrame(info->initiator_frame);
  int initiator_frame_routing_id = initiator_render_frame
                                       ? initiator_render_frame->GetRoutingID()
                                       : MSG_ROUTING_NONE;

  mojom::BeginNavigationParamsPtr begin_navigation_params =
      mojom::BeginNavigationParams::New(
          initiator_frame_routing_id,
          GetWebURLRequestHeadersAsString(info->url_request), load_flags,
          info->url_request.GetSkipServiceWorker(),
          GetRequestContextTypeForWebURLRequest(info->url_request),
          GetRequestDestinationForWebURLRequest(info->url_request),
          GetMixedContentContextTypeForWebURLRequest(info->url_request),
          is_form_submission, was_initiated_by_link_click, searchable_form_url,
          searchable_form_encoding, client_side_redirect_url,
          initiator ? base::make_optional<base::Value>(std::move(*initiator))
                    : base::nullopt,
          info->url_request.GetExtraData()->force_ignore_site_for_cookies(),
          info->url_request.TrustTokenParams()
              ? info->url_request.TrustTokenParams()->Clone()
              : nullptr,
          info->impression
              ? base::make_optional<Impression>(
                    ConvertWebImpressionToImpression(*info->impression))
              : base::nullopt,
          renderer_before_unload_start, renderer_before_unload_end);

  mojo::PendingAssociatedRemote<mojom::NavigationClient>
      navigation_client_remote;
  BindNavigationClient(
      navigation_client_remote.InitWithNewEndpointAndPassReceiver());
  navigation_client_impl_->MarkWasInitiatedInThisFrame();

  mojo::PendingRemote<blink::mojom::NavigationInitiator> navigation_initiator =
      std::move(info->navigation_initiator_remote);

  bool current_frame_has_download_sandbox_flag = !frame_->IsAllowedToDownload();
  bool has_download_sandbox_flag =
      info->initiator_frame_has_download_sandbox_flag ||
      current_frame_has_download_sandbox_flag;
  bool from_ad = info->initiator_frame_is_ad || frame_->IsAdSubframe();

  GetFrameHost()->BeginNavigation(
      MakeCommonNavigationParams(frame_->GetSecurityOrigin(), std::move(info),
                                 load_flags, has_download_sandbox_flag, from_ad,
                                 is_history_navigation_in_new_child_frame),
      std::move(begin_navigation_params), std::move(blob_url_token),
      std::move(navigation_client_remote), std::move(navigation_initiator));
}

void RenderFrameImpl::DecodeDataURL(
    const mojom::CommonNavigationParams& common_params,
    const mojom::CommitNavigationParams& commit_params,
    std::string* mime_type,
    std::string* charset,
    std::string* data,
    GURL* base_url) {
  // A loadData request with a specified base URL.
  GURL data_url = common_params.url;
#if defined(OS_ANDROID)
  if (!commit_params.data_url_as_string.empty()) {
#if DCHECK_IS_ON()
    {
      std::string mime_type_tmp, charset_tmp, data_tmp;
      DCHECK(net::DataURL::Parse(data_url, &mime_type_tmp, &charset_tmp,
                                 &data_tmp));
      DCHECK(data_tmp.empty());
    }
#endif
    data_url = GURL(commit_params.data_url_as_string);
    if (!data_url.is_valid() || !data_url.SchemeIs(url::kDataScheme)) {
      data_url = common_params.url;
    }
  }
#endif
  if (net::DataURL::Parse(data_url, mime_type, charset, data)) {
    *base_url = common_params.base_url_for_data_url.is_empty()
                    ? common_params.url
                    : common_params.base_url_for_data_url;
  } else {
    CHECK(false) << "Invalid URL passed: "
                 << common_params.url.possibly_invalid_spec();
  }
}

void RenderFrameImpl::SendUpdateState() {
  if (current_history_item_.IsNull())
    return;

  GetFrameHost()->UpdateState(
      SingleHistoryItemToPageState(current_history_item_));
}

GURL RenderFrameImpl::GetLoadingUrl() const {
  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();

  GURL overriden_url;
  if (MaybeGetOverriddenURL(document_loader, &overriden_url))
    return overriden_url;

  return document_loader->GetUrl();
}

media::MediaPermission* RenderFrameImpl::GetMediaPermission() {
  if (!media_permission_dispatcher_)
    media_permission_dispatcher_.reset(new MediaPermissionDispatcher(this));
  return media_permission_dispatcher_.get();
}

#if BUILDFLAG(ENABLE_PLUGINS)
void RenderFrameImpl::HandlePepperImeCommit(const base::string16& text) {
  if (text.empty())
    return;

  if (!IsPepperAcceptingCompositionEvents()) {
    // For pepper plugins unable to handle IME events, send the plugin a
    // sequence of characters instead.
    base::i18n::UTF16CharIterator iterator(&text);
    int32_t i = 0;
    while (iterator.Advance()) {
      blink::WebKeyboardEvent char_event(blink::WebInputEvent::Type::kChar,
                                         blink::WebInputEvent::kNoModifiers,
                                         ui::EventTimeForNow());
      char_event.windows_key_code = text[i];
      char_event.native_key_code = text[i];

      const int32_t char_start = i;
      for (; i < iterator.array_pos(); ++i) {
        char_event.text[i - char_start] = text[i];
        char_event.unmodified_text[i - char_start] = text[i];
      }

      if (GetLocalRootRenderWidget()->GetWebWidget())
        GetLocalRootRenderWidget()->GetWebWidget()->HandleInputEvent(
            blink::WebCoalescedInputEvent(char_event, ui::LatencyInfo()));
    }
  } else {
    // Mimics the order of events sent by WebKit.
    // See WebCore::Editor::setComposition() for the corresponding code.
    focused_pepper_plugin_->HandleCompositionEnd(text);
    focused_pepper_plugin_->HandleTextInput(text);
  }
  pepper_composition_text_.clear();
}
#endif  // ENABLE_PLUGINS

void RenderFrameImpl::RegisterMojoInterfaces() {
  GetAssociatedInterfaceRegistry()->AddInterface(base::BindRepeating(
      &RenderFrameImpl::BindAutoplayConfiguration, weak_factory_.GetWeakPtr()));

  GetAssociatedInterfaceRegistry()->AddInterface(base::BindRepeating(
      &RenderFrameImpl::BindFrameBindingsControl, weak_factory_.GetWeakPtr()));

  GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&RenderFrameImpl::BindFrameNavigationControl,
                          weak_factory_.GetWeakPtr()));

  GetAssociatedInterfaceRegistry()->AddInterface(base::BindRepeating(
      &RenderFrameImpl::BindNavigationClient, weak_factory_.GetWeakPtr()));

  GetAssociatedInterfaceRegistry()->AddInterface(base::BindRepeating(
      &RenderFrameImpl::BindFullscreen, weak_factory_.GetWeakPtr()));

  GetAssociatedInterfaceRegistry()->AddInterface(base::BindRepeating(
      &RenderFrameImpl::BindMhtmlFileWriter, base::Unretained(this)));

  GetAssociatedInterfaceRegistry()->AddInterface(base::BindRepeating(
      &RenderAccessibilityManager::BindReceiver,
      base::Unretained(render_accessibility_manager_.get())));
}

void RenderFrameImpl::BindMhtmlFileWriter(
    mojo::PendingAssociatedReceiver<mojom::MhtmlFileWriter> receiver) {
  mhtml_file_writer_receiver_.Bind(
      std::move(receiver), GetTaskRunner(blink::TaskType::kInternalDefault));
}

// TODO(https://crbug.com/787252): Move this method to Blink, and eliminate
// the plumbing logic through blink::WebLocalFrameClient.
void RenderFrameImpl::CheckIfAudioSinkExistsAndIsAuthorized(
    const blink::WebString& sink_id,
    blink::WebSetSinkIdCompleteCallback completion_callback) {
  std::move(
      blink::ConvertToOutputDeviceStatusCB(std::move(completion_callback)))
      .Run(blink::WebAudioDeviceFactory::GetOutputDeviceInfo(
               GetWebFrame()->GetLocalFrameToken(),
               media::AudioSinkParameters(base::UnguessableToken(),
                                          sink_id.Utf8()))
               .device_status());
}

std::unique_ptr<blink::WebURLLoaderFactory>
RenderFrameImpl::CreateURLLoaderFactory() {
  if (!RenderThreadImpl::current()) {
    // Some tests (e.g. RenderViewTests) do not have RenderThreadImpl,
    // and must create a factory override instead.
    if (web_url_loader_factory_override_for_test_)
      return web_url_loader_factory_override_for_test_->Clone();

    // If the override does not exist, try looking in the ancestor chain since
    // we might have created child frames and asked them to create a URL loader
    // factory.
    for (auto* ancestor = GetWebFrame()->Parent(); ancestor;
         ancestor = ancestor->Parent()) {
      RenderFrameImpl* ancestor_frame = RenderFrameImpl::FromWebFrame(ancestor);
      if (ancestor_frame &&
          ancestor_frame->web_url_loader_factory_override_for_test_) {
        return ancestor_frame->web_url_loader_factory_override_for_test_
            ->Clone();
      }
    }
    // At this point we can't create anything.
    NOTREACHED();
    return nullptr;
  }
  return std::make_unique<FrameURLLoaderFactory>(weak_factory_.GetWeakPtr());
}

void RenderFrameImpl::OnStopLoading() {
  for (auto& observer : observers_)
    observer.OnStop();
}

void RenderFrameImpl::MaybeProxyURLLoaderFactory(
    blink::CrossVariantMojoReceiver<
        network::mojom::URLLoaderFactoryInterfaceBase>* factory_receiver) {
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver(
      std::move(*factory_receiver));
  GetContentClient()->renderer()->MaybeProxyURLLoaderFactory(this, &receiver);
  *factory_receiver = std::move(receiver);
}

void RenderFrameImpl::DraggableRegionsChanged() {
  for (auto& observer : observers_)
    observer.DraggableRegionsChanged();
}

bool RenderFrameImpl::IsBrowserSideNavigationPending() {
  return browser_side_navigation_pending_;
}

void RenderFrameImpl::LoadHTMLString(const std::string& html,
                                     const GURL& base_url,
                                     const std::string& text_encoding,
                                     const GURL& unreachable_url,
                                     bool replace_current_item) {
  AssertNavigationCommits assert_navigation_commits(
      this, kMayReplaceInitialEmptyDocument);

  pending_loader_factories_ = CreateLoaderFactoryBundle(
      ChildPendingURLLoaderFactoryBundle::CreateFromDefaultFactoryImpl(
          std::make_unique<network::NotImplementedURLLoaderFactory>()),
      base::nullopt,  // |subresource_overrides|
      {});            // prefetch_loader_factory

  auto navigation_params = std::make_unique<WebNavigationParams>();
  navigation_params->url = base_url;
  WebNavigationParams::FillStaticResponse(navigation_params.get(), "text/html",
                                          WebString::FromUTF8(text_encoding),
                                          html);
  navigation_params->unreachable_url = unreachable_url;
  navigation_params->frame_load_type =
      replace_current_item ? blink::WebFrameLoadType::kReplaceCurrentItem
                           : blink::WebFrameLoadType::kStandard;
  navigation_params->service_worker_network_provider =
      ServiceWorkerNetworkProviderForFrame::CreateInvalidInstance();
  frame_->CommitNavigation(std::move(navigation_params), BuildDocumentState());
}

scoped_refptr<base::SingleThreadTaskRunner> RenderFrameImpl::GetTaskRunner(
    blink::TaskType task_type) {
  return GetWebFrame()->GetTaskRunner(task_type);
}

int RenderFrameImpl::GetEnabledBindings() {
  return enabled_bindings_;
}

void RenderFrameImpl::SetAccessibilityModeForTest(ui::AXMode new_mode) {
  render_accessibility_manager_->SetMode(new_mode.mode());
}

scoped_refptr<network::SharedURLLoaderFactory>
RenderFrameImpl::GetURLLoaderFactory() {
  return GetLoaderFactoryBundle();
}

const RenderFrameMediaPlaybackOptions&
RenderFrameImpl::GetRenderFrameMediaPlaybackOptions() {
  return renderer_media_playback_options_;
}

void RenderFrameImpl::SetRenderFrameMediaPlaybackOptions(
    const RenderFrameMediaPlaybackOptions& opts) {
  renderer_media_playback_options_ = opts;
}

void RenderFrameImpl::SetAllowsCrossBrowsingInstanceFrameLookup() {
  GetWebFrame()->SetAllowsCrossBrowsingInstanceFrameLookup();
}

bool RenderFrameImpl::IsAccessibilityEnabled() const {
  return render_accessibility_manager_->GetAccessibilityMode().has_mode(
      ui::AXMode::kWebContents);
}

#if BUILDFLAG(ENABLE_PLUGINS)
void RenderFrameImpl::PepperInstanceCreated(
    PepperPluginInstanceImpl* instance) {
  active_pepper_instances_.insert(instance);

  Send(new FrameHostMsg_PepperInstanceCreated(routing_id_,
                                              instance->pp_instance()));
}

void RenderFrameImpl::PepperInstanceDeleted(
    PepperPluginInstanceImpl* instance) {
  active_pepper_instances_.erase(instance);

  if (focused_pepper_plugin_ == instance)
    PepperFocusChanged(instance, false);

  RenderFrameImpl* const render_frame = instance->render_frame();
  if (render_frame) {
    render_frame->Send(new FrameHostMsg_PepperInstanceDeleted(
        render_frame->GetRoutingID(), instance->pp_instance()));
  }
}

void RenderFrameImpl::PepperFocusChanged(PepperPluginInstanceImpl* instance,
                                         bool focused) {
  if (focused)
    focused_pepper_plugin_ = instance;
  else if (focused_pepper_plugin_ == instance)
    focused_pepper_plugin_ = nullptr;

  GetLocalRootRenderWidget()->UpdateTextInputState();
  GetLocalRootRenderWidget()->UpdateSelectionBounds();
}

void RenderFrameImpl::PepperStartsPlayback(PepperPluginInstanceImpl* instance) {
  RenderFrameImpl* const render_frame = instance->render_frame();
  if (render_frame) {
    render_frame->Send(new FrameHostMsg_PepperStartsPlayback(
        render_frame->GetRoutingID(), instance->pp_instance()));
  }
}

void RenderFrameImpl::PepperStopsPlayback(PepperPluginInstanceImpl* instance) {
  RenderFrameImpl* const render_frame = instance->render_frame();
  if (render_frame) {
    render_frame->Send(new FrameHostMsg_PepperStopsPlayback(
        render_frame->GetRoutingID(), instance->pp_instance()));
  }
}

void RenderFrameImpl::OnSetPepperVolume(int32_t pp_instance, double volume) {
  PepperPluginInstanceImpl* instance = static_cast<PepperPluginInstanceImpl*>(
      PepperPluginInstance::Get(pp_instance));
  if (instance)
    instance->audio_controller().SetVolume(volume);
}
#endif  // ENABLE_PLUGINS

void RenderFrameImpl::ShowCreatedWindow(bool opened_by_user_gesture,
                                        RenderWidget* render_widget_to_show,
                                        WebNavigationPolicy policy,
                                        const gfx::Rect& initial_rect) {
  // |render_widget_to_show| is the main RenderWidget for a pending window
  // created by this object, but not yet shown. The tab is currently offscreen,
  // and still owned by the opener. Sending |FrameHostMsg_ShowCreatedWindow|
  // will move it off the opener's pending list, and put it in its own tab or
  // window.
  //
  // This call happens only for renderer-created windows; for example, when a
  // tab is created by script via window.open().
  GetFrameHost()->ShowCreatedWindow(
      render_widget_to_show->routing_id(),
      RenderViewImpl::NavigationPolicyToDisposition(policy), initial_rect,
      opened_by_user_gesture);
}

blink::WebComputedAXTree* RenderFrameImpl::GetOrCreateWebComputedAXTree() {
  if (!computed_ax_tree_)
    computed_ax_tree_ = std::make_unique<AomContentAxTree>(this);
  return computed_ax_tree_.get();
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
RenderFrameImpl::CreateWebSocketHandshakeThrottle() {
  WebLocalFrame* web_local_frame = GetWebFrame();
  if (!web_local_frame)
    return nullptr;
  auto* render_frame = content::RenderFrame::FromWebFrame(web_local_frame);
  if (!render_frame)
    return nullptr;
  int render_frame_id = render_frame->GetRoutingID();

  // Lazily create the provider.
  if (!websocket_handshake_throttle_provider_) {
    websocket_handshake_throttle_provider_ =
        GetContentClient()
            ->renderer()
            ->CreateWebSocketHandshakeThrottleProvider();
    if (!websocket_handshake_throttle_provider_)
      return nullptr;
  }

  return websocket_handshake_throttle_provider_->CreateThrottle(
      render_frame_id,
      render_frame->GetTaskRunner(blink::TaskType::kInternalDefault));
}

void RenderFrameImpl::AbortCommitNavigation() {
  DCHECK(navigation_client_impl_);
  // Interface disconnection will trigger
  // RenderFrameHostImpl::OnCrossDocumentCommitProcessed().
  navigation_client_impl_.reset();
}

bool RenderFrameImpl::GetCaretBoundsFromFocusedPlugin(gfx::Rect& rect) {
#if BUILDFLAG(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    rect = focused_pepper_plugin_->GetCaretBounds();
    return true;
  }
#endif
  return false;
}

void RenderFrameImpl::AddMessageToConsoleImpl(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message,
    bool discard_duplicates) {
  blink::WebConsoleMessage wcm(level, WebString::FromUTF8(message));
  frame_->AddMessageToConsole(wcm, discard_duplicates);
}

void RenderFrameImpl::SetWebURLLoaderFactoryOverrideForTest(
    std::unique_ptr<blink::WebURLLoaderFactoryForTest> factory) {
  web_url_loader_factory_override_for_test_ = std::move(factory);
}

gfx::RectF RenderFrameImpl::ElementBoundsInWindow(
    const blink::WebElement& element) {
  blink::WebRect bounding_box_in_window = element.BoundsInViewport();
  GetLocalRootRenderWidget()->ConvertViewportToWindow(&bounding_box_in_window);
  return gfx::RectF(bounding_box_in_window);
}

void RenderFrameImpl::ConvertViewportToWindow(blink::WebRect* rect) {
  GetLocalRootRenderWidget()->ConvertViewportToWindow(rect);
}

float RenderFrameImpl::GetDeviceScaleFactor() {
  return GetLocalRootRenderWidget()
      ->GetWebWidget()
      ->GetScreenInfo()
      .device_scale_factor;
}

}  // namespace content
