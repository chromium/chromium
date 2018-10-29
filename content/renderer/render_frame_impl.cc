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
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/asan_invalid_access.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/i18n/char_iterator.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/common/accessibility_messages.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_security_policy/content_security_policy.h"
#include "content/common/content_security_policy_header.h"
#include "content/common/download/mhtml_save_status.h"
#include "content/common/edit_command.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/navigation_gesture.h"
#include "content/common/navigation_params.h"
#include "content/common/page_messages.h"
#include "content/common/possibly_associated_wrapper_shared_url_loader_factory.h"
#include "content/common/renderer_host.mojom.h"
#include "content/common/savable_subframe.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/appcache_info.h"
#include "content/public/common/bind_interface_helpers.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/favicon_url.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/page_state.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_loader_throttle.h"
#include "content/public/common/url_utils.h"
#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/context_menu_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/accessibility/aom_content_ax_tree.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/appcache/appcache_dispatcher.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/content_security_policy_util.h"
#include "content/renderer/context_menu_params_builder.h"
#include "content/renderer/crash_helpers.h"
#include "content/renderer/dom_automation_controller.h"
#include "content/renderer/effective_connection_type_helper.h"
#include "content/renderer/external_popup_menu.h"
#include "content/renderer/frame_owner_properties.h"
#include "content/renderer/gpu/gpu_benchmarking_extension.h"
#include "content/renderer/gpu/layer_tree_view.h"
#include "content/renderer/history_entry.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/image_downloader/image_downloader_impl.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/frame_input_handler_impl.h"
#include "content/renderer/input/input_target_client_impl.h"
#include "content/renderer/input/widget_input_handler_manager.h"
#include "content/renderer/installedapp/related_apps_fetcher.h"
#include "content/renderer/internal_document_state_data.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/tracked_child_url_loader_factory_bundle.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/loader/web_worker_fetch_context_impl.h"
#include "content/renderer/loader/weburlresponse_extradata_impl.h"
#include "content/renderer/low_memory_mode_controller.h"
#include "content/renderer/manifest/manifest_change_notifier.h"
#include "content/renderer/manifest/manifest_manager.h"
#include "content/renderer/media/audio/audio_device_factory.h"
#include "content/renderer/media/audio/audio_output_ipc_factory.h"
#include "content/renderer/media/audio/audio_renderer_sink_cache.h"
#include "content/renderer/media/media_permission_dispatcher.h"
#include "content/renderer/media/stream/media_stream_device_observer.h"
#include "content/renderer/media/stream/user_media_client_impl.h"
#include "content/renderer/media/webrtc/rtc_peer_connection_handler.h"
#include "content/renderer/mojo/blink_interface_registry_impl.h"
#include "content/renderer/navigation_client.h"
#include "content/renderer/navigation_state.h"
#include "content/renderer/pepper/pepper_audio_controller.h"
#include "content/renderer/pepper/plugin_instance_throttler_impl.h"
#include "content/renderer/push_messaging/push_messaging_client.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/renderer_webapplicationcachehost_impl.h"
#include "content/renderer/resource_timing_info_conversions.h"
#include "content/renderer/savable_resources.h"
#include "content/renderer/service_worker/service_worker_network_provider.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "content/renderer/shared_worker/shared_worker_repository.h"
#include "content/renderer/skia_benchmarking_extension.h"
#include "content/renderer/stats_collection_controller.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "content/renderer/web_ui_extension.h"
#include "content/renderer/web_ui_extension_data.h"
#include "crypto/sha2.h"
#include "media/blink/webmediaplayer_util.h"
#include "net/base/data_url.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/common/frame/user_activation_update_type.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/modules/permissions/permission.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_context_features.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_file_chooser_completion.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_frame_serializer.h"
#include "third_party/blink/public/web/web_frame_serializer_cache_control_policy.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_navigation_timings.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_document.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_scoped_user_gesture.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_searchable_form_data.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_surrounding_text.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
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
#include "third_party/blink/public/platform/web_float_point.h"
#endif

using base::Time;
using base::TimeDelta;
using blink::WebContentDecryptionModule;
using blink::WebContextMenuData;
using blink::WebData;
using blink::WebDocumentLoader;
using blink::WebDocument;
using blink::WebDOMEvent;
using blink::WebDOMMessageEvent;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebExternalPopupMenu;
using blink::WebExternalPopupMenuClient;
using blink::WebFrame;
using blink::WebFrameLoadType;
using blink::WebFrameSerializer;
using blink::WebFrameSerializerClient;
using blink::WebHistoryItem;
using blink::WebHTTPBody;
using blink::WebLayerTreeView;
using blink::WebLocalFrame;
using blink::WebMediaPlayer;
using blink::WebMediaPlayerClient;
using blink::WebMediaPlayerEncryptedMediaClient;
using blink::WebNavigationPolicy;
using blink::WebNavigationType;
using blink::WebNode;
using blink::WebPluginDocument;
using blink::WebPluginParams;
using blink::WebPoint;
using blink::WebPopupMenuInfo;
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
using blink::WebUserGestureIndicator;
using blink::WebVector;
using blink::WebView;
using blink::mojom::SelectionMenuBehavior;
using network::mojom::ReferrerPolicy;

#if defined(OS_ANDROID)
using blink::WebFloatPoint;
using blink::WebFloatRect;
#endif

namespace content {

namespace {

const int kExtraCharsBeforeAndAfterSelection = 100;

// Maximum number of burst download requests allowed.
const int kBurstDownloadLimit = 10;

const PreviewsState kDisabledPreviewsBits =
    PREVIEWS_OFF | PREVIEWS_NO_TRANSFORM;

// Print up to |kMaxCertificateWarningMessages| console messages per frame
// about certificates that will be distrusted in future.
const uint32_t kMaxCertificateWarningMessages = 10;

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

ui::PageTransition GetTransitionType(blink::WebDocumentLoader* document_loader,
                                     blink::WebLocalFrame* frame,
                                     bool loading) {
  NavigationState* navigation_state =
      NavigationState::FromDocumentLoader(document_loader);
  ui::PageTransition default_transition =
      navigation_state->IsContentInitiated()
          ? ui::PAGE_TRANSITION_LINK
          : navigation_state->common_params().transition;
  if (navigation_state->WasWithinSameDocument())
    return default_transition;
  if (loading || document_loader->GetResponse().IsNull()) {
    if (document_loader->ReplacesCurrentHistoryItem() && frame->Parent()) {
      // Subframe navigations that don't add session history items must be
      // marked with AUTO_SUBFRAME. See also didFailProvisionalLoad for how we
      // handle loading of error pages.
      return ui::PAGE_TRANSITION_AUTO_SUBFRAME;
    }
    bool is_form_submit = document_loader->GetNavigationType() ==
                              blink::kWebNavigationTypeFormSubmitted ||
                          document_loader->GetNavigationType() ==
                              blink::kWebNavigationTypeFormResubmitted;
    if (ui::PageTransitionCoreTypeIs(default_transition,
                                     ui::PAGE_TRANSITION_LINK) &&
        is_form_submit) {
      return ui::PAGE_TRANSITION_FORM_SUBMIT;
    }
  }
  return default_transition;
}

WebURLResponseExtraDataImpl* GetExtraDataFromResponse(
    const WebURLResponse& response) {
  return static_cast<WebURLResponseExtraDataImpl*>(response.GetExtraData());
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
// url is the same as ds->getRequest()->url(). If the WebDocumentLoader belongs
// to a frame was loaded by loadData, the original url will be
// ds->unreachableURL()
GURL GetOriginalRequestURL(WebDocumentLoader* document_loader) {
  GURL overriden_url;
  if (MaybeGetOverriddenURL(document_loader, &overriden_url))
    return overriden_url;

  std::vector<GURL> redirects;
  GetRedirectChain(document_loader, &redirects);
  if (!redirects.empty())
    return redirects.at(0);

  return document_loader->OriginalRequest().Url();
}

// Returns false unless this is a top-level navigation.
bool IsTopLevelNavigation(WebFrame* frame) {
  return frame->Parent() == nullptr;
}

WebURLRequest CreateURLRequestForNavigation(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    std::unique_ptr<NavigationResponseOverrideParameters> response_override,
    bool is_view_source_mode_enabled) {
  // Use the original navigation url to construct the WebURLRequest. The
  // WebURLloaderImpl will replay the redirects afterwards and will eventually
  // commit the final url.
  const GURL navigation_url = !request_params.original_url.is_empty()
                                  ? request_params.original_url
                                  : common_params.url;
  const std::string navigation_method = !request_params.original_method.empty()
                                            ? request_params.original_method
                                            : common_params.method;
  WebURLRequest request(navigation_url);
  request.SetHTTPMethod(WebString::FromUTF8(navigation_method));

  if (is_view_source_mode_enabled)
    request.SetCacheMode(blink::mojom::FetchCacheMode::kForceCache);

  WebString web_referrer;
  if (common_params.referrer.url.is_valid()) {
    web_referrer = WebSecurityPolicy::GenerateReferrerHeader(
        common_params.referrer.policy, common_params.url,
        WebString::FromUTF8(common_params.referrer.url.spec()));
    request.SetHTTPReferrer(web_referrer, common_params.referrer.policy);
    if (!web_referrer.IsEmpty()) {
      request.SetHTTPOriginIfNeeded(
          WebSecurityOrigin(url::Origin::Create(common_params.referrer.url)));
    }
  }

  if (common_params.post_data) {
    request.SetHTTPBody(GetWebHTTPBodyForRequestBody(*common_params.post_data));
    if (!request_params.post_content_type.empty()) {
      request.AddHTTPHeaderField(
          WebString::FromASCII(net::HttpRequestHeaders::kContentType),
          WebString::FromASCII(request_params.post_content_type));
    }
  }

  if (!web_referrer.IsEmpty() || common_params.referrer.policy !=
                                     network::mojom::ReferrerPolicy::kDefault) {
    request.SetHTTPReferrer(web_referrer, common_params.referrer.policy);
  }

  request.SetPreviewsState(
      static_cast<WebURLRequest::PreviewsState>(common_params.previews_state));

  request.SetOriginPolicy(WebString::FromUTF8(common_params.origin_policy));

  auto extra_data = std::make_unique<RequestExtraData>();
  extra_data->set_navigation_response_override(std::move(response_override));
  extra_data->set_navigation_initiated_by_renderer(
      request_params.nav_entry_id == 0);
  request.SetExtraData(std::move(extra_data));
  request.SetWasDiscarded(request_params.was_discarded);

  return request;
}

CommonNavigationParams MakeCommonNavigationParams(
    const blink::WebLocalFrameClient::NavigationPolicyInfo& info,
    int load_flags,
    base::TimeTicks input_start) {
  Referrer referrer(
      GURL(info.url_request.HttpHeaderField(WebString::FromUTF8("Referer"))
               .Latin1()),
      info.url_request.GetReferrerPolicy());

  // No history-navigation is expected to happen.
  DCHECK(info.navigation_type != blink::kWebNavigationTypeBackForward);

  // Determine the navigation type. No same-document navigation is expected
  // because it is loaded immediately by the FrameLoader.
  FrameMsg_Navigate_Type::Value navigation_type =
      FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT;
  if (info.navigation_type == blink::kWebNavigationTypeReload) {
    if (load_flags & net::LOAD_BYPASS_CACHE)
      navigation_type = FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE;
    else
      navigation_type = FrameMsg_Navigate_Type::RELOAD;
  }

  base::Optional<SourceLocation> source_location;
  if (!info.source_location.url.IsNull()) {
    source_location = SourceLocation(info.source_location.url.Latin1(),
                                     info.source_location.line_number,
                                     info.source_location.column_number);
  }

  CSPDisposition should_check_main_world_csp =
      info.should_check_main_world_content_security_policy ==
              blink::kWebContentSecurityPolicyDispositionCheck
          ? CSPDisposition::CHECK
          : CSPDisposition::DO_NOT_CHECK;

  const RequestExtraData* extra_data =
      static_cast<RequestExtraData*>(info.url_request.GetExtraData());
  DCHECK(extra_data);
  return CommonNavigationParams(
      info.url_request.Url(), referrer, extra_data->transition_type(),
      navigation_type, true, info.replaces_current_history_item, GURL(), GURL(),
      static_cast<PreviewsState>(info.url_request.GetPreviewsState()),
      base::TimeTicks::Now(), info.url_request.HttpMethod().Latin1(),
      GetRequestBodyForWebURLRequest(info.url_request), source_location,
      false /* started_from_context_menu */, info.url_request.HasUserGesture(),
      InitiatorCSPInfo(
          should_check_main_world_csp,
          BuildContentSecurityPolicyList(info.url_request.GetInitiatorCSP()),
          info.url_request.GetInitiatorCSP().self_source.has_value()
              ? base::Optional<CSPSource>(BuildCSPSource(
                    info.url_request.GetInitiatorCSP().self_source.value()))
              : base::nullopt),
      input_start);
}

WebFrameLoadType NavigationTypeToLoadType(
    FrameMsg_Navigate_Type::Value navigation_type,
    bool should_replace_current_entry,
    bool has_valid_page_state) {
  switch (navigation_type) {
    case FrameMsg_Navigate_Type::RELOAD:
    case FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL:
      return WebFrameLoadType::kReload;

    case FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE:
      return WebFrameLoadType::kReloadBypassingCache;

    case FrameMsg_Navigate_Type::HISTORY_SAME_DOCUMENT:
    case FrameMsg_Navigate_Type::HISTORY_DIFFERENT_DOCUMENT:
      return WebFrameLoadType::kBackForward;

    case FrameMsg_Navigate_Type::RESTORE:
    case FrameMsg_Navigate_Type::RESTORE_WITH_POST:
      if (has_valid_page_state)
        return WebFrameLoadType::kBackForward;
      // If there is no valid page state, fall through to the default case.
      FALLTHROUGH;

    case FrameMsg_Navigate_Type::SAME_DOCUMENT:
    case FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT:
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
RenderFrameImpl::CreateRenderWidgetForChildLocalRootFunction
    g_create_render_widget = nullptr;
RenderFrameImpl::RenderWidgetForChildLocalRootInitializedCallback
    g_render_widget_initialized = nullptr;

WebString ConvertRelativePathToHtmlAttribute(const base::FilePath& path) {
  DCHECK(!path.IsAbsolute());
  return WebString::FromUTF8(
      std::string("./") +
      path.NormalizePathSeparatorsTo(FILE_PATH_LITERAL('/')).AsUTF8Unsafe());
}

// Implementation of WebFrameSerializer::LinkRewritingDelegate that responds
// based on the payload of FrameMsg_GetSerializedHtmlWithLocalLinks.
class LinkRewritingDelegate : public WebFrameSerializer::LinkRewritingDelegate {
 public:
  LinkRewritingDelegate(
      const std::map<GURL, base::FilePath>& url_to_local_path,
      const std::map<int, base::FilePath>& frame_routing_id_to_local_path)
      : url_to_local_path_(url_to_local_path),
        frame_routing_id_to_local_path_(frame_routing_id_to_local_path) {}

  bool RewriteFrameSource(WebFrame* frame, WebString* rewritten_link) override {
    int routing_id = RenderFrame::GetRoutingIdForWebFrame(frame);
    auto it = frame_routing_id_to_local_path_.find(routing_id);
    if (it == frame_routing_id_to_local_path_.end())
      return false;  // This can happen because of https://crbug.com/541354.

    const base::FilePath& local_path = it->second;
    *rewritten_link = ConvertRelativePathToHtmlAttribute(local_path);
    return true;
  }

  bool RewriteLink(const WebURL& url, WebString* rewritten_link) override {
    auto it = url_to_local_path_.find(url);
    if (it == url_to_local_path_.end())
      return false;

    const base::FilePath& local_path = it->second;
    *rewritten_link = ConvertRelativePathToHtmlAttribute(local_path);
    return true;
  }

 private:
  const std::map<GURL, base::FilePath>& url_to_local_path_;
  const std::map<int, base::FilePath>& frame_routing_id_to_local_path_;
};

// Implementation of WebFrameSerializer::MHTMLPartsGenerationDelegate that
// 1. Bases shouldSkipResource and getContentID responses on contents of
//    FrameMsg_SerializeAsMHTML_Params.
// 2. Stores digests of urls of serialized resources (i.e. urls reported via
//    shouldSkipResource) into |serialized_resources_uri_digests| passed
//    to the constructor.
class MHTMLPartsGenerationDelegate
    : public WebFrameSerializer::MHTMLPartsGenerationDelegate {
 public:
  MHTMLPartsGenerationDelegate(
      const FrameMsg_SerializeAsMHTML_Params& params,
      std::set<std::string>* serialized_resources_uri_digests)
      : params_(params),
        serialized_resources_uri_digests_(serialized_resources_uri_digests) {
    DCHECK(serialized_resources_uri_digests_);
  }

  bool ShouldSkipResource(const WebURL& url) override {
    std::string digest =
        crypto::SHA256HashString(params_.salt + GURL(url).spec());

    // Skip if the |url| already covered by serialization of an *earlier* frame.
    if (base::ContainsKey(params_.digests_of_uris_to_skip, digest))
      return true;

    // Let's record |url| as being serialized for the *current* frame.
    auto pair = serialized_resources_uri_digests_->insert(digest);
    bool insertion_took_place = pair.second;
    DCHECK(insertion_took_place);  // Blink should dedupe within a frame.

    return false;
  }

  blink::WebFrameSerializerCacheControlPolicy CacheControlPolicy() override {
    return params_.mhtml_cache_control_policy;
  }

  bool UseBinaryEncoding() override { return params_.mhtml_binary_encoding; }

  bool RemovePopupOverlay() override {
    return params_.mhtml_popup_overlay_removal;
  }

  bool UsePageProblemDetectors() override {
    return params_.mhtml_problem_detection;
  }

 private:
  const FrameMsg_SerializeAsMHTML_Params& params_;
  std::set<std::string>* serialized_resources_uri_digests_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLPartsGenerationDelegate);
};

bool IsHttpPost(const blink::WebURLRequest& request) {
  return request.HttpMethod().Utf8() == "POST";
}

// Writes to file the serialized and encoded MHTML data from WebThreadSafeData
// instances.
MhtmlSaveStatus WriteMHTMLToDisk(std::vector<WebThreadSafeData> mhtml_contents,
                                 base::File file) {
  TRACE_EVENT0("page-serialization", "WriteMHTMLToDisk (RenderFrameImpl)");
  SCOPED_UMA_HISTOGRAM_TIMER(
      "PageSerialization.MhtmlGeneration.WriteToDiskTime.SingleFrame");
  DCHECK(!RenderThread::Get()) << "Should not run in the main renderer thread";
  MhtmlSaveStatus save_status = MhtmlSaveStatus::SUCCESS;
  for (const WebThreadSafeData& data : mhtml_contents) {
    if (!data.IsEmpty() &&
        file.WriteAtCurrentPos(data.Data(), data.size()) < 0) {
      save_status = MhtmlSaveStatus::FILE_WRITTING_ERROR;
      break;
    }
  }
  // Explicitly close |file| here to make sure to include any flush operations
  // in the UMA metric.
  file.Close();
  return save_status;
}

FaviconURL::IconType ToFaviconType(blink::WebIconURL::Type type) {
  switch (type) {
    case blink::WebIconURL::kTypeFavicon:
      return FaviconURL::IconType::kFavicon;
    case blink::WebIconURL::kTypeTouch:
      return FaviconURL::IconType::kTouchIcon;
    case blink::WebIconURL::kTypeTouchPrecomposed:
      return FaviconURL::IconType::kTouchPrecomposedIcon;
    case blink::WebIconURL::kTypeInvalid:
      return FaviconURL::IconType::kInvalid;
  }
  NOTREACHED();
  return FaviconURL::IconType::kInvalid;
}

std::vector<gfx::Size> ConvertToFaviconSizes(
    const blink::WebVector<blink::WebSize>& web_sizes) {
  std::vector<gfx::Size> result;
  result.reserve(web_sizes.size());
  for (const blink::WebSize& web_size : web_sizes)
    result.push_back(gfx::Size(web_size));
  return result;
}

// Use this for histograms with dynamically generated names, which otherwise
// can't use the UMA_HISTOGRAM_MEMORY_MB macro without code duplication.
void RecordSuffixedMemoryMBHistogram(base::StringPiece name,
                                     base::StringPiece suffix,
                                     int sample_mb) {
  std::string name_with_suffix;
  name.CopyToString(&name_with_suffix);
  suffix.AppendToString(&name_with_suffix);
  base::UmaHistogramMemoryMB(name_with_suffix, sample_mb);
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

// See also LOG_NAVIGATION_TIMING_HISTOGRAM in NavigationHandleImpl.
void RecordReadyToCommitUntilCommitHistogram(base::TimeDelta delay,
                                             ui::PageTransition transition) {
  UMA_HISTOGRAM_TIMES("Navigation.Renderer.ReadyToCommitUntilCommit", delay);
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {
    UMA_HISTOGRAM_TIMES(
        "Navigation.Renderer.ReadyToCommitUntilCommit.BackForward", delay);
  } else if (ui::PageTransitionCoreTypeIs(transition,
                                          ui::PAGE_TRANSITION_RELOAD)) {
    UMA_HISTOGRAM_TIMES("Navigation.Renderer.ReadyToCommitUntilCommit.Reload",
                        delay);
  } else if (ui::PageTransitionIsNewNavigation(transition)) {
    UMA_HISTOGRAM_TIMES(
        "Navigation.Renderer.ReadyToCommitUntilCommit.NewNavigation", delay);
  } else {
    NOTREACHED() << "Invalid page transition: " << transition;
  }
}

blink::mojom::BlobURLTokenPtrInfo CloneBlobURLToken(
    mojo::MessagePipeHandle handle) {
  if (!handle.is_valid())
    return nullptr;
  blink::mojom::BlobURLTokenPtrInfo result;
  blink::mojom::BlobURLTokenPtr token(
      blink::mojom::BlobURLTokenPtrInfo(mojo::ScopedMessagePipeHandle(handle),
                                        blink::mojom::BlobURLToken::Version_));
  token->Clone(MakeRequest(&result));
  ignore_result(token.PassInterface().PassHandle().release());
  return result;
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
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    base::TimeTicks time_commit_requested,
    mojom::FrameNavigationControl::CommitNavigationCallback commit_callback,
    const network::ResourceResponseHead* head) {
  std::unique_ptr<DocumentState> document_state(new DocumentState());
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state.get());

  DCHECK(!common_params.navigation_start.is_null());
  DCHECK(!common_params.url.SchemeIs(url::kJavaScriptScheme));

  if (common_params.navigation_type == FrameMsg_Navigate_Type::RESTORE) {
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
    internal_data->set_cache_policy_override(
        blink::mojom::FetchCacheMode::kDefault);
  }

  internal_data->set_is_overriding_user_agent(
      request_params.is_overriding_user_agent);
  internal_data->set_must_reset_scroll_and_scale_state(
      common_params.navigation_type ==
      FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL);
  document_state->set_can_load_local_resources(
      request_params.can_load_local_resources);

  if (head) {
    if (head->headers)
      internal_data->set_http_status_code(head->headers->response_code());
    document_state->set_was_fetched_via_spdy(head->was_fetched_via_spdy);
    document_state->set_was_alpn_negotiated(head->was_alpn_negotiated);
    document_state->set_alpn_negotiated_protocol(
        head->alpn_negotiated_protocol);
    document_state->set_was_alternate_protocol_available(
        head->was_alternate_protocol_available);
    document_state->set_connection_info(head->connection_info);
  }

  bool load_data = !common_params.base_url_for_data_url.is_empty() &&
                   !common_params.history_url_for_data_url.is_empty() &&
                   common_params.url.SchemeIs(url::kDataScheme);
  document_state->set_was_load_data_with_base_url_request(load_data);
  if (load_data)
    document_state->set_data_url(common_params.url);

  InternalDocumentStateData::FromDocumentState(document_state.get())
      ->set_navigation_state(NavigationState::CreateBrowserInitiated(
          common_params, request_params, time_commit_requested,
          std::move(commit_callback)));
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
  request->SetURL(blink::WebURL(GURL(path)));
}

// Packs all navigation timings sent by the browser to a blink understandable
// format, blink::WebNavigationTimings.
blink::WebNavigationTimings BuildNavigationTimings(
    base::TimeTicks navigation_start,
    const NavigationTiming& browser_navigation_timings,
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

// Packs all loading data sent by the browser to a blink understandable
// format, blink::WebNavigationParams.
std::unique_ptr<blink::WebNavigationParams> BuildNavigationParams(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
        service_worker_network_provider) {
  std::unique_ptr<blink::WebNavigationParams> navigation_params =
      std::make_unique<blink::WebNavigationParams>();
  navigation_params->navigation_timings = BuildNavigationTimings(
      common_params.navigation_start, request_params.navigation_timing,
      common_params.input_start);

  if (common_params.source_location.has_value()) {
    navigation_params->source_location.url =
        WebString::FromLatin1(common_params.source_location->url);
    navigation_params->source_location.line_number =
        common_params.source_location->line_number;
    navigation_params->source_location.column_number =
        common_params.source_location->column_number;
  }

  navigation_params->is_user_activated =
      request_params.was_activated == WasActivatedOption::kYes;
  navigation_params->service_worker_network_provider =
      std::move(service_worker_network_provider);
  return navigation_params;
}

}  // namespace

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

    mojom::KeepAliveHandlePtr keep_alive_handle;
    if (request.GetKeepalive()) {
      frame_->GetFrameHost()->IssueKeepAliveHandle(
          mojo::MakeRequest(&keep_alive_handle));
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

const std::string& UniqueNameForWebFrame(blink::WebFrame* frame) {
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

std::vector<base::StringPiece>
RenderFrameImpl::UniqueNameFrameAdapter::CollectAncestorNames(
    BeginPoint begin_point,
    bool (*should_stop)(base::StringPiece)) const {
  std::vector<base::StringPiece> result;
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
    RenderViewImpl* render_view,
    int32_t routing_id,
    service_manager::mojom::InterfaceProviderPtr interface_provider,
    const base::UnguessableToken& devtools_frame_token) {
  DCHECK(routing_id != MSG_ROUTING_NONE);
  CreateParams params(render_view, routing_id, std::move(interface_provider),
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
  auto iter = g_routing_id_frame_map.Get().find(routing_id);
  if (iter != g_routing_id_frame_map.Get().end())
    return iter->second;
  return nullptr;
}

// static
RenderFrameImpl* RenderFrameImpl::CreateMainFrame(
    RenderViewImpl* render_view,
    int32_t routing_id,
    service_manager::mojom::InterfaceProviderPtr interface_provider,
    int32_t widget_routing_id,
    bool hidden,
    const ScreenInfo& screen_info,
    CompositorDependencies* compositor_deps,
    blink::WebFrame* opener,
    const base::UnguessableToken& devtools_frame_token,
    const FrameReplicationState& replicated_state,
    bool has_committed_real_load) {
  // A main frame RenderFrame must have a RenderWidget.
  DCHECK_NE(MSG_ROUTING_NONE, widget_routing_id);

  RenderFrameImpl* render_frame = RenderFrameImpl::Create(
      render_view, routing_id, std::move(interface_provider),
      devtools_frame_token);
  render_frame->InitializeBlameContext(nullptr);
  WebLocalFrame* web_frame = WebLocalFrame::CreateMainFrame(
      render_view->webview(), render_frame,
      render_frame->blink_interface_registry_.get(), opener,
      // This conversion is a little sad, as this often comes from a
      // WebString...
      WebString::FromUTF8(replicated_state.name),
      replicated_state.frame_policy.sandbox_flags);
  if (has_committed_real_load)
    web_frame->SetCommittedFirstRealLoad();

  // The RenderViewImpl and its RenderWidget already exist by the time we get
  // here.
  // TODO(crbug.com/419087): We probably want to create the RenderWidget here
  // though (when we make the WebFrameWidget?).
  RenderWidget* render_widget = render_view->GetWidget();

  // Non-owning pointer that is self-referencing and destroyed by calling
  // Close(). The RenderViewImpl has a RenderWidget already, but not a
  // WebFrameWidget, which is now attached here.
  auto* web_frame_widget = blink::WebFrameWidget::CreateForMainFrame(
      render_view->WidgetClient(), web_frame);
  render_view->AttachWebFrameWidget(web_frame_widget);
  // TODO(crbug.com/419087): This was added in 6ccadf770766e89c3 to prevent an
  // empty ScreenInfo, but the WebView has already been created and initialized
  // by RenderViewImpl, so this is surely redundant?
  render_widget->UpdateWebViewWithDeviceScaleFactor();

  render_frame->render_widget_ = render_widget;
  render_frame->in_frame_tree_ = true;
  return render_frame;
}

// static
void RenderFrameImpl::CreateFrame(
    int routing_id,
    service_manager::mojom::InterfaceProviderPtr interface_provider,
    int proxy_routing_id,
    int opener_routing_id,
    int parent_routing_id,
    int previous_sibling_routing_id,
    const base::UnguessableToken& devtools_frame_token,
    const FrameReplicationState& replicated_state,
    CompositorDependencies* compositor_deps,
    const mojom::CreateFrameWidgetParams& widget_params,
    const FrameOwnerProperties& frame_owner_properties,
    bool has_committed_real_load) {
  // TODO(danakj): Split this method into two pieces. The first block makes a
  // WebLocalFrame and collects the RenderView and RenderFrame for it. The
  // second block uses that to make/setup a RenderWidget, if needed.
  RenderViewImpl* render_view = nullptr;
  RenderFrameImpl* render_frame = nullptr;
  blink::WebLocalFrame* web_frame = nullptr;
  if (proxy_routing_id == MSG_ROUTING_NONE) {
    // TODO(alexmos): This path is currently used only:
    // 1) When recreating a RenderFrame after a crash.
    // 2) In tests that issue this IPC directly.
    // These two cases should be cleaned up to also pass a proxy_routing_id,
    // which would allow removing this branch altogether.  See
    // https://crbug.com/756790.

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
        parent_proxy->render_view(), routing_id, std::move(interface_provider),
        devtools_frame_token);
    render_frame->InitializeBlameContext(FromRoutingID(parent_routing_id));
    render_frame->unique_name_helper_.set_propagated_name(
        replicated_state.unique_name);
    web_frame = parent_web_frame->CreateLocalChild(
        replicated_state.scope, WebString::FromUTF8(replicated_state.name),
        replicated_state.frame_policy.sandbox_flags, render_frame,
        render_frame->blink_interface_registry_.get(),
        previous_sibling_web_frame,
        replicated_state.frame_policy.container_policy,
        ConvertFrameOwnerPropertiesToWebFrameOwnerProperties(
            frame_owner_properties),
        replicated_state.frame_owner_element_type,
        ResolveOpener(opener_routing_id));

    // The RenderFrame is created and inserted into the frame tree in the above
    // call to createLocalChild.
    render_frame->in_frame_tree_ = true;
  } else {
    RenderFrameProxy* proxy =
        RenderFrameProxy::FromRoutingID(proxy_routing_id);
    // The remote frame could've been detached while the remote-to-local
    // navigation was being initiated in the browser process. Drop the
    // navigation and don't create the frame in that case.  See
    // https://crbug.com/526304.
    if (!proxy)
      return;

    // This path is creating a local frame. It may or may not be a local root,
    // depending if the frame's parent is local or remote. It may also be the
    // main frame, as in the case where a navigation to the current process'
    // origin replaces a remote main frame (the proxy's web_frame()) with a
    // local one.
    const bool proxy_is_main_frame = !proxy->web_frame()->Parent();

    render_view = proxy->render_view();
    render_frame = RenderFrameImpl::Create(render_view, routing_id,
                                           std::move(interface_provider),
                                           devtools_frame_token);
    render_frame->InitializeBlameContext(nullptr);
    render_frame->proxy_routing_id_ = proxy_routing_id;
    proxy->set_provisional_frame_routing_id(routing_id);
    web_frame = blink::WebLocalFrame::CreateProvisional(
        render_frame, render_frame->blink_interface_registry_.get(),
        proxy->web_frame(), replicated_state.frame_policy.sandbox_flags,
        replicated_state.frame_policy.container_policy);
    // The new |web_frame| is a main frame iff the proxy's frame was.
    DCHECK_EQ(proxy_is_main_frame, !web_frame->Parent());
  }

  DCHECK(render_view);
  DCHECK(render_frame);
  DCHECK(web_frame);

  const bool is_main_frame = !web_frame->Parent();

  // Child frames require there to be a |parent_routing_id| present, for the
  // remote parent frame. Though it is only used if the |proxy_routing_id| is
  // not given, which happens in some corner cases.
  if (!is_main_frame)
    DCHECK_NE(parent_routing_id, MSG_ROUTING_NONE);

  // We now have a WebLocalFrame for the new frame. The next step is to set
  // up a RenderWidget for it, if it is needed.
  //
  // If there is no widget routing id, then the new frame is not a local root,
  // and does not need a RenderWidget. In that case we'll do nothing. Otherwise
  // it does.
  if (is_main_frame) {
    // For a main frame, we use the RenderWidget already attached to the
    // RenderView (this is being changed by https://crbug.com/419087).

    // Main frames are always local roots, so they should always have a routing
    // id. Surprisingly, this routing id is *not* used though, as the routing id
    // on the existing RenderWidget is not changed. (I don't know why.)
    // TODO(crbug.com/888105): It's a bug that the RenderWidget is not using
    // this routing id.
    DCHECK_NE(widget_params.routing_id, MSG_ROUTING_NONE);

    // The RenderViewImpl and its RenderWidget already exist by the time we
    // get here (we get them from the RenderFrameProxy).
    // TODO(crbug.com/419087): We probably want to create the RenderWidget
    // here though (when we make the WebFrameWidget?).
    RenderWidget* render_widget = render_view->GetWidget();

    // Non-owning pointer that is self-referencing and destroyed by calling
    // Close(). The RenderViewImpl has a RenderWidget already, but not a
    // WebFrameWidget, which is now attached here.
    auto* web_frame_widget = blink::WebFrameWidget::CreateForMainFrame(
        render_view->WidgetClient(), web_frame);
    render_view->AttachWebFrameWidget(web_frame_widget);
    // TODO(crbug.com/419087): This was added in 6ccadf770766e89c3 to prevent
    // an empty ScreenInfo, but the WebView has already been created and
    // initialized by RenderViewImpl, so this is surely redundant? It will be
    // pulling the device scale factor off the WebView itself.
    render_widget->UpdateWebViewWithDeviceScaleFactor();

    render_frame->render_widget_ = render_widget;
  } else if (widget_params.routing_id != MSG_ROUTING_NONE) {
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

    // TODO(crbug.com/419087): This is grabbing something off the view's
    // widget but if the main frame is remote this widget would not be valid?
    const ScreenInfo& screen_info_from_main_frame =
        render_view->GetWidget()->GetWebScreenInfo();

    // Makes a new RenderWidget for the child local root. It provides the
    // local root with a new compositing, painting, and input coordinate
    // space/context.
    scoped_refptr<RenderWidget> render_widget;
    if (g_create_render_widget) {
      // LayoutTest hooks inject a different type (subclass) for RenderWidget,
      // allowing it to override the behaviour of the WebWidgetClient which
      // RenderWidget provides.
      render_widget = g_create_render_widget(
          widget_params.routing_id, compositor_deps, WidgetType::kFrame,
          screen_info_from_main_frame, blink::kWebDisplayModeUndefined,
          /*swapped_out=*/false, widget_params.hidden,
          /*never_visible=*/false);
    } else {
      render_widget = base::MakeRefCounted<RenderWidget>(
          widget_params.routing_id, compositor_deps, WidgetType::kFrame,
          screen_info_from_main_frame, blink::kWebDisplayModeUndefined,
          /*swapped_out=*/false, widget_params.hidden,
          /*never_visible=*/false);
    }

    // Non-owning pointer that is self-referencing and destroyed by calling
    // Close(). We use the new RenderWidget as the client for this
    // WebFrameWidget, *not* the RenderWidget of the MainFrame, which is
    // accessible from the RenderViewImpl.
    auto* web_frame_widget = blink::WebFrameWidget::CreateForChildLocalRoot(
        render_widget.get(), web_frame);

    // Adds a reference on RenderWidget, making it self-referencing. So it
    // will not be destroyed by scoped_refptr unless Close() has been called
    // and run.
    render_widget->InitForChildLocalRoot(web_frame_widget);
    // TODO(crbug.com/419087): This was added in 6ccadf770766e89c3 to prevent
    // an empty ScreenInfo, but the WebView has already been created and
    // initialized by RenderViewImpl, so this is surely redundant? It will be
    // pulling the device scale factor off the WebView itself.
    render_widget->UpdateWebViewWithDeviceScaleFactor();

    // LayoutTest hooks to set up the injected type for RenderWidget.
    if (g_render_widget_initialized)
      g_render_widget_initialized(render_widget.get());

    render_frame->render_widget_ = std::move(render_widget);
  }

  if (has_committed_real_load)
    web_frame->SetCommittedFirstRealLoad();

  render_frame->Initialize();
}

// static
RenderFrame* RenderFrame::FromWebFrame(blink::WebLocalFrame* web_frame) {
  return RenderFrameImpl::FromWebFrame(web_frame);
}

// static
void RenderFrame::ForEach(RenderFrameVisitor* visitor) {
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
  auto iter = g_frame_map.Get().find(web_frame);
  if (iter != g_frame_map.Get().end())
    return iter->second;
  return nullptr;
}

// static
void RenderFrameImpl::InstallCreateHook(
    CreateRenderFrameImplFunction create_frame,
    CreateRenderWidgetForChildLocalRootFunction create_widget,
    RenderWidgetForChildLocalRootInitializedCallback widget_initialized) {
  DCHECK(!g_create_render_frame_impl);
  DCHECK(!g_create_render_widget);
  DCHECK(!g_render_widget_initialized);
  g_create_render_frame_impl = create_frame;
  g_create_render_widget = create_widget;
  g_render_widget_initialized = widget_initialized;
}

// static
blink::WebFrame* RenderFrameImpl::ResolveOpener(int opener_frame_routing_id) {
  if (opener_frame_routing_id == MSG_ROUTING_NONE)
    return nullptr;

  // Opener routing ID could refer to either a RenderFrameProxy or a
  // RenderFrame, so need to check both.
  RenderFrameProxy* opener_proxy =
      RenderFrameProxy::FromRoutingID(opener_frame_routing_id);
  if (opener_proxy)
    return opener_proxy->web_frame();

  RenderFrameImpl* opener_frame =
      RenderFrameImpl::FromRoutingID(opener_frame_routing_id);
  if (opener_frame)
    return opener_frame->GetWebFrame();

  return nullptr;
}

blink::WebURL RenderFrameImpl::OverrideFlashEmbedWithHTML(
    const blink::WebURL& url) {
  return GetContentClient()->renderer()->OverrideFlashEmbedWithHTML(url);
}

// RenderFrameImpl::CreateParams --------------------------------------------

RenderFrameImpl::CreateParams::CreateParams(
    RenderViewImpl* render_view,
    int32_t routing_id,
    service_manager::mojom::InterfaceProviderPtr interface_provider,
    const base::UnguessableToken& devtools_frame_token)
    : render_view(render_view),
      routing_id(routing_id),
      interface_provider(std::move(interface_provider)),
      devtools_frame_token(devtools_frame_token) {}
RenderFrameImpl::CreateParams::~CreateParams() = default;
RenderFrameImpl::CreateParams::CreateParams(CreateParams&&) = default;
RenderFrameImpl::CreateParams& RenderFrameImpl::CreateParams::operator=(
    CreateParams&&) = default;

// RenderFrameImpl ----------------------------------------------------------
RenderFrameImpl::RenderFrameImpl(CreateParams params)
    : frame_(nullptr),
      is_main_frame_(true),
      unique_name_frame_adapter_(this),
      unique_name_helper_(&unique_name_frame_adapter_),
      in_browser_initiated_detach_(false),
      in_frame_tree_(false),
      render_view_(params.render_view),
      routing_id_(params.routing_id),
      proxy_routing_id_(MSG_ROUTING_NONE),
#if BUILDFLAG(ENABLE_PLUGINS)
      plugin_power_saver_helper_(nullptr),
#endif
      cookie_jar_(this),
      selection_text_offset_(0),
      selection_range_(gfx::Range::InvalidRange()),
      handling_select_range_(false),
      web_user_media_client_(nullptr),
      push_messaging_client_(nullptr),
      render_accessibility_(nullptr),
      previews_state_(PREVIEWS_UNSPECIFIED),
      effective_connection_type_(
          blink::WebEffectiveConnectionType::kTypeUnknown),
      is_pasting_(false),
      suppress_further_dialogs_(false),
      blame_context_(nullptr),
#if BUILDFLAG(ENABLE_PLUGINS)
      focused_pepper_plugin_(nullptr),
      pepper_last_mouse_event_target_(nullptr),
#endif
      autoplay_configuration_binding_(this),
      frame_binding_(this),
      host_zoom_binding_(this),
      frame_bindings_control_binding_(this),
      frame_navigation_control_binding_(this),
      fullscreen_binding_(this),
      navigation_client_impl_(nullptr),
      has_accessed_initial_document_(false),
      media_factory_(this,
                     base::Bind(&RenderFrameImpl::RequestOverlayRoutingToken,
                                base::Unretained(this))),
      input_target_client_impl_(this),
      devtools_frame_token_(params.devtools_frame_token),
      weak_factory_(this) {
  // The InterfaceProvider to access Mojo services exposed by the RFHI must be
  // provided at construction time. See: https://crbug.com/729021/.
  CHECK(params.interface_provider.is_bound());
  remote_interfaces_.Bind(std::move(params.interface_provider));
  blink_interface_registry_.reset(new BlinkInterfaceRegistryImpl(
      registry_.GetWeakPtr(), associated_interfaces_.GetWeakPtr()));

  // Must call after binding our own remote interfaces.
  media_factory_.SetupMojo();

  std::pair<RoutingIDFrameMap::iterator, bool> result =
      g_routing_id_frame_map.Get().insert(std::make_pair(routing_id_, this));
  CHECK(result.second) << "Inserting a duplicate item.";

  RenderThread::Get()->AddRoute(routing_id_, this);

  // Everything below subclasses RenderFrameObserver and is automatically
  // deleted when the RenderFrame gets deleted.
#if defined(OS_ANDROID)
  new GinJavaBridgeDispatcher(this);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  // Manages its own lifetime.
  plugin_power_saver_helper_ = new PluginPowerSaverHelper(this);
#endif

  manifest_manager_ = std::make_unique<ManifestManager>(this);
  // TODO(ajwong): This always returns true as is_main_frame_ gets initialized
  // later in RenderFrameImpl::Initialize(). Should the conditional be in
  // RenderFrameImpl::Initialize()?  https://crbug.com/840533
  if (IsMainFrame()) {
    // Manages its own lifetime.
    new ManifestChangeNotifier(this);
  }
}

mojom::FrameHost* RenderFrameImpl::GetFrameHost() {
  if (!frame_host_ptr_.is_bound())
    GetRemoteAssociatedInterfaces()->GetInterface(&frame_host_ptr_);
  return frame_host_ptr_.get();
}

RenderFrameImpl::~RenderFrameImpl() {
  // If file chooser is still waiting for answer, dispatch empty answer.
  if (file_chooser_completion_) {
    file_chooser_completion_->DidChooseFile(WebVector<WebString>());
    file_chooser_completion_ = nullptr;
  }

  for (auto& observer : observers_)
    observer.RenderFrameGone();
  for (auto& observer : observers_)
    observer.OnDestruct();

  base::trace_event::TraceLog::GetInstance()->RemoveProcessLabel(routing_id_);

  if (auto* factory = AudioOutputIPCFactory::get())
    factory->MaybeDeregisterRemoteFactory(GetRoutingID());

  // |thread| may be null in tests.
  if (auto* thread = RenderThreadImpl::current()) {
    if (auto* controller = thread->low_memory_mode_controller())
      controller->OnFrameDestroyed(IsMainFrame());
  }

  if (is_main_frame_) {
    // Ensure the RenderView doesn't point to this object, once it is destroyed.
    // TODO(nasko): Add a check that the |main_render_frame_| of |render_view_|
    // is |this|, once the object is no longer leaked.
    // See https://crbug.com/464764.
    render_view_->main_render_frame_ = nullptr;
  }

  g_routing_id_frame_map.Get().erase(routing_id_);
  RenderThread::Get()->RemoveRoute(routing_id_);
}

void RenderFrameImpl::Initialize() {
  is_main_frame_ = !frame_->Parent();

  GetRenderWidget()->RegisterRenderFrame(this);

  RenderFrameImpl* parent_frame =
      RenderFrameImpl::FromWebFrame(frame_->Parent());
  if (parent_frame) {
    previews_state_ = parent_frame->GetPreviewsState();
    effective_connection_type_ = parent_frame->GetEffectiveConnectionType();
  }

  bool is_tracing_rail = false;
  bool is_tracing_navigation = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("navigation", &is_tracing_navigation);
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("rail", &is_tracing_rail);
  if (is_tracing_rail || is_tracing_navigation) {
    int parent_id = RenderFrame::GetRoutingIdForWebFrame(frame_->Parent());
    TRACE_EVENT2("navigation,rail", "RenderFrameImpl::Initialize",
                 "id", routing_id_,
                 "parent", parent_id);
  }

  // |thread| may be null in tests.
  if (auto* thread = RenderThreadImpl::current()) {
    if (auto* controller = thread->low_memory_mode_controller())
      controller->OnFrameCreated(IsMainFrame());
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  new PepperBrowserConnection(this);
#endif
  shared_worker_repository_ =
      std::make_unique<SharedWorkerRepository>(GetInterfaceProvider());
  GetWebFrame()->SetSharedWorkerRepositoryClient(
      shared_worker_repository_.get());

  RegisterMojoInterfaces();

  // We delay calling this until we have the WebFrame so that any observer or
  // embedder can call GetWebFrame on any RenderFrame.
  GetContentClient()->renderer()->RenderFrameCreated(this);

  // AudioOutputIPCFactory may be null in tests.
  if (auto* factory = AudioOutputIPCFactory::get())
    factory->RegisterRemoteFactory(GetRoutingID(), GetRemoteInterfaces());

  AudioRendererSinkCache::ObserveFrame(this);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDomAutomationController))
    enabled_bindings_ |= BINDINGS_POLICY_DOM_AUTOMATION;
  if (command_line.HasSwitch(switches::kStatsCollectionController))
    enabled_bindings_ |= BINDINGS_POLICY_STATS_COLLECTION;
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    frame_request_blocker_ = base::MakeRefCounted<FrameRequestBlocker>();
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

RenderWidget* RenderFrameImpl::GetRenderWidget() {
  return GetLocalRoot()->render_widget_.get();
}

#if BUILDFLAG(ENABLE_PLUGINS)
void RenderFrameImpl::PepperPluginCreated(RendererPpapiHost* host) {
  for (auto& observer : observers_)
    observer.DidCreatePepperPlugin(host);
}

void RenderFrameImpl::PepperDidChangeCursor(
    PepperPluginInstanceImpl* instance,
    const blink::WebCursorInfo& cursor) {
  // Update the cursor appearance immediately if the requesting plugin is the
  // one which receives the last mouse event. Otherwise, the new cursor won't be
  // picked up until the plugin gets the next input event. That is bad if, e.g.,
  // the plugin would like to set an invisible cursor when there isn't any user
  // input for a while.
  if (instance == pepper_last_mouse_event_target_)
    GetRenderWidget()->DidChangeCursor(cursor);
}

void RenderFrameImpl::PepperDidReceiveMouseEvent(
    PepperPluginInstanceImpl* instance) {
  set_pepper_last_mouse_event_target(instance);
}

void RenderFrameImpl::PepperTextInputTypeChanged(
    PepperPluginInstanceImpl* instance) {
  if (instance != focused_pepper_plugin_)
    return;

  GetRenderWidget()->UpdateTextInputState();

  FocusedNodeChangedForAccessibility(WebNode());
}

void RenderFrameImpl::PepperCaretPositionChanged(
    PepperPluginInstanceImpl* instance) {
  if (instance != focused_pepper_plugin_)
    return;
  GetRenderWidget()->UpdateSelectionBounds();
}

void RenderFrameImpl::PepperCancelComposition(
    PepperPluginInstanceImpl* instance) {
  if (instance != focused_pepper_plugin_)
    return;
  if (mojom::WidgetInputHandlerHost* host = GetRenderWidget()
                                                ->widget_input_handler_manager()
                                                ->GetWidgetInputHandlerHost()) {
    host->ImeCancelComposition();
  }
#if defined(OS_MACOSX) || defined(USE_AURA)
  GetRenderWidget()->UpdateCompositionInfo(
      false /* not an immediate request */);
#endif
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
  WebFrame* main_frame = render_view()->webview()->MainFrame();
  if (main_frame->IsWebLocalFrame())
    main_frame_url = main_frame->ToWebLocalFrame()->GetDocument().Url();

  mojom::WidgetPtr widget_channel;
  mojom::WidgetRequest widget_channel_request =
      mojo::MakeRequest(&widget_channel);

  // Synchronous IPC to obtain a routing id for the fullscreen widget.
  int32_t fullscreen_widget_routing_id = MSG_ROUTING_NONE;
  if (!RenderThreadImpl::current_render_message_filter()
           ->CreateFullscreenWidget(render_view()->GetRoutingID(),
                                    std::move(widget_channel),
                                    &fullscreen_widget_routing_id)) {
    return nullptr;
  }
  RenderWidget::ShowCallback show_callback =
      base::BindOnce(&RenderViewImpl::ShowCreatedFullscreenWidget,
                     render_view()->GetWeakPtr());

  // TODO(fsamuel): It's not clear if we should be passing in the
  // web ScreenInfo or the original ScreenInfo here.
  RenderWidgetFullscreenPepper* widget = RenderWidgetFullscreenPepper::Create(
      fullscreen_widget_routing_id, std::move(show_callback),
      GetRenderWidget()->compositor_deps(), plugin, std::move(main_frame_url),
      GetRenderWidget()->GetWebScreenInfo(), std::move(widget_channel_request));
  // TODO(nick): The show() handshake seems like unnecessary complexity here,
  // since there's no real delay between CreateFullscreenWidget and
  // ShowCreatedFullscreenWidget. Would it be simpler to have the
  // CreateFullscreenWidget mojo method implicitly show the window, and skip the
  // subsequent step?
  widget->Show(blink::kWebNavigationPolicyIgnore);
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
    const std::vector<blink::WebImeTextSpan>& ime_text_spans,
    int selection_start,
    int selection_end) {
  render_view_->OnImeSetComposition(text, ime_text_spans,
                                    gfx::Range::InvalidRange(), selection_start,
                                    selection_end);
}

void RenderFrameImpl::SimulateImeCommitText(
    const base::string16& text,
    const std::vector<blink::WebImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range) {
  render_view_->OnImeCommitText(text, ime_text_spans, replacement_range, 0);
}

void RenderFrameImpl::SimulateImeFinishComposingText(bool keep_selection) {
  render_view_->OnImeFinishComposingText(keep_selection);
}

void RenderFrameImpl::OnImeSetComposition(
    const base::string16& text,
    const std::vector<blink::WebImeTextSpan>& ime_text_spans,
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

MediaStreamDeviceObserver* RenderFrameImpl::GetMediaStreamDeviceObserver() {
  if (!web_user_media_client_)
    InitializeUserMediaClient();
  return web_user_media_client_
             ? web_user_media_client_->media_stream_device_observer()
             : nullptr;
}

void RenderFrameImpl::ScriptedPrint(bool user_initiated) {
  for (auto& observer : observers_)
    observer.ScriptedPrint(user_initiated);
}

bool RenderFrameImpl::Send(IPC::Message* message) {
  return RenderThread::Get()->Send(message);
}

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
void RenderFrameImpl::DidHideExternalPopupMenu() {
  // We need to clear external_popup_menu_ as soon as ExternalPopupMenu::close
  // is called. Otherwise, createExternalPopupMenu() for new popup will fail.
  external_popup_menu_.reset();
}
#endif

bool RenderFrameImpl::OnMessageReceived(const IPC::Message& msg) {
  // Forward Page IPCs to the RenderView.
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
    IPC_MESSAGE_HANDLER(FrameMsg_BeforeUnload, OnBeforeUnload)
    IPC_MESSAGE_HANDLER(FrameMsg_SwapOut, OnSwapOut)
    IPC_MESSAGE_HANDLER(FrameMsg_SwapIn, OnSwapIn)
    IPC_MESSAGE_HANDLER(FrameMsg_Delete, OnDeleteFrame)
    IPC_MESSAGE_HANDLER(FrameMsg_Stop, OnStop)
    IPC_MESSAGE_HANDLER(FrameMsg_DroppedNavigation, OnDroppedNavigation)
    IPC_MESSAGE_HANDLER(FrameMsg_Collapse, OnCollapse)
    IPC_MESSAGE_HANDLER(FrameMsg_ContextMenuClosed, OnContextMenuClosed)
    IPC_MESSAGE_HANDLER(FrameMsg_CustomContextMenuAction,
                        OnCustomContextMenuAction)
#if BUILDFLAG(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER(FrameMsg_SetPepperVolume, OnSetPepperVolume)
#endif
    IPC_MESSAGE_HANDLER(FrameMsg_CopyImageAt, OnCopyImageAt)
    IPC_MESSAGE_HANDLER(FrameMsg_SaveImageAt, OnSaveImageAt)
    IPC_MESSAGE_HANDLER(FrameMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(FrameMsg_JavaScriptExecuteRequest,
                        OnJavaScriptExecuteRequest)
    IPC_MESSAGE_HANDLER(FrameMsg_JavaScriptExecuteRequestForTests,
                        OnJavaScriptExecuteRequestForTests)
    IPC_MESSAGE_HANDLER(FrameMsg_JavaScriptExecuteRequestInIsolatedWorld,
                        OnJavaScriptExecuteRequestInIsolatedWorld)
    IPC_MESSAGE_HANDLER(FrameMsg_VisualStateRequest,
                        OnVisualStateRequest)
    IPC_MESSAGE_HANDLER(FrameMsg_Reload, OnReload)
    IPC_MESSAGE_HANDLER(FrameMsg_ReloadLoFiImages, OnReloadLoFiImages)
    IPC_MESSAGE_HANDLER(FrameMsg_TextSurroundingSelectionRequest,
                        OnTextSurroundingSelectionRequest)
    IPC_MESSAGE_HANDLER(FrameMsg_SetAccessibilityMode,
                        OnSetAccessibilityMode)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_SnapshotTree,
                        OnSnapshotAccessibilityTree)
    IPC_MESSAGE_HANDLER(FrameMsg_UpdateOpener, OnUpdateOpener)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateFramePolicy, OnDidUpdateFramePolicy)
    IPC_MESSAGE_HANDLER(FrameMsg_SetFrameOwnerProperties,
                        OnSetFrameOwnerProperties)
    IPC_MESSAGE_HANDLER(FrameMsg_AdvanceFocus, OnAdvanceFocus)
    IPC_MESSAGE_HANDLER(FrameMsg_AdvanceFocusInForm, OnAdvanceFocusInForm)
    IPC_MESSAGE_HANDLER(FrameMsg_SetFocusedFrame, OnSetFocusedFrame)
    IPC_MESSAGE_HANDLER(FrameMsg_SetTextTrackSettings,
                        OnTextTrackSettingsChanged)
    IPC_MESSAGE_HANDLER(FrameMsg_CheckCompleted, OnCheckCompleted)
    IPC_MESSAGE_HANDLER(FrameMsg_PostMessageEvent, OnPostMessageEvent)
    IPC_MESSAGE_HANDLER(FrameMsg_ReportContentSecurityPolicyViolation,
                        OnReportContentSecurityPolicyViolation)
    IPC_MESSAGE_HANDLER(FrameMsg_GetSavableResourceLinks,
                        OnGetSavableResourceLinks)
    IPC_MESSAGE_HANDLER(FrameMsg_GetSerializedHtmlWithLocalLinks,
                        OnGetSerializedHtmlWithLocalLinks)
    IPC_MESSAGE_HANDLER(FrameMsg_SerializeAsMHTML, OnSerializeAsMHTML)
    IPC_MESSAGE_HANDLER(FrameMsg_EnableViewSourceMode, OnEnableViewSourceMode)
    IPC_MESSAGE_HANDLER(FrameMsg_SuppressFurtherDialogs,
                        OnSuppressFurtherDialogs)
    IPC_MESSAGE_HANDLER(FrameMsg_RunFileChooserResponse, OnFileChooserResponse)
    IPC_MESSAGE_HANDLER(FrameMsg_ClearFocusedElement, OnClearFocusedElement)
    IPC_MESSAGE_HANDLER(FrameMsg_BlinkFeatureUsageReport,
                        OnBlinkFeatureUsageReport)
    IPC_MESSAGE_HANDLER(FrameMsg_MixedContentFound, OnMixedContentFound)
    IPC_MESSAGE_HANDLER(FrameMsg_SetOverlayRoutingToken,
                        OnSetOverlayRoutingToken)
    IPC_MESSAGE_HANDLER(FrameMsg_NotifyUserActivation, OnNotifyUserActivation)
    IPC_MESSAGE_HANDLER(FrameMsg_MediaPlayerActionAt, OnMediaPlayerActionAt)
    IPC_MESSAGE_HANDLER(FrameMsg_RenderFallbackContent, OnRenderFallbackContent)
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(FrameMsg_SelectPopupMenuItem, OnSelectPopupMenuItem)
#else
    IPC_MESSAGE_HANDLER(FrameMsg_SelectPopupMenuItems, OnSelectPopupMenuItems)
#endif
#endif

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

void RenderFrameImpl::BindFullscreen(
    mojom::FullscreenVideoElementHandlerAssociatedRequest request) {
  fullscreen_binding_.Bind(std::move(request),
                           GetTaskRunner(blink::TaskType::kInternalIPC));
}

void RenderFrameImpl::BindAutoplayConfiguration(
    blink::mojom::AutoplayConfigurationClientAssociatedRequest request) {
  autoplay_configuration_binding_.Bind(
      std::move(request), GetTaskRunner(blink::TaskType::kInternalIPC));
}

void RenderFrameImpl::BindFrame(
    const service_manager::BindSourceInfo& browser_info,
    mojom::FrameRequest request) {
  browser_info_ = browser_info;
  frame_binding_.Bind(
      std::move(request),
      frame_ ? GetTaskRunner(blink::TaskType::kInternalIPC) : nullptr);
}

void RenderFrameImpl::BindFrameBindingsControl(
    mojom::FrameBindingsControlAssociatedRequest request) {
  frame_bindings_control_binding_.Bind(
      std::move(request), GetTaskRunner(blink::TaskType::kInternalIPC));
}

void RenderFrameImpl::BindFrameNavigationControl(
    mojom::FrameNavigationControlAssociatedRequest request) {
  frame_navigation_control_binding_.Bind(
      std::move(request), GetTaskRunner(blink::TaskType::kInternalIPC));
}

void RenderFrameImpl::BindNavigationClient(
    mojom::NavigationClientAssociatedRequest request) {
  navigation_client_impl_ = std::make_unique<NavigationClient>(this);
  navigation_client_impl_->Bind(std::move(request));
}

blink::mojom::ManifestManager& RenderFrameImpl::GetManifestManager() {
  return *manifest_manager_;
}

void RenderFrameImpl::OnBeforeUnload(bool is_reload) {
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::OnBeforeUnload",
               "id", routing_id_);
  // Save the routing_id, as the RenderFrameImpl can be deleted in
  // dispatchBeforeUnloadEvent. See https://crbug.com/666714 for details.
  int routing_id = routing_id_;

  base::TimeTicks before_unload_start_time = base::TimeTicks::Now();

  // This will execute the BeforeUnload event in this frame and all of its
  // local descendant frames, including children of remote frames.  The browser
  // process will send separate IPCs to dispatch beforeunload in any
  // out-of-process child frames.
  bool proceed = frame_->DispatchBeforeUnloadEvent(is_reload);

  base::TimeTicks before_unload_end_time = base::TimeTicks::Now();
  RenderThread::Get()->Send(new FrameHostMsg_BeforeUnload_ACK(
      routing_id, proceed, before_unload_start_time, before_unload_end_time));
}

void RenderFrameImpl::OnSwapOut(
    int proxy_routing_id,
    bool is_loading,
    const FrameReplicationState& replicated_frame_state) {
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::OnSwapOut",
               "id", routing_id_);
  RenderFrameProxy* proxy = nullptr;

  // Swap this RenderFrame out so the frame can navigate to a page rendered by
  // a different process.  This involves running the unload handler and
  // clearing the page.

  // Send an UpdateState message before we get deleted.
  SendUpdateState();

  // There should always be a proxy to replace this RenderFrame.  Create it now
  // so its routing id is registered for receiving IPC messages.
  CHECK_NE(proxy_routing_id, MSG_ROUTING_NONE);
  proxy = RenderFrameProxy::CreateProxyToReplaceFrame(
      this, proxy_routing_id, replicated_frame_state.scope);

  // Synchronously run the unload handler before sending the ACK.
  // TODO(creis): Call dispatchUnloadEvent unconditionally here to support
  // unload on subframes as well.
  if (is_main_frame_)
    frame_->DispatchUnloadEvent();

  // Swap out and stop sending any IPC messages that are not ACKs.
  if (is_main_frame_)
    render_view_->SetSwappedOut(true);

  RenderViewImpl* render_view = render_view_;
  bool is_main_frame = is_main_frame_;
  int routing_id = GetRoutingID();

  // Before |this| is destroyed, grab the TaskRunner to be used for sending the
  // SwapOut ACK.  This will be used to schedule SwapOut ACK to be sent after
  // any postMessage IPCs scheduled from the unload event above.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetTaskRunner(blink::TaskType::kPostedMessage);

  // Now that all of the cleanup is complete and the browser side is notified,
  // start using the RenderFrameProxy.
  //
  // The swap call deletes this RenderFrame via frameDetached.  Do not access
  // any members after this call.
  //
  // TODO(creis): WebFrame::swap() can return false.  Most of those cases
  // should be due to the frame being detached during unload (in which case
  // the necessary cleanup has happened anyway), but it might be possible for
  // it to return false without detaching.  Catch any cases that the
  // RenderView's main_render_frame_ isn't cleared below (whether swap returns
  // false or not).
  bool success = frame_->Swap(proxy->web_frame());

  // For main frames, the swap should have cleared the RenderView's pointer to
  // this frame.
  if (is_main_frame)
    CHECK(!render_view->main_render_frame_);

  if (!success) {
    // The swap can fail when the frame is detached during swap (this can
    // happen while running the unload handlers). When that happens, delete
    // the proxy.
    proxy->FrameDetached(blink::WebRemoteFrameClient::DetachType::kSwap);
    return;
  }

  if (is_loading)
    proxy->OnDidStartLoading();

  // Initialize the WebRemoteFrame with the replication state passed by the
  // process that is now rendering the frame.
  proxy->SetReplicatedState(replicated_frame_state);

  // Notify the browser that this frame was swapped. Use the RenderThread
  // directly because |this| is deleted.  Post a task to send the ACK, so that
  // any postMessage IPCs scheduled from the unload handler are sent before
  // the ACK (see https://crbug.com/857274).
  auto send_swapout_ack = base::BindOnce(
      [](int routing_id, bool is_main_frame) {
        RenderThread::Get()->Send(new FrameHostMsg_SwapOut_ACK(routing_id));
      },
      routing_id, is_main_frame);
  task_runner->PostTask(FROM_HERE, std::move(send_swapout_ack));
}

void RenderFrameImpl::OnSwapIn() {
  SwapIn();
}

void RenderFrameImpl::OnDeleteFrame() {
  // TODO(nasko): If this message is received right after a commit has
  // swapped a RenderFrameProxy with this RenderFrame, the proxy needs to be
  // recreated in addition to the RenderFrame being deleted.
  // See https://crbug.com/569683 for details.
  in_browser_initiated_detach_ = true;

  // This will result in a call to RenderFrameImpl::frameDetached, which
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

  render_view()->webview()->DidCloseContextMenu();
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
    render_view_->webview()->PerformCustomContextMenuAction(action);
  }
}

#if defined(OS_MACOSX)
void RenderFrameImpl::OnCopyToFindPboard() {
  // Since the find pasteboard supports only plain text, this can be simpler
  // than the |OnCopy()| case.
  if (frame_->HasSelection()) {
    if (!clipboard_host_) {
      auto* platform = RenderThreadImpl::current_blink_platform_impl();
      platform->GetConnector()->BindInterface(platform->GetBrowserServiceName(),
                                              &clipboard_host_);
    }
    base::string16 selection = frame_->SelectionAsText().Utf16();
    clipboard_host_->WriteStringToFindPboard(selection);
  }
}
#endif

void RenderFrameImpl::OnCopyImageAt(int x, int y) {
  blink::WebFloatRect viewport_position(x, y, 0, 0);
  GetRenderWidget()->ConvertWindowToViewport(&viewport_position);
  frame_->CopyImageAt(WebPoint(viewport_position.x, viewport_position.y));
}

void RenderFrameImpl::OnSaveImageAt(int x, int y) {
  blink::WebFloatRect viewport_position(x, y, 0, 0);
  GetRenderWidget()->ConvertWindowToViewport(&viewport_position);
  frame_->SaveImageAt(WebPoint(viewport_position.x, viewport_position.y));
}

void RenderFrameImpl::OnAddMessageToConsole(ConsoleMessageLevel level,
                                            const std::string& message) {
  AddMessageToConsole(level, message);
}

void RenderFrameImpl::OnJavaScriptExecuteRequest(
    const base::string16& jscript,
    int id,
    bool notify_result) {
  TRACE_EVENT_INSTANT0("test_tracing", "OnJavaScriptExecuteRequest",
                       TRACE_EVENT_SCOPE_THREAD);

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> result = frame_->ExecuteScriptAndReturnValue(
      WebScriptSource(WebString::FromUTF16(jscript)));

  HandleJavascriptExecutionResult(jscript, id, notify_result, result);
}

void RenderFrameImpl::OnJavaScriptExecuteRequestForTests(
    const base::string16& jscript,
    int id,
    bool notify_result,
    bool has_user_gesture) {
  TRACE_EVENT_INSTANT0("test_tracing", "OnJavaScriptExecuteRequestForTests",
                       TRACE_EVENT_SCOPE_THREAD);

  // A bunch of tests expect to run code in the context of a user gesture, which
  // can grant additional privileges (e.g. the ability to create popups).
  std::unique_ptr<blink::WebScopedUserGesture> gesture(
      has_user_gesture ? new blink::WebScopedUserGesture(frame_) : nullptr);
  v8::HandleScope handle_scope(blink::MainThreadIsolate());
  v8::Local<v8::Value> result = frame_->ExecuteScriptAndReturnValue(
      WebScriptSource(WebString::FromUTF16(jscript)));

  HandleJavascriptExecutionResult(jscript, id, notify_result, result);
}

void RenderFrameImpl::OnJavaScriptExecuteRequestInIsolatedWorld(
    const base::string16& jscript,
    int id,
    bool notify_result,
    int world_id) {
  TRACE_EVENT_INSTANT0("test_tracing",
                       "OnJavaScriptExecuteRequestInIsolatedWorld",
                       TRACE_EVENT_SCOPE_THREAD);

  if (world_id <= ISOLATED_WORLD_ID_GLOBAL ||
      world_id > ISOLATED_WORLD_ID_MAX) {
    // Return if the world_id is not valid. world_id is passed as a plain int
    // over IPC and needs to be verified here, in the IPC endpoint.
    NOTREACHED();
    return;
  }

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebScriptSource script = WebScriptSource(WebString::FromUTF16(jscript));
  JavaScriptIsolatedWorldRequest* request = new JavaScriptIsolatedWorldRequest(
      id, notify_result, routing_id_, weak_factory_.GetWeakPtr());
  frame_->RequestExecuteScriptInIsolatedWorld(
      world_id, &script, 1, false, WebLocalFrame::kSynchronous, request);
}

RenderFrameImpl::JavaScriptIsolatedWorldRequest::JavaScriptIsolatedWorldRequest(
    int id,
    bool notify_result,
    int routing_id,
    base::WeakPtr<RenderFrameImpl> render_frame_impl)
    : id_(id),
      notify_result_(notify_result),
      routing_id_(routing_id),
      render_frame_impl_(render_frame_impl) {
}

RenderFrameImpl::JavaScriptIsolatedWorldRequest::
    ~JavaScriptIsolatedWorldRequest() {
}

void RenderFrameImpl::JavaScriptIsolatedWorldRequest::Completed(
    const blink::WebVector<v8::Local<v8::Value>>& result) {
  if (!render_frame_impl_.get()) {
    return;
  }

  if (notify_result_) {
    base::ListValue list;
    if (!result.IsEmpty()) {
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
      for (const auto& value : result) {
        std::unique_ptr<base::Value> result_value(
            converter.FromV8Value(value, context));
        list.Append(result_value ? std::move(result_value)
                                 : std::make_unique<base::Value>());
      }
    } else {
      list.Set(0, std::make_unique<base::Value>());
    }
    render_frame_impl_.get()->Send(
        new FrameHostMsg_JavaScriptExecuteResponse(routing_id_, id_, list));
  }

  delete this;
}

void RenderFrameImpl::HandleJavascriptExecutionResult(
    const base::string16& jscript,
    int id,
    bool notify_result,
    v8::Local<v8::Value> result) {
  if (notify_result) {
    base::ListValue list;
    if (!result.IsEmpty()) {
      v8::Local<v8::Context> context = frame_->MainWorldScriptContext();
      v8::Context::Scope context_scope(context);
      V8ValueConverterImpl converter;
      converter.SetDateAllowed(true);
      converter.SetRegExpAllowed(true);
      std::unique_ptr<base::Value> result_value(
          converter.FromV8Value(result, context));
      list.Set(0, result_value ? std::move(result_value)
                               : std::make_unique<base::Value>());
    } else {
      list.Set(0, std::make_unique<base::Value>());
    }
    Send(new FrameHostMsg_JavaScriptExecuteResponse(routing_id_, id, list));
  }
}

void RenderFrameImpl::OnVisualStateRequest(uint64_t id) {
  GetRenderWidget()->QueueMessage(
      new FrameHostMsg_VisualStateResponse(routing_id_, id));
}

void RenderFrameImpl::OnSetAccessibilityMode(ui::AXMode new_mode) {
  if (accessibility_mode_ == new_mode)
    return;
  ui::AXMode old_mode = accessibility_mode_;
  accessibility_mode_ = new_mode;

  if (new_mode.has_mode(ui::AXMode::kWebContents) &&
      !old_mode.has_mode(ui::AXMode::kWebContents)) {
    render_accessibility_ = new RenderAccessibilityImpl(this, new_mode);
  } else if (!new_mode.has_mode(ui::AXMode::kWebContents) &&
             old_mode.has_mode(ui::AXMode::kWebContents)) {
    delete render_accessibility_;
    render_accessibility_ = nullptr;
  }

  for (auto& observer : observers_)
    observer.AccessibilityModeChanged();
}

void RenderFrameImpl::OnSnapshotAccessibilityTree(int callback_id,
                                                  ui::AXMode ax_mode) {
  AXContentTreeUpdate response;
  RenderAccessibilityImpl::SnapshotAccessibilityTree(this, &response, ax_mode);
  Send(new AccessibilityHostMsg_SnapshotResponse(
      routing_id_, callback_id, response));
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

void RenderFrameImpl::OnUpdateOpener(int opener_routing_id) {
  WebFrame* opener = ResolveOpener(opener_routing_id);
  frame_->SetOpener(opener);
}

void RenderFrameImpl::OnDidUpdateFramePolicy(
    const blink::FramePolicy& frame_policy) {
  frame_->SetFrameOwnerPolicy(frame_policy.sandbox_flags,
                              frame_policy.container_policy);
}

void RenderFrameImpl::OnSetFrameOwnerProperties(
    const FrameOwnerProperties& frame_owner_properties) {
  DCHECK(frame_);
  frame_->SetFrameOwnerProperties(
      ConvertFrameOwnerPropertiesToWebFrameOwnerProperties(
          frame_owner_properties));
}

void RenderFrameImpl::OnAdvanceFocus(blink::WebFocusType type,
                                     int32_t source_routing_id) {
  RenderFrameProxy* source_frame =
      RenderFrameProxy::FromRoutingID(source_routing_id);
  if (!source_frame) {
    render_view_->webview()->SetInitialFocus(type ==
                                             blink::kWebFocusTypeBackward);
    return;
  }

  render_view_->webview()->AdvanceFocusAcrossFrames(
      type, source_frame->web_frame(), frame_);
}

void RenderFrameImpl::OnAdvanceFocusInForm(blink::WebFocusType focus_type) {
  if (render_view_->webview()->FocusedFrame() != frame_)
    return;
  frame_->AdvanceFocusInForm(focus_type);
}

void RenderFrameImpl::OnSetFocusedFrame() {
  // This uses focusDocumentView rather than setFocusedFrame so that focus/blur
  // events are properly dispatched on any currently focused elements.
  render_view_->webview()->FocusDocumentView(frame_);
}

void RenderFrameImpl::OnTextTrackSettingsChanged(
    const FrameMsg_TextTrackSettings_Params& params) {
  DCHECK(!frame_->Parent());
  if (!render_view_->webview())
    return;

  if (params.text_tracks_enabled) {
    render_view_->webview()->GetSettings()->SetTextTrackKindUserPreference(
        WebSettings::TextTrackKindUserPreference::kCaptions);
  } else {
    render_view_->webview()->GetSettings()->SetTextTrackKindUserPreference(
        WebSettings::TextTrackKindUserPreference::kDefault);
  }
  render_view_->webview()->GetSettings()->SetTextTrackBackgroundColor(
      WebString::FromUTF8(params.text_track_background_color));
  render_view_->webview()->GetSettings()->SetTextTrackFontFamily(
      WebString::FromUTF8(params.text_track_font_family));
  render_view_->webview()->GetSettings()->SetTextTrackFontStyle(
      WebString::FromUTF8(params.text_track_font_style));
  render_view_->webview()->GetSettings()->SetTextTrackFontVariant(
      WebString::FromUTF8(params.text_track_font_variant));
  render_view_->webview()->GetSettings()->SetTextTrackTextColor(
      WebString::FromUTF8(params.text_track_text_color));
  render_view_->webview()->GetSettings()->SetTextTrackTextShadow(
      WebString::FromUTF8(params.text_track_text_shadow));
  render_view_->webview()->GetSettings()->SetTextTrackTextSize(
      WebString::FromUTF8(params.text_track_text_size));
}

void RenderFrameImpl::OnCheckCompleted() {
  frame_->CheckCompleted();
}

void RenderFrameImpl::OnPostMessageEvent(FrameMsg_PostMessage_Params params) {
  // This function is called on the per-thread task runner via legacy IPC. From
  // the investigation of task duration on some web sites [1], this IPC message
  // processing is one of the heaviest tasks. Use a per-frame task runner
  // instead to get more efficient scheduing.
  // [1] http://bit.ly/2MqaXfw
  //
  // TODO(hajimehoshi): Replace this legacy IPC usage with Mojo after message
  // ordering is controllable.

  // Ensure the message data is owned by |params| itself so that the data is
  // live even after moved.
  params.message->data.EnsureDataIsOwned();

  frame_->GetTaskRunner(blink::TaskType::kPostedMessage)
      ->PostTask(FROM_HERE,
                 base::BindOnce(&RenderFrameImpl::PostMessageEvent,
                                weak_factory_.GetWeakPtr(), std::move(params)));
}

void RenderFrameImpl::PostMessageEvent(FrameMsg_PostMessage_Params params) {
  // Find the source frame if it exists.
  WebFrame* source_frame = nullptr;
  if (params.source_routing_id != MSG_ROUTING_NONE) {
    RenderFrameProxy* source_proxy =
        RenderFrameProxy::FromRoutingID(params.source_routing_id);
    if (source_proxy)
      source_frame = source_proxy->web_frame();
  }

  // We must pass in the target_origin to do the security check on this side,
  // since it may have changed since the original postMessage call was made.
  WebSecurityOrigin target_origin;
  if (!params.target_origin.empty()) {
    target_origin = WebSecurityOrigin::CreateFromString(
        WebString::FromUTF16(params.target_origin));
  }

  WebDOMMessageEvent msg_event(std::move(params.message->data),
                               WebString::FromUTF16(params.source_origin),
                               source_frame, frame_->GetDocument());

  frame_->DispatchMessageEventWithOriginCheck(
      target_origin, msg_event, params.message->data.has_user_gesture);
}

void RenderFrameImpl::OnReload(bool bypass_cache) {
  frame_->StartReload(bypass_cache ? WebFrameLoadType::kReloadBypassingCache
                                   : WebFrameLoadType::kReload);
}

void RenderFrameImpl::OnReloadLoFiImages() {
  previews_state_ = PREVIEWS_NO_TRANSFORM;
  GetWebFrame()->ReloadLoFiImages();
}

void RenderFrameImpl::OnTextSurroundingSelectionRequest(uint32_t max_length) {
  blink::WebSurroundingText surrounding_text(frame_, max_length);

  if (surrounding_text.IsEmpty()) {
    // |surrounding_text| might not be correctly initialized, for example if
    // |frame_->SelectionRange().IsNull()|, in other words, if there was no
    // selection.
    Send(new FrameHostMsg_TextSurroundingSelectionResponse(
        routing_id_, base::string16(), 0, 0));
    return;
  }

  Send(new FrameHostMsg_TextSurroundingSelectionResponse(
      routing_id_, surrounding_text.TextContent().Utf16(),
      surrounding_text.StartOffsetInTextContent(),
      surrounding_text.EndOffsetInTextContent()));
}

bool RenderFrameImpl::RunJavaScriptDialog(JavaScriptDialogType type,
                                          const base::string16& message,
                                          const base::string16& default_value,
                                          base::string16* result) {
  // Don't allow further dialogs if we are waiting to swap out, since the
  // ScopedPageLoadDeferrer in our stack prevents it.
  if (suppress_further_dialogs_)
    return false;

  int32_t message_length = static_cast<int32_t>(message.length());
  if (WebUserGestureIndicator::ProcessedUserGestureSinceLoad(frame_)) {
    UMA_HISTOGRAM_COUNTS_1M("JSDialogs.CharacterCount.UserGestureSinceLoad",
                            message_length);
  } else {
    UMA_HISTOGRAM_COUNTS_1M("JSDialogs.CharacterCount.NoUserGestureSinceLoad",
                            message_length);
  }

  if (is_main_frame_)
    UMA_HISTOGRAM_COUNTS_1M("JSDialogs.CharacterCount.MainFrame",
                            message_length);
  else
    UMA_HISTOGRAM_COUNTS_1M("JSDialogs.CharacterCount.Subframe",
                            message_length);

  // 10k ought to be enough for anyone.
  const base::string16::size_type kMaxMessageSize = 10 * 1024;
  base::string16 truncated_message = message.substr(0, kMaxMessageSize);

  bool success = false;
  base::string16 result_temp;
  if (!result)
    result = &result_temp;

  Send(new FrameHostMsg_RunJavaScriptDialog(
      routing_id_, truncated_message, default_value, type, &success, result));
  return success;
}

void RenderFrameImpl::DidFailProvisionalLoadInternal(
    const WebURLError& error,
    blink::WebHistoryCommitType commit_type,
    const base::Optional<std::string>& error_page_content,
    std::unique_ptr<blink::WebNavigationParams> navigation_params,
    std::unique_ptr<blink::WebDocumentLoader::ExtraData> document_state) {
  TRACE_EVENT1("navigation,benchmark,rail",
               "RenderFrameImpl::didFailProvisionalLoad", "id", routing_id_);
  // Note: It is important this notification occur before DidStopLoading so the
  //       SSL manager can react to the provisional load failure before being
  //       notified the load stopped.
  //
  NotifyObserversOfFailedProvisionalLoad(error);

  WebDocumentLoader* document_loader = frame_->GetProvisionalDocumentLoader();
  if (!document_loader)
    return;

  const WebURLRequest& failed_request = document_loader->GetRequest();

  // Notify the browser that we failed a provisional load with an error.
  SendFailedProvisionalLoad(failed_request, error, frame_);

  if (!ShouldDisplayErrorPageForFailedLoad(error.reason(), error.url()))
    return;

  // Make sure we never show errors in view source mode.
  frame_->EnableViewSourceMode(false);

  NavigationState* navigation_state =
      NavigationState::FromDocumentLoader(document_loader);

  // If this is a failed back/forward/reload navigation, then we need to do a
  // 'replace' load.  This is necessary to avoid messing up session history.
  // Otherwise, we do a normal load, which simulates a 'go' navigation as far
  // as session history is concerned.
  bool replace = commit_type != blink::kWebStandardCommit;

  // If we failed on a browser initiated request, then make sure that our error
  // page load is regarded as the same browser initiated request.
  if (!navigation_state->IsContentInitiated()) {
    document_state = BuildDocumentStateFromParams(
        navigation_state->common_params(), navigation_state->request_params(),
        base::TimeTicks(),  // Not used for failed navigation.
        CommitNavigationCallback(), nullptr);
    navigation_params = BuildNavigationParams(
        navigation_state->common_params(), navigation_state->request_params(),
        BuildServiceWorkerNetworkProviderForNavigation(
            nullptr /* request_params */,
            nullptr /* controller_service_worker_info */));
  }

  LoadNavigationErrorPage(failed_request, error, replace, nullptr,
                          error_page_content, std::move(navigation_params),
                          std::move(document_state));
}

void RenderFrameImpl::NotifyObserversOfFailedProvisionalLoad(
    const blink::WebURLError& error) {
  for (auto& observer : render_view_->observers())
    observer.DidFailProvisionalLoad(frame_, error);
  for (auto& observer : observers_)
    observer.DidFailProvisionalLoad(error);
}

void RenderFrameImpl::LoadNavigationErrorPage(
    const WebURLRequest& failed_request,
    const WebURLError& error,
    bool replace,
    HistoryEntry* entry,
    const base::Optional<std::string>& error_page_content,
    std::unique_ptr<blink::WebNavigationParams> navigation_params,
    std::unique_ptr<blink::WebDocumentLoader::ExtraData> navigation_data) {
  std::string error_html;
  if (error_page_content.has_value()) {
    error_html = error_page_content.value();
    GetContentClient()->renderer()->PrepareErrorPage(this, failed_request,
                                                     error, nullptr, nullptr);
  } else {
    GetContentClient()->renderer()->PrepareErrorPage(
        this, failed_request, error, &error_html, nullptr);
  }
  LoadNavigationErrorPageInternal(error_html, error.url(), replace, entry,
                                  std::move(navigation_params),
                                  std::move(navigation_data), &failed_request);
}

void RenderFrameImpl::LoadNavigationErrorPageForHttpStatusError(
    const WebURLRequest& failed_request,
    const GURL& unreachable_url,
    int http_status,
    bool replace,
    HistoryEntry* entry,
    std::unique_ptr<blink::WebNavigationParams> navigation_params,
    std::unique_ptr<blink::WebDocumentLoader::ExtraData> navigation_data) {
  std::string error_html;
  GetContentClient()->renderer()->PrepareErrorPageForHttpStatusError(
      this, failed_request, unreachable_url, http_status, &error_html, nullptr);
  LoadNavigationErrorPageInternal(error_html, unreachable_url, replace, entry,
                                  std::move(navigation_params),
                                  std::move(navigation_data), &failed_request);
}

void RenderFrameImpl::LoadNavigationErrorPageInternal(
    const std::string& error_html,
    const GURL& error_url,
    bool replace,
    HistoryEntry* history_entry,
    std::unique_ptr<blink::WebNavigationParams> navigation_params,
    std::unique_ptr<blink::WebDocumentLoader::ExtraData> navigation_data,
    const WebURLRequest* failed_request) {
  blink::WebFrameLoadType frame_load_type =
      history_entry ? blink::WebFrameLoadType::kBackForward
                    : blink::WebFrameLoadType::kStandard;
  const blink::WebHistoryItem& history_item =
      history_entry ? history_entry->root() : blink::WebHistoryItem();
  if (replace && frame_load_type == WebFrameLoadType::kStandard)
    frame_load_type = WebFrameLoadType::kReplaceCurrentItem;

  // Failed navigations will always provide a |failed_request|.  Error induced
  // by the client/renderer side after a commit won't have a |failed_request|.
  bool is_client_redirect = !failed_request;

  WebURLRequest new_request;
  if (failed_request)
    new_request = *failed_request;
  new_request.SetURL(GURL(kUnreachableWebDataURL));

  // Locally generated error pages should not be cached (in particular they
  // should not inherit the cache mode from |failed_request|).
  new_request.SetCacheMode(blink::mojom::FetchCacheMode::kNoStore);

  frame_->CommitDataNavigation(new_request, error_html, "text/html", "UTF-8",
                               error_url, frame_load_type, history_item,
                               is_client_redirect, std::move(navigation_params),
                               std::move(navigation_data));
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
  return render_accessibility_;
}

int RenderFrameImpl::GetRoutingID() {
  return routing_id_;
}

blink::WebLocalFrame* RenderFrameImpl::GetWebFrame() {
  DCHECK(frame_);
  return frame_;
}

const WebPreferences& RenderFrameImpl::GetWebkitPreferences() {
  return render_view_->GetWebkitPreferences();
}

const RendererPreferences& RenderFrameImpl::GetRendererPreferences() const {
  return render_view_->renderer_preferences();
}

int RenderFrameImpl::ShowContextMenu(ContextMenuClient* client,
                                     const ContextMenuParams& params) {
  DCHECK(client);  // A null client means "internal" when we issue callbacks.
  ContextMenuParams our_params(params);

  blink::WebRect position_in_window(params.x, params.y, 0, 0);
  GetRenderWidget()->ConvertViewportToWindow(&position_in_window);
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

void RenderFrameImpl::BindToFrame(WebLocalFrame* web_frame) {
  DCHECK(!frame_);

  std::pair<FrameMap::iterator, bool> result =
      g_frame_map.Get().emplace(web_frame, this);
  CHECK(result.second) << "Inserting a duplicate item.";

  frame_ = web_frame;
}

blink::WebPlugin* RenderFrameImpl::CreatePlugin(
    const WebPluginInfo& info,
    const blink::WebPluginParams& params,
    std::unique_ptr<content::PluginInstanceThrottler> throttler) {
#if BUILDFLAG(ENABLE_PLUGINS)
  if (info.type == WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN) {
    // |delegate| deletes itself.
    BrowserPluginDelegate* delegate =
        GetContentClient()->renderer()->CreateBrowserPluginDelegate(
            this, info, params.mime_type.Utf8(), GURL(params.url));
    return BrowserPluginManager::Get()->CreateBrowserPlugin(
        this, delegate->GetWeakPtr());
  }

  base::Optional<url::Origin> origin_lock;
  if (base::FeatureList::IsEnabled(features::kPdfIsolation) &&
      GetContentClient()->renderer()->IsOriginIsolatedPepperPlugin(info.path)) {
    origin_lock = url::Origin::Create(GURL(params.url));
  }

  bool pepper_plugin_was_registered = false;
  scoped_refptr<PluginModule> pepper_module(PluginModule::Create(
      this, info, origin_lock, &pepper_plugin_was_registered));
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

void RenderFrameImpl::LoadErrorPage(int reason) {
  WebURLError error(reason, frame_->GetDocument().Url());

  std::string error_html;
  GetContentClient()->renderer()->PrepareErrorPage(
      this, frame_->GetDocumentLoader()->GetRequest(), error, &error_html,
      nullptr);

  LoadNavigationErrorPageInternal(
      error_html, error.url(), true /* replace */, nullptr /* history_entry */,
      nullptr /* navigation_params */, nullptr /* navigation_data */,
      nullptr /* failed_request */);
}

void RenderFrameImpl::ExecuteJavaScript(const base::string16& javascript) {
  OnJavaScriptExecuteRequest(javascript, 0, false);
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
    ChildThreadImpl* thread = ChildThreadImpl::current();
    if (thread) {
      blink::mojom::AssociatedInterfaceProviderAssociatedPtr remote_interfaces;
      thread->GetRemoteRouteProvider()->GetRoute(
          routing_id_, mojo::MakeRequest(&remote_interfaces));
      remote_associated_interfaces_ =
          std::make_unique<blink::AssociatedInterfaceProvider>(
              std::move(remote_interfaces),
              GetTaskRunner(blink::TaskType::kInternalIPC));
    } else {
      // In some tests the thread may be null,
      // so set up a self-contained interface provider instead.
      remote_associated_interfaces_ =
          std::make_unique<blink::AssociatedInterfaceProvider>(
              GetTaskRunner(blink::TaskType::kInternalIPC));
    }
  }
  return remote_associated_interfaces_.get();
}

#if BUILDFLAG(ENABLE_PLUGINS)
void RenderFrameImpl::RegisterPeripheralPlugin(
    const url::Origin& content_origin,
    const base::Closure& unthrottle_callback) {
  return plugin_power_saver_helper_->RegisterPeripheralPlugin(
      content_origin, unthrottle_callback);
}

RenderFrame::PeripheralContentStatus
RenderFrameImpl::GetPeripheralContentStatus(
    const url::Origin& main_frame_origin,
    const url::Origin& content_origin,
    const gfx::Size& unobscured_size,
    RecordPeripheralDecision record_decision) const {
  return plugin_power_saver_helper_->GetPeripheralContentStatus(
      main_frame_origin, content_origin, unobscured_size, record_decision);
}

void RenderFrameImpl::WhitelistContentOrigin(
    const url::Origin& content_origin) {
  return plugin_power_saver_helper_->WhitelistContentOrigin(content_origin);
}

void RenderFrameImpl::PluginDidStartLoading() {
  DidStartLoading();
}

void RenderFrameImpl::PluginDidStopLoading() {
  DidStopLoading();
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

bool RenderFrameImpl::IsFTPDirectoryListing() {
  WebURLResponseExtraDataImpl* extra_data =
      GetExtraDataFromResponse(frame_->GetDocumentLoader()->GetResponse());
  return extra_data ? extra_data->is_ftp_directory_listing() : false;
}

void RenderFrameImpl::AttachGuest(int element_instance_id) {
  BrowserPluginManager::Get()->Attach(element_instance_id);
}

void RenderFrameImpl::DetachGuest(int element_instance_id) {
  BrowserPluginManager::Get()->Detach(element_instance_id);
}

void RenderFrameImpl::SetSelectedText(const base::string16& selection_text,
                                      size_t offset,
                                      const gfx::Range& range) {
  Send(new FrameHostMsg_SelectionChanged(routing_id_, selection_text,
                                         static_cast<uint32_t>(offset), range));
}

void RenderFrameImpl::SetZoomLevel(double zoom_level) {
  render_view_->UpdateZoomLevel(zoom_level);
}

double RenderFrameImpl::GetZoomLevel() const {
  return render_view_->page_zoom_level();
}

void RenderFrameImpl::AddMessageToConsole(ConsoleMessageLevel level,
                                          const std::string& message) {
  blink::WebConsoleMessage::Level target_level =
      blink::WebConsoleMessage::kLevelInfo;
  switch (level) {
    case CONSOLE_MESSAGE_LEVEL_VERBOSE:
      target_level = blink::WebConsoleMessage::kLevelVerbose;
      break;
    case CONSOLE_MESSAGE_LEVEL_INFO:
      target_level = blink::WebConsoleMessage::kLevelInfo;
      break;
    case CONSOLE_MESSAGE_LEVEL_WARNING:
      target_level = blink::WebConsoleMessage::kLevelWarning;
      break;
    case CONSOLE_MESSAGE_LEVEL_ERROR:
      target_level = blink::WebConsoleMessage::kLevelError;
      break;
  }

  blink::WebConsoleMessage wcm(target_level, WebString::FromUTF8(message));
  frame_->AddMessageToConsole(wcm);
}

void RenderFrameImpl::SetPreviewsState(PreviewsState previews_state) {
  previews_state_ = previews_state;
}

PreviewsState RenderFrameImpl::GetPreviewsState() const {
  return previews_state_;
}

bool RenderFrameImpl::IsPasting() const {
  return is_pasting_;
}

// blink::mojom::FullscreenVideoElementHandler implementation ------------------
void RenderFrameImpl::RequestFullscreenVideoElement() {
  WebElement video_element =
      frame_->GetDocument().GetElementsByHTMLTagName("video").FirstItem();

  if (!video_element.IsNull()) {
    // This is always initiated from browser side (which should require the user
    // interacting with ui) which suffices for a user gesture even though there
    // will have been no input to the frame at this point.
    blink::WebScopedUserGesture gesture(frame_);

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

// mojom::Frame implementation -------------------------------------------------

void RenderFrameImpl::GetInterfaceProvider(
    service_manager::mojom::InterfaceProviderRequest request) {
  service_manager::Connector* connector = ChildThread::Get()->GetConnector();
  service_manager::mojom::InterfaceProviderPtr provider;
  interface_provider_bindings_.AddBinding(this, mojo::MakeRequest(&provider));
  connector->FilterInterfaces(mojom::kNavigation_FrameSpec,
                              browser_info_.identity, std::move(request),
                              std::move(provider));
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
  if (IsMainFrame() && (enabled_bindings_flags & BINDINGS_POLICY_WEB_UI) &&
      !(enabled_bindings_ & BINDINGS_POLICY_WEB_UI)) {
    // TODO(sammc): Move WebUIExtensionData to be a RenderFrameObserver.
    // WebUIExtensionData deletes itself when |render_view_| is destroyed.
    new WebUIExtensionData(render_view_);
  }

  enabled_bindings_ |= enabled_bindings_flags;

  // Keep track of the total bindings accumulated in this process.
  RenderProcess::current()->AddBindings(enabled_bindings_flags);
}

// mojom::FrameNavigationControl implementation --------------------------------

void RenderFrameImpl::CommitNavigation(
    const network::ResourceResponseHead& head,
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
    base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
    network::mojom::URLLoaderFactoryPtr prefetch_loader_factory,
    const base::UnguessableToken& devtools_navigation_token,
    CommitNavigationCallback callback) {
  DCHECK(!IsRendererDebugURL(common_params.url));
  DCHECK(
      !FrameMsg_Navigate_Type::IsSameDocument(common_params.navigation_type));
  // If this was a renderer-initiated navigation (nav_entry_id == 0) from this
  // frame, but it was aborted, then ignore it.
  if (!browser_side_navigation_pending_ &&
      !browser_side_navigation_pending_url_.is_empty() &&
      browser_side_navigation_pending_url_ == request_params.original_url &&
      request_params.nav_entry_id == 0) {
    browser_side_navigation_pending_url_ = GURL();
    std::move(callback).Run(blink::mojom::CommitResult::Aborted);
    return;
  }

  // If the request was initiated in the context of a user gesture then make
  // sure that the navigation also executes in the context of a user gesture.
  std::unique_ptr<blink::WebScopedUserGesture> gesture(
      common_params.has_user_gesture ? new blink::WebScopedUserGesture(frame_)
                                     : nullptr);

  // Sanity check that the browser always sends us new loader factories on
  // cross-document navigations with the Network Service enabled.
  DCHECK(common_params.url.SchemeIs(url::kJavaScriptScheme) ||
         !base::FeatureList::IsEnabled(network::features::kNetworkService) ||
         subresource_loader_factories);

  SetupLoaderFactoryBundle(std::move(subresource_loader_factories),
                           std::move(subresource_overrides),
                           std::move(prefetch_loader_factory));

  // Clear pending navigations which weren't sent to the browser because we
  // did not get a didStartProvisionalLoad() notification for them.
  pending_navigation_info_.reset(nullptr);

  // If the navigation is for "view source", the WebLocalFrame needs to be put
  // in a special mode.
  if (request_params.is_view_source)
    frame_->EnableViewSourceMode(true);

  PrepareFrameForCommit(common_params.url, request_params);

  // We only save metrics of the main frame's main resource to the
  // document state. In view source mode, we effectively let the user
  // see the source of the server's error page instead of using custom
  // one derived from the metrics saved to document state.
  const network::ResourceResponseHead* response_head = nullptr;
  if (!frame_->Parent() && !frame_->IsViewSourceModeEnabled())
    response_head = &head;
  std::unique_ptr<DocumentState> document_state(BuildDocumentStateFromParams(
      common_params, request_params, base::TimeTicks::Now(),
      std::move(callback), response_head));

  blink::WebFrameLoadType load_type = NavigationTypeToLoadType(
      common_params.navigation_type, common_params.should_replace_current_entry,
      request_params.page_state.IsValid());

  WebHistoryItem item_for_history_navigation;
  blink::mojom::CommitResult commit_status = blink::mojom::CommitResult::Ok;

  if (load_type == WebFrameLoadType::kBackForward) {
    // We must know the nav entry ID of the page we are navigating back to,
    // which should be the case because history navigations are routed via the
    // browser.
    DCHECK_NE(0, request_params.nav_entry_id);

    // Check that the history navigation can commit.
    commit_status = PrepareForHistoryNavigationCommit(
        common_params.navigation_type, request_params,
        &item_for_history_navigation, &load_type);
  }

  base::OnceClosure continue_navigation;
  if (commit_status == blink::mojom::CommitResult::Ok) {
    base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();
    // Check if the navigation being committed originated as a client redirect.
    bool is_client_redirect =
        !!(common_params.transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT);

    // Perform a navigation for loadDataWithBaseURL if needed (for main frames).
    // Note: the base URL might be invalid, so also check the data URL string.
    bool should_load_data_url = !common_params.base_url_for_data_url.is_empty();
#if defined(OS_ANDROID)
    should_load_data_url |= !request_params.data_url_as_string.empty();
#endif
    if (is_main_frame_ && should_load_data_url) {
      LoadDataURL(common_params, request_params, load_type,
                  item_for_history_navigation, is_client_redirect,
                  std::move(document_state));
    } else {
      WebURLRequest request = CreateURLRequestForCommit(
          common_params, request_params, std::move(url_loader_client_endpoints),
          head);
      // Note: we cannot use base::AutoReset here, since |this| can be deleted
      // in the next call and auto reset will introduce use-after-free bug.
      committing_main_request_ = true;
      frame_->CommitNavigation(
          request, load_type, item_for_history_navigation, is_client_redirect,
          devtools_navigation_token,
          BuildNavigationParams(
              common_params, request_params,
              BuildServiceWorkerNetworkProviderForNavigation(
                  &request_params, std::move(controller_service_worker_info))),
          std::move(document_state));
      // The commit can result in this frame being removed. Use a
      // WeakPtr as an easy way to detect whether this has occured. If so, this
      // method should return immediately and not touch any part of the object,
      // otherwise it will result in a use-after-free bug.
      if (!weak_this)
        return;
      committing_main_request_ = false;
      RequestExtraData* extra_data =
          static_cast<RequestExtraData*>(request.GetExtraData());
      continue_navigation =
          extra_data->TakeContinueNavigationFunctionOwnerShip();
    }
  } else {
    // The browser expects the frame to be loading this navigation. Inform it
    // that the load stopped if needed.
    if (frame_ && !frame_->IsLoading())
      Send(new FrameHostMsg_DidStopLoading(routing_id_));
  }

  // Reset the source location now that the commit checks have been processed.
  frame_->GetDocumentLoader()->ResetSourceLocation();
  if (frame_->GetProvisionalDocumentLoader())
    frame_->GetProvisionalDocumentLoader()->ResetSourceLocation();

  // Continue the navigation.
  // TODO(arthursonzogni): Pass the data needed to continue the navigation to
  // this function instead of storing it in the
  // NavigationResponseOverrideParameters. The architecture of committing the
  // navigation in the renderer process should be simplified and avoid going
  // through the ResourceFetcher for the main resource.
  if (continue_navigation) {
    base::AutoReset<bool> replaying(&replaying_main_response_, true);
    std::move(continue_navigation).Run();
  }
}

void RenderFrameImpl::CommitFailedNavigation(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    bool has_stale_copy_in_cache,
    int error_code,
    const base::Optional<std::string>& error_page_content,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
    CommitFailedNavigationCallback callback) {
  DCHECK(
      !FrameMsg_Navigate_Type::IsSameDocument(common_params.navigation_type));
  RenderFrameImpl::PrepareRenderViewForNavigation(common_params.url,
                                                  request_params);

  // Log a console message for subframe loads that failed due to a legacy
  // Symantec certificate that has been distrusted or is slated for distrust
  // soon. Most failed resource loads are logged in Blink, but Blink doesn't get
  // notified when a subframe resource fails to load like other resources, so
  // log it here.
  if (frame_->Parent() && error_code == net::ERR_CERT_SYMANTEC_LEGACY) {
    ReportLegacySymantecCert(common_params.url, true /* did_fail */);
  }

  GetContentClient()->SetActiveURL(
      common_params.url, frame_->Top()->GetSecurityOrigin().ToString().Utf8());

  SetupLoaderFactoryBundle(std::move(subresource_loader_factories),
                           base::nullopt /* subresource_overrides */,
                           nullptr /* prefetch_loader_factory */);

  // Send the provisional load failure.
  WebURLError error(
      error_code, 0,
      has_stale_copy_in_cache ? WebURLError::HasCopyInCache::kTrue
                              : WebURLError::HasCopyInCache::kFalse,
      WebURLError::IsWebSecurityViolation::kFalse, common_params.url);
  WebURLRequest failed_request = CreateURLRequestForNavigation(
      common_params, request_params,
      /*response_override=*/nullptr, frame_->IsViewSourceModeEnabled());

  if (!ShouldDisplayErrorPageForFailedLoad(error_code, common_params.url)) {
    // The browser expects this frame to be loading an error page. Inform it
    // that the load stopped.
    std::move(callback).Run(blink::mojom::CommitResult::Aborted);
    Send(new FrameHostMsg_DidStopLoading(routing_id_));
    browser_side_navigation_pending_ = false;
    browser_side_navigation_pending_url_ = GURL();
    return;
  }

  // On load failure, a frame can ask its owner to render fallback content.
  // When that happens, don't load an error page.
  WebLocalFrame::FallbackContentResult fallback_result =
      frame_->MaybeRenderFallbackContent(error);
  if (fallback_result != WebLocalFrame::NoFallbackContent) {
    if (fallback_result == WebLocalFrame::NoLoadInProgress) {
      // If the frame wasn't loading but was fallback-eligible, the fallback
      // content won't be shown. However, showing an error page isn't right
      // either, as the frame has already been populated with something
      // unrelated to this navigation failure. In that case, just send a stop
      // IPC to the browser to unwind its state, and leave the frame as-is.
      std::move(callback).Run(blink::mojom::CommitResult::Aborted);
      Send(new FrameHostMsg_DidStopLoading(routing_id_));
    } else {
      std::move(callback).Run(blink::mojom::CommitResult::Ok);
    }
    browser_side_navigation_pending_ = false;
    browser_side_navigation_pending_url_ = GURL();
    return;
  }

  // Make sure errors are not shown in view source mode.
  frame_->EnableViewSourceMode(false);

  // Replace the current history entry in reloads, and loads of the same url.
  // This corresponds to Blink's notion of a standard commit.
  // Also replace the current history entry if the browser asked for it
  // specifically.
  // TODO(clamy): see if initial commits in subframes should be handled
  // separately.
  bool is_reload_or_history =
      FrameMsg_Navigate_Type::IsReload(common_params.navigation_type) ||
      FrameMsg_Navigate_Type::IsHistory(common_params.navigation_type);
  bool replace = is_reload_or_history || common_params.url == GetLoadingUrl() ||
                 common_params.should_replace_current_entry;
  std::unique_ptr<HistoryEntry> history_entry;
  if (request_params.page_state.IsValid())
    history_entry = PageStateToHistoryEntry(request_params.page_state);

  // The load of the error page can result in this frame being removed.
  // Use a WeakPtr as an easy way to detect whether this has occured. If so,
  // this method should return immediately and not touch any part of the object,
  // otherwise it will result in a use-after-free bug.
  base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();

  std::unique_ptr<blink::WebNavigationParams> navigation_params =
      BuildNavigationParams(common_params, request_params,
                            BuildServiceWorkerNetworkProviderForNavigation(
                                &request_params, nullptr));
  std::unique_ptr<DocumentState> navigation_data(BuildDocumentStateFromParams(
      common_params, request_params, base::TimeTicks(), std::move(callback),
      nullptr));

  // For renderer initiated navigations, we send out a didFailProvisionalLoad()
  // notification.
  bool had_provisional_document_loader = frame_->GetProvisionalDocumentLoader();
  if (request_params.nav_entry_id == 0) {
    blink::WebHistoryCommitType commit_type =
        replace ? blink::kWebHistoryInertCommit : blink::kWebStandardCommit;

    // Note: had_provisional_document_loader can be false in cases such as cross
    // process failures, e.g. error pages.
    if (had_provisional_document_loader) {
      DidFailProvisionalLoadInternal(error, commit_type, error_page_content,
                                     std::move(navigation_params),
                                     std::move(navigation_data));
    } else {
      NotifyObserversOfFailedProvisionalLoad(error);
    }
    if (!weak_this)
      return;
  }

  // If we didn't call DidFailProvisionalLoad above, LoadNavigationErrorPage
  // wasn't called, so do it now.
  if (request_params.nav_entry_id != 0 || !had_provisional_document_loader) {
    LoadNavigationErrorPage(failed_request, error, replace, history_entry.get(),
                            error_page_content, std::move(navigation_params),
                            std::move(navigation_data));
    if (!weak_this)
      return;
  }

  browser_side_navigation_pending_ = false;
  browser_side_navigation_pending_url_ = GURL();
}

void RenderFrameImpl::CommitSameDocumentNavigation(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    CommitSameDocumentNavigationCallback callback) {
  DCHECK(!IsRendererDebugURL(common_params.url));
  DCHECK(!FrameMsg_Navigate_Type::IsReload(common_params.navigation_type));
  DCHECK(!request_params.is_view_source);
  DCHECK(FrameMsg_Navigate_Type::IsSameDocument(common_params.navigation_type));

  // If the request was initiated in the context of a user gesture then make
  // sure that the navigation also executes in the context of a user gesture.
  std::unique_ptr<blink::WebScopedUserGesture> gesture(
      common_params.has_user_gesture ? new blink::WebScopedUserGesture(frame_)
                                     : nullptr);

  PrepareFrameForCommit(common_params.url, request_params);

  blink::WebFrameLoadType load_type = NavigationTypeToLoadType(
      common_params.navigation_type, common_params.should_replace_current_entry,
      request_params.page_state.IsValid());

  blink::mojom::CommitResult commit_status = blink::mojom::CommitResult::Ok;
  WebHistoryItem item_for_history_navigation;

  if (common_params.navigation_type ==
      FrameMsg_Navigate_Type::HISTORY_SAME_DOCUMENT) {
    DCHECK(request_params.page_state.IsValid());
    // We must know the nav entry ID of the page we are navigating back to,
    // which should be the case because history navigations are routed via the
    // browser.
    DCHECK_NE(0, request_params.nav_entry_id);
    DCHECK(!request_params.is_history_navigation_in_new_child);
    commit_status = PrepareForHistoryNavigationCommit(
        common_params.navigation_type, request_params,
        &item_for_history_navigation, &load_type);
  }

  if (commit_status == blink::mojom::CommitResult::Ok) {
    base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();
    bool is_client_redirect =
        !!(common_params.transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT);
    DocumentState* original_document_state =
        DocumentState::FromDocumentLoader(frame_->GetDocumentLoader());
    std::unique_ptr<DocumentState> document_state =
        original_document_state->Clone();
    InternalDocumentStateData* internal_data =
        InternalDocumentStateData::FromDocumentState(document_state.get());
    internal_data->CopyFrom(
        InternalDocumentStateData::FromDocumentState(original_document_state));
    internal_data->set_navigation_state(NavigationState::CreateBrowserInitiated(
        common_params, request_params,
        base::TimeTicks(),  // Not used for same-document navigation.
        CommitNavigationCallback()));

    // Load the request.
    commit_status = frame_->CommitSameDocumentNavigation(
        common_params.url, load_type, item_for_history_navigation,
        is_client_redirect, std::move(document_state));

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
    Send(new FrameHostMsg_DidStopLoading(routing_id_));
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
    Send(new FrameHostMsg_DidStopLoading(routing_id_));
}

void RenderFrameImpl::UpdateSubresourceLoaderFactories(
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loaders) {
  DCHECK(loader_factories_);
  DCHECK(loader_factories_->IsHostChildURLLoaderFactoryBundle());
  static_cast<HostChildURLLoaderFactoryBundle*>(loader_factories_.get())
      ->UpdateThisAndAllClones(std::move(subresource_loaders));
}

void RenderFrameImpl::BindDevToolsAgent(
    blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
    blink::mojom::DevToolsAgentAssociatedRequest request) {
  frame_->BindDevToolsAgent(host.PassHandle(), request.PassHandle());
}

// mojom::HostZoom implementation ----------------------------------------------

void RenderFrameImpl::SetHostZoomLevel(const GURL& url, double zoom_level) {
  // TODO(wjmaclean): We should see if this restriction is really necessary,
  // since it isn't enforced in other parts of the page zoom system (e.g.
  // when a users changes the zoom of a currently displayed page). Android
  // has no UI for this, so in theory the following code would normally just use
  // the default zoom anyways.
#if !defined(OS_ANDROID)
  // On Android, page zoom isn't used, and in case of WebView, text zoom is used
  // for legacy WebView text scaling emulation. Thus, the code that resets
  // the zoom level from this map will be effectively resetting text zoom level.
  host_zoom_levels_[url] = zoom_level;
#endif
}

// blink::WebLocalFrameClient implementation
// ----------------------------------------

bool RenderFrameImpl::IsPluginHandledExternally(
    const blink::WebElement& plugin_element,
    const blink::WebURL& url,
    const blink::WebString& suggested_mime_type) {
#if BUILDFLAG(ENABLE_PLUGINS)
  if (!BrowserPluginManager::Get()) {
    // BrowserPluginManager needs a RenderThreadImpl, but some renderer tests
    // use a MockRenderThread instead.
    return false;
  }
  // TODO(ekaramad): The instance ID is mostly used for GuestView attaching and
  // lookup. See if this can be removed (https://crbug.com/659750).
  // The instance ID will not be consumed if the contents cannot be rendered
  // externally.
  int32_t tentative_element_instance_id =
      BrowserPluginManager::Get()->GetNextInstanceID();
  return GetContentClient()->renderer()->MaybeCreateMimeHandlerView(
      this, plugin_element, GURL(url), suggested_mime_type.Utf8(),
      tentative_element_instance_id);
#else
  return false;
#endif
}

blink::WebPlugin* RenderFrameImpl::CreatePlugin(
    const blink::WebPluginParams& params) {
  blink::WebPlugin* plugin = nullptr;
  if (GetContentClient()->renderer()->OverrideCreatePlugin(this, params,
                                                           &plugin)) {
    return plugin;
  }

  if (params.mime_type.ContainsOnlyASCII() &&
      params.mime_type.Ascii() == kBrowserPluginMimeType) {
    // |delegate| deletes itself.
    BrowserPluginDelegate* delegate =
        GetContentClient()->renderer()->CreateBrowserPluginDelegate(
            this, WebPluginInfo(), kBrowserPluginMimeType, GURL(params.url));
    return BrowserPluginManager::Get()->CreateBrowserPlugin(
        this, delegate->GetWeakPtr());
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
    WebMediaPlayerEncryptedMediaClient* encrypted_client,
    WebContentDecryptionModule* initial_cdm,
    const blink::WebString& sink_id,
    blink::WebLayerTreeView* layer_tree_view) {
  const cc::LayerTreeSettings& settings =
      GetRenderWidget()->layer_tree_view()->GetLayerTreeSettings();
  return media_factory_.CreateMediaPlayer(source, client, encrypted_client,
                                          initial_cdm, sink_id, layer_tree_view,
                                          settings);
}

std::unique_ptr<blink::WebApplicationCacheHost>
RenderFrameImpl::CreateApplicationCacheHost(
    blink::WebApplicationCacheHostClient* client) {
  if (!frame_ || !frame_->View())
    return nullptr;

  NavigationState* navigation_state = NavigationState::FromDocumentLoader(
      frame_->GetProvisionalDocumentLoader()
          ? frame_->GetProvisionalDocumentLoader()
          : frame_->GetDocumentLoader());

  return std::make_unique<RendererWebApplicationCacheHostImpl>(
      RenderViewImpl::FromWebView(frame_->View()), client,
      RenderThreadImpl::current()->appcache_dispatcher()->backend_proxy(),
      navigation_state->request_params().appcache_host_id, routing_id_);
}

std::unique_ptr<blink::WebContentSettingsClient>
RenderFrameImpl::CreateWorkerContentSettingsClient() {
  if (!frame_ || !frame_->View())
    return nullptr;
  return GetContentClient()->renderer()->CreateWorkerContentSettingsClient(
      this);
}

std::unique_ptr<blink::WebWorkerFetchContext>
RenderFrameImpl::CreateWorkerFetchContext() {
  blink::WebServiceWorkerNetworkProvider* web_provider =
      frame_->GetDocumentLoader()->GetServiceWorkerNetworkProvider();
  DCHECK(web_provider);
  ServiceWorkerNetworkProvider* provider =
      ServiceWorkerNetworkProvider::FromWebServiceWorkerNetworkProvider(
          web_provider);
  mojom::ServiceWorkerWorkerClientRequest service_worker_client_request;
  mojom::ServiceWorkerWorkerClientRegistryPtrInfo
      service_worker_worker_client_registry_ptr_info;
  mojom::ServiceWorkerContainerHostPtrInfo container_host_ptr_info;
  ServiceWorkerProviderContext* provider_context = provider->context();
  // Some sandboxed iframes are not allowed to use service worker so don't have
  // a real service worker provider, so the provider context is null.
  if (provider_context) {
    provider_context->CloneWorkerClientRegistry(
        mojo::MakeRequest(&service_worker_worker_client_registry_ptr_info));

    mojom::ServiceWorkerWorkerClientPtr worker_client_ptr;
    service_worker_client_request = mojo::MakeRequest(&worker_client_ptr);
    provider_context->RegisterWorkerClient(std::move(worker_client_ptr));

    if (blink::ServiceWorkerUtils::IsServicificationEnabled())
      container_host_ptr_info = provider_context->CloneContainerHostPtrInfo();
  }

  mojom::RendererPreferenceWatcherPtr watcher;
  mojom::RendererPreferenceWatcherRequest watcher_request =
      mojo::MakeRequest(&watcher);
  render_view()->RegisterRendererPreferenceWatcherForWorker(std::move(watcher));

  auto worker_fetch_context = std::make_unique<WebWorkerFetchContextImpl>(
      render_view_->renderer_preferences(), std::move(watcher_request),
      std::move(service_worker_client_request),
      std::move(service_worker_worker_client_registry_ptr_info),
      std::move(container_host_ptr_info), GetLoaderFactoryBundle()->Clone(),
      GetLoaderFactoryBundle()->CloneWithoutDefaultFactory(),
      GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
          URLLoaderThrottleProviderType::kWorker),
      GetContentClient()
          ->renderer()
          ->CreateWebSocketHandshakeThrottleProvider(),
      ChildThreadImpl::current()->thread_safe_sender(),
      ChildThreadImpl::current()->GetConnector()->Clone());

  worker_fetch_context->set_ancestor_frame_id(routing_id_);
  worker_fetch_context->set_frame_request_blocker(frame_request_blocker_);
  worker_fetch_context->set_site_for_cookies(
      frame_->GetDocument().SiteForCookies());
  worker_fetch_context->set_is_secure_context(
      frame_->GetDocument().IsSecureContext());
  worker_fetch_context->set_service_worker_provider_id(provider->provider_id());
  worker_fetch_context->set_is_controlled_by_service_worker(
      provider->IsControlledByServiceWorker());
  worker_fetch_context->set_origin_url(
      GURL(frame_->GetDocument().Url()).GetOrigin());
  if (provider_context)
    worker_fetch_context->set_client_id(provider_context->client_id());

  for (auto& observer : observers_)
    observer.WillCreateWorkerFetchContext(worker_fetch_context.get());
  return std::move(worker_fetch_context);
}

WebExternalPopupMenu* RenderFrameImpl::CreateExternalPopupMenu(
    const WebPopupMenuInfo& popup_menu_info,
    WebExternalPopupMenuClient* popup_menu_client) {
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  // An IPC message is sent to the browser to build and display the actual
  // popup. The user could have time to click a different select by the time
  // the popup is shown. In that case external_popup_menu_ is non NULL.
  // By returning NULL in that case, we instruct Blink to cancel that new
  // popup. So from the user perspective, only the first one will show, and
  // will have to close the first one before another one can be shown.
  if (external_popup_menu_)
    return NULL;
  external_popup_menu_.reset(
      new ExternalPopupMenu(this, popup_menu_info, popup_menu_client));
  if (render_view_->screen_metrics_emulator_) {
    render_view_->SetExternalPopupOriginAdjustmentsForEmulation(
        external_popup_menu_.get(),
        render_view_->screen_metrics_emulator_.get());
  }
  return external_popup_menu_.get();
#else
  return nullptr;
#endif
}

blink::WebCookieJar* RenderFrameImpl::CookieJar() {
  return &cookie_jar_;
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
  ServiceWorkerNetworkProvider* provider =
      ServiceWorkerNetworkProvider::FromWebServiceWorkerNetworkProvider(
          frame_->GetDocumentLoader()->GetServiceWorkerNetworkProvider());
  if (!provider->context()) {
    // The context can be null when the frame is sandboxed.
    return nullptr;
  }
  return std::make_unique<WebServiceWorkerProviderImpl>(provider->context());
}

service_manager::InterfaceProvider* RenderFrameImpl::GetInterfaceProvider() {
  return &remote_interfaces_;
}

blink::AssociatedInterfaceProvider*
RenderFrameImpl::GetRemoteNavigationAssociatedInterfaces() {
  return GetRemoteAssociatedInterfaces();
}

void RenderFrameImpl::DidAccessInitialDocument() {
  DCHECK(!frame_->Parent());
  // NOTE: Do not call back into JavaScript here, since this call is made from a
  // V8 security check.

  // If the request hasn't yet committed, notify the browser process that it is
  // no longer safe to show the pending URL of the main frame, since a URL spoof
  // is now possible. (If the request has committed, the browser already knows.)
  if (!has_accessed_initial_document_) {
    NavigationState* navigation_state =
        NavigationState::FromDocumentLoader(frame_->GetDocumentLoader());
    if (!navigation_state->request_committed()) {
      Send(new FrameHostMsg_DidAccessInitialDocument(routing_id_));
    }
  }

  has_accessed_initial_document_ = true;
}

blink::WebLocalFrame* RenderFrameImpl::CreateChildFrame(
    blink::WebLocalFrame* parent,
    blink::WebTreeScopeType scope,
    const blink::WebString& name,
    const blink::WebString& fallback_name,
    blink::WebSandboxFlags sandbox_flags,
    const blink::ParsedFeaturePolicy& container_policy,
    const blink::WebFrameOwnerProperties& frame_owner_properties,
    blink::FrameOwnerElementType frame_owner_element_type) {
  DCHECK_EQ(frame_, parent);

  // Synchronously notify the browser of a child frame creation to get the
  // routing_id for the RenderFrame.
  int child_routing_id = MSG_ROUTING_NONE;
  mojo::MessagePipeHandle child_interface_provider_handle;
  base::UnguessableToken devtools_frame_token;
  FrameHostMsg_CreateChildFrame_Params params;
  params.parent_routing_id = routing_id_;
  params.scope = scope;
  params.frame_name = name.Utf8();
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
  params.frame_policy = {sandbox_flags, container_policy};
  params.frame_owner_properties =
      ConvertWebFrameOwnerPropertiesToFrameOwnerProperties(
          frame_owner_properties);
  params.frame_owner_element_type = frame_owner_element_type;
  Send(new FrameHostMsg_CreateChildFrame(params, &child_routing_id,
                                         &child_interface_provider_handle,
                                         &devtools_frame_token));

  // Allocation of routing id failed, so we can't create a child frame. This can
  // happen if the synchronous IPC message above has failed.  This can
  // legitimately happen when the browser process has already destroyed
  // RenderProcessHost, but the renderer process hasn't quit yet.
  if (child_routing_id == MSG_ROUTING_NONE)
    return nullptr;

  CHECK(child_interface_provider_handle.is_valid());
  service_manager::mojom::InterfaceProviderPtr child_interface_provider;
  child_interface_provider.Bind(
      service_manager::mojom::InterfaceProviderPtrInfo(
          mojo::ScopedMessagePipeHandle(child_interface_provider_handle), 0u),
      GetTaskRunner(blink::TaskType::kInternalIPC));

  // This method is always called by local frames, never remote frames.

  // Tracing analysis uses this to find main frames when this value is
  // MSG_ROUTING_NONE, and build the frame tree otherwise.
  TRACE_EVENT2("navigation,rail", "RenderFrameImpl::createChildFrame",
               "id", routing_id_,
               "child", child_routing_id);

  // Create the RenderFrame and WebLocalFrame, linking the two.
  RenderFrameImpl* child_render_frame = RenderFrameImpl::Create(
      render_view_, child_routing_id, std::move(child_interface_provider),
      devtools_frame_token);
  child_render_frame->unique_name_helper_.set_propagated_name(
      params.frame_unique_name);
  if (params.is_created_by_script)
    child_render_frame->unique_name_helper_.Freeze();
  child_render_frame->InitializeBlameContext(this);
  blink::WebLocalFrame* web_frame = parent->CreateLocalChild(
      scope, child_render_frame,
      child_render_frame->blink_interface_registry_.get());

  child_render_frame->in_frame_tree_ = true;
  child_render_frame->Initialize();

  return web_frame;
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

void RenderFrameImpl::DidChangeOpener(blink::WebFrame* opener) {
  // Only a local frame should be able to update another frame's opener.
  DCHECK(!opener || opener->IsWebLocalFrame());

  int opener_routing_id =
      opener ? RenderFrameImpl::FromWebFrame(opener->ToWebLocalFrame())
                   ->GetRoutingID()
             : MSG_ROUTING_NONE;
  Send(new FrameHostMsg_DidChangeOpener(routing_id_, opener_routing_id));
}

void RenderFrameImpl::FrameDetached(DetachType type) {
  for (auto& observer : observers_)
    observer.FrameDetached();

  // Send a state update before the frame is detached.
  SendUpdateState();

  // We only notify the browser process when the frame is being detached for
  // removal and it was initiated from the renderer process.
  if (!in_browser_initiated_detach_ && type == DetachType::kRemove)
    Send(new FrameHostMsg_Detach(routing_id_));

  // Clean up the associated RenderWidget for the frame, if there is one.
  GetRenderWidget()->UnregisterRenderFrame(this);
  if (render_widget_)
    render_widget_->CloseForFrame();

  // We need to clean up subframes by removing them from the map and deleting
  // the RenderFrameImpl.  In contrast, the main frame is owned by its
  // containing RenderViewHost (so that they have the same lifetime), so only
  // removal from the map is needed and no deletion.
  auto it = g_frame_map.Get().find(frame_);
  CHECK(it != g_frame_map.Get().end());
  CHECK_EQ(it->second, this);
  g_frame_map.Get().erase(it);

  // |frame_| may not be referenced after this, so clear the pointer since
  // the actual WebLocalFrame may not be deleted immediately and other methods
  // may try to access it.
  frame_->Close();
  frame_ = nullptr;

  // If this was a provisional frame with an associated proxy, tell the proxy
  // that it's no longer associated with this frame.
  if (proxy_routing_id_ != MSG_ROUTING_NONE) {
    RenderFrameProxy* proxy =
        RenderFrameProxy::FromRoutingID(proxy_routing_id_);

    // |proxy| should always exist.  Detaching the proxy would've also detached
    // this provisional frame.  The proxy should also not be associated with
    // another provisional frame at this point.
    CHECK(proxy);
    CHECK_EQ(routing_id_, proxy->provisional_frame_routing_id());

    proxy->set_provisional_frame_routing_id(MSG_ROUTING_NONE);
  }

  delete this;
  // Object is invalid after this point.
}

void RenderFrameImpl::FrameFocused() {
  Send(new FrameHostMsg_FrameFocused(routing_id_));
}

void RenderFrameImpl::WillCommitProvisionalLoad() {
  for (auto& observer : observers_)
    observer.WillCommitProvisionalLoad();
}

void RenderFrameImpl::DidChangeName(const blink::WebString& name) {
  if (current_history_item_.IsNull()) {
    // Once a navigation has committed, the unique name must no longer change to
    // avoid breaking back/forward navigations: https://crbug.com/607205
    unique_name_helper_.UpdateName(name.Utf8());
  }
  GetFrameHost()->DidChangeName(name.Utf8(), unique_name_helper_.value());

  if (!committed_first_load_)
    name_changed_before_first_commit_ = true;
}

void RenderFrameImpl::DidEnforceInsecureRequestPolicy(
    blink::WebInsecureRequestPolicy policy) {
  GetFrameHost()->EnforceInsecureRequestPolicy(policy);
}

void RenderFrameImpl::DidEnforceInsecureNavigationsSet(
    const std::vector<uint32_t>& set) {
  GetFrameHost()->EnforceInsecureNavigationsSet(set);
}

void RenderFrameImpl::DidChangeFramePolicy(
    blink::WebFrame* child_frame,
    blink::WebSandboxFlags flags,
    const blink::ParsedFeaturePolicy& container_policy) {
  Send(new FrameHostMsg_DidChangeFramePolicy(
      routing_id_, RenderFrame::GetRoutingIdForWebFrame(child_frame),
      {flags, container_policy}));
}

void RenderFrameImpl::DidSetFramePolicyHeaders(
    blink::WebSandboxFlags flags,
    const blink::ParsedFeaturePolicy& parsed_header) {
  // If either Feature Policy or Sandbox Flags are different from the default
  // (empty) values, then send them to the browser.
  if (!parsed_header.empty() || flags != blink::WebSandboxFlags::kNone) {
    GetFrameHost()->DidSetFramePolicyHeaders(flags, parsed_header);
  }
}

void RenderFrameImpl::DidAddContentSecurityPolicies(
    const blink::WebVector<blink::WebContentSecurityPolicy>& policies) {
  std::vector<ContentSecurityPolicy> content_policies;
  for (const auto& policy : policies)
    content_policies.push_back(BuildContentSecurityPolicy(policy));

  Send(new FrameHostMsg_DidAddContentSecurityPolicies(routing_id_,
                                                      content_policies));
}

void RenderFrameImpl::DidChangeFrameOwnerProperties(
    blink::WebFrame* child_frame,
    const blink::WebFrameOwnerProperties& frame_owner_properties) {
  Send(new FrameHostMsg_DidChangeFrameOwnerProperties(
      routing_id_, RenderFrame::GetRoutingIdForWebFrame(child_frame),
      ConvertWebFrameOwnerPropertiesToFrameOwnerProperties(
          frame_owner_properties)));
}

void RenderFrameImpl::DidMatchCSS(
    const blink::WebVector<blink::WebString>& newly_matching_selectors,
    const blink::WebVector<blink::WebString>& stopped_matching_selectors) {
  for (auto& observer : observers_)
    observer.DidMatchCSS(newly_matching_selectors, stopped_matching_selectors);
}

void RenderFrameImpl::UpdateUserActivationState(
    blink::UserActivationUpdateType update_type) {
  Send(new FrameHostMsg_UpdateUserActivationState(routing_id_, update_type));
}

void RenderFrameImpl::SetHasReceivedUserGestureBeforeNavigation(bool value) {
  Send(new FrameHostMsg_SetHasReceivedUserGestureBeforeNavigation(routing_id_,
                                                                  value));
}

void RenderFrameImpl::SetMouseCapture(bool capture) {
  GetRenderWidget()->SetMouseCapture(capture);
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
  logging::LogSeverity log_severity = logging::LOG_VERBOSE;
  switch (message.level) {
    case blink::WebConsoleMessage::kLevelVerbose:
      log_severity = logging::LOG_VERBOSE;
      break;
    case blink::WebConsoleMessage::kLevelInfo:
      log_severity = logging::LOG_INFO;
      break;
    case blink::WebConsoleMessage::kLevelWarning:
      log_severity = logging::LOG_WARNING;
      break;
    case blink::WebConsoleMessage::kLevelError:
      log_severity = logging::LOG_ERROR;
      break;
    default:
      log_severity = logging::LOG_VERBOSE;
  }

  if (ShouldReportDetailedMessageForSource(source_name)) {
    for (auto& observer : observers_) {
      observer.DetailedConsoleMessageAdded(
          message.text.Utf16(), source_name.Utf16(), stack_trace.Utf16(),
          source_line, static_cast<uint32_t>(log_severity));
    }
  }

  Send(new FrameHostMsg_DidAddMessageToConsole(
      routing_id_, static_cast<int32_t>(log_severity), message.text.Utf16(),
      static_cast<int32_t>(source_line), source_name.Utf16()));
}

void RenderFrameImpl::DownloadURL(
    const blink::WebURLRequest& request,
    CrossOriginRedirects cross_origin_redirect_behavior,
    mojo::ScopedMessagePipeHandle blob_url_token) {
  if (ShouldThrottleDownload())
    return;

  FrameHostMsg_DownloadUrl_Params params;
  params.render_view_id = render_view_->GetRoutingID();
  params.render_frame_id = GetRoutingID();
  params.url = request.Url();
  params.referrer = RenderViewImpl::GetReferrerFromRequest(frame_, request);
  params.initiator_origin = request.RequestorOrigin();
  if (request.GetSuggestedFilename().has_value())
    params.suggested_name = request.GetSuggestedFilename()->Utf16();
  params.follow_cross_origin_redirects =
      (cross_origin_redirect_behavior == CrossOriginRedirects::kFollow);
  params.blob_url_token = blob_url_token.release();

  Send(new FrameHostMsg_DownloadUrl(params));
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
    // This is either a placeholder document loader or an initial empty
    // document.
    document_loader->SetExtraData(BuildDocumentState());
    document_loader->SetServiceWorkerNetworkProvider(
        BuildServiceWorkerNetworkProviderForNavigation(
            nullptr /* request_params */, nullptr /* controller_info */));
  }
}

void RenderFrameImpl::DidStartProvisionalLoad(
    blink::WebDocumentLoader* document_loader,
    blink::WebURLRequest& request,
    mojo::ScopedMessagePipeHandle navigation_initiator_handle) {
  // In fast/loader/stop-provisional-loads.html, we abort the load before this
  // callback is invoked.
  if (!document_loader)
    return;

  TRACE_EVENT2("navigation,benchmark,rail",
               "RenderFrameImpl::didStartProvisionalLoad", "id", routing_id_,
               "url", document_loader->GetRequest().Url().GetString().Utf8());

  // If we have a pending navigation to be sent to the browser send it here.
  if (pending_navigation_info_.get()) {
    NavigationPolicyInfo info(request);
    info.navigation_type = pending_navigation_info_->navigation_type;
    info.default_policy = pending_navigation_info_->policy;
    info.replaces_current_history_item =
        pending_navigation_info_->replaces_current_history_item;
    info.is_history_navigation_in_new_child_frame =
        pending_navigation_info_->history_navigation_in_new_child_frame;
    info.is_client_redirect = pending_navigation_info_->client_redirect;
    info.triggering_event_info =
        pending_navigation_info_->triggering_event_info;
    info.form = pending_navigation_info_->form;
    info.source_location = pending_navigation_info_->source_location;
    info.devtools_initiator_info =
        pending_navigation_info_->devtools_initiator_info;
    info.blob_url_token =
        pending_navigation_info_->blob_url_token.PassInterface().PassHandle();
    info.input_start = pending_navigation_info_->input_start;

    pending_navigation_info_.reset(nullptr);
    BeginNavigation(info, std::move(navigation_initiator_handle));
  }

  NavigationState* navigation_state =
      NavigationState::FromDocumentLoader(document_loader);
  for (auto& observer : observers_) {
    observer.DidStartProvisionalLoad(document_loader,
                                     navigation_state->IsContentInitiated());
  }
}

void RenderFrameImpl::DidFailProvisionalLoad(
    const WebURLError& error,
    blink::WebHistoryCommitType commit_type) {
  DidFailProvisionalLoadInternal(error, commit_type, base::nullopt, nullptr,
                                 nullptr);
}

void RenderFrameImpl::DidCommitProvisionalLoad(
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type,
    blink::WebGlobalObjectReusePolicy global_object_reuse_policy) {
  TRACE_EVENT2("navigation,rail", "RenderFrameImpl::didCommitProvisionalLoad",
               "id", routing_id_,
               "url", GetLoadingUrl().possibly_invalid_spec());
  // TODO(dcheng): Remove this UMA once we have enough measurements.
  // Record the number of subframes where window.name changes between the
  // creation of the frame and the first commit that records a history entry
  // with a persisted unique name. We'd like to make unique name immutable to
  // simplify code, but it's unclear if there are site that depend on the
  // following pattern:
  //   1. Create a new subframe.
  //   2. Assign it a window.name.
  //   3. Navigate it.
  //
  // If unique name are immutable, then it's possible that session history would
  // become less reliable for subframes:
  //   * A subframe with no initial name will receive a generated name that
  //     depends on DOM insertion order instead of using a name baed on the
  //     window.name assigned in step 2.
  //   * A subframe may intentionally try to choose a non-conflicting
  //     window.name if it detects a conflict. Immutability would prevent this
  //     from having the desired effect.
  //
  // The logic for when to record the UMA is a bit subtle:
  //   * if |committed_first_load_| is false and |current_history_item_| is
  //     null, then this is being called to commit the initial empty document.
  //     Don't record the UMA yet. |current_history_item_| will be non-null in
  //     subsequent invocations of this callback.
  //   * if |committed_first_load_| is false and |current_history_item_| is
  //     *not* null, then the initial empty document has already committed.
  //     Record if window.name has changed.
  if (!committed_first_load_ && !current_history_item_.IsNull()) {
    if (!IsMainFrame()) {
      UMA_HISTOGRAM_BOOLEAN(
          "SessionRestore.SubFrameUniqueNameChangedBeforeFirstCommit",
          name_changed_before_first_commit_);
    }
    // TODO(dcheng): This signal is likely calculated incorrectly, and will be
    // removed in a followup CL (as we've decided to try to preserve backwards
    // compatibility as much as possible for the time being).
    committed_first_load_ = true;
  }

  NavigationState* navigation_state =
      NavigationState::FromDocumentLoader(frame_->GetDocumentLoader());
  DCHECK(!navigation_state->WasWithinSameDocument());
  const WebURLResponse& web_url_response =
      frame_->GetDocumentLoader()->GetResponse();
  WebURLResponseExtraDataImpl* extra_data =
      GetExtraDataFromResponse(web_url_response);

  // Only update the PreviewsState and effective connection type states for new
  // main frame documents. Subframes inherit from the main frame and should not
  // change at commit time.
  if (is_main_frame_) {
    previews_state_ =
        frame_->GetDocumentLoader()->GetRequest().GetPreviewsState();
    if (extra_data) {
      effective_connection_type_ =
          EffectiveConnectionTypeToWebEffectiveConnectionType(
              extra_data->effective_connection_type());
    } else {
      effective_connection_type_ =
          blink::WebEffectiveConnectionType::kTypeUnknown;
    }
  }

  if (proxy_routing_id_ != MSG_ROUTING_NONE) {
    // If this is a provisional frame associated with a proxy (i.e., a frame
    // created for a remote-to-local navigation), swap it into the frame tree
    // now.
    if (!SwapIn())
      return;
  }

  // Navigations that change the document represent a new content source.  Keep
  // track of that on the widget to help the browser process detect when stale
  // compositor frames are being shown after a commit.
  if (is_main_frame_) {
    GetRenderWidget()->DidNavigate();

    // Update the URL used to key Ukm metrics in the compositor if the
    // navigation is not in the same document, which represents a new source
    // URL.
    // Note that this is only done for the main frame since the metrics for all
    // frames are keyed to the main frame's URL.
    if (GetRenderWidget()->layer_tree_view())
      GetRenderWidget()->layer_tree_view()->SetURLForUkm(GetLoadingUrl());
  }

  service_manager::mojom::InterfaceProviderRequest
      remote_interface_provider_request;
  if (global_object_reuse_policy !=
      blink::WebGlobalObjectReusePolicy::kUseExisting) {
    // If we're navigating to a new document, bind |remote_interfaces_| to a new
    // message pipe. The request end of the new InterfaceProvider interface will
    // be sent over as part of DidCommitProvisionalLoad. After the RFHI receives
    // the commit confirmation, it will immediately close the old message pipe
    // to avoid GetInterface calls racing with navigation commit, and bind the
    // request end of the message pipe created here.
    service_manager::mojom::InterfaceProviderPtr interfaces_provider;
    remote_interface_provider_request = mojo::MakeRequest(&interfaces_provider);

    // Must initialize |remote_interfaces_| with a new working pipe *before*
    // observers receive DidCommitProvisionalLoad, so they can already request
    // remote interfaces. The interface requests will be serviced once the
    // InterfaceProvider interface request is bound by the RenderFrameHostImpl.
    remote_interfaces_.Close();
    remote_interfaces_.Bind(std::move(interfaces_provider));

    // AudioOutputIPCFactory may be null in tests.
    if (auto* factory = AudioOutputIPCFactory::get()) {
      // The RendererAudioOutputStreamFactory must be readily accessible on the
      // IO thread when it's needed, because the main thread may block while
      // waiting for the factory call to finish on the IO thread, so if we tried
      // to lazily initialize it, we could deadlock.
      //
      // TODO(https://crbug.com/668275): Still, it is odd for one specific
      // factory to be registered here, make this a RenderFrameObserver.
      // code.
      factory->MaybeDeregisterRemoteFactory(GetRoutingID());
      factory->RegisterRemoteFactory(GetRoutingID(), GetRemoteInterfaces());
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

  navigation_state->RunCommitNavigationCallback(blink::mojom::CommitResult::Ok);

  ui::PageTransition transition = GetTransitionType(frame_->GetDocumentLoader(),
                                                    frame_, true /* loading */);
  DidCommitNavigationInternal(item, commit_type,
                              false /* was_within_same_document */, transition,
                              std::move(remote_interface_provider_request));

  // Record time between receiving the message to commit the navigation until it
  // has committed. Only successful cross-document navigation handled by the
  // browser process are taken into account.
  if (!navigation_state->time_commit_requested().is_null()) {
    RecordReadyToCommitUntilCommitHistogram(
        base::TimeTicks::Now() - navigation_state->time_commit_requested(),
        transition);
  }

  // If we end up reusing this WebRequest (for example, due to a #ref click),
  // we don't want the transition type to persist.  Just clear it.
  navigation_state->set_transition_type(ui::PAGE_TRANSITION_LINK);

  // Check whether we have new encoding name.
  UpdateEncoding(frame_, frame_->View()->PageEncoding().Utf8());

  // Reset certificate warning state that prevents log spam.
  num_certificate_warning_messages_ = 0;
  certificate_warning_origins_.clear();
}

void RenderFrameImpl::DidCreateNewDocument() {
  for (auto& observer : observers_)
    observer.DidCreateNewDocument();
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
    GpuBenchmarking::Install(this);

  if (command_line.HasSwitch(switches::kEnableSkiaBenchmarking))
    SkiaBenchmarking::Install(frame_);

  for (auto& observer : render_view_->observers())
    observer.DidClearWindowObject(frame_);
  for (auto& observer : observers_)
    observer.DidClearWindowObject();
}

void RenderFrameImpl::DidCreateDocumentElement() {
  // Notify the browser about non-blank documents loading in the top frame.
  GURL url = frame_->GetDocument().Url();
  if (url.is_valid() && url.spec() != url::kAboutBlankURL) {
    // TODO(nasko): Check if webview()->mainFrame() is the same as the
    // frame_->tree()->top().
    blink::WebFrame* main_frame = render_view_->webview()->MainFrame();
    if (frame_ == main_frame) {
      // For now, don't remember plugin zoom values.  We don't want to mix them
      // with normal web content (i.e. a fixed layout plugin would usually want
      // them different).
      render_view_->Send(new ViewHostMsg_DocumentAvailableInMainFrame(
          render_view_->GetRoutingID(),
          frame_->GetDocument().IsPluginDocument()));
    }
  }

  for (auto& observer : observers_)
    observer.DidCreateDocumentElement();
}

void RenderFrameImpl::RunScriptsAtDocumentElementAvailable() {
  GetContentClient()->renderer()->RunScriptsAtDocumentStart(this);
  // Do not use |this|! ContentClient might have deleted them by now!
}

void RenderFrameImpl::DidReceiveTitle(const blink::WebString& title,
                                      blink::WebTextDirection direction) {
  // Ignore all but top level navigations.
  if (!frame_->Parent()) {
    base::trace_event::TraceLog::GetInstance()->UpdateProcessLabel(
        routing_id_, title.Utf8());

    base::string16 title16 = title.Utf16();
    base::string16 shortened_title = title16.substr(0, kMaxTitleChars);
    Send(new FrameHostMsg_UpdateTitle(routing_id_,
                                      shortened_title, direction));
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

void RenderFrameImpl::DidChangeIcon(blink::WebIconURL::Type icon_type) {
  SendUpdateFaviconURL();
}

void RenderFrameImpl::SendUpdateFaviconURL() {
  if (frame_->Parent())
    return;

  blink::WebIconURL::Type icon_types_mask =
      static_cast<blink::WebIconURL::Type>(
          blink::WebIconURL::kTypeFavicon |
          blink::WebIconURL::kTypeTouchPrecomposed |
          blink::WebIconURL::kTypeTouch);

  WebVector<blink::WebIconURL> icon_urls = frame_->IconURLs(icon_types_mask);
  if (icon_urls.empty())
    return;

  std::vector<FaviconURL> urls;
  urls.reserve(icon_urls.size());
  for (const blink::WebIconURL& icon_url : icon_urls) {
    urls.push_back(FaviconURL(icon_url.GetIconURL(),
                              ToFaviconType(icon_url.IconType()),
                              ConvertToFaviconSizes(icon_url.Sizes())));
  }
  DCHECK_EQ(icon_urls.size(), urls.size());

  Send(new FrameHostMsg_UpdateFaviconURL(GetRoutingID(), urls));
}

void RenderFrameImpl::DidFinishDocumentLoad() {
  TRACE_EVENT1("navigation,benchmark,rail",
               "RenderFrameImpl::didFinishDocumentLoad", "id", routing_id_);
  Send(new FrameHostMsg_DidFinishDocumentLoad(routing_id_));

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

  // If this is an empty document with an http status code indicating an error,
  // we may want to display our own error page, so the user doesn't end up
  // with an unexplained blank page.
  if (!document_is_empty)
    return;

  // Display error page instead of a blank page, if appropriate.
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentLoader(
          frame_->GetDocumentLoader());
  int http_status_code = internal_data->http_status_code();
  if (GetContentClient()->renderer()->HasErrorPage(http_status_code)) {
    // This call may run scripts, e.g. via the beforeunload event.
    std::unique_ptr<DocumentState> document_state(BuildDocumentState());
    std::unique_ptr<blink::WebNavigationParams> navigation_params =
        std::make_unique<blink::WebNavigationParams>();
    navigation_params->service_worker_network_provider =
        BuildServiceWorkerNetworkProviderForNavigation(
            nullptr /* request_params */, nullptr /* controller_info */);
    LoadNavigationErrorPageForHttpStatusError(
        frame_->GetDocumentLoader()->GetRequest(), frame_->GetDocument().Url(),
        http_status_code, true /* replace */, nullptr /* entry */,
        std::move(navigation_params), std::move(document_state));
  }
  // Do not use |this| or |frame_| here without checking |weak_self|.
}

void RenderFrameImpl::RunScriptsAtDocumentIdle() {
  GetContentClient()->renderer()->RunScriptsAtDocumentIdle(this);
  // ContentClient might have deleted |this| by now!
}

void RenderFrameImpl::DidHandleOnloadEvents() {
  if (!frame_->Parent()) {
    Send(new FrameHostMsg_DocumentOnLoadCompleted(routing_id_));
  }
  for (auto& observer : observers_)
    observer.DidHandleOnloadEvents();
}

void RenderFrameImpl::DidFailLoad(const WebURLError& error,
                                  blink::WebHistoryCommitType commit_type) {
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::didFailLoad",
               "id", routing_id_);
  // TODO(nasko): Move implementation here. No state needed.
  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();
  DCHECK(document_loader);

  const WebURLRequest& failed_request = document_loader->GetRequest();
  base::string16 error_description;
  GetContentClient()->renderer()->GetErrorDescription(failed_request, error,
                                                      &error_description);
  Send(new FrameHostMsg_DidFailLoadWithError(
      routing_id_, failed_request.Url(), error.reason(), error_description));
}

void RenderFrameImpl::DidFinishLoad() {
  TRACE_EVENT1("navigation,benchmark,rail",
               "RenderFrameImpl::didFinishLoad", "id", routing_id_);
  if (!frame_->Parent()) {
    TRACE_EVENT_INSTANT0("WebCore,benchmark,rail", "LoadFinished",
                         TRACE_EVENT_SCOPE_PROCESS);
  }

  for (auto& observer : observers_)
    observer.DidFinishLoad();

  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();
  Send(new FrameHostMsg_DidFinishLoad(routing_id_,
                                      document_loader->GetRequest().Url()));

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

  ui::PageTransition transition = GetTransitionType(frame_->GetDocumentLoader(),
                                                    frame_, true /* loading */);
  DidCommitNavigationInternal(item, commit_type,
                              true /* was_within_same_document */, transition,
                              nullptr /* remote_interface_provider_request */);
}

void RenderFrameImpl::DidUpdateCurrentHistoryItem() {
  render_view_->StartNavStateSyncTimerIfNecessary(this);
}

void RenderFrameImpl::DidChangeThemeColor() {
  if (frame_->Parent())
    return;

  Send(new FrameHostMsg_DidChangeThemeColor(
      routing_id_, frame_->GetDocument().ThemeColor()));
}

void RenderFrameImpl::ForwardResourceTimingToParent(
    const blink::WebResourceTimingInfo& info) {
  Send(new FrameHostMsg_ForwardResourceTimingToParent(
      routing_id_, WebResourceTimingInfoToResourceTimingInfo(info)));
}

void RenderFrameImpl::DispatchLoad() {
  Send(new FrameHostMsg_DispatchLoad(routing_id_));
}

blink::WebEffectiveConnectionType
RenderFrameImpl::GetEffectiveConnectionType() {
  return effective_connection_type_;
}

void RenderFrameImpl::SetEffectiveConnectionTypeForTesting(
    blink::WebEffectiveConnectionType type) {
  effective_connection_type_ = type;
}

blink::WebURLRequest::PreviewsState RenderFrameImpl::GetPreviewsStateForFrame()
    const {
  PreviewsState disabled_state = previews_state_ & kDisabledPreviewsBits;
  if (disabled_state) {
    // Sanity check disabled vs. enabled bits here before passing on.
    DCHECK(!(previews_state_ & ~kDisabledPreviewsBits)) << previews_state_;
    return disabled_state;
  }
  return static_cast<WebURLRequest::PreviewsState>(previews_state_);
}

void RenderFrameImpl::DidBlockFramebust(const WebURL& url) {
  Send(new FrameHostMsg_DidBlockFramebust(GetRoutingID(), url));
}

base::UnguessableToken RenderFrameImpl::GetDevToolsFrameToken() {
  return devtools_frame_token_;
}

void RenderFrameImpl::RenderFallbackContentInParentProcess() {
  Send(new FrameHostMsg_RenderFallbackContentInParentProcess(routing_id_));
}

void RenderFrameImpl::AbortClientNavigation() {
  browser_side_navigation_pending_ = false;
  if (!IsPerNavigationMojoInterfaceEnabled())
    Send(new FrameHostMsg_AbortNavigation(routing_id_));
}

void RenderFrameImpl::DidChangeSelection(bool is_empty_selection) {
  if (!GetRenderWidget()->input_handler().handling_input_event() &&
      !handling_select_range_)
    return;

  if (is_empty_selection)
    selection_text_.clear();

  // UpdateTextInputState should be called before SyncSelectionIfRequired.
  // UpdateTextInputState may send TextInputStateChanged to notify the focus
  // was changed, and SyncSelectionIfRequired may send SelectionChanged
  // to notify the selection was changed.  Focus change should be notified
  // before selection change.
  GetRenderWidget()->UpdateTextInputState();
  SyncSelectionIfRequired();
}

bool RenderFrameImpl::HandleCurrentKeyboardEvent() {
  bool did_execute_command = false;
  for (auto command : GetRenderWidget()->edit_commands()) {
    // In gtk and cocoa, it's possible to bind multiple edit commands to one
    // key (but it's the exception). Once one edit command is not executed, it
    // seems safest to not execute the rest.
    if (!frame_->ExecuteCommand(blink::WebString::FromUTF8(command.name),
                                blink::WebString::FromUTF8(command.value)))
      break;
    did_execute_command = true;
  }

  return did_execute_command;
}

void RenderFrameImpl::RunModalAlertDialog(const blink::WebString& message) {
  RunJavaScriptDialog(JAVASCRIPT_DIALOG_TYPE_ALERT, message.Utf16(),
                      base::string16(), nullptr);
}

bool RenderFrameImpl::RunModalConfirmDialog(const blink::WebString& message) {
  return RunJavaScriptDialog(JAVASCRIPT_DIALOG_TYPE_CONFIRM, message.Utf16(),
                             base::string16(), nullptr);
}

bool RenderFrameImpl::RunModalPromptDialog(
    const blink::WebString& message,
    const blink::WebString& default_value,
    blink::WebString* actual_value) {
  base::string16 result;
  bool ok = RunJavaScriptDialog(JAVASCRIPT_DIALOG_TYPE_PROMPT, message.Utf16(),
                                default_value.Utf16(), &result);
  if (ok)
    *actual_value = WebString::FromUTF16(result);
  return ok;
}

bool RenderFrameImpl::RunModalBeforeUnloadDialog(bool is_reload) {
  // Don't allow further dialogs if we are waiting to swap out, since the
  // ScopedPageLoadDeferrer in our stack prevents it.
  if (suppress_further_dialogs_)
    return false;

  bool success = false;
  // This is an ignored return value, but is included so we can accept the same
  // response as RunJavaScriptDialog.
  base::string16 ignored_result;
  Send(new FrameHostMsg_RunBeforeUnloadConfirm(routing_id_, is_reload, &success,
                                               &ignored_result));
  return success;
}

bool RenderFrameImpl::RunFileChooser(
    const blink::WebFileChooserParams& params,
    blink::WebFileChooserCompletion* chooser_completion) {
  blink::mojom::FileChooserParams ipc_params;
  ipc_params.mode = params.mode;
  ipc_params.title = params.title.Utf16();
  ipc_params.accept_types.reserve(params.accept_types.size());
  for (const auto& type : params.accept_types)
    ipc_params.accept_types.push_back(type.Utf16());
  ipc_params.need_local_path = params.need_local_path;
  ipc_params.use_media_capture = params.use_media_capture;
  ipc_params.requestor = params.requestor;
  return RunFileChooser(ipc_params, chooser_completion);
}

bool RenderFrameImpl::RunFileChooser(
    const blink::mojom::FileChooserParams& params,
    blink::WebFileChooserCompletion* chooser_completion) {
  // Do not open the file dialog in a hidden RenderFrame.
  if (IsHidden())
    return false;
  DCHECK(!file_chooser_completion_);
  file_chooser_completion_ = chooser_completion;
  Send(new FrameHostMsg_RunFileChooser(routing_id_, params));
  return true;
}

void RenderFrameImpl::ShowContextMenu(const blink::WebContextMenuData& data) {
  ContextMenuParams params = ContextMenuParamsBuilder::Build(data);
  blink::WebRect position_in_window(params.x, params.y, 0, 0);
  GetRenderWidget()->ConvertViewportToWindow(&position_in_window);
  params.x = position_in_window.x;
  params.y = position_in_window.y;
  GetRenderWidget()->OnShowHostContextMenu(&params);
  if (GetRenderWidget()->has_host_context_menu_location()) {
    params.x = GetRenderWidget()->host_context_menu_location().x();
    params.y = GetRenderWidget()->host_context_menu_location().y();
  }

  // Serializing a GURL longer than kMaxURLChars will fail, so don't do
  // it.  We replace it with an empty GURL so the appropriate items are disabled
  // in the context menu.
  // TODO(jcivelli): http://crbug.com/45160 This prevents us from saving large
  //                 data encoded images.  We should have a way to save them.
  if (params.src_url.spec().size() > url::kMaxURLChars)
    params.src_url = GURL();

  blink::WebRect selection_in_window(data.selection_rect);
  GetRenderWidget()->ConvertViewportToWindow(&selection_in_window);
  params.selection_rect = selection_in_window;

#if defined(OS_ANDROID)
  // The Samsung Email app relies on the context menu being shown after the
  // javascript onselectionchanged is triggered.
  // See crbug.com/729488
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&RenderFrameImpl::ShowDeferredContextMenu,
                            weak_factory_.GetWeakPtr(), params));
#else
  ShowDeferredContextMenu(params);
#endif
}

void RenderFrameImpl::ShowDeferredContextMenu(const ContextMenuParams& params) {
  Send(new FrameHostMsg_ContextMenu(routing_id_, params));
}

void RenderFrameImpl::SaveImageFromDataURL(const blink::WebString& data_url) {
  // Note: We should basically send GURL but we use size-limited string instead
  // in order to send a larger data url to save a image for <canvas> or <img>.
  if (data_url.length() < kMaxLengthOfDataURLString) {
    Send(new FrameHostMsg_SaveImageFromDataURL(render_view_->GetRoutingID(),
                                               routing_id_, data_url.Utf8()));
  }
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

void RenderFrameImpl::WillSendRequest(blink::WebURLRequest& request) {
  if (committing_main_request_ &&
      request.GetFrameType() !=
          network::mojom::RequestContextFrameType::kNone) {
    // We should not process this request, as it was already processed
    // as part of BeginNavigation.
    return;
  }

  if (render_view_->renderer_preferences_.enable_do_not_track)
    request.SetHTTPHeaderField(blink::WebString::FromUTF8(kDoNotTrackHeader),
                               "1");

  WebDocumentLoader* provisional_document_loader =
      frame_->GetProvisionalDocumentLoader();
  WebDocumentLoader* document_loader = provisional_document_loader
                                           ? provisional_document_loader
                                           : frame_->GetDocumentLoader();
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentLoader(document_loader);
  NavigationState* navigation_state = internal_data->navigation_state();
  ui::PageTransition transition_type =
      GetTransitionType(document_loader, frame_, false /* loading */);
  if (provisional_document_loader &&
      provisional_document_loader->IsClientRedirect()) {
    transition_type = ui::PageTransitionFromInt(
        transition_type | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  }

  ApplyFilePathAlias(&request);
  GURL new_url;
  bool attach_same_site_cookies = false;
  base::Optional<url::Origin> initiator_origin =
      request.RequestorOrigin().IsNull()
          ? base::Optional<url::Origin>()
          : base::Optional<url::Origin>(request.RequestorOrigin());
  GetContentClient()->renderer()->WillSendRequest(
      frame_, transition_type, request.Url(),
      base::OptionalOrNullptr(initiator_origin), &new_url,
      &attach_same_site_cookies);
  if (!new_url.is_empty())
    request.SetURL(WebURL(new_url));

  if (internal_data->is_cache_policy_override_set())
    request.SetCacheMode(internal_data->cache_policy_override());

  // The request's extra data may indicate that we should set a custom user
  // agent. This needs to be done here, after WebKit is through with setting the
  // user agent on its own.
  WebString custom_user_agent;
  std::unique_ptr<NavigationResponseOverrideParameters> response_override;
  if (request.GetExtraData()) {
    RequestExtraData* old_extra_data =
        static_cast<RequestExtraData*>(request.GetExtraData());

    custom_user_agent = old_extra_data->custom_user_agent();
    if (!custom_user_agent.IsNull()) {
      if (custom_user_agent.IsEmpty())
        request.ClearHTTPHeaderField("User-Agent");
      else
        request.SetHTTPHeaderField("User-Agent", custom_user_agent);
    }
    response_override =
        old_extra_data->TakeNavigationResponseOverrideOwnership();
  }

  // Set an empty HTTP origin header for non GET methods if none is currently
  // present.
  request.SetHTTPOriginIfNeeded(WebSecurityOrigin::CreateUnique());

  WebFrame* parent = frame_->Parent();

  ResourceType resource_type = WebURLRequestToResourceType(request);
  WebDocument frame_document = frame_->GetDocument();
  if (!request.GetExtraData())
    request.SetExtraData(std::make_unique<RequestExtraData>());
  auto* extra_data = static_cast<RequestExtraData*>(request.GetExtraData());
  extra_data->set_visibility_state(VisibilityState());
  extra_data->set_custom_user_agent(custom_user_agent);
  extra_data->set_render_frame_id(routing_id_);
  extra_data->set_is_main_frame(!parent);
  extra_data->set_allow_download(
      navigation_state->common_params().allow_download);
  extra_data->set_transition_type(transition_type);
  extra_data->set_navigation_response_override(std::move(response_override));
  bool is_for_no_state_prefetch =
      GetContentClient()->renderer()->IsPrefetchOnly(this, request);
  extra_data->set_is_for_no_state_prefetch(is_for_no_state_prefetch);
  extra_data->set_initiated_in_secure_context(frame_document.IsSecureContext());
  extra_data->set_attach_same_site_cookies(attach_same_site_cookies);
  extra_data->set_frame_request_blocker(frame_request_blocker_);

  request.SetDownloadToNetworkCacheOnly(
      is_for_no_state_prefetch && resource_type != RESOURCE_TYPE_MAIN_FRAME);

  // The RenderThreadImpl or its URLLoaderThrottleProvider member may not be
  // valid in some tests.
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (render_thread && render_thread->url_loader_throttle_provider()) {
    extra_data->set_url_loader_throttles(
        render_thread->url_loader_throttle_provider()->CreateThrottles(
            routing_id_, request, resource_type));
  }

  if (request.GetPreviewsState() == WebURLRequest::kPreviewsUnspecified) {
    if (is_main_frame_ && !navigation_state->request_committed()) {
      request.SetPreviewsState(static_cast<WebURLRequest::PreviewsState>(
          navigation_state->common_params().previews_state));
    } else {
      WebURLRequest::PreviewsState request_previews_state =
          static_cast<WebURLRequest::PreviewsState>(previews_state_);

      // The decision of whether or not to enable Client Lo-Fi is made earlier
      // in the request lifetime, in LocalFrame::MaybeAllowImagePlaceholder(),
      // so don't add the Client Lo-Fi bit to the request here.
      request_previews_state &= ~(WebURLRequest::kClientLoFiOn);
      request_previews_state &= ~(WebURLRequest::kLazyImageLoadDeferred);
      if (request_previews_state == WebURLRequest::kPreviewsUnspecified)
        request_previews_state = WebURLRequest::kPreviewsOff;

      request.SetPreviewsState(request_previews_state);
    }
  }

  // This is an instance where we embed a copy of the routing id
  // into the data portion of the message. This can cause problems if we
  // don't register this id on the browser side, since the download manager
  // expects to find a RenderViewHost based off the id.
  request.SetRequestorID(render_view_->GetRoutingID());
  request.SetHasUserGesture(
      WebUserGestureIndicator::IsProcessingUserGesture(frame_));

  if (!render_view_->renderer_preferences_.enable_referrers)
    request.SetHTTPReferrer(WebString(),
                            network::mojom::ReferrerPolicy::kDefault);
}

void RenderFrameImpl::DidReceiveResponse(
    const blink::WebURLResponse& response) {
  // For main resource, this is done in CommitNavigation instead.
  // TODO(dgozman): get rid of this method once we always go through
  // CommitNavigation, even for urls like about:blank.
  if (replaying_main_response_)
    return;

  // Only do this for responses that correspond to a provisional data source
  // of the top-most frame.  If we have a provisional data source, then we
  // can't have any sub-resources yet, so we know that this response must
  // correspond to a frame load.
  if (!frame_->GetProvisionalDocumentLoader() || frame_->Parent())
    return;

  // If we are in view source mode, then just let the user see the source of
  // the server's error page.
  if (frame_->IsViewSourceModeEnabled())
    return;

  DocumentState* document_state =
      DocumentState::FromDocumentLoader(frame_->GetProvisionalDocumentLoader());
  int http_status_code = response.HttpStatusCode();

  // Record page load flags.
  WebURLResponseExtraDataImpl* extra_data = GetExtraDataFromResponse(response);
  if (extra_data) {
    document_state->set_was_fetched_via_spdy(
        extra_data->was_fetched_via_spdy());
    document_state->set_was_alpn_negotiated(extra_data->was_alpn_negotiated());
    document_state->set_alpn_negotiated_protocol(
        response.AlpnNegotiatedProtocol().Utf8());
    document_state->set_was_alternate_protocol_available(
        extra_data->was_alternate_protocol_available());
    document_state->set_connection_info(response.ConnectionInfo());
  }
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);
  internal_data->set_http_status_code(http_status_code);
}

void RenderFrameImpl::DidLoadResourceFromMemoryCache(
    const blink::WebURLRequest& request,
    const blink::WebURLResponse& response) {
  // The recipients of this message have no use for data: URLs: they don't
  // affect the page's insecure content list and are not in the disk cache. To
  // prevent large (1M+) data: URLs from crashing in the IPC system, we simply
  // filter them out here.
  if (request.Url().ProtocolIs(url::kDataScheme))
    return;

  // Let the browser know we loaded a resource from the memory cache.  This
  // message is needed to display the correct SSL indicators.
  Send(new FrameHostMsg_DidLoadResourceFromMemoryCache(
      routing_id_, request.Url(), request.HttpMethod().Utf8(),
      response.MimeType().Utf8(), WebURLRequestToResourceType(request)));
}

void RenderFrameImpl::DidStartResponse(
    int request_id,
    const network::ResourceResponseHead& response_head,
    content::ResourceType resource_type) {
  for (auto& observer : observers_)
    observer.DidStartResponse(request_id, response_head, resource_type);
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

void RenderFrameImpl::DidDisplayInsecureContent() {
  Send(new FrameHostMsg_DidDisplayInsecureContent(routing_id_));
}

void RenderFrameImpl::DidContainInsecureFormAction() {
  Send(new FrameHostMsg_DidContainInsecureFormAction(routing_id_));
}

void RenderFrameImpl::DidRunInsecureContent(
    const blink::WebSecurityOrigin& origin,
    const blink::WebURL& target) {
  Send(new FrameHostMsg_DidRunInsecureContent(
      routing_id_, GURL(origin.ToString().Utf8()), target));
  GetContentClient()->renderer()->RecordRapporURL(
      "ContentSettings.MixedScript.RanMixedScript",
      GURL(origin.ToString().Utf8()));
}

void RenderFrameImpl::DidDisplayContentWithCertificateErrors() {
  Send(new FrameHostMsg_DidDisplayContentWithCertificateErrors(routing_id_));
}

void RenderFrameImpl::DidRunContentWithCertificateErrors() {
  Send(new FrameHostMsg_DidRunContentWithCertificateErrors(routing_id_));
}

void RenderFrameImpl::ReportLegacySymantecCert(const blink::WebURL& url,
                                               bool did_fail) {
  url::Origin origin = url::Origin::Create(GURL(url));
  // To prevent log spam, only log the message once per origin.
  if (certificate_warning_origins_.find(origin) !=
      certificate_warning_origins_.end()) {
    return;
  }

  // After |kMaxCertificateWarningMessages| warnings, stop printing messages to
  // the console. At exactly |kMaxCertificateWarningMessages| warnings, print a
  // message that additional resources on the page use legacy certificates
  // without specifying which exact resources. Before
  // |kMaxCertificateWarningMessages| messages, print the exact resource URL in
  // the message to help the developer pinpoint the problematic resources.
  if (num_certificate_warning_messages_ > kMaxCertificateWarningMessages)
    return;

  std::string console_message;

  if (num_certificate_warning_messages_ == kMaxCertificateWarningMessages) {
    if (did_fail) {
      console_message =
          "Additional resources on this page were loaded with "
          "SSL certificates that have been "
          "distrusted. See "
          "https://g.co/chrome/symantecpkicerts for "
          "more information.";
    } else {
      console_message =
          "Additional resources on this page were loaded with "
          "SSL certificates that will be "
          "distrusted in the future. "
          "Once distrusted, users will be prevented from "
          "loading these resources. See "
          "https://g.co/chrome/symantecpkicerts for "
          "more information.";
    }
  } else {
    // The embedder is given a chance to override the message for certs that
    // will be distrusted in future, but not for certs that have already been
    // distrusted. (This is because there is no embedder-specific release
    // information in the message for certs that have already been distrusted.)
    if (did_fail) {
      console_message = base::StringPrintf(
          "The SSL certificate used to load resources from %s"
          " has been distrusted. See "
          "https://g.co/chrome/symantecpkicerts for "
          "more information.",
          origin.Serialize().c_str());
    } else if (!GetContentClient()
                    ->renderer()
                    ->OverrideLegacySymantecCertConsoleMessage(
                        GURL(url), &console_message)) {
      console_message = base::StringPrintf(
          "The SSL certificate used to load resources from %s"
          " will be "
          "distrusted in the future. "
          "Once distrusted, users will be prevented from "
          "loading these resources. See "
          "https://g.co/chrome/symantecpkicerts for "
          "more information.",
          origin.Serialize().c_str());
    }
  }
  num_certificate_warning_messages_++;
  certificate_warning_origins_.insert(origin);
  // To avoid spamming the console, use Verbose message level for subframe
  // resources and for certificates that will be distrusted in future, and only
  // use the warning level for main-frame resources or resources that have
  // already been distrusted.
  AddMessageToConsole((frame_->Parent() && !did_fail)
                          ? CONSOLE_MESSAGE_LEVEL_VERBOSE
                          : CONSOLE_MESSAGE_LEVEL_WARNING,
                      console_message);
}

void RenderFrameImpl::DidChangePerformanceTiming() {
  for (auto& observer : observers_)
    observer.DidChangePerformanceTiming();
}

void RenderFrameImpl::DidObserveLoadingBehavior(
    blink::WebLoadingBehaviorFlag behavior) {
  for (auto& observer : observers_)
    observer.DidObserveLoadingBehavior(behavior);
}

void RenderFrameImpl::DidObserveNewFeatureUsage(
    blink::mojom::WebFeature feature) {
  for (auto& observer : observers_)
    observer.DidObserveNewFeatureUsage(feature);
}

void RenderFrameImpl::DidObserveNewCssPropertyUsage(int css_property,
                                                    bool is_animated) {
  for (auto& observer : observers_)
    observer.DidObserveNewCssPropertyUsage(css_property, is_animated);
}

void RenderFrameImpl::DidObserveLayoutJank(double jank_fraction) {
  for (auto& observer : observers_)
    observer.DidObserveLayoutJank(jank_fraction);
}

bool RenderFrameImpl::ShouldTrackUseCounter(const blink::WebURL& url) {
  return GetContentClient()->renderer()->ShouldTrackUseCounter(url);
}

void RenderFrameImpl::DidCreateScriptContext(v8::Local<v8::Context> context,
                                             int world_id) {
  if ((enabled_bindings_ & BINDINGS_POLICY_MOJO_WEB_UI) && IsMainFrame() &&
      world_id == ISOLATED_WORLD_ID_GLOBAL) {
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

blink::WebPushClient* RenderFrameImpl::PushClient() {
  if (!push_messaging_client_)
    push_messaging_client_ = new PushMessagingClient(this);
  return push_messaging_client_;
}

blink::WebRelatedAppsFetcher* RenderFrameImpl::GetRelatedAppsFetcher() {
  if (!related_apps_fetcher_)
    related_apps_fetcher_.reset(new RelatedAppsFetcher(&GetManifestManager()));

  return related_apps_fetcher_.get();
}

void RenderFrameImpl::WillStartUsingPeerConnectionHandler(
    blink::WebRTCPeerConnectionHandler* handler) {
  static_cast<RTCPeerConnectionHandler*>(handler)->associateWithFrame(frame_);
}

blink::WebUserMediaClient* RenderFrameImpl::UserMediaClient() {
  if (!web_user_media_client_)
    InitializeUserMediaClient();
  return web_user_media_client_;
}

blink::WebEncryptedMediaClient* RenderFrameImpl::EncryptedMediaClient() {
  return media_factory_.EncryptedMediaClient();
}

blink::WebString RenderFrameImpl::UserAgentOverride() {
  if (!render_view_->webview() || !render_view_->webview()->MainFrame() ||
      render_view_->renderer_preferences_.user_agent_override.empty()) {
    return blink::WebString();
  }

  // TODO(nasko): When the top-level frame is remote, there is no
  // WebDocumentLoader associated with it, so the checks below are not valid.
  // Temporarily return early and fix properly as part of
  // https://crbug.com/426555.
  if (render_view_->webview()->MainFrame()->IsWebRemoteFrame())
    return blink::WebString();
  WebLocalFrame* main_frame =
      render_view_->webview()->MainFrame()->ToWebLocalFrame();

  // If we're in the middle of committing a load, the data source we need
  // will still be provisional.
  WebDocumentLoader* document_loader = nullptr;
  if (main_frame->GetProvisionalDocumentLoader())
    document_loader = main_frame->GetProvisionalDocumentLoader();
  else
    document_loader = main_frame->GetDocumentLoader();

  InternalDocumentStateData* internal_data =
      document_loader
          ? InternalDocumentStateData::FromDocumentLoader(document_loader)
          : nullptr;
  if (internal_data && internal_data->is_overriding_user_agent())
    return WebString::FromUTF8(
        render_view_->renderer_preferences_.user_agent_override);
  return blink::WebString();
}

blink::WebString RenderFrameImpl::DoNotTrackValue() {
  if (render_view_->renderer_preferences_.enable_do_not_track)
    return WebString::FromUTF8("1");
  return WebString();
}

mojom::RendererAudioInputStreamFactory*
RenderFrameImpl::GetAudioInputStreamFactory() {
  if (!audio_input_stream_factory_)
    GetRemoteInterfaces()->GetInterface(&audio_input_stream_factory_);
  return audio_input_stream_factory_.get();
}

bool RenderFrameImpl::ShouldBlockWebGL() {
  bool blocked = true;
  Send(new FrameHostMsg_Are3DAPIsBlocked(
      routing_id_, url::Origin(frame_->Top()->GetSecurityOrigin()).GetURL(),
      THREE_D_API_TYPE_WEBGL, &blocked));
  return blocked;
}

bool RenderFrameImpl::AllowContentInitiatedDataUrlNavigations(
    const blink::WebURL& url) {
  // Error pages can navigate to data URLs.
  return url.GetString() == kUnreachableWebDataURL;
}

void RenderFrameImpl::PostAccessibilityEvent(const blink::WebAXObject& obj,
                                             ax::mojom::Event event) {
  if (render_accessibility_)
    render_accessibility_->HandleWebAccessibilityEvent(obj, event);
}

void RenderFrameImpl::MarkWebAXObjectDirty(const blink::WebAXObject& obj,
                                           bool subtree) {
  if (render_accessibility_)
    render_accessibility_->MarkWebAXObjectDirty(obj, subtree);
}

void RenderFrameImpl::HandleAccessibilityFindInPageResult(
    int identifier,
    int match_index,
    const blink::WebNode& start_node,
    int start_offset,
    const blink::WebNode& end_node,
    int end_offset) {
  if (render_accessibility_) {
    render_accessibility_->HandleAccessibilityFindInPageResult(
        identifier, match_index, blink::WebAXObject::FromWebNode(start_node),
        start_offset, blink::WebAXObject::FromWebNode(end_node), end_offset);
  }
}

void RenderFrameImpl::DidChangeManifest() {
  for (auto& observer : observers_)
    observer.DidChangeManifest();
}

void RenderFrameImpl::EnterFullscreen(
    const blink::WebFullscreenOptions& options) {
  Send(new FrameHostMsg_EnterFullscreen(routing_id_, options));
}

void RenderFrameImpl::ExitFullscreen() {
  Send(new FrameHostMsg_ExitFullscreen(routing_id_));
}

void RenderFrameImpl::FullscreenStateChanged(bool is_fullscreen) {
  GetFrameHost()->FullscreenStateChanged(is_fullscreen);
}

void RenderFrameImpl::SuddenTerminationDisablerChanged(
    bool present,
    blink::WebSuddenTerminationDisablerType disabler_type) {
  Send(new FrameHostMsg_SuddenTerminationDisablerChanged(routing_id_, present,
                                                         disabler_type));
}

void RenderFrameImpl::RegisterProtocolHandler(const WebString& scheme,
                                              const WebURL& url,
                                              const WebString& title) {
  bool user_gesture = WebUserGestureIndicator::IsProcessingUserGesture(frame_);
  Send(new FrameHostMsg_RegisterProtocolHandler(routing_id_, scheme.Utf8(), url,
                                                title.Utf16(), user_gesture));
}

void RenderFrameImpl::UnregisterProtocolHandler(const WebString& scheme,
                                                const WebURL& url) {
  bool user_gesture = WebUserGestureIndicator::IsProcessingUserGesture(frame_);
  Send(new FrameHostMsg_UnregisterProtocolHandler(routing_id_, scheme.Utf8(),
                                                  url, user_gesture));
}

void RenderFrameImpl::DidSerializeDataForFrame(
    const WebVector<char>& data,
    WebFrameSerializerClient::FrameSerializationStatus status) {
  bool end_of_data =
      status == WebFrameSerializerClient::kCurrentFrameIsFinished;
  Send(new FrameHostMsg_SerializedHtmlWithLocalLinksResponse(
      routing_id_, std::string(data.Data(), data.size()), end_of_data));
}

void RenderFrameImpl::AddObserver(RenderFrameObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderFrameImpl::RemoveObserver(RenderFrameObserver* observer) {
  observer->RenderFrameGone();
  observers_.RemoveObserver(observer);
}

void RenderFrameImpl::OnStop() {
  DCHECK(frame_);

  // The stopLoading call may run script, which may cause this frame to be
  // detached/deleted.  If that happens, return immediately.
  base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();
  frame_->StopLoading();
  if (!weak_this)
    return;

  for (auto& observer : observers_)
    observer.OnStop();
}

void RenderFrameImpl::OnDroppedNavigation() {
  browser_side_navigation_pending_ = false;
  browser_side_navigation_pending_url_ = GURL();
  frame_->ClientDroppedNavigation();
}

void RenderFrameImpl::OnCollapse(bool collapsed) {
  frame_->Collapse(collapsed);
}

void RenderFrameImpl::WasHidden() {
  for (auto& observer : observers_)
    observer.WasHidden();

#if BUILDFLAG(ENABLE_PLUGINS)
  for (auto* plugin : active_pepper_instances_)
    plugin->PageVisibilityChanged(false);
#endif  // ENABLE_PLUGINS

  if (GetWebFrame()->FrameWidget()) {
    GetWebFrame()->FrameWidget()->SetVisibilityState(VisibilityState());
  }
}

void RenderFrameImpl::WasShown() {
  for (auto& observer : observers_)
    observer.WasShown();

#if BUILDFLAG(ENABLE_PLUGINS)
  for (auto* plugin : active_pepper_instances_)
    plugin->PageVisibilityChanged(true);
#endif  // ENABLE_PLUGINS

  if (GetWebFrame()->FrameWidget()) {
    GetWebFrame()->FrameWidget()->SetVisibilityState(VisibilityState());
  }
}

void RenderFrameImpl::WidgetWillClose() {
  for (auto& observer : observers_)
    observer.WidgetWillClose();
}

bool RenderFrameImpl::IsMainFrame() {
  return is_main_frame_;
}

bool RenderFrameImpl::IsHidden() {
  return GetRenderWidget()->is_hidden();
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
    ui::PageTransition transition) {
  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();
  const WebURLRequest& request = document_loader->GetRequest();
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
      navigation_state->request_params().intended_as_new_entry;
  params->should_replace_current_entry =
      document_loader->ReplacesCurrentHistoryItem();
  params->post_id = -1;
  params->nav_entry_id = navigation_state->request_params().nav_entry_id;
  // We need to track the RenderViewHost routing_id because of downstream
  // dependencies (https://crbug.com/392171 DownloadRequestHandle,
  // SaveFileManager, ResourceDispatcherHostImpl, MediaStreamUIProxy and
  // possibly others). They look up the view based on the ID stored in the
  // resource requests. Once those dependencies are unwound or moved to
  // RenderFrameHost (https://crbug.com/304341) we can move the client to be
  // based on the routing_id of the RenderFrameHost.
  params->render_view_routing_id = render_view_->routing_id();

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
  params->insecure_navigations_set = frame_->GetInsecureRequestToUpgrade();

  params->has_potentially_trustworthy_unique_origin =
      frame_origin.IsUnique() && frame_origin.IsPotentiallyTrustworthy();

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

  params->content_source_id = GetRenderWidget()->GetContentSourceId();

  params->method = request.HttpMethod().Latin1();
  if (params->method == "POST")
    params->post_id = ExtractPostId(current_history_item_);

  params->item_sequence_number = current_history_item_.ItemSequenceNumber();
  params->document_sequence_number =
      current_history_item_.DocumentSequenceNumber();

  // If the page contained a client redirect (meta refresh, document.loc...),
  // set the referrer appropriately.
  if (document_loader->IsClientRedirect()) {
    params->referrer =
        Referrer(params->redirects[0],
                 document_loader->GetRequest().GetReferrerPolicy());
  } else {
    params->referrer = RenderViewImpl::GetReferrerFromRequest(
        frame_, document_loader->GetRequest());
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
        navigation_state->request_params().should_clear_history_list;
  } else {
    // Subframe navigation: the type depends on whether this navigation
    // generated a new session history entry. When they do generate a session
    // history entry, it means the user initiated the navigation and we should
    // mark it as such.
    if (commit_type == blink::kWebStandardCommit)
      params->transition = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
    else
      params->transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;

    DCHECK(!navigation_state->request_params().should_clear_history_list);
    params->history_list_was_cleared = false;
  }

  // Standard URLs must match the reported origin, when it is not unique.
  // This check is very similar to RenderFrameHostImpl::CanCommitOrigin, but
  // adapted to the renderer process side.
  if (!params->origin.opaque() && params->url.IsStandard() &&
      render_view_->GetWebkitPreferences().web_security_enabled) {
    // Exclude file: URLs when settings allow them access any origin.
    if (params->origin.scheme() != url::kFileScheme ||
        !render_view_->GetWebkitPreferences()
             .allow_universal_access_from_file_urls) {
      CHECK(params->origin.IsSameOriginWith(url::Origin::Create(params->url)))
          << " url:" << params->url << " origin:" << params->origin;
    }
  }
  params->request_id = response.RequestId();

  return params;
}

void RenderFrameImpl::UpdateZoomLevel() {
  if (!frame_->Parent()) {
    // Reset the zoom limits in case a plugin had changed them previously. This
    // will also call us back which will cause us to send a message to
    // update WebContentsImpl.
    render_view_->webview()->ZoomLimitsChanged(
        ZoomFactorToZoomLevel(kMinimumZoomFactor),
        ZoomFactorToZoomLevel(kMaximumZoomFactor));

    // Set zoom level, but don't do it for full-page plugin since they don't use
    // the same zoom settings.
    auto host_zoom = host_zoom_levels_.find(GetLoadingUrl());
    if (render_view_->webview()->MainFrame()->IsWebLocalFrame() &&
        render_view_->webview()
            ->MainFrame()
            ->ToWebLocalFrame()
            ->GetDocument()
            .IsPluginDocument()) {
      // Reset the zoom levels for plugins.
      render_view_->SetZoomLevel(0);
    } else {
      // If the zoom level is not found, then do nothing. In-page navigation
      // relies on not changing the zoom level in this case.
      if (host_zoom != host_zoom_levels_.end())
        render_view_->SetZoomLevel(host_zoom->second);
    }

    if (host_zoom != host_zoom_levels_.end()) {
      // This zoom level was merely recorded transiently for this load.  We can
      // erase it now.  If at some point we reload this page, the browser will
      // send us a new, up-to-date zoom level.
      host_zoom_levels_.erase(host_zoom);
    }
  } else {
    // Subframes should match the zoom level of the main frame.
    render_view_->SetZoomLevel(render_view_->page_zoom_level());
  }
}

bool RenderFrameImpl::UpdateNavigationHistory(
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type) {
  NavigationState* navigation_state =
      NavigationState::FromDocumentLoader(frame_->GetDocumentLoader());
  const RequestNavigationParams& request_params =
      navigation_state->request_params();

  // Update the current history item for this frame.
  current_history_item_ = item;
  // Note: don't reference |item| after this point, as its value may not match
  // |current_history_item_|.
  current_history_item_.SetTarget(
      blink::WebString::FromUTF8(unique_name_helper_.value()));
  bool is_new_navigation = commit_type == blink::kWebStandardCommit;
  if (request_params.should_clear_history_list) {
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
  } else if (request_params.nav_entry_id != 0 &&
             !request_params.intended_as_new_entry) {
    render_view_->history_list_offset_ =
        navigation_state->request_params().pending_history_list_offset;
  }

  if (commit_type == blink::WebHistoryCommitType::kWebBackForwardCommit)
    render_view_->DidCommitProvisionalHistoryLoad();

  return is_new_navigation;
}

void RenderFrameImpl::NotifyObserversOfNavigationCommit(
    bool is_new_navigation,
    bool is_same_document,
    ui::PageTransition transition) {
  for (auto& observer : render_view_->observers_)
    observer.DidCommitProvisionalLoad(frame_, is_new_navigation);
  for (auto& observer : observers_)
    observer.DidCommitProvisionalLoad(is_same_document, transition);
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

  bool is_new_navigation = UpdateNavigationHistory(item, commit_type);
  NotifyObserversOfNavigationCommit(
      is_new_navigation, navigation_state->WasWithinSameDocument(), transition);

  if (internal_data->must_reset_scroll_and_scale_state()) {
    render_view_->webview()->ResetScrollAndScaleState();
    internal_data->set_must_reset_scroll_and_scale_state(false);
  }
  UpdateZoomLevel();

  if (!frame_->Parent()) {  // Only for top frames.
    RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
    if (render_thread_impl) {  // Can be NULL in tests.
      render_thread_impl->histogram_customizer()->RenderViewNavigatedToHost(
          GURL(GetLoadingUrl()).host(), RenderView::GetRenderViewCount());
    }
  }

  // Remember that we've already processed this request, so we don't update
  // the session history again.  We do this regardless of whether this is
  // a session history navigation, because if we attempted a session history
  // navigation without valid HistoryItem state, WebCore will think it is a
  // new navigation.
  navigation_state->set_request_committed(true);

  // If we are a top frame navigation to another document we should clear any
  // existing autoplay flags on the Page. This is because flags are stored at
  // the page level so subframes would only add to them.
  if (!frame_->Parent() && !navigation_state->WasWithinSameDocument()) {
    render_view_->webview()->ClearAutoplayFlags();
  }

  // Set the correct autoplay flags on the Page and wipe the cached origin so
  // this will not be used incorrectly.
  if (url::Origin(frame_->GetSecurityOrigin()) == autoplay_flags_.first) {
    render_view_->webview()->AddAutoplayFlags(autoplay_flags_.second);
    autoplay_flags_.first = url::Origin();
  }
}

void RenderFrameImpl::DidCommitNavigationInternal(
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type,
    bool was_within_same_document,
    ui::PageTransition transition,
    service_manager::mojom::InterfaceProviderRequest
        remote_interface_provider_request) {
  DCHECK(!(was_within_same_document &&
           remote_interface_provider_request.is_pending()));
  UpdateStateForCommit(item, commit_type, transition);

  // This invocation must precede any calls to allowScripts(), allowImages(), or
  // allowPlugins() for the new page. This ensures that when these functions
  // send ViewHostMsg_ContentBlocked messages, those arrive after the browser
  // process has already been informed of the provisional load committing.
  if (was_within_same_document) {
    GetFrameHost()->DidCommitSameDocumentNavigation(
        MakeDidCommitProvisionalLoadParams(commit_type, transition));
  } else {
    GetFrameHost()->DidCommitProvisionalLoad(
        MakeDidCommitProvisionalLoadParams(commit_type, transition),
        std::move(remote_interface_provider_request));
  }
}

void RenderFrameImpl::PrepareFrameForCommit(
    const GURL& url,
    const RequestNavigationParams& request_params) {
  browser_side_navigation_pending_ = false;
  browser_side_navigation_pending_url_ = GURL();

  GetContentClient()->SetActiveURL(
      url, frame_->Top()->GetSecurityOrigin().ToString().Utf8());

  RenderFrameImpl::PrepareRenderViewForNavigation(url, request_params);
}

blink::mojom::CommitResult RenderFrameImpl::PrepareForHistoryNavigationCommit(
    FrameMsg_Navigate_Type::Value navigation_type,
    const RequestNavigationParams& request_params,
    WebHistoryItem* item_for_history_navigation,
    blink::WebFrameLoadType* load_type) {
  DCHECK(navigation_type == FrameMsg_Navigate_Type::HISTORY_SAME_DOCUMENT ||
         navigation_type ==
             FrameMsg_Navigate_Type::HISTORY_DIFFERENT_DOCUMENT ||
         navigation_type == FrameMsg_Navigate_Type::RESTORE ||
         navigation_type == FrameMsg_Navigate_Type::RESTORE_WITH_POST);
  std::unique_ptr<HistoryEntry> entry =
      PageStateToHistoryEntry(request_params.page_state);
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
  history_subframe_unique_names_ = request_params.subframe_unique_names;

  if (navigation_type == FrameMsg_Navigate_Type::HISTORY_SAME_DOCUMENT) {
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

  // If this navigation is to a history item for a new child frame, we may
  // want to ignore it in some cases.  If a Javascript navigation (i.e.,
  // client redirect) interrupted it and has either been scheduled,
  // started loading, or has committed, we should ignore the history item.
  bool interrupted_by_client_redirect =
      frame_->IsNavigationScheduledWithin(0) ||
      frame_->GetProvisionalDocumentLoader() || !current_history_item_.IsNull();
  if (request_params.is_history_navigation_in_new_child &&
      interrupted_by_client_redirect) {
    return blink::mojom::CommitResult::Aborted;
  }

  return blink::mojom::CommitResult::Ok;
}

bool RenderFrameImpl::SwapIn() {
  CHECK_NE(proxy_routing_id_, MSG_ROUTING_NONE);
  CHECK(!in_frame_tree_);

  // The proxy should always exist.  If it was detached while the provisional
  // LocalFrame was being navigated, the provisional frame would've been
  // cleaned up by RenderFrameProxy::frameDetached.  See
  // https://crbug.com/526304 and https://crbug.com/568676 for context.
  RenderFrameProxy* proxy = RenderFrameProxy::FromRoutingID(proxy_routing_id_);
  CHECK(proxy);

  unique_name_helper_.set_propagated_name(proxy->unique_name());

  // Note: Calling swap() will detach and delete |proxy|, so do not reference it
  // after this.
  if (!proxy->web_frame()->Swap(frame_))
    return false;

  proxy_routing_id_ = MSG_ROUTING_NONE;
  in_frame_tree_ = true;

  // If this is the main frame going from a remote frame to a local frame,
  // it needs to set RenderViewImpl's pointer for the main frame to itself,
  // ensure RenderWidget is no longer in swapped out mode.
  if (is_main_frame_) {
    CHECK(!render_view_->main_render_frame_);
    render_view_->main_render_frame_ = this;
    if (render_view_->is_swapped_out()) {
      render_view_->SetSwappedOut(false);
    }
    render_view_->UpdateWebViewWithDeviceScaleFactor();
  }

  return true;
}

void RenderFrameImpl::DidStartLoading() {
  // TODO(dgozman): consider removing this callback.
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::didStartLoading",
               "id", routing_id_);
}

void RenderFrameImpl::DidStopLoading() {
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::didStopLoading",
               "id", routing_id_);

  // Any subframes created after this point won't be considered part of the
  // current history navigation (if this was one), so we don't need to track
  // this state anymore.
  history_subframe_unique_names_.clear();

  SendUpdateFaviconURL();

  Send(new FrameHostMsg_DidStopLoading(routing_id_));
}

void RenderFrameImpl::DidChangeLoadProgress(double load_progress) {
  Send(new FrameHostMsg_DidChangeLoadProgress(routing_id_, load_progress));
}

void RenderFrameImpl::FocusedNodeChanged(const WebNode& node) {
  has_scrolled_focused_editable_node_into_rect_ = false;
  bool is_editable = false;
  gfx::Rect node_bounds;
  if (!node.IsNull() && node.IsElementNode()) {
    WebElement element = const_cast<WebNode&>(node).To<WebElement>();
    blink::WebRect rect = element.BoundsInViewport();
    GetRenderWidget()->ConvertViewportToWindow(&rect);
    is_editable = element.IsEditable();
    node_bounds = gfx::Rect(rect);
  }
  Send(new FrameHostMsg_FocusedNodeChanged(routing_id_, is_editable,
                                           node_bounds));
  // Ensures that further text input state can be sent even when previously
  // focused input and the newly focused input share the exact same state.
  GetRenderWidget()->ClearTextInputState();

  for (auto& observer : observers_)
    observer.FocusedNodeChanged(node);
}

void RenderFrameImpl::FocusedNodeChangedForAccessibility(const WebNode& node) {
  if (render_accessibility())
    render_accessibility()->AccessibilityFocusedNodeChanged(node);
}

void RenderFrameImpl::OnReportContentSecurityPolicyViolation(
    const content::CSPViolationParams& violation_params) {
  frame_->ReportContentSecurityPolicyViolation(
      BuildWebContentSecurityPolicyViolation(violation_params));
}

WebNavigationPolicy RenderFrameImpl::DecidePolicyForNavigation(
    const NavigationPolicyInfo& info) {
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
  const GURL& url = info.url_request.Url();

#ifdef OS_ANDROID
  bool render_view_was_created_by_renderer =
      render_view_->was_created_by_renderer_;
  // The handlenavigation API is deprecated and will be removed once
  // crbug.com/325351 is resolved.
  if (!IsURLHandledByNetworkStack(url) && !url.is_empty() &&
      GetContentClient()->renderer()->HandleNavigation(
          this, true /* is_content_initiated */,
          render_view_was_created_by_renderer, frame_, info.url_request,
          info.navigation_type, info.default_policy, false /* is_redirect */)) {
    return blink::kWebNavigationPolicyIgnore;
  }
#endif

  // If the browser is interested, then give it a chance to look at the request.
  if (IsTopLevelNavigation(frame_) &&
      render_view_->renderer_preferences_
          .browser_handles_all_top_level_requests) {
    OpenURL(info, /*is_history_navigation_in_new_child=*/false);
    return blink::kWebNavigationPolicyIgnore;  // Suppress the load here.
  }

  // Back/forward navigations in newly created subframes should be sent to the
  // browser if there is a matching FrameNavigationEntry, and if it isn't just
  // staying at about:blank.  If this frame isn't in the map of unique names
  // that have history items, or if it's staying at the initial about:blank URL,
  // fall back to loading the default url.  (We remove each name as we encounter
  // it, because it will only be used once as the frame is created.)
  if (info.is_history_navigation_in_new_child_frame && frame_->Parent()) {
    // Check whether the browser has a history item for this frame that isn't
    // just staying at the initial about:blank document.
    bool should_ask_browser = false;
    RenderFrameImpl* parent = RenderFrameImpl::FromWebFrame(frame_->Parent());
    auto iter = parent->history_subframe_unique_names_.find(
        unique_name_helper_.value());
    if (iter != parent->history_subframe_unique_names_.end()) {
      bool history_item_is_about_blank = iter->second;
      should_ask_browser =
          !history_item_is_about_blank || url != url::kAboutBlankURL;
      parent->history_subframe_unique_names_.erase(iter);
    }

    if (should_ask_browser) {
      // Don't do this if |info| also says it is a client redirect, in which
      // case JavaScript on the page is trying to interrupt the history
      // navigation.
      if (!info.is_client_redirect) {
        OpenURL(info, /*is_history_navigation_in_new_child=*/true);
        // TODO(japhet): This case wants to flag the frame as loading and do
        // nothing else. It'd be nice if it could go through the placeholder
        // DocumentLoader path, too.
        frame_->MarkAsLoading();
        return blink::kWebNavigationPolicyIgnore;
      } else {
        // Client redirects during an initial history load should attempt to
        // cancel the history navigation.  They will create a provisional
        // document loader, causing the history load to be ignored in
        // NavigateInternal, and this IPC will try to cancel any cross-process
        // history load.
        GetFrameHost()->CancelInitialHistoryLoad();
      }
    }
  }

  // Use the frame's original request's URL rather than the document's URL for
  // subsequent checks.  For a popup, the document's URL may become the opener
  // window's URL if the opener has called document.write().
  // See http://crbug.com/93517.
  GURL old_url(frame_->GetDocumentLoader()->GetRequest().Url());

  // Detect when we're crossing a permission-based boundary (e.g. into or out of
  // an extension or app origin, leaving a WebUI page, etc). We only care about
  // top-level navigations (not iframes). But we sometimes navigate to
  // about:blank to clear a tab, and we want to still allow that.
  if (!frame_->Parent() && !url.SchemeIs(url::kAboutScheme) &&
      !url.is_empty()) {
    // All navigations to or from WebUI URLs or within WebUI-enabled
    // RenderProcesses must be handled by the browser process so that the
    // correct bindings and data sources can be registered.
    // Similarly, navigations to view-source URLs or within ViewSource mode
    // must be handled by the browser process (except for reloads - those are
    // safe to leave within the renderer).
    // Lastly, access to file:// URLs from non-file:// URL pages must be
    // handled by the browser so that ordinary renderer processes don't get
    // blessed with file permissions.
    int cumulative_bindings = RenderProcess::current()->GetEnabledBindings();
    bool is_initial_navigation = render_view_->history_list_length_ == 0;
    bool should_fork =
        HasWebUIScheme(url) || HasWebUIScheme(old_url) ||
        (cumulative_bindings & kWebUIBindingsPolicyMask) ||
        url.SchemeIs(kViewSourceScheme) ||
        (frame_->IsViewSourceModeEnabled() &&
         info.navigation_type != blink::kWebNavigationTypeReload);
    if (!should_fork && url.SchemeIs(url::kFileScheme)) {
      // Fork non-file to file opens.  Note that this may fork unnecessarily if
      // another tab (hosting a file or not) targeted this one before its
      // initial navigation, but that shouldn't cause a problem.
      should_fork = !old_url.SchemeIs(url::kFileScheme);
    }

    if (!should_fork) {
      // Give the embedder a chance.
      should_fork = GetContentClient()->renderer()->ShouldFork(
          frame_, url, info.url_request.HttpMethod().Utf8(),
          is_initial_navigation, false /* is_redirect */);
    }

    if (should_fork) {
      OpenURL(info, /*is_history_navigation_in_new_child=*/false);
      return blink::kWebNavigationPolicyIgnore;  // Suppress the load here.
    }
  }

  bool should_dispatch_before_unload =
      info.default_policy == blink::kWebNavigationPolicyCurrentTab &&
      // No need to dispatch beforeunload if the frame has not committed a
      // navigation and contains an empty initial document.
      (has_accessed_initial_document_ || !current_history_item_.IsNull());

  if (should_dispatch_before_unload) {
    // Execute the BeforeUnload event. If asked not to proceed or the frame is
    // destroyed, ignore the navigation.
    // Keep a WeakPtr to this RenderFrameHost to detect if executing the
    // BeforeUnload event destriyed this frame.
    base::WeakPtr<RenderFrameImpl> weak_self = weak_factory_.GetWeakPtr();

    if (!frame_->DispatchBeforeUnloadEvent(info.navigation_type ==
                                           blink::kWebNavigationTypeReload) ||
        !weak_self) {
      return blink::kWebNavigationPolicyIgnore;
    }
  }

  // When an MHTML Archive is present, it should be used to serve iframe content
  // instead of doing a network request.
  bool use_archive =
      (info.archive_status == NavigationPolicyInfo::ArchiveStatus::Present) &&
      !url.SchemeIs(url::kDataScheme);

  if (info.default_policy == blink::kWebNavigationPolicyCurrentTab) {
    if (!info.form.IsNull()) {
      for (auto& observer : observers_)
        observer.WillSubmitForm(info.form);
    }
    // If the navigation is not synchronous, send it to the browser.  This
    // includes navigations with no request being sent to the network stack.
    if (!use_archive && IsURLHandledByNetworkStack(url)) {
      pending_navigation_info_.reset(new PendingNavigationInfo(info));
      return blink::kWebNavigationPolicyHandledByClient;
    } else {
      return blink::kWebNavigationPolicyCurrentTab;
    }
  }

  if (info.default_policy == blink::kWebNavigationPolicyDownload) {
    blink::mojom::BlobURLTokenPtrInfo blob_url_token =
        CloneBlobURLToken(info.blob_url_token.get());
    DownloadURL(info.url_request,
                blink::WebLocalFrameClient::CrossOriginRedirects::kFollow,
                blob_url_token.PassHandle());
  } else {
    OpenURL(info, /*is_history_navigation_in_new_child=*/false);
  }
  return blink::kWebNavigationPolicyIgnore;
}

void RenderFrameImpl::OnGetSavableResourceLinks() {
  std::vector<GURL> resources_list;
  std::vector<SavableSubframe> subframes;
  SavableResourcesResult result(&resources_list, &subframes);

  if (!GetSavableResourceLinksForFrame(frame_, &result)) {
    Send(new FrameHostMsg_SavableResourceLinksError(routing_id_));
    return;
  }

  Referrer referrer = Referrer(frame_->GetDocument().Url(),
                               frame_->GetDocument().GetReferrerPolicy());

  Send(new FrameHostMsg_SavableResourceLinksResponse(
      routing_id_, resources_list, referrer, subframes));
}

void RenderFrameImpl::OnGetSerializedHtmlWithLocalLinks(
    const std::map<GURL, base::FilePath>& url_to_local_path,
    const std::map<int, base::FilePath>& frame_routing_id_to_local_path) {
  // Convert input to the canonical way of passing a map into a Blink API.
  LinkRewritingDelegate delegate(url_to_local_path,
                                 frame_routing_id_to_local_path);

  // Serialize the frame (without recursing into subframes).
  WebFrameSerializer::Serialize(GetWebFrame(),
                                this,  // WebFrameSerializerClient.
                                &delegate);
}

void RenderFrameImpl::OnSerializeAsMHTML(
    const FrameMsg_SerializeAsMHTML_Params& params) {
  TRACE_EVENT0("page-serialization", "RenderFrameImpl::OnSerializeAsMHTML");
  base::TimeTicks start_time = base::TimeTicks::Now();
  // Unpack IPC payload.
  base::File file = IPC::PlatformFileForTransitToFile(params.destination_file);
  const WebString mhtml_boundary =
      WebString::FromUTF8(params.mhtml_boundary_marker);
  DCHECK(!mhtml_boundary.IsEmpty());

  // Holds WebThreadSafeData instances for some or all of header, contents and
  // footer.
  std::vector<WebThreadSafeData> mhtml_contents;
  std::set<std::string> serialized_resources_uri_digests;
  MHTMLPartsGenerationDelegate delegate(params,
                                        &serialized_resources_uri_digests);

  MhtmlSaveStatus save_status = MhtmlSaveStatus::SUCCESS;
  bool has_some_data = false;

  // Generate MHTML header if needed.
  if (IsMainFrame()) {
    TRACE_EVENT0("page-serialization",
                 "RenderFrameImpl::OnSerializeAsMHTML header");
    // The returned data can be empty if the main frame should be skipped. If
    // the main frame is skipped, then the whole archive is bad.
    mhtml_contents.emplace_back(WebFrameSerializer::GenerateMHTMLHeader(
        mhtml_boundary, GetWebFrame(), &delegate));
    if (mhtml_contents.back().IsEmpty())
      save_status = MhtmlSaveStatus::FRAME_SERIALIZATION_FORBIDDEN;
    else
      has_some_data = true;
  }

  // Generate MHTML parts.  Note that if this is not the main frame, then even
  // skipping the whole parts generation step is not an error - it simply
  // results in an omitted resource in the final file.
  if (save_status == MhtmlSaveStatus::SUCCESS) {
    TRACE_EVENT0("page-serialization",
                 "RenderFrameImpl::OnSerializeAsMHTML parts serialization");
    // The returned data can be empty if the frame should be skipped, but this
    // is OK.
    mhtml_contents.emplace_back(WebFrameSerializer::GenerateMHTMLParts(
        mhtml_boundary, GetWebFrame(), &delegate));
    has_some_data |= !mhtml_contents.back().IsEmpty();
  }

  // Note: the MHTML footer is written by the browser process, after the last
  // frame is serialized by a renderer process.

  // Note: we assume RenderFrameImpl::OnWriteMHTMLToDiskComplete and the rest of
  // this function will be fast enough to not need to be accounted for in this
  // metric.
  base::TimeDelta main_thread_use_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES(
      "PageSerialization.MhtmlGeneration.RendererMainThreadTime.SingleFrame",
      main_thread_use_time);

  if (save_status == MhtmlSaveStatus::SUCCESS && has_some_data) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::Bind(&WriteMHTMLToDisk, base::Passed(&mhtml_contents),
                   base::Passed(&file)),
        base::Bind(&RenderFrameImpl::OnWriteMHTMLToDiskComplete,
                   weak_factory_.GetWeakPtr(), params.job_id,
                   base::Passed(&serialized_resources_uri_digests),
                   main_thread_use_time));
  } else {
    file.Close();
    OnWriteMHTMLToDiskComplete(params.job_id, serialized_resources_uri_digests,
                               main_thread_use_time, save_status);
  }
}

void RenderFrameImpl::OnWriteMHTMLToDiskComplete(
    int job_id,
    std::set<std::string> serialized_resources_uri_digests,
    base::TimeDelta main_thread_use_time,
    MhtmlSaveStatus save_status) {
  TRACE_EVENT1("page-serialization",
               "RenderFrameImpl::OnWriteMHTMLToDiskComplete",
               "frame save status", GetMhtmlSaveStatusLabel(save_status));
  DCHECK(RenderThread::Get()) << "Must run in the main renderer thread";
  // Notify the browser process about completion.
  // Note: we assume this method is fast enough to not need to be accounted for
  // in PageSerialization.MhtmlGeneration.RendererMainThreadTime.SingleFrame.
  Send(new FrameHostMsg_SerializeAsMHTMLResponse(
      routing_id_, job_id, save_status, serialized_resources_uri_digests,
      main_thread_use_time));
}

#ifndef STATIC_ASSERT_ENUM
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)
#undef STATIC_ASSERT_ENUM
#endif

void RenderFrameImpl::OnEnableViewSourceMode() {
  DCHECK(frame_);
  DCHECK(!frame_->Parent());
  frame_->EnableViewSourceMode(true);
}

void RenderFrameImpl::OnSuppressFurtherDialogs() {
  suppress_further_dialogs_ = true;
}

void RenderFrameImpl::OnFileChooserResponse(
    const std::vector<blink::mojom::FileChooserFileInfoPtr>& files) {
  // This could happen if we navigated to a different page before the user
  // closed the chooser.
  if (!file_chooser_completion_)
    return;

  // Convert Chrome's SelectedFileInfo list to WebKit's.
  WebVector<blink::WebFileChooserCompletion::SelectedFileInfo> selected_files(
      files.size());
  size_t current_size = 0;
  for (size_t i = 0; i < files.size(); ++i) {
    blink::WebFileChooserCompletion::SelectedFileInfo selected_file;
    if (files[i]->is_file_system()) {
      auto& fs_info = *files[i]->get_file_system();
      selected_file.file_system_url = fs_info.url;
      selected_file.length = fs_info.length;
      selected_file.modification_time = fs_info.modification_time;
    } else {
      selected_file.file_path = files[i]->get_native_file()->file_path;

      // Exclude files whose paths can't be converted into WebStrings. Blink
      // won't be able to handle these, and the browser process would kill the
      // renderer when it claims to have chosen an empty file path.
      if (blink::FilePathToWebString(selected_file.file_path).IsEmpty())
        continue;

      selected_file.display_name =
          WebString::FromUTF16(files[i]->get_native_file()->display_name);
    }
    selected_files[current_size] = selected_file;
    current_size++;
  }

  // If not all files were included, truncate the WebVector.
  if (current_size < selected_files.size()) {
    WebVector<blink::WebFileChooserCompletion::SelectedFileInfo> truncated_list(
        selected_files.Data(), current_size);
    selected_files.Swap(truncated_list);
  }

  blink::WebFileChooserCompletion* completion = file_chooser_completion_;
  file_chooser_completion_ = nullptr;
  completion->DidChooseFile(selected_files);
}

void RenderFrameImpl::OnClearFocusedElement() {
  // TODO(ekaramad): Should we add a method to WebLocalFrame instead and avoid
  // calling this on the WebView?
  if (auto* webview = render_view_->GetWebView())
    webview->ClearFocusedElement();
}

void RenderFrameImpl::OnBlinkFeatureUsageReport(const std::set<int>& features) {
  frame_->BlinkFeatureUsageReport(features);
}

void RenderFrameImpl::OnMixedContentFound(
    const FrameMsg_MixedContentFound_Params& params) {
  blink::WebSourceLocation source_location;
  source_location.url = WebString::FromLatin1(params.source_location.url);
  source_location.line_number = params.source_location.line_number;
  source_location.column_number = params.source_location.column_number;
  auto request_context = static_cast<blink::mojom::RequestContextType>(
      params.request_context_type);
  frame_->MixedContentFound(params.main_resource_url, params.mixed_content_url,
                            request_context, params.was_allowed,
                            params.had_redirect, source_location);
}

void RenderFrameImpl::OnSetOverlayRoutingToken(
    const base::UnguessableToken& token) {
  overlay_routing_token_ = token;
  for (const auto& cb : pending_routing_token_callbacks_)
    cb.Run(overlay_routing_token_.value());
  pending_routing_token_callbacks_.clear();
}

void RenderFrameImpl::RequestOverlayRoutingToken(
    media::RoutingTokenCallback callback) {
  if (overlay_routing_token_.has_value()) {
    std::move(callback).Run(overlay_routing_token_.value());
    return;
  }

  // Send a request to the host for the token.  We'll notify |callback| when it
  // arrives later.
  Send(new FrameHostMsg_RequestOverlayRoutingToken(routing_id_));

  pending_routing_token_callbacks_.push_back(std::move(callback));
}

void RenderFrameImpl::OnNotifyUserActivation() {
  frame_->NotifyUserActivation();
}

void RenderFrameImpl::OnMediaPlayerActionAt(
    const gfx::PointF& location,
    const blink::WebMediaPlayerAction& action) {
  blink::WebFloatRect viewport_position(location.x(), location.y(), 0, 0);
  GetRenderWidget()->ConvertWindowToViewport(&viewport_position);
  frame_->PerformMediaPlayerAction(
      WebPoint(viewport_position.x, viewport_position.y), action);
}

void RenderFrameImpl::OnRenderFallbackContent() const {
  frame_->RenderFallbackContent();
}

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)
void RenderFrameImpl::OnSelectPopupMenuItem(int selected_index) {
  if (external_popup_menu_ == NULL)
    return;

  blink::WebScopedUserGesture gesture(frame_);
  external_popup_menu_->DidSelectItem(selected_index);
  external_popup_menu_.reset();
}
#else
void RenderFrameImpl::OnSelectPopupMenuItems(
    bool canceled,
    const std::vector<int>& selected_indices) {
  // It is possible to receive more than one of these calls if the user presses
  // a select faster than it takes for the show-select-popup IPC message to make
  // it to the browser UI thread. Ignore the extra-messages.
  // TODO(jcivelli): http:/b/5793321 Implement a better fix, as detailed in bug.
  if (!external_popup_menu_)
    return;

  blink::WebScopedUserGesture gesture(frame_);
  external_popup_menu_->DidSelectItems(canceled, selected_indices);
  external_popup_menu_.reset();
}
#endif
#endif

void RenderFrameImpl::OpenURL(const NavigationPolicyInfo& info,
                              bool is_history_navigation_in_new_child) {
  WebNavigationPolicy policy = info.default_policy;
  FrameHostMsg_OpenURL_Params params;
  params.url = info.url_request.Url();
  params.uses_post = IsHttpPost(info.url_request);
  params.resource_request_body =
      GetRequestBodyForWebURLRequest(info.url_request);
  params.extra_headers = GetWebURLRequestHeadersAsString(info.url_request);
  params.referrer =
      RenderViewImpl::GetReferrerFromRequest(frame_, info.url_request);
  params.disposition = RenderViewImpl::NavigationPolicyToDisposition(policy);
  params.triggering_event_info = info.triggering_event_info;
  params.blob_url_token =
      CloneBlobURLToken(info.blob_url_token.get()).PassHandle().release();
  params.should_replace_current_entry =
      info.replaces_current_history_item && render_view_->history_list_length_;
  params.user_gesture = info.has_user_gesture;
  if (GetContentClient()->renderer()->AllowPopup())
    params.user_gesture = true;

  // TODO(csharrison,dgozman): FrameLoader::StartNavigation already consumes for
  // all main frame navigations, except in the case where page A is navigating
  // page B (e.g. using anchor targets). This edge case can go away when
  // UserActivationV2 ships, which would make the conditional below redundant.
  if (is_main_frame_ || policy == blink::kWebNavigationPolicyNewBackgroundTab ||
      policy == blink::kWebNavigationPolicyNewForegroundTab ||
      policy == blink::kWebNavigationPolicyNewWindow ||
      policy == blink::kWebNavigationPolicyNewPopup) {
    WebUserGestureIndicator::ConsumeUserGesture(frame_);
  }

  if (is_history_navigation_in_new_child)
    params.is_history_navigation_in_new_child = true;

  Send(new FrameHostMsg_OpenURL(routing_id_, params));
}

WebURLRequest RenderFrameImpl::CreateURLRequestForCommit(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    const network::ResourceResponseHead& head) {
  // This will override the url requested by the WebURLLoader, as well as
  // provide it with the response to the request.
  std::unique_ptr<NavigationResponseOverrideParameters> response_override(
      new NavigationResponseOverrideParameters());
  response_override->url_loader_client_endpoints =
      std::move(url_loader_client_endpoints);
  response_override->response = head;
  response_override->redirects = request_params.redirects;
  response_override->redirect_responses = request_params.redirect_response;
  response_override->redirect_infos = request_params.redirect_infos;

  WebURLRequest request = CreateURLRequestForNavigation(
      common_params, request_params, std::move(response_override),
      frame_->IsViewSourceModeEnabled());
  request.SetFrameType(IsTopLevelNavigation(frame_)
                           ? network::mojom::RequestContextFrameType::kTopLevel
                           : network::mojom::RequestContextFrameType::kNested);
  request.SetRequestorID(render_view_->GetRoutingID());
  static_cast<RequestExtraData*>(request.GetExtraData())
      ->set_render_frame_id(routing_id_);

#if defined(OS_ANDROID)
  request.SetHasUserGesture(common_params.has_user_gesture);
#endif

  request.SetNavigationStartTime(common_params.navigation_start);

  return request;
}

ChildURLLoaderFactoryBundle* RenderFrameImpl::GetLoaderFactoryBundle() {
  if (!loader_factories_) {
    RenderFrameImpl* creator = RenderFrameImpl::FromWebFrame(
        frame_->Parent() ? frame_->Parent() : frame_->Opener());
    if (creator) {
      auto bundle_info =
          base::WrapUnique(static_cast<TrackedChildURLLoaderFactoryBundleInfo*>(
              creator->GetLoaderFactoryBundle()->Clone().release()));
      loader_factories_ =
          base::MakeRefCounted<TrackedChildURLLoaderFactoryBundle>(
              std::move(bundle_info));
    } else {
      SetupLoaderFactoryBundle(nullptr,
                               base::nullopt /* subresource_overrides */,
                               nullptr /* prefetch_loader_factory */);
    }
  }
  return loader_factories_.get();
}

void RenderFrameImpl::SetupLoaderFactoryBundle(
    std::unique_ptr<URLLoaderFactoryBundleInfo> info,
    base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    network::mojom::URLLoaderFactoryPtr prefetch_loader_factory) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();

  loader_factories_ = base::MakeRefCounted<HostChildURLLoaderFactoryBundle>(
      GetTaskRunner(blink::TaskType::kInternalLoading));

  // In some tests |render_thread| could be null.
  if (render_thread) {
    loader_factories_->Update(render_thread->blink_platform_impl()
                                  ->CreateDefaultURLLoaderFactoryBundle()
                                  ->PassInterface(),
                              base::nullopt);
  }

  if (info) {
    loader_factories_->Update(
        std::make_unique<ChildURLLoaderFactoryBundleInfo>(std::move(info)),
        std::move(subresource_overrides));
  }
  if (prefetch_loader_factory) {
    loader_factories_->SetPrefetchLoaderFactory(
        std::move(prefetch_loader_factory));
  }
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
  if (selection_text_offset_ != offset ||
      selection_range_ != range ||
      selection_text_ != text) {
    selection_text_ = text;
    selection_text_offset_ = offset;
    selection_range_ = range;
    SetSelectedText(text, offset, range);
  }
  GetRenderWidget()->UpdateSelectionBounds();
}

void RenderFrameImpl::SetCustomURLLoaderFactory(
    network::mojom::URLLoaderFactoryPtr factory) {
  GetLoaderFactoryBundle()->SetDefaultFactory(std::move(factory));
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
  if (!GetRenderWidget()->layer_tree_view()->HasPendingPageScaleAnimation() &&
      autofill_client) {
    autofill_client->DidCompleteFocusChangeInFrame();
  }
}

void RenderFrameImpl::DidChangeVisibleViewport() {
  has_scrolled_focused_editable_node_into_rect_ = false;
}

void RenderFrameImpl::InitializeUserMediaClient() {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (!render_thread)  // Will be NULL during unit tests.
    return;

  DCHECK(!web_user_media_client_);
  web_user_media_client_ = new UserMediaClientImpl(
      this, RenderThreadImpl::current()->GetPeerConnectionDependencyFactory(),
      std::make_unique<MediaStreamDeviceObserver>(this),
      GetTaskRunner(blink::TaskType::kInternalMedia));
}

void RenderFrameImpl::PrepareRenderViewForNavigation(
    const GURL& url,
    const RequestNavigationParams& request_params) {
  DCHECK(render_view_->webview());

  if (is_main_frame_) {
    for (auto& observer : render_view_->observers_)
      observer.Navigate(url);
  }

  render_view_->history_list_offset_ =
      request_params.current_history_list_offset;
  render_view_->history_list_length_ =
      request_params.current_history_list_length;
}

namespace {
std::unique_ptr<base::DictionaryValue> GetDevToolsInitiator(
    const WebString& initiator_str) {
  if (initiator_str.IsNull())
    return nullptr;
  std::unique_ptr<base::DictionaryValue> initiator =
      base::DictionaryValue::From(base::JSONReader::Read(initiator_str.Utf8()));
  if (!initiator)
    return nullptr;
  // TODO(kozy,caseq): the hack below is due to the fact that initiators include
  // the chain of async callstacks that results in a tree of Values so deep
  // that it triggers mojo structure nesting limit upon deserialization.
  // See https://crbug.com/809996 for more details.
  // We trim async stacks here, but it should be possible to capture locations
  // without async stacks (or with custom limit on their number) instead.
  base::Value* parent = initiator->FindPath({"stack", "parent"});
  if (parent && parent->is_dict())
    parent->RemoveKey("parent");
  return initiator;
}
}  // namespace

void RenderFrameImpl::BeginNavigation(
    const NavigationPolicyInfo& info,
    mojo::ScopedMessagePipeHandle navigation_initiator_handle) {
  browser_side_navigation_pending_ = true;
  browser_side_navigation_pending_url_ = info.url_request.Url();

  blink::WebURLRequest& request = info.url_request;

  // Set SiteForCookies.
  WebDocument frame_document = frame_->GetDocument();
  if (request.GetFrameType() ==
      network::mojom::RequestContextFrameType::kTopLevel)
    request.SetSiteForCookies(request.Url());
  else
    request.SetSiteForCookies(frame_document.SiteForCookies());

  // Note: At this stage, the goal is to apply all the modifications the
  // renderer wants to make to the request, and then send it to the browser, so
  // that the actual network request can be started. Ideally, all such
  // modifications should take place in willSendRequest, and in the
  // implementation of willSendRequest for the various InspectorAgents
  // (devtools).
  //
  // TODO(clamy): Apply devtools override.
  // TODO(clamy): Make sure that navigation requests are not modified somewhere
  // else in blink.
  WillSendRequest(request);

  // Update the transition type of the request for client side redirects.
  if (!info.url_request.GetExtraData())
    info.url_request.SetExtraData(std::make_unique<RequestExtraData>());
  if (info.is_client_redirect) {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(info.url_request.GetExtraData());
    extra_data->set_transition_type(ui::PageTransitionFromInt(
        extra_data->transition_type() | ui::PAGE_TRANSITION_CLIENT_REDIRECT));
  }

  // TODO(clamy): Same-document navigations should not be sent back to the
  // browser.
  // TODO(clamy): Data urls should not be sent back to the browser either.
  // These values are assumed on the browser side for navigations. These checks
  // ensure the renderer has the correct values.
  DCHECK_EQ(network::mojom::FetchRequestMode::kNavigate,
            info.url_request.GetFetchRequestMode());
  DCHECK_EQ(network::mojom::FetchCredentialsMode::kInclude,
            info.url_request.GetFetchCredentialsMode());
  DCHECK_EQ(network::mojom::FetchRedirectMode::kManual,
            info.url_request.GetFetchRedirectMode());
  DCHECK(frame_->Parent() ||
         info.url_request.GetFrameType() ==
             network::mojom::RequestContextFrameType::kTopLevel);
  DCHECK(!frame_->Parent() ||
         info.url_request.GetFrameType() ==
             network::mojom::RequestContextFrameType::kNested);

  DCHECK(!info.url_request.RequestorOrigin().IsNull());
  base::Optional<url::Origin> initiator_origin =
      base::Optional<url::Origin>(info.url_request.RequestorOrigin());

  bool is_form_submission =
      info.navigation_type == blink::kWebNavigationTypeFormSubmitted ||
      info.navigation_type == blink::kWebNavigationTypeFormResubmitted;

  GURL searchable_form_url;
  std::string searchable_form_encoding;
  if (!info.form.IsNull()) {
    WebSearchableFormData web_searchable_form_data(info.form);
    searchable_form_url = web_searchable_form_data.Url();
    searchable_form_encoding = web_searchable_form_data.Encoding().Utf8();
  }

  GURL client_side_redirect_url;
  if (info.is_client_redirect)
    client_side_redirect_url = frame_->GetDocument().Url();

  blink::mojom::BlobURLTokenPtr blob_url_token(
      CloneBlobURLToken(info.blob_url_token.get()));

  int load_flags = GetLoadFlagsForWebURLRequest(info.url_request);
  std::unique_ptr<base::DictionaryValue> initiator =
      GetDevToolsInitiator(info.devtools_initiator_info);
  mojom::BeginNavigationParamsPtr begin_navigation_params =
      mojom::BeginNavigationParams::New(
          GetWebURLRequestHeadersAsString(info.url_request), load_flags,
          info.url_request.GetSkipServiceWorker(),
          GetRequestContextTypeForWebURLRequest(info.url_request),
          GetMixedContentContextTypeForWebURLRequest(info.url_request),
          is_form_submission, searchable_form_url, searchable_form_encoding,
          initiator_origin, client_side_redirect_url,
          initiator ? base::make_optional<base::Value>(std::move(*initiator))
                    : base::nullopt);

  mojom::NavigationClientAssociatedPtrInfo navigation_client_info;
  if (IsPerNavigationMojoInterfaceEnabled()) {
    WebDocumentLoader* document_loader = frame_->GetProvisionalDocumentLoader();
    NavigationState* navigation_state =
        NavigationState::FromDocumentLoader(document_loader);
    BindNavigationClient(mojo::MakeRequest(&navigation_client_info));
    navigation_state->set_navigation_client(std::move(navigation_client_impl_));
  }

  blink::mojom::NavigationInitiatorPtr initiator_ptr(
      blink::mojom::NavigationInitiatorPtrInfo(
          std::move(navigation_initiator_handle), 0));

  GetFrameHost()->BeginNavigation(
      MakeCommonNavigationParams(info, load_flags, info.input_start),
      std::move(begin_navigation_params), std::move(blob_url_token),
      std::move(navigation_client_info), std::move(initiator_ptr));
}

void RenderFrameImpl::LoadDataURL(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    blink::WebFrameLoadType load_type,
    blink::WebHistoryItem item_for_history_navigation,
    bool is_client_redirect,
    std::unique_ptr<blink::WebDocumentLoader::ExtraData> navigation_data) {
  // A loadData request with a specified base URL.
  GURL data_url = common_params.url;
#if defined(OS_ANDROID)
  if (!request_params.data_url_as_string.empty()) {
#if DCHECK_IS_ON()
    {
      std::string mime_type, charset, data;
      DCHECK(net::DataURL::Parse(data_url, &mime_type, &charset, &data));
      DCHECK(data.empty());
    }
#endif
    data_url = GURL(request_params.data_url_as_string);
    if (!data_url.is_valid() || !data_url.SchemeIs(url::kDataScheme)) {
      data_url = common_params.url;
    }
  }
#endif
  std::string mime_type, charset, data;
  if (net::DataURL::Parse(data_url, &mime_type, &charset, &data)) {
    const GURL base_url = common_params.base_url_for_data_url.is_empty()
                              ? common_params.url
                              : common_params.base_url_for_data_url;

    frame_->CommitDataNavigation(
        WebURLRequest(base_url), WebData(data.c_str(), data.length()),
        WebString::FromUTF8(mime_type), WebString::FromUTF8(charset),
        // Needed so that history-url-only changes don't become reloads.
        common_params.history_url_for_data_url, load_type,
        item_for_history_navigation, is_client_redirect,
        BuildNavigationParams(
            common_params, request_params,
            BuildServiceWorkerNetworkProviderForNavigation(
                &request_params, nullptr /* controller_service_worker_info */)),
        std::move(navigation_data));
  } else {
    CHECK(false) << "Invalid URL passed: "
                 << common_params.url.possibly_invalid_spec();
  }
}

void RenderFrameImpl::SendUpdateState() {
  if (current_history_item_.IsNull())
    return;

  Send(new FrameHostMsg_UpdateState(
      routing_id_, SingleHistoryItemToPageState(current_history_item_)));
}

void RenderFrameImpl::SendFailedProvisionalLoad(
    const blink::WebURLRequest& request,
    const WebURLError& error,
    blink::WebLocalFrame* frame) {
  bool show_repost_interstitial =
      (error.reason() == net::ERR_CACHE_MISS &&
       base::EqualsASCII(request.HttpMethod().Utf16(), "POST"));

  FrameHostMsg_DidFailProvisionalLoadWithError_Params params;
  params.error_code = error.reason();
  GetContentClient()->renderer()->GetErrorDescription(
      request, error, &params.error_description);
  params.url = error.url(),
  params.showing_repost_interstitial = show_repost_interstitial;
  Send(new FrameHostMsg_DidFailProvisionalLoadWithError(routing_id_, params));
}

bool RenderFrameImpl::ShouldDisplayErrorPageForFailedLoad(
    int error_code,
    const GURL& unreachable_url) {
  // Don't display an error page if this is simply a cancelled load.  Aside
  // from being dumb, Blink doesn't expect it and it will cause a crash.
  if (error_code == net::ERR_ABORTED)
    return false;

  // Don't display "client blocked" error page if browser has asked us not to.
  if (error_code == net::ERR_BLOCKED_BY_CLIENT &&
      render_view_->renderer_preferences_.disable_client_blocked_error_page) {
    return false;
  }

  // Allow the embedder to suppress an error page.
  if (GetContentClient()->renderer()->ShouldSuppressErrorPage(
          this, unreachable_url)) {
    return false;
  }

  return true;
}

GURL RenderFrameImpl::GetLoadingUrl() const {
  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();

  GURL overriden_url;
  if (MaybeGetOverriddenURL(document_loader, &overriden_url))
    return overriden_url;

  const WebURLRequest& request = document_loader->GetRequest();
  return request.Url();
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
      blink::WebKeyboardEvent char_event(blink::WebInputEvent::kChar,
                                         blink::WebInputEvent::kNoModifiers,
                                         ui::EventTimeForNow());
      char_event.windows_key_code = text[i];
      char_event.native_key_code = text[i];

      const int32_t char_start = i;
      for (; i < iterator.array_pos(); ++i) {
        char_event.text[i - char_start] = text[i];
        char_event.unmodified_text[i - char_start] = text[i];
      }

      if (GetRenderWidget()->GetWebWidget())
        GetRenderWidget()->GetWebWidget()->HandleInputEvent(
            blink::WebCoalescedInputEvent(char_event));
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
  GetAssociatedInterfaceRegistry()->AddInterface(base::Bind(
      &RenderFrameImpl::BindAutoplayConfiguration, weak_factory_.GetWeakPtr()));

  GetAssociatedInterfaceRegistry()->AddInterface(base::Bind(
      &RenderFrameImpl::BindFrameBindingsControl, weak_factory_.GetWeakPtr()));

  GetAssociatedInterfaceRegistry()->AddInterface(
      base::Bind(&RenderFrameImpl::BindFrameNavigationControl,
                 weak_factory_.GetWeakPtr()));

  if (IsPerNavigationMojoInterfaceEnabled()) {
    GetAssociatedInterfaceRegistry()->AddInterface(base::BindRepeating(
        &RenderFrameImpl::BindNavigationClient, weak_factory_.GetWeakPtr()));
  }

  GetAssociatedInterfaceRegistry()->AddInterface(base::BindRepeating(
      &RenderFrameImpl::BindFullscreen, weak_factory_.GetWeakPtr()));

  registry_.AddInterface(base::Bind(&FrameInputHandlerImpl::CreateMojoService,
                                    weak_factory_.GetWeakPtr()));

  registry_.AddInterface(
      base::Bind(&InputTargetClientImpl::BindToRequest,
                 base::Unretained(&input_target_client_impl_)));

  registry_.AddInterface(
      base::Bind(&RenderFrameImpl::BindWidget, weak_factory_.GetWeakPtr()));

  if (!frame_->Parent()) {
    // Only main frame have ImageDownloader service.
    registry_.AddInterface(base::Bind(&ImageDownloaderImpl::CreateMojoService,
                                      base::Unretained(this)));

    // Host zoom is per-page, so only added on the main frame.
    GetAssociatedInterfaceRegistry()->AddInterface(base::Bind(
        &RenderFrameImpl::OnHostZoomClientRequest, weak_factory_.GetWeakPtr()));

    // Web manifests are only requested for main frames.
    registry_.AddInterface(
        base::Bind(&ManifestManager::BindToRequest,
                   base::Unretained(manifest_manager_.get())));
  }
}

void RenderFrameImpl::OnHostZoomClientRequest(
    mojom::HostZoomAssociatedRequest request) {
  DCHECK(!host_zoom_binding_.is_bound());
  host_zoom_binding_.Bind(std::move(request),
                          GetTaskRunner(blink::TaskType::kInternalIPC));
}

void RenderFrameImpl::CheckIfAudioSinkExistsAndIsAuthorized(
    const blink::WebString& sink_id,
    std::unique_ptr<blink::WebSetSinkIdCallbacks> callbacks) {
  std::move(media::ConvertToOutputDeviceStatusCB(std::move(callbacks)))
      .Run(AudioDeviceFactory::GetOutputDeviceInfo(
               GetRoutingID(), media::AudioSinkParameters(0, sink_id.Utf8()))
               .device_status());
}

blink::mojom::PageVisibilityState RenderFrameImpl::VisibilityState() const {
  const RenderFrameImpl* local_root = GetLocalRoot();
  blink::mojom::PageVisibilityState current_state =
      local_root->render_widget_->is_hidden()
          ? blink::mojom::PageVisibilityState::kHidden
          : blink::mojom::PageVisibilityState::kVisible;
  blink::mojom::PageVisibilityState override_state = current_state;
  if (GetContentClient()->renderer()->ShouldOverridePageVisibilityState(
          this, &override_state))
    return override_state;
  return current_state;
}

std::unique_ptr<blink::WebURLLoaderFactory>
RenderFrameImpl::CreateURLLoaderFactory() {
  if (!RenderThreadImpl::current()) {
    // Some tests (e.g. RenderViewTests) do not have RenderThreadImpl,
    // use the platform's default WebURLLoaderFactoryImpl for them.
    return WebURLLoaderFactoryImpl::CreateTestOnlyFactory();
  }
  return std::make_unique<FrameURLLoaderFactory>(weak_factory_.GetWeakPtr());
}

void RenderFrameImpl::DraggableRegionsChanged() {
  for (auto& observer : observers_)
    observer.DraggableRegionsChanged();
}

void RenderFrameImpl::ScrollRectToVisibleInParentFrame(
    const blink::WebRect& rect_to_scroll,
    const blink::WebScrollIntoViewParams& params) {
  DCHECK(IsLocalRoot());
  Send(new FrameHostMsg_ScrollRectToVisibleInParentFrame(
      routing_id_, rect_to_scroll, params));
}

void RenderFrameImpl::BubbleLogicalScrollInParentFrame(
    blink::WebScrollDirection direction,
    blink::WebScrollGranularity granularity) {
  DCHECK(IsLocalRoot());
  DCHECK(!IsMainFrame());
  Send(new FrameHostMsg_BubbleLogicalScrollInParentFrame(routing_id_, direction,
                                                         granularity));
}

blink::mojom::PageVisibilityState RenderFrameImpl::GetVisibilityState() const {
  return VisibilityState();
}

bool RenderFrameImpl::IsBrowserSideNavigationPending() {
  return browser_side_navigation_pending_;
}

scoped_refptr<base::SingleThreadTaskRunner> RenderFrameImpl::GetTaskRunner(
    blink::TaskType task_type) {
  return GetWebFrame()->GetTaskRunner(task_type);
}

int RenderFrameImpl::GetEnabledBindings() const {
  return enabled_bindings_;
}

void RenderFrameImpl::FrameDidCallFocus() {
  Send(new FrameHostMsg_FrameDidCallFocus(routing_id_));
}

void RenderFrameImpl::SetAccessibilityModeForTest(ui::AXMode new_mode) {
  OnSetAccessibilityMode(new_mode);
}

scoped_refptr<network::SharedURLLoaderFactory>
RenderFrameImpl::GetURLLoaderFactory() {
  return GetLoaderFactoryBundle();
}

#if BUILDFLAG(ENABLE_PLUGINS)
void RenderFrameImpl::PepperInstanceCreated(
    PepperPluginInstanceImpl* instance) {
  active_pepper_instances_.insert(instance);

  Send(new FrameHostMsg_PepperInstanceCreated(
      routing_id_, instance->pp_instance()));
}

void RenderFrameImpl::PepperInstanceDeleted(
    PepperPluginInstanceImpl* instance) {
  active_pepper_instances_.erase(instance);

  if (pepper_last_mouse_event_target_ == instance)
    pepper_last_mouse_event_target_ = nullptr;
  if (focused_pepper_plugin_ == instance)
    PepperFocusChanged(instance, false);

  RenderFrameImpl* const render_frame = instance->render_frame();
  if (render_frame) {
    render_frame->Send(
        new FrameHostMsg_PepperInstanceDeleted(
            render_frame->GetRoutingID(),
            instance->pp_instance()));
  }
}

void RenderFrameImpl::PepperFocusChanged(PepperPluginInstanceImpl* instance,
                                         bool focused) {
  if (focused)
    focused_pepper_plugin_ = instance;
  else if (focused_pepper_plugin_ == instance)
    focused_pepper_plugin_ = nullptr;

  GetRenderWidget()->UpdateTextInputState();
  GetRenderWidget()->UpdateSelectionBounds();
}

void RenderFrameImpl::PepperStartsPlayback(PepperPluginInstanceImpl* instance) {
  RenderFrameImpl* const render_frame = instance->render_frame();
  if (render_frame) {
    render_frame->Send(
        new FrameHostMsg_PepperStartsPlayback(
            render_frame->GetRoutingID(),
            instance->pp_instance()));
  }
}

void RenderFrameImpl::PepperStopsPlayback(PepperPluginInstanceImpl* instance) {
  RenderFrameImpl* const render_frame = instance->render_frame();
  if (render_frame) {
    render_frame->Send(
        new FrameHostMsg_PepperStopsPlayback(
            render_frame->GetRoutingID(),
            instance->pp_instance()));
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
  Send(new FrameHostMsg_ShowCreatedWindow(
      GetRoutingID(), render_widget_to_show->routing_id(),
      RenderViewImpl::NavigationPolicyToDisposition(policy), initial_rect,
      opened_by_user_gesture));
}

void RenderFrameImpl::RenderWidgetSetFocus(bool enable) {
#if BUILDFLAG(ENABLE_PLUGINS)
  // Notify all Pepper plugins.
  for (auto* plugin : active_pepper_instances_)
    plugin->SetContentAreaFocus(enable);
#endif
}

void RenderFrameImpl::RenderWidgetWillHandleMouseEvent() {
#if BUILDFLAG(ENABLE_PLUGINS)
  // This method is called for every mouse event that the RenderWidget receives.
  // And then the mouse event is forwarded to blink, which dispatches it to the
  // event target. Potentially a Pepper plugin will receive the event.
  // In order to tell whether a plugin gets the last mouse event and which it
  // is, we set |pepper_last_mouse_event_target_| to null here. If a plugin gets
  // the event, it will notify us via DidReceiveMouseEvent() and set itself as
  // |pepper_last_mouse_event_target_|.
  pepper_last_mouse_event_target_ = nullptr;
#endif
}

blink::mojom::ControllerServiceWorkerMode
RenderFrameImpl::IsControlledByServiceWorker() {
  blink::WebServiceWorkerNetworkProvider* web_provider =
      frame_->GetDocumentLoader()->GetServiceWorkerNetworkProvider();
  if (!web_provider)
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  ServiceWorkerNetworkProvider* provider =
      ServiceWorkerNetworkProvider::FromWebServiceWorkerNetworkProvider(
          web_provider);
  return provider->IsControlledByServiceWorker();
}

RenderFrameImpl::PendingNavigationInfo::PendingNavigationInfo(
    const NavigationPolicyInfo& info)
    : navigation_type(info.navigation_type),
      policy(info.default_policy),
      replaces_current_history_item(info.replaces_current_history_item),
      history_navigation_in_new_child_frame(
          info.is_history_navigation_in_new_child_frame),
      client_redirect(info.is_client_redirect),
      triggering_event_info(info.triggering_event_info),
      form(info.form),
      source_location(info.source_location),
      devtools_initiator_info(info.devtools_initiator_info),
      blob_url_token(CloneBlobURLToken(info.blob_url_token.get())),
      input_start(info.input_start) {}

RenderFrameImpl::PendingNavigationInfo::~PendingNavigationInfo() = default;

void RenderFrameImpl::BindWidget(mojom::WidgetRequest request) {
  GetRenderWidget()->SetWidgetBinding(std::move(request));
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
      render_frame_id);
}

bool RenderFrameImpl::ShouldThrottleDownload() {
  const auto now = base::TimeTicks::Now();
  if (num_burst_download_requests_ == 0) {
    burst_download_start_time_ = now;
  } else if (num_burst_download_requests_ >= kBurstDownloadLimit) {
    static constexpr auto kBurstDownloadLimitResetInterval =
        TimeDelta::FromSeconds(1);
    if (now - burst_download_start_time_ > kBurstDownloadLimitResetInterval) {
      num_burst_download_requests_ = 1;
      burst_download_start_time_ = now;
      return false;
    }
    return true;
  }

  num_burst_download_requests_++;
  return false;
}

std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
RenderFrameImpl::BuildServiceWorkerNetworkProviderForNavigation(
    const RequestNavigationParams* request_params,
    mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info) {
  scoped_refptr<network::SharedURLLoaderFactory> fallback_factory =
      network::SharedURLLoaderFactory::Create(
          GetLoaderFactoryBundle()->CloneWithoutDefaultFactory());
  return ServiceWorkerNetworkProvider::CreateForNavigation(
      routing_id_, request_params, frame_,
      std::move(controller_service_worker_info), std::move(fallback_factory));
}

}  // namespace content
