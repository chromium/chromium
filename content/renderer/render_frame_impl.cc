// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/debug/alias.h"
#include "base/debug/asan_invalid_access.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_util.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/content_switches_internal.h"
#include "content/common/debug_utils.h"
#include "content/common/features.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.mojom.h"
#include "content/common/main_frame_counter.h"
#include "content/common/navigation_client.mojom.h"
#include "content/common/navigation_gesture.h"
#include "content/common/navigation_params_utils.h"
#include "content/common/renderer_host.mojom.h"
#include "content/common/web_package/signed_exchange_utils.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/extra_mojo_js_features.mojom.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/public/renderer/window_features_converter.h"
#include "content/renderer/accessibility/aom_content_ax_tree.h"
#include "content/renderer/accessibility/ax_tree_snapshotter_impl.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/accessibility/render_accessibility_manager.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/background_resource_fetch_assets.h"
#include "content/renderer/content_security_policy_util.h"
#include "content/renderer/document_state.h"
#include "content/renderer/dom_automation_controller.h"
#include "content/renderer/effective_connection_type_helper.h"
#include "content/renderer/frame_owner_properties_converter.h"
#include "content/renderer/gpu_benchmarking_extension.h"
#include "content/renderer/media/media_permission_dispatcher.h"
#include "content/renderer/mhtml_handle_writer.h"
#include "content/renderer/mojo/blink_interface_registry_impl.h"
#include "content/renderer/navigation_client.h"
#include "content/renderer/navigation_state.h"
#include "content/renderer/pepper/pepper_audio_controller.h"
#include "content/renderer/policy_container_util.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
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
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
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
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/common/loader/record_load_histograms.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/navigation/navigation_params_mojom_traits.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/frame/view_transition_state.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-shared.h"
#include "third_party/blink/public/mojom/loader/fetch_later.mojom.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/page/widget.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/tracked_child_url_loader_factory_bundle.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/weak_wrapper_resource_load_info_notifier.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_dedicated_or_shared_worker_fetch_context.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"
#include "third_party/blink/public/web/modules/media/audio/audio_output_ipc_factory.h"
#include "third_party/blink/public/web/modules/media/web_media_player_util.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_frame_serializer.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_link_preview_triggerer.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_navigation_timings.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_performance_metrics_for_nested_contexts.h"
#include "third_party/blink/public/web/web_picture_in_picture_window_options.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_document.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_savable_resources_test_support.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_searchable_form_data.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_v8_features.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/events/base_event_utils.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-microtask-queue.h"

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/renderer/pepper/pepper_browser_connection.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_plugin_registry.h"
#include "content/renderer/pepper/pepper_webplugin_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include <cpu-features.h>

#include "content/renderer/java/gin_java_bridge_dispatcher.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#endif

using base::Time;
using blink::ContextMenuData;
using blink::WebContentDecryptionModule;
using blink::WebData;
using blink::WebDocument;
using blink::WebDocumentLoader;
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
const char kCommitRenderFrame[] = "Navigation.CommitRenderFrame";

// Time, in seconds, we delay before sending content state changes (such as form
// state and scroll position) to the browser. We delay sending changes to avoid
// spamming the browser.
// To avoid having tab/session restore require sending a message to get the
// current content state during tab closing we use a shorter timeout for the
// foreground renderer. This means there is a small window of time from which
// content state is modified and not sent to session restore, but this is
// better than having to wake up all renderers during shutdown.
constexpr base::TimeDelta kDelaySecondsForContentStateSyncHidden =
    base::Seconds(5);
constexpr base::TimeDelta kDelaySecondsForContentStateSync = base::Seconds(1);

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
typedef std::map<int, RenderFrameImpl*> RoutingIDFrameMap;
static base::LazyInstance<RoutingIDFrameMap>::DestructorAtExit
    g_routing_id_frame_map = LAZY_INSTANCE_INITIALIZER;
#endif

typedef std::map<blink::WebFrame*, RenderFrameImpl*> FrameMap;
base::LazyInstance<FrameMap>::DestructorAtExit g_frame_map =
    LAZY_INSTANCE_INITIALIZER;

// Please keep in sync with "RendererBlockedURLReason" in
// tools/metrics/histograms/metadata/navigation/enums.xml. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class RendererBlockedURLReason {
  kInvalidURL = 0,
  kTooLongURL = 1,
  kBadAboutURL = 2,

  kMaxValue = kBadAboutURL
};

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
                                     bool is_in_fenced_frame_tree,
                                     WebNavigationType navigation_type) {
  if (is_in_fenced_frame_tree) {
    // Navigations inside fenced frame trees do not add session history items
    // and must be marked with PAGE_TRANSITION_AUTO_SUBFRAME. This is set
    // regardless of the `is_main_frame` value since this is inside a fenced
    // frame tree and should behave the same as iframes.
    return ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  }
  if (replaces_current_item && !is_main_frame) {
    // Subframe navigations that don't add session history items must be
    // marked with AUTO_SUBFRAME. See also DidFailProvisionalLoad for how we
    // handle loading of error pages.
    return ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  }
  bool is_form_submit =
      navigation_type == blink::kWebNavigationTypeFormSubmitted ||
      navigation_type == blink::kWebNavigationTypeFormResubmittedBackForward ||
      navigation_type == blink::kWebNavigationTypeFormResubmittedReload;
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
                                     bool is_main_frame,
                                     bool is_in_fenced_frame_tree) {
  NavigationState* navigation_state =
      DocumentState::FromDocumentLoader(document_loader)->navigation_state();
  ui::PageTransition default_transition =
      navigation_state->IsForSynchronousCommit()
          ? ui::PAGE_TRANSITION_LINK
          : ui::PageTransitionFromInt(
                navigation_state->common_params().transition);
  if (!is_in_fenced_frame_tree && navigation_state->WasWithinSameDocument())
    return default_transition;
  return GetTransitionType(default_transition,
                           document_loader->ReplacesCurrentHistoryItem(),
                           is_main_frame, is_in_fenced_frame_tree,
                           document_loader->GetNavigationType());
}

// Ensure that the renderer does not send commit URLs to the browser process
// that are known to be unsupported. Ideally these would be caught earlier in
// Blink and not get this far. Histograms are reported (similar to those in
// RenderProcessHostImpl::FilterURL) to track the cases that should be handled
// earlier. See https://crbug.com/40066983.
bool IsValidCommitUrl(const GURL& url) {
  // Invalid URLs are not accepted by the browser process.
  if (!url.is_valid()) {
    base::UmaHistogramEnumeration("Navigation.Renderer.BlockedForFilterURL",
                                  RendererBlockedURLReason::kInvalidURL);
    return false;
  }

  // Do not send a URL longer than Mojo will serialize.
  if (url.possibly_invalid_spec().length() > url::kMaxURLChars) {
    base::UmaHistogramEnumeration("Navigation.Renderer.BlockedForFilterURL",
                                  RendererBlockedURLReason::kTooLongURL);
    return false;
  }

  // Any about: URLs must be either about:blank or about:srcdoc (optionally with
  // fragments).
  if (url.SchemeIs(url::kAboutScheme) &&
      (!url.IsAboutBlank() && !url.IsAboutSrcdoc())) {
    base::UmaHistogramEnumeration("Navigation.Renderer.BlockedForFilterURL",
                                  RendererBlockedURLReason::kBadAboutURL);
    return false;
  }

  return true;
}

// Gets URL that should override the default getter for this data source
// (if any), storing it in |output|. Returns true if there is an override URL.
bool MaybeGetOverriddenURL(WebDocumentLoader* document_loader, GURL* output) {
  DocumentState* document_state =
      DocumentState::FromDocumentLoader(document_loader);
  // `document_state` may be null if it was taken from the loader, e.g. when
  // committing the result of evaluating a javascript: URL,
  // `FrameLoader::CommitNavigation()` takes the `DocumentState`. Early
  // returning here means the answer may be inaccurate, but this can only
  // happen when the replaced `Document` is being detached and about to go
  // away.
  if (!document_state) {
    return false;
  }

  // If this document is loaded by a loadDataWithBaseURL request, then the URLs
  // saved in the DocumentLoader will be the user-supplied base URL (used as the
  // "document URL") and history URL (used as the "unreachable URL"/"URL for
  // history"). However, we want to return the data: URL (the URL originally
  // sent by the browser to commit the navigation) here.
  // TODO(crbug.com/40187600): Since the DocumentState stays as long as
  // the Document stays the same, this means the data: URL will be returned even
  // after same-document navigations. Investigate whether this is intended or
  // not.
  if (document_state->was_load_data_with_base_url_request()) {
    *output = document_state->data_url();
    return true;
  }

  // The "unreachable URL" is only set in two cases:
  // - An error page, where the "unreachable URL" is set to the URL that failed
  // to load. We want the URL bar to show that URL, and the session history
  // entry should also use that URL (instead of "chrome-error://chromewebdata"
  // which is used as the "document URL" for the DocumentLoader).
  // - A loadDataWithBaseURL, where the "unreachable URL" is set to the "history
  // URL". This case should never reach this point as it's handled above, where
  // we return the original data: URL instead.
  if (document_loader->HasUnreachableURL()) {
    *output = document_loader->UnreachableWebURL();
    return true;
  }

  return false;
}

// Returns false unless this is a top-level navigation.
bool IsTopLevelNavigation(WebFrame* frame) {
  return frame->Parent() == nullptr && !frame->View()->IsFencedFrameRoot();
}

void FillNavigationParamsRequest(
    const blink::mojom::CommonNavigationParams& common_params,
    const blink::mojom::CommitNavigationParams& commit_params,
    blink::WebNavigationParams* navigation_params) {
  // Use the original navigation url to start with. We'll replay the redirects
  // afterwards and will eventually arrive to the final url.
  navigation_params->url = !commit_params.original_url.is_empty()
                               ? commit_params.original_url
                               : common_params.url;
  navigation_params->http_method = WebString::FromASCII(
      !commit_params.original_method.empty() ? commit_params.original_method
                                             : common_params.method);

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
        blink::GetWebHTTPBodyForRequestBody(*common_params.post_data);
    if (!commit_params.post_content_type.empty()) {
      navigation_params->http_content_type =
          WebString::FromASCII(commit_params.post_content_type);
    }
  }

  // Set the request initiator origin, which is supplied by the browser
  // process. It is present in cases such as navigating a frame in a different
  // process, which is routed through `blink::RemoteFrame` and the origin is
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
      blink::WebURLResponse web_response = blink::WebURLResponse::Create(
          exchange->inner_url, *exchange->inner_response,
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

  navigation_params->had_transient_user_activation =
      common_params.has_user_gesture;

  WebVector<WebString> web_origin_trials;
  web_origin_trials.reserve(commit_params.force_enabled_origin_trials.size());
  for (const auto& trial : commit_params.force_enabled_origin_trials)
    web_origin_trials.emplace_back(WebString::FromASCII(trial));
  navigation_params->force_enabled_origin_trials = web_origin_trials;

  if (!commit_params.early_hints_preloaded_resources.empty()) {
    navigation_params->early_hints_preloaded_resources = WebVector<WebURL>();
    for (const auto& resource : commit_params.early_hints_preloaded_resources) {
      navigation_params->early_hints_preloaded_resources.emplace_back(resource);
    }
  }

  // Pass on the `initiator_base_url` sent via the common_params for srcdoc and
  // about:blank documents. This will be picked up in DocumentLoader.
  // Note: It's possible for initiator_base_url to be empty if this is an
  // error srcdoc page. See test
  // NavigationRequestBrowserTest.OriginForSrcdocErrorPageInSubframe.
  if (common_params.initiator_base_url) {
    CHECK(common_params.url.IsAboutSrcdoc() ||
          common_params.url.IsAboutBlank());
    navigation_params->fallback_base_url =
        common_params.initiator_base_url.value();
  } else {
    navigation_params->fallback_base_url = WebURL();
  }
}

blink::mojom::CommonNavigationParamsPtr MakeCommonNavigationParams(
    const WebSecurityOrigin& current_origin,
    std::unique_ptr<blink::WebNavigationInfo> info,
    int load_flags,
    bool has_download_sandbox_flag,
    bool from_ad,
    bool is_history_navigation_in_new_child_frame,
    network::mojom::RequestDestination request_destination) {
  // A valid RequestorOrigin is always expected to be present.
  DCHECK(!info->url_request.RequestorOrigin().IsNull());

  blink::mojom::ReferrerPtr referrer = blink::mojom::Referrer::New(
      blink::WebStringToGURL(info->url_request.ReferrerString()),
      info->url_request.GetReferrerPolicy());

  // No history-navigation is expected to happen.
  DCHECK(info->navigation_type != blink::kWebNavigationTypeBackForward);

  // Determine the navigation type. No same-document navigation is expected
  // because it is loaded immediately by the FrameLoader.
  blink::mojom::NavigationType navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  if (info->navigation_type == blink::kWebNavigationTypeReload) {
    if (load_flags & net::LOAD_BYPASS_CACHE)
      navigation_type = blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE;
    else
      navigation_type = blink::mojom::NavigationType::RELOAD;
  }

  auto source_location = network::mojom::SourceLocation::New(
      info->source_location.url.Latin1(), info->source_location.line_number,
      info->source_location.column_number);

  const blink::WebURLRequestExtraData* url_request_extra_data =
      static_cast<blink::WebURLRequestExtraData*>(
          info->url_request.GetURLRequestExtraData().get());
  DCHECK(url_request_extra_data);

  // Convert from WebVector<int> to std::vector<int>.
  std::vector<int> initiator_origin_trial_features(
      info->initiator_origin_trial_features.begin(),
      info->initiator_origin_trial_features.end());

  blink::NavigationDownloadPolicy download_policy;
  download_policy.ApplyDownloadFramePolicy(
      info->is_opener_navigation, info->url_request.HasUserGesture(),
      info->url_request.RequestorOrigin().CanAccess(current_origin),
      has_download_sandbox_flag, from_ad);

  std::optional<GURL> initiator_base_url;
  GURL requestor_base_url(info->requestor_base_url);
  // Make sure the url length doesn't exceed the limit enforced by Mojo when
  // it's sent to the browser process.
  if (requestor_base_url.is_valid() &&
      requestor_base_url.possibly_invalid_spec().length() <=
          url::kMaxURLChars) {
    initiator_base_url = requestor_base_url;
  }
  return blink::mojom::CommonNavigationParams::New(
      info->url_request.Url(), info->url_request.RequestorOrigin(),
      initiator_base_url, std::move(referrer),
      url_request_extra_data->transition_type(), navigation_type,
      download_policy,
      info->frame_load_type == WebFrameLoadType::kReplaceCurrentItem, GURL(),
      base::TimeTicks::Now(), info->url_request.HttpMethod().Latin1(),
      blink::GetRequestBodyForWebURLRequest(info->url_request),
      std::move(source_location), false /* started_from_context_menu */,
      info->url_request.HasUserGesture(),
      info->url_request.HasTextFragmentToken(),
      info->should_check_main_world_content_security_policy,
      initiator_origin_trial_features, info->href_translate.Latin1(),
      is_history_navigation_in_new_child_frame, info->input_start,
      request_destination);
}

WebFrameLoadType NavigationTypeToLoadType(
    blink::mojom::NavigationType navigation_type,
    bool should_replace_current_entry) {
  switch (navigation_type) {
    case blink::mojom::NavigationType::RELOAD:
      return WebFrameLoadType::kReload;

    case blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE:
      return WebFrameLoadType::kReloadBypassingCache;

    case blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT:
    case blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT:
      return WebFrameLoadType::kBackForward;

    case blink::mojom::NavigationType::RESTORE:
    case blink::mojom::NavigationType::RESTORE_WITH_POST:
      return WebFrameLoadType::kRestore;

    case blink::mojom::NavigationType::SAME_DOCUMENT:
    case blink::mojom::NavigationType::DIFFERENT_DOCUMENT:
      return should_replace_current_entry
                 ? WebFrameLoadType::kReplaceCurrentItem
                 : WebFrameLoadType::kStandard;

    default:
      NOTREACHED_IN_MIGRATION();
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
    handler_remote_->DidReceiveData(std::string(data.data(), data.size()));

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
      const base::flat_map<blink::FrameToken, base::FilePath>&
          frame_token_to_local_path)
      : url_to_local_path_(url_to_local_path),
        frame_token_to_local_path_(frame_token_to_local_path) {}

  bool RewriteFrameSource(WebFrame* frame, WebString* rewritten_link) override {
    const blink::FrameToken frame_token = frame->GetFrameToken();
    auto it = frame_token_to_local_path_->find(frame_token);
    if (it == frame_token_to_local_path_->end()) {
      return false;  // This can happen because of https://crbug.com/541354.
    }

    const base::FilePath& local_path = it->second;
    *rewritten_link = ConvertRelativePathToHtmlAttribute(local_path);
    return true;
  }

  bool RewriteLink(const WebURL& url, WebString* rewritten_link) override {
    auto it = url_to_local_path_->find(GURL(url));
    if (it == url_to_local_path_->end()) {
      return false;
    }

    const base::FilePath& local_path = it->second;
    *rewritten_link = ConvertRelativePathToHtmlAttribute(local_path);
    return true;
  }

 private:
  const raw_ref<const base::flat_map<GURL, base::FilePath>> url_to_local_path_;
  const raw_ref<const base::flat_map<blink::FrameToken, base::FilePath>>
      frame_token_to_local_path_;
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
      handle_ = std::make_unique<MHTMLFileHandleWriter>(
          std::move(main_thread_task_runner), std::move(callback),
          std::move(params.output_handle->get_file_handle()));
    } else {
      handle_ = std::make_unique<MHTMLProducerHandleWriter>(
          std::move(main_thread_task_runner), std::move(callback),
          std::move(params.output_handle->get_producer_handle()));
    }
  }

  MHTMLHandleWriterDelegate(const MHTMLHandleWriterDelegate&) = delete;
  MHTMLHandleWriterDelegate& operator=(const MHTMLHandleWriterDelegate&) =
      delete;

  void WriteContents(std::vector<WebThreadSafeData> mhtml_contents) {
    // MHTMLHandleWriter::WriteContents calls MHTMLHandleWriter::Finish
    // eventually.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&MHTMLHandleWriter::WriteContents, std::move(handle_),
                       std::move(mhtml_contents)));
  }

  // Within the context of the delegate, only for premature write finish.
  void Finish(mojom::MhtmlSaveStatus save_status) {
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::BindOnce(&MHTMLHandleWriter::Finish,
                                              std::move(handle_), save_status));
  }

 private:
  std::unique_ptr<MHTMLHandleWriter> handle_;
};

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
  document_state->set_navigation_state(
      NavigationState::CreateForSynchronousCommit());
  return document_state;
}

// Creates a fully functional DocumentState in the case where we have
// navigation parameters available in the RenderFrameImpl.
std::unique_ptr<DocumentState> BuildDocumentStateFromParams(
    const blink::mojom::CommonNavigationParams& common_params,
    const blink::mojom::CommitNavigationParams& commit_params,
    mojom::NavigationClient::CommitNavigationCallback commit_callback,
    std::unique_ptr<NavigationClient> navigation_client,
    int request_id,
    bool was_initiated_in_this_frame) {
  std::unique_ptr<DocumentState> document_state(new DocumentState());

  DCHECK(!common_params.navigation_start.is_null());
  DCHECK(!common_params.url.SchemeIs(url::kJavaScriptScheme));

  document_state->set_is_overriding_user_agent(
      commit_params.is_overriding_user_agent);
  document_state->set_request_id(request_id);

  // If this is a loadDataWithBaseURL request, save the commit URL so that we
  // can send a DidCommit message with the URL that was originally sent by the
  // browser in CommonNavigationParams (See MaybeGetOverriddenURL()).
  document_state->set_was_load_data_with_base_url_request(
      commit_params.is_load_data_with_base_url);
  if (commit_params.is_load_data_with_base_url)
    document_state->set_data_url(common_params.url);

  document_state->set_navigation_state(
      NavigationState::CreateForCrossDocumentCommit(
          common_params.Clone(), commit_params.Clone(),
          std::move(commit_callback), std::move(navigation_client),
          was_initiated_in_this_frame));
  return document_state;
}

std::optional<WebURL> ApplyFilePathAlias(const WebURL& target) {
  const base::CommandLine::StringType file_url_path_alias =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kFileUrlPathAlias);
  if (file_url_path_alias.empty()) {
    return std::nullopt;
  }

  const auto alias_mapping =
      base::SplitString(file_url_path_alias, FILE_PATH_LITERAL("="),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (alias_mapping.size() != 2) {
    LOG(ERROR) << "Invalid file path alias format.";
    return std::nullopt;
  }

#if BUILDFLAG(IS_WIN)
  std::wstring path = base::UTF16ToWide(target.GetString().Utf16());
  const std::wstring file_prefix =
      base::ASCIIToWide(url::kFileScheme) +
      base::ASCIIToWide(url::kStandardSchemeSeparator);
#else
  std::string path = target.GetString().Utf8();
  const std::string file_prefix =
      std::string(url::kFileScheme) + url::kStandardSchemeSeparator;
#endif
  if (!base::StartsWith(path, file_prefix + alias_mapping[0],
                        base::CompareCase::SENSITIVE)) {
    return std::nullopt;
  }

  base::ReplaceFirstSubstringAfterOffset(&path, 0, alias_mapping[0],
                                         alias_mapping[1]);
#if BUILDFLAG(IS_WIN)
  return blink::WebURL(GURL(base::WideToUTF8(path)));
#else
  return blink::WebURL(GURL(path));
#endif
}

// Packs all navigation timings sent by the browser to a blink understandable
// format, blink::WebNavigationTimings.
blink::WebNavigationTimings BuildNavigationTimings(
    base::TimeTicks navigation_start,
    const blink::mojom::NavigationTiming& browser_navigation_timings,
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
  renderer_navigation_timings.parent_resource_timing_access =
      browser_navigation_timings.parent_resource_timing_access;

  renderer_navigation_timings.system_entropy_at_navigation_start =
      browser_navigation_timings.system_entropy_at_navigation_start;

  return renderer_navigation_timings;
}

WebHistoryItem NavigationApiHistoryEntryPtrToWebHistoryItem(
    const blink::mojom::NavigationApiHistoryEntry& entry) {
  return WebHistoryItem(
      WebString::FromUTF16(entry.url), WebString::FromUTF16(entry.key),
      WebString::FromUTF16(entry.id), entry.item_sequence_number,
      entry.document_sequence_number, WebString::FromUTF16(entry.state));
}

// Fills navigation data sent by the browser to a blink understandable
// format, blink::WebNavigationParams.
void FillMiscNavigationParams(
    const blink::mojom::CommonNavigationParams& common_params,
    blink::mojom::CommitNavigationParams& commit_params,
    blink::WebNavigationParams* navigation_params) {
  navigation_params->navigation_timings = BuildNavigationTimings(
      common_params.navigation_start, *commit_params.navigation_timing,
      common_params.input_start);
  if (!commit_params.redirect_infos.empty()) {
    navigation_params->navigation_timings.critical_ch_restart =
        commit_params.redirect_infos.back().critical_ch_restart_time;
  }

  navigation_params->is_user_activated =
      commit_params.was_activated == blink::mojom::WasActivatedOption::kYes;

  navigation_params->has_text_fragment_token =
      common_params.text_fragment_token;

  navigation_params->is_browser_initiated = commit_params.is_browser_initiated;

  navigation_params->is_cross_site_cross_browsing_context_group =
      commit_params.is_cross_site_cross_browsing_context_group;

  navigation_params->should_have_sticky_user_activation =
      commit_params.should_have_sticky_user_activation;

#if BUILDFLAG(IS_ANDROID)
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
  navigation_params->storage_key = std::move(commit_params.storage_key);

  navigation_params->frame_policy = commit_params.frame_policy;

  if (common_params.navigation_type == blink::mojom::NavigationType::RESTORE) {
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

  navigation_params->origin_agent_cluster = commit_params.origin_agent_cluster;
  navigation_params->origin_agent_cluster_left_as_default =
      commit_params.origin_agent_cluster_left_as_default;

  navigation_params->reduced_accept_language =
      WebString::FromASCII(commit_params.reduced_accept_language);
  navigation_params->enabled_client_hints.reserve(
      commit_params.enabled_client_hints.size());
  for (auto enabled_hint : commit_params.enabled_client_hints)
    navigation_params->enabled_client_hints.emplace_back(enabled_hint);

  if (commit_params.http_response_code != -1)
    navigation_params->http_status_code = commit_params.http_response_code;

  // Copy the modified runtime features from `commit_params` to send to the
  // Blink renderer class WebLocalFrameImpl.
  navigation_params->modified_runtime_features =
      commit_params.modified_runtime_features;

  // Populate the arrays of non-current entries for the window.navigation API.
  auto& entry_arrays = commit_params.navigation_api_history_entry_arrays;
  navigation_params->navigation_api_back_entries.reserve(
      entry_arrays->back_entries.size());
  for (const auto& entry : entry_arrays->back_entries) {
    navigation_params->navigation_api_back_entries.emplace_back(
        NavigationApiHistoryEntryPtrToWebHistoryItem(*entry));
  }
  navigation_params->navigation_api_forward_entries.reserve(
      entry_arrays->forward_entries.size());
  for (const auto& entry : entry_arrays->forward_entries) {
    navigation_params->navigation_api_forward_entries.emplace_back(
        NavigationApiHistoryEntryPtrToWebHistoryItem(*entry));
  }
  if (entry_arrays->previous_entry) {
    navigation_params->navigation_api_previous_entry =
        NavigationApiHistoryEntryPtrToWebHistoryItem(
            *entry_arrays->previous_entry);
  }

  if (commit_params.fenced_frame_properties) {
    navigation_params->fenced_frame_properties =
        commit_params.fenced_frame_properties;

    if (commit_params.fenced_frame_properties->nested_urn_config_pairs() &&
        commit_params.fenced_frame_properties->nested_urn_config_pairs()
            ->potentially_opaque_value.has_value()) {
      const auto& nested_urn_config_pairs_value =
          commit_params.fenced_frame_properties->nested_urn_config_pairs()
              ->potentially_opaque_value.value();
      DCHECK_EQ(blink::MaxAdAuctionAdComponents(),
                nested_urn_config_pairs_value.size());
      navigation_params->ad_auction_components.emplace();
      for (const auto& nested_urn_config_pair : nested_urn_config_pairs_value) {
        const GURL& urn = nested_urn_config_pair.first;
        DCHECK(urn.SchemeIs(url::kUrnScheme));
        navigation_params->ad_auction_components->push_back(blink::WebURL(urn));
      }
    }
  }

  navigation_params->ancestor_or_self_has_cspee =
      commit_params.ancestor_or_self_has_cspee;

  navigation_params->browsing_context_group_info =
      commit_params.browsing_context_group_info;

  navigation_params->content_settings =
      std::move(commit_params.content_settings);

  if (commit_params.cookie_deprecation_label.has_value()) {
    navigation_params->cookie_deprecation_label =
        WebString::FromASCII(*commit_params.cookie_deprecation_label);
  }
}

std::string GetUniqueNameOfWebFrame(WebFrame* web_frame) {
  if (web_frame->IsWebLocalFrame())
    return RenderFrameImpl::FromWebFrame(web_frame)->unique_name();
  return web_frame->ToWebRemoteFrame()->UniqueName().Utf8();
}

perfetto::protos::pbzero::FrameDeleteIntention FrameDeleteIntentionToProto(
    mojom::FrameDeleteIntention intent) {
  using ProtoLevel = perfetto::protos::pbzero::FrameDeleteIntention;
  switch (intent) {
    case mojom::FrameDeleteIntention::kNotMainFrame:
      return ProtoLevel::FRAME_DELETE_INTENTION_NOT_MAIN_FRAME;
    case mojom::FrameDeleteIntention::kSpeculativeMainFrameForShutdown:
      return ProtoLevel::
          FRAME_DELETE_INTENTION_SPECULATIVE_MAIN_FRAME_FOR_SHUTDOWN;
    case mojom::FrameDeleteIntention::
        kSpeculativeMainFrameForNavigationCancelled:
      return ProtoLevel::
          FRAME_DELETE_INTENTION_SPECULATIVE_MAIN_FRAME_FOR_NAVIGATION_CANCELLED;
  }
  // All cases should've been handled by the switch case above.
  NOTREACHED_IN_MIGRATION();
  return ProtoLevel::FRAME_DELETE_INTENTION_NOT_MAIN_FRAME;
}

void CallClientDeferMediaLoad(base::WeakPtr<RenderFrameImpl> frame,
                              bool has_played_media_before,
                              base::OnceClosure closure) {
  if (!frame)
    return;
  GetContentClient()->renderer()->DeferMediaLoad(
      frame.get(), has_played_media_before, std::move(closure));
}

void LogCommitHistograms(base::TimeTicks commit_sent,
                         bool is_main_frame,
                         const GURL& new_page_url) {
  if (!base::TimeTicks::IsConsistentAcrossProcesses())
    return;

  const char* frame_type = is_main_frame ? "MainFrame" : "Subframe";
  auto now = base::TimeTicks::Now();
  base::UmaHistogramTimes(
      base::StrCat({"Navigation.RendererCommitDelay.", frame_type}),
      now - commit_sent);
  if (auto* task = base::TaskAnnotator::CurrentTaskForThread()) {
    base::UmaHistogramTimes(
        base::StrCat({"Navigation.RendererCommitQueueTime.", frame_type}),
        now - task->queue_time);
  }

  // Some tests don't set the render thread.
  if (!RenderThreadImpl::current())
    return;

  base::TimeTicks run_loop_start_time =
      RenderThreadImpl::current()->run_loop_start_time();
  // If the commit was sent before the run loop was started for this process,
  // the navigation was likely delayed while waiting for the process to start.
  if (commit_sent < run_loop_start_time) {
    base::UmaHistogramTimes(
        base::StrCat({"Navigation.RendererCommitProcessWaitTime.", frame_type}),
        run_loop_start_time - commit_sent);
  }

  // We want to record the following metric just one time per process.
  static bool is_first_commit = true;
  if (is_first_commit) {
    is_first_commit = false;
    if (run_loop_start_time <= now && new_page_url.is_valid() &&
        new_page_url.SchemeIsHTTPOrHTTPS()) {
      const char* const name =
          is_main_frame
              ? "Navigation.RendererRunLoopStartToFirstCommitNavigation2."
                "MainFrame"
              : "Navigation.RendererRunLoopStartToFirstCommitNavigation2."
                "Subframe";
      const auto trace_id = TRACE_ID_WITH_SCOPE(
          name, TRACE_ID_LOCAL(RenderThreadImpl::current()));
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
          "navigation", name, trace_id, run_loop_start_time, "url",
          new_page_url);
      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0("navigation", name,
                                                     trace_id, now);
      base::UmaHistogramLongTimes(name, now - run_loop_start_time);
    }
  }
}

content::mojom::WindowContainerType WindowFeaturesToContainerType(
    const blink::WebWindowFeatures& window_features) {
  if (window_features.background) {
    if (window_features.persistent)
      return content::mojom::WindowContainerType::PERSISTENT;
    else
      return content::mojom::WindowContainerType::BACKGROUND;
  } else {
    return content::mojom::WindowContainerType::NORMAL;
  }
}

WindowOpenDisposition NavigationPolicyToDisposition(
    WebNavigationPolicy policy) {
  switch (policy) {
    case blink::kWebNavigationPolicyDownload:
      return WindowOpenDisposition::SAVE_TO_DISK;
    case blink::kWebNavigationPolicyCurrentTab:
      return WindowOpenDisposition::CURRENT_TAB;
    case blink::kWebNavigationPolicyNewBackgroundTab:
      return WindowOpenDisposition::NEW_BACKGROUND_TAB;
    case blink::kWebNavigationPolicyNewForegroundTab:
      return WindowOpenDisposition::NEW_FOREGROUND_TAB;
    case blink::kWebNavigationPolicyNewWindow:
      return WindowOpenDisposition::NEW_WINDOW;
    case blink::kWebNavigationPolicyNewPopup:
      return WindowOpenDisposition::NEW_POPUP;
    case blink::kWebNavigationPolicyPictureInPicture:
      return WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;
  }
  NOTREACHED_IN_MIGRATION() << "Unexpected WebNavigationPolicy";
  return WindowOpenDisposition::IGNORE_ACTION;
}

bool ShouldNotifySubresourceResponseStarted(blink::RendererPreferences pref) {
  if (!base::FeatureList::IsEnabled(
          features::kReduceSubresourceResponseStartedIPC)) {
    return true;
  }
  return pref.send_subresource_notification;
}

// Initialize the WebFrameWidget with compositing. Only local root frames
// create a widget.
// `previous_widget` indicates whether the compositor for the frame which
// is being replaced by this frame should be used instead of creating a new
// compositor instance.
void InitializeFrameWidgetForFrame(
    WebLocalFrame& frame,
    blink::WebFrameWidget* previous_widget,
    mojom::CreateFrameWidgetParamsPtr widget_params,
    const bool is_for_nested_main_frame) {
  CHECK(widget_params);

  const bool is_main_frame = !frame.Parent();
  CHECK(is_main_frame || !is_for_nested_main_frame);

  const bool is_for_scalable_page =
      is_main_frame && !frame.View()->IsFencedFrameRoot();

  const auto frame_sink_id =
      previous_widget ? previous_widget->GetFrameSinkId()
                      : viz::FrameSinkId(RenderThread::Get()->GetClientId(),
                                         widget_params->routing_id);
  auto* web_frame_widget = frame.InitializeFrameWidget(
      std::move(widget_params->frame_widget_host),
      std::move(widget_params->frame_widget),
      std::move(widget_params->widget_host), std::move(widget_params->widget),
      frame_sink_id, is_for_nested_main_frame, is_for_scalable_page,
      /*hidden=*/true);

  if (previous_widget) {
    web_frame_widget->InitializeCompositingFromPreviousWidget(
        widget_params->visual_properties.screen_infos,
        /*settings=*/nullptr, *previous_widget);
  } else {
    web_frame_widget->InitializeCompositing(
        widget_params->visual_properties.screen_infos,
        /*settings=*/nullptr);
  }

  // The WebFrameWidget should start with valid VisualProperties, including a
  // non-zero size. While WebFrameWidget would not normally receive IPCs and
  // thus would not get VisualProperty updates while the frame is provisional,
  // we need at least one update to them in order to meet expectations in the
  // renderer, and that update comes as part of the CreateFrame message.
  // TODO(crbug.com/40387047): This could become part of WebFrameWidget Init.
  web_frame_widget->ApplyVisualProperties(widget_params->visual_properties);
}

}  // namespace

// Implementation of WebFrameSerializer::MHTMLPartsGenerationDelegate that
// 1. Bases shouldSkipResource and getContentID responses on contents of
//    SerializeAsMHTMLParams.
// 2. Stores digests of urls of serialized resources (i.e. urls reported via
//    shouldSkipResource) into |serialized_resources_uri_digests| passed
//    to the constructor.
class MHTMLPartsGenerationDelegateImpl final
    : public WebFrameSerializer::MHTMLPartsGenerationDelegate {
 public:
  explicit MHTMLPartsGenerationDelegateImpl(
      mojom::SerializeAsMHTMLParamsPtr params)
      : params_(std::move(params)) {
    // Digests must be sorted for binary search.
    DCHECK(std::is_sorted(params_->digests_of_uris_to_skip.begin(),
                          params_->digests_of_uris_to_skip.end()));
    // URLs are not duplicated.
    DCHECK(base::ranges::adjacent_find(params_->digests_of_uris_to_skip) ==
           params_->digests_of_uris_to_skip.end());
  }

  MHTMLPartsGenerationDelegateImpl(const MHTMLPartsGenerationDelegateImpl&) =
      delete;
  MHTMLPartsGenerationDelegateImpl& operator=(
      const MHTMLPartsGenerationDelegateImpl&) = delete;

  bool ShouldSkipResource(const WebURL& url) override {
    std::string digest =
        crypto::SHA256HashString(params_->salt + GURL(url).spec());

    // Skip if the |url| already covered by serialization of an *earlier* frame.
    if (std::binary_search(params_->digests_of_uris_to_skip.begin(),
                           params_->digests_of_uris_to_skip.end(), digest)) {
      return true;
    }

    // Let's record |url| as being serialized for the *current* frame.
    auto pair = serialized_resources_uri_digests_.insert(digest);
    bool insertion_took_place = pair.second;
    DCHECK(insertion_took_place);  // Blink should dedupe within a frame.

    return false;
  }

  bool UseBinaryEncoding() override { return params_->mhtml_binary_encoding; }

  bool RemovePopupOverlay() override {
    return params_->mhtml_popup_overlay_removal;
  }

  std::unordered_set<std::string> TakeSerializedResourcesUriDigests() {
    return std::move(serialized_resources_uri_digests_);
  }
  mojom::SerializeAsMHTMLParamsPtr TakeParams() { return std::move(params_); }

 private:
  std::vector<std::string> digests_of_uris_to_skip_;
  mojom::SerializeAsMHTMLParamsPtr params_;
  std::unordered_set<std::string> serialized_resources_uri_digests_;
};

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
    body_loader_->StartLoadingBody(this);
  }

  MHTMLBodyLoaderClient(const MHTMLBodyLoaderClient&) = delete;
  MHTMLBodyLoaderClient& operator=(const MHTMLBodyLoaderClient&) = delete;

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
  void BodyDataReceived(base::span<const char> data) override {
    data_.Append(data.data(), data.size());
  }

  void BodyLoadingFinished(base::TimeTicks completion_time,
                           int64_t total_encoded_data_length,
                           int64_t total_encoded_body_length,
                           int64_t total_decoded_body_length,
                           const std::optional<WebURLError>& error) override {
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
  raw_ptr<RenderFrameImpl> frame_;
  bool committing_ = false;
  WebData data_;
  std::unique_ptr<blink::WebNavigationParams> navigation_params_;
  std::unique_ptr<blink::WebNavigationBodyLoader> body_loader_;
  base::OnceCallback<void(std::unique_ptr<blink::WebNavigationParams>)>
      done_callback_;
};

RenderFrameImpl::UniqueNameFrameAdapter::UniqueNameFrameAdapter(
    RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {}

RenderFrameImpl::UniqueNameFrameAdapter::~UniqueNameFrameAdapter() {}

bool RenderFrameImpl::UniqueNameFrameAdapter::IsMainFrame() const {
  return render_frame_->IsMainFrame();
}

bool RenderFrameImpl::UniqueNameFrameAdapter::IsCandidateUnique(
    std::string_view name) const {
  // This method is currently O(N), where N = number of frames in the tree.
  DCHECK(!name.empty());

  for (blink::WebFrame* frame = GetWebFrame()->Top(); frame;
       frame = frame->TraverseNext()) {
    if (GetUniqueNameOfWebFrame(frame) == name)
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
    bool (*should_stop)(std::string_view)) const {
  std::vector<std::string> result;
  for (blink::WebFrame* frame = begin_point == BeginPoint::kParentFrame
                                    ? GetWebFrame()->Parent()
                                    : GetWebFrame();
       frame; frame = frame->Parent()) {
    result.push_back(GetUniqueNameOfWebFrame(frame));
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
    const blink::LocalFrameToken& frame_token,
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<mojom::Frame> frame_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
        associated_interface_provider,
    const base::UnguessableToken& devtools_frame_token,
    bool is_for_nested_main_frame) {
  DCHECK(routing_id != MSG_ROUTING_NONE);
  CreateParams params(agent_scheduling_group, frame_token, routing_id,
                      std::move(frame_receiver),
                      std::move(associated_interface_provider),
                      devtools_frame_token, is_for_nested_main_frame);

  if (g_create_render_frame_impl)
    return g_create_render_frame_impl(std::move(params));
  else
    return new RenderFrameImpl(std::move(params));
}

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
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
#endif

// static
RenderFrameImpl* RenderFrameImpl::CreateMainFrame(
    AgentSchedulingGroup& agent_scheduling_group,
    blink::WebView* web_view,
    blink::WebFrame* opener,
    bool is_for_nested_main_frame,
    bool is_for_scalable_page,
    blink::mojom::FrameReplicationStatePtr replication_state,
    const base::UnguessableToken& devtools_frame_token,
    mojom::CreateLocalMainFrameParamsPtr params,
    const blink::WebURL& base_url) {
  // A main frame RenderFrame must have a RenderWidget.
  DCHECK_NE(MSG_ROUTING_NONE, params->widget_params->routing_id);

  RenderFrameImpl* render_frame = RenderFrameImpl::Create(
      agent_scheduling_group, params->frame_token, params->routing_id,
      std::move(params->frame),
      std::move(params->associated_interface_provider_remote),
      devtools_frame_token, is_for_nested_main_frame);

  WebLocalFrame* web_frame = WebLocalFrame::CreateMainFrame(
      web_view, render_frame, render_frame->blink_interface_registry_.get(),
      std::move(params->interface_broker), params->frame_token,
      params->document_token,
      ToWebPolicyContainer(std::move(params->policy_container)), opener,
      // This conversion is a little sad, as this often comes from a
      // WebString...
      WebString::FromUTF8(replication_state->name),
      replication_state->frame_policy.sandbox_flags, base_url);
  if (!params->is_on_initial_empty_document)
    render_frame->frame_->SetIsNotOnInitialEmptyDocument();

  CHECK(!params->widget_params->previous_frame_token_for_compositor_reuse);

  // Non-owning pointer that is self-referencing and destroyed by calling
  // Close(). The RenderViewImpl has a RenderWidget already, but not a
  // WebFrameWidget, which is now attached here.
  blink::WebFrameWidget* web_frame_widget = web_frame->InitializeFrameWidget(
      std::move(params->widget_params->frame_widget_host),
      std::move(params->widget_params->frame_widget),
      std::move(params->widget_params->widget_host),
      std::move(params->widget_params->widget),
      viz::FrameSinkId(RenderThread::Get()->GetClientId(),
                       params->widget_params->routing_id),
      is_for_nested_main_frame, is_for_scalable_page,
      /*hidden=*/true);
  web_frame_widget->InitializeCompositing(
      params->widget_params->visual_properties.screen_infos,
      /*settings=*/nullptr);

  // The WebFrame created here was already attached to the Page as its main
  // frame, and the WebFrameWidget has been initialized, so we can call
  // WebView's DidAttachLocalMainFrame().
  render_frame->GetWebView()->DidAttachLocalMainFrame();

  // The WebFrameWidget should start with valid VisualProperties, including a
  // non-zero size. While WebFrameWidget would not normally receive IPCs and
  // thus would not get VisualProperty updates while the frame is provisional,
  // we need at least one update to them in order to meet expectations in the
  // renderer, and that update comes as part of the CreateFrame message.
  // TODO(crbug.com/40387047): This could become part of WebFrameWidget Init.
  web_frame_widget->ApplyVisualProperties(
      params->widget_params->visual_properties);

  render_frame->in_frame_tree_ = true;
  render_frame->Initialize(nullptr);

  if (params->subresource_loader_factories
          ->IsTrackedChildPendingURLLoaderFactoryBundle()) {
    // In renderer-initiated creation of a new main frame (e.g. popup without
    // rel=noopener), `params->subresource_loader_factories` are inherited from
    // the creator frame (e.g. TrackedChildURLLoaderFactoryBundle will be
    // updated when the creator's bundle recovers from a NetworkService crash).
    // See also https://crbug.com/1194763#c5.
    render_frame->SetLoaderFactoryBundle(
        base::MakeRefCounted<blink::TrackedChildURLLoaderFactoryBundle>(
            base::WrapUnique(
                static_cast<blink::TrackedChildPendingURLLoaderFactoryBundle*>(
                    params->subresource_loader_factories.release()))));
  } else {
    // In browser-initiated creation of a new main frame (e.g. popup with
    // rel=noopener, or when creating a new tab) the Browser process provides
    // `params->subresource_loader_factories`.
    render_frame->SetLoaderFactoryBundle(
        render_frame->CreateLoaderFactoryBundle(
            std::move(params->subresource_loader_factories),
            /*subresource_overrides=*/std::nullopt,
            /*subresource_proxying_loader_factory=*/mojo::NullRemote(),
            /*keep_alive_loader_factory=*/mojo::NullRemote(),
            /*fetch_later_loader_factory=*/mojo::NullAssociatedRemote()));
  }

  return render_frame;
}

// static
void RenderFrameImpl::CreateFrame(
    AgentSchedulingGroup& agent_scheduling_group,
    const blink::LocalFrameToken& frame_token,
    int routing_id,
    mojo::PendingAssociatedReceiver<mojom::Frame> frame_receiver,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker,
    mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
        associated_interface_provider,
    blink::WebView* web_view,
    const std::optional<blink::FrameToken>& previous_frame_token,
    const std::optional<blink::FrameToken>& opener_frame_token,
    const std::optional<blink::FrameToken>& parent_frame_token,
    const std::optional<blink::FrameToken>& previous_sibling_frame_token,
    const base::UnguessableToken& devtools_frame_token,
    blink::mojom::TreeScopeType tree_scope_type,
    blink::mojom::FrameReplicationStatePtr replicated_state,
    mojom::CreateFrameWidgetParamsPtr widget_params,
    blink::mojom::FrameOwnerPropertiesPtr frame_owner_properties,
    bool is_on_initial_empty_document,
    const blink::DocumentToken& document_token,
    blink::mojom::PolicyContainerPtr policy_container,
    bool is_for_nested_main_frame) {
  // TODO(danakj): Split this method into two pieces. The first block makes a
  // WebLocalFrame and collects the `blink::WebView` and RenderFrame for it. The
  // second block uses that to make a RenderWidget, if needed.
  RenderFrameImpl* render_frame = nullptr;
  blink::WebLocalFrame* web_frame = nullptr;
  if (!previous_frame_token) {
    // TODO(alexmos): This path is currently used only:
    // 1) When recreating a non-main RenderFrame after a crash.
    // 2) In tests that issue this IPC directly.
    // These two cases should be cleaned up to also pass a previous_frame_token,
    // which would allow removing this branch altogether.  See
    // https://crbug.com/756790.

    CHECK(parent_frame_token);
    WebFrame* parent_web_frame =
        WebFrame::FromFrameToken(parent_frame_token.value());

    // If the browser is sending a valid parent routing id, it should already
    // be created and registered.
    CHECK(parent_web_frame);
    CHECK(parent_web_frame->IsWebRemoteFrame());

    blink::WebFrame* previous_sibling_web_frame = nullptr;
    if (previous_sibling_frame_token) {
      previous_sibling_web_frame =
          blink::WebFrame::FromFrameToken(previous_sibling_frame_token.value());
    }

    // `web_view` would only be set by the function caller when creating a
    // provisional local main frame for a new WebView, which is handled in the
    // other branch of this if clause. Meanwhile, this branch handles subframe
    // creation case only, and we should use the parent's WebView in that case.
    CHECK(!web_view);
    web_view = parent_web_frame->View();

    // Create the RenderFrame and WebLocalFrame, linking the two.
    render_frame = RenderFrameImpl::Create(
        agent_scheduling_group, frame_token, routing_id,
        std::move(frame_receiver), std::move(associated_interface_provider),
        devtools_frame_token, is_for_nested_main_frame);
    render_frame->unique_name_helper_.set_propagated_name(
        replicated_state->unique_name);
    WebFrame* opener = nullptr;
    if (opener_frame_token)
      opener = WebFrame::FromFrameToken(opener_frame_token.value());
    web_frame = parent_web_frame->ToWebRemoteFrame()->CreateLocalChild(
        tree_scope_type, WebString::FromUTF8(replicated_state->name),
        replicated_state->frame_policy, render_frame,
        render_frame->blink_interface_registry_.get(),
        previous_sibling_web_frame,
        frame_owner_properties->To<blink::WebFrameOwnerProperties>(),
        frame_token, opener, document_token,
        std::move(browser_interface_broker),
        ToWebPolicyContainer(std::move(policy_container)));

    // The RenderFrame is created and inserted into the frame tree in the above
    // call to createLocalChild.
    render_frame->in_frame_tree_ = true;
  } else {
    WebFrame* previous_web_frame =
        WebFrame::FromFrameToken(previous_frame_token.value());
    // The previous frame could've been detached while the navigation was being
    // initiated in the browser process. Drop the navigation and don't create
    // the frame in that case.
    // See https://crbug.com/526304.
    if (!previous_web_frame)
      return;

    // This path is creating a local frame. It may or may not be a local root,
    // depending on whether the frame's parent is local or remote. It may also
    // be the main frame, which will be a provisional frame that can either
    // replace a remote main frame in the same WebView or a local main frame in
    // a different WebView.
    if (web_view) {
      // When a `web_view` is set by the caller, it must be for a provisional
      // main frame that will do a local frame swap. In this case, the WebView
      // must be different from the previous frame's WebView.
      CHECK(!previous_web_frame->Parent());
      CHECK_NE(web_view, previous_web_frame->View());
    } else {
      // When not set explicitly, reuse the previous frame's WebView.
      web_view = previous_web_frame->View();
    }
    render_frame = RenderFrameImpl::Create(
        agent_scheduling_group, frame_token, routing_id,
        std::move(frame_receiver), std::move(associated_interface_provider),
        devtools_frame_token, is_for_nested_main_frame);
    web_frame = blink::WebLocalFrame::CreateProvisional(
        render_frame, render_frame->blink_interface_registry_.get(),
        std::move(browser_interface_broker), frame_token, previous_web_frame,
        replicated_state->frame_policy,
        WebString::FromUTF8(replicated_state->name), web_view);
    // The new |web_frame| is a main frame iff the previous frame was.
    DCHECK_EQ(!previous_web_frame->Parent(), !web_frame->Parent());
    // Clone the current unique name so web tests that log frame unique names
    // output something meaningful. At `SwapIn()` time, the unique name will be
    // updated to the latest value.
    render_frame->unique_name_helper_.set_propagated_name(
        GetUniqueNameOfWebFrame(previous_web_frame));
  }

  CHECK(web_view);
  CHECK(render_frame);
  CHECK(web_frame);

  bool is_main_frame = !web_frame->Parent();

  // Child frames require there to be a |parent_routing_id| present, for the
  // remote parent frame. Though it is only used if the |previous_frame_token|
  // is not given, which happens in some corner cases.
  if (!is_main_frame)
    DCHECK(parent_frame_token);

  // We now have a WebLocalFrame for the new frame. The next step is to make
  // a RenderWidget (aka WebWidgetClient) for it, if it is a local root.
  // TODO(crbug.com/40387047): Can we merge this `is_main_frame` block with
  // RenderFrameImpl::CreateMainFrame()?
  if (is_main_frame) {
    // Main frames are always local roots, so they should always have a
    // |widget_params| (and it always comes with a routing id).
    DCHECK(widget_params);
    DCHECK_NE(widget_params->routing_id, MSG_ROUTING_NONE);

    render_frame->MaybeInitializeWidget(std::move(widget_params));

    // Note that we do *not* call WebView's DidAttachLocalMainFrame() here yet
    // because this frame is provisional and not attached to the Page yet. We
    // will tell WebViewImpl about it once it is swapped in.
  } else if (widget_params) {
    DCHECK(widget_params->routing_id != MSG_ROUTING_NONE);

    // This frame is a child local root, so we require a separate RenderWidget
    // for it from any other frames in the frame tree. Each local root defines
    // a separate context/coordinate space/world for compositing, painting,
    // input, etc. And each local root has a RenderWidget which provides
    // such services independent from other RenderWidgets.
    //
    // Notably, we do not attempt to reuse the main frame's RenderWidget (if the
    // main frame in this frame tree is local) as that RenderWidget is
    // functioning in a different local root. Because this is a child local
    // root, it implies there is some remote frame ancestor between this frame
    // and the main frame, thus its coordinate space etc is not known relative
    // to the main frame.
    render_frame->MaybeInitializeWidget(std::move(widget_params));
  }

  if (!is_on_initial_empty_document) {
    render_frame->frame_->SetIsNotOnInitialEmptyDocument();
  }

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

blink::WebURL RenderFrameImpl::OverrideFlashEmbedWithHTML(
    const blink::WebURL& url) {
  return GetContentClient()->renderer()->OverrideFlashEmbedWithHTML(url);
}

// RenderFrameImpl::CreateParams --------------------------------------------

RenderFrameImpl::CreateParams::CreateParams(
    AgentSchedulingGroup& agent_scheduling_group,
    const blink::LocalFrameToken& frame_token,
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<mojom::Frame> frame_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
        associated_interface_provider,
    const base::UnguessableToken& devtools_frame_token,
    bool is_for_nested_main_frame)
    : agent_scheduling_group(&agent_scheduling_group),
      frame_token(frame_token),
      routing_id(routing_id),
      frame_receiver(std::move(frame_receiver)),
      associated_interface_provider(std::move(associated_interface_provider)),
      devtools_frame_token(devtools_frame_token),
      is_for_nested_main_frame(is_for_nested_main_frame) {}
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
      frame_token_(params.frame_token),
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
      routing_id_(params.routing_id),
#endif
      process_label_id_(
          base::trace_event::TraceLog::GetInstance()->GetNewProcessLabelId()),
      selection_text_offset_(0),
      selection_range_(gfx::Range::InvalidRange()),
      render_accessibility_manager_(
          std::make_unique<RenderAccessibilityManager>(this)),
      weak_wrapper_resource_load_info_notifier_(
          std::make_unique<blink::WeakWrapperResourceLoadInfoNotifier>(this)),
#if BUILDFLAG(ENABLE_PPAPI)
      focused_pepper_plugin_(nullptr),
#endif
      navigation_client_impl_(nullptr),
      media_factory_(
          this,
          base::BindRepeating(&RenderFrameImpl::RequestOverlayRoutingToken,
                              base::Unretained(this))),
      devtools_frame_token_(params.devtools_frame_token),
      is_for_nested_main_frame_(params.is_for_nested_main_frame) {
  TRACE_EVENT_WITH_FLOW0("navigation", "RenderFrameImpl::RenderFrameImpl",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(RenderThread::IsMainThread());
  blink_interface_registry_ = std::make_unique<BlinkInterfaceRegistryImpl>(
      registry_.GetWeakPtr(), associated_interfaces_.GetWeakPtr());

  DCHECK(params.frame_receiver.is_valid());
  pending_frame_receiver_ = std::move(params.frame_receiver);

  // Save the pending remote for lazy binding in
  // `GetRemoteAssociatedInterfaces().
  DCHECK(params.associated_interface_provider.is_valid());
  pending_associated_interface_provider_remote_ =
      std::move(params.associated_interface_provider);

  delayed_state_sync_timer_.SetTaskRunner(
      agent_scheduling_group_->agent_group_scheduler().DefaultTaskRunner());

  // Must call after binding our own remote interfaces.
  media_factory_.SetupMojo();

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  std::pair<RoutingIDFrameMap::iterator, bool> result =
      g_routing_id_frame_map.Get().insert(std::make_pair(routing_id_, this));
  CHECK(result.second) << "Inserting a duplicate item.";
#endif
}

mojom::FrameHost* RenderFrameImpl::GetFrameHost() {
  if (!frame_host_remote_.is_bound())
    GetRemoteAssociatedInterfaces()->GetInterface(&frame_host_remote_);
  return frame_host_remote_.get();
}

RenderFrameImpl::~RenderFrameImpl() {
  TRACE_EVENT_WITH_FLOW0("navigation", "RenderFrameImpl::~RenderFrameImpl",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);
  for (auto& observer : observers_)
    observer.OnDestruct();
  for (auto& observer : observers_)
    observer.RenderFrameGone();

  web_media_stream_device_observer_.reset();

  if (initialized_ && is_main_frame_)
    MainFrameCounter::DecrementCount();

  base::trace_event::TraceLog::GetInstance()->RemoveProcessLabel(
      process_label_id_);
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  g_routing_id_frame_map.Get().erase(routing_id_);
#endif
  agent_scheduling_group_->RemoveFrameRoute(frame_token_
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
                                            ,
                                            routing_id_
#endif
  );
}

void RenderFrameImpl::Initialize(blink::WebFrame* parent) {
  initialized_ = true;
  is_main_frame_ = !parent;
  if (is_main_frame_)
    MainFrameCounter::IncrementCount();

  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::Initialize", "frame_token",
               frame_token_);

#if BUILDFLAG(ENABLE_PPAPI)
  new PepperBrowserConnection(this);
#endif

  RegisterMojoInterfaces();

  {
    TRACE_EVENT("navigation", "ContentRendererClient::RenderFrameCreated");
    // We delay calling this until we have the WebFrame so that any observer or
    // embedder can call GetWebFrame on any RenderFrame.
    GetContentClient()->renderer()->RenderFrameCreated(this);
  }

  // blink::AudioOutputIPCFactory::io_task_runner_ may be null in tests.
  auto& factory = blink::AudioOutputIPCFactory::GetInstance();
  if (factory.io_task_runner()) {
    factory.RegisterRemoteFactory(GetWebFrame()->GetLocalFrameToken(),
                                  GetBrowserInterfaceBroker());
  }

  // Bind this class to mojom::Frame and to the message router for legacy IPC.
  // These must be called after |frame_| is set since binding requires a
  // per-frame task runner.
  frame_receiver_.Bind(
      std::move(pending_frame_receiver_),
      GetTaskRunner(blink::TaskType::kInternalNavigationAssociated));
  agent_scheduling_group_->AddFrameRoute(
      frame_token_,
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
      routing_id_,
#endif
      this, GetTaskRunner(blink::TaskType::kInternalNavigationAssociated));
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

blink::WebFrameWidget* RenderFrameImpl::GetLocalRootWebFrameWidget() {
  return frame_->LocalRoot()->FrameWidget();
}

#if BUILDFLAG(ENABLE_PPAPI)
void RenderFrameImpl::PepperPluginCreated(RendererPpapiHost* host) {
  for (auto& observer : observers_)
    observer.DidCreatePepperPlugin(host);
}

void RenderFrameImpl::PepperTextInputTypeChanged(
    PepperPluginInstanceImpl* instance) {
  if (instance != focused_pepper_plugin_)
    return;

  GetLocalRootWebFrameWidget()->UpdateTextInputState();
}

void RenderFrameImpl::PepperCaretPositionChanged(
    PepperPluginInstanceImpl* instance) {
  if (instance != focused_pepper_plugin_)
    return;
  GetLocalRootWebFrameWidget()->UpdateSelectionBounds();
}

void RenderFrameImpl::PepperCancelComposition(
    PepperPluginInstanceImpl* instance) {
  if (instance != focused_pepper_plugin_)
    return;
  GetLocalRootWebFrameWidget()->CancelCompositionForPepper();
}

void RenderFrameImpl::PepperSelectionChanged(
    PepperPluginInstanceImpl* instance) {
  if (instance != focused_pepper_plugin_)
    return;

  // We have no reason to believe the locally cached last synced selection is
  // invalid so we do not need to force the update if it matches our last synced
  // value.
  SyncSelectionIfRequired(blink::SyncCondition::kNotForced);
}

#endif  // BUILDFLAG(ENABLE_PPAPI)

void RenderFrameImpl::ScriptedPrint() {
  bool user_initiated = GetLocalRootWebFrameWidget()->HandlingInputEvent();
  for (auto& observer : observers_)
    observer.ScriptedPrint(user_initiated);
}

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
bool RenderFrameImpl::Send(IPC::Message* message) {
  return agent_scheduling_group_->Send(message);
}

bool RenderFrameImpl::OnMessageReceived(const IPC::Message& msg) {
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
  return false;
}
#endif

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
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "RenderFrameImpl::SetUpSharedMemoryForSmoothness",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(shared_memory.IsValid());
  for (auto& observer : observers_) {
    DCHECK(shared_memory.IsValid());
    if (observer.SetUpSmoothnessReporting(shared_memory))
      break;
  }
}

void RenderFrameImpl::BindAutoplayConfiguration(
    mojo::PendingAssociatedReceiver<blink::mojom::AutoplayConfigurationClient>
        receiver) {
  autoplay_configuration_receiver_.reset();
  autoplay_configuration_receiver_.Bind(
      std::move(receiver),
      GetTaskRunner(blink::TaskType::kInternalNavigationAssociated));
}

void RenderFrameImpl::BindFrameBindingsControl(
    mojo::PendingAssociatedReceiver<mojom::FrameBindingsControl> receiver) {
  frame_bindings_control_receiver_.Bind(
      std::move(receiver),
      GetTaskRunner(blink::TaskType::kInternalNavigationAssociated));
}

void RenderFrameImpl::BindNavigationClient(
    mojo::PendingAssociatedReceiver<mojom::NavigationClient> receiver) {
  navigation_client_impl_ = std::make_unique<NavigationClient>(this);
  navigation_client_impl_->Bind(std::move(receiver));
}

void RenderFrameImpl::BindNavigationClientWithParams(
    mojo::PendingAssociatedReceiver<mojom::NavigationClient> receiver,
    blink::mojom::BeginNavigationParamsPtr begin_params,
    blink::mojom::CommonNavigationParamsPtr common_params,
    bool is_duplicate_navigation) {
  if (navigation_client_impl_) {
    navigation_client_impl_->ResetForNewNavigation(is_duplicate_navigation);
  }
  navigation_client_impl_ = std::make_unique<NavigationClient>(
      this, std::move(begin_params), std::move(common_params));
  navigation_client_impl_->Bind(std::move(receiver));
}

// Unload this RenderFrame so the frame can navigate to a document rendered by
// a different process. We also allow this process to exit if there are no other
// active RenderFrames in it.
// This executes the unload handlers on this frame and its local descendants.
void RenderFrameImpl::Unload(
    bool is_loading,
    blink::mojom::FrameReplicationStatePtr replicated_frame_state,
    const blink::RemoteFrameToken& proxy_frame_token,
    blink::mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
    blink::mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces) {
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::UnloadFrame", "frame_token",
               frame_token_);
  DCHECK(!base::RunLoop::IsNestedOnCurrentThread());

  // Send an UpdateState message before we get deleted.
  // TODO(dcheng): Improve this comment to clarify why it's important to sent
  // state updates.
  SendUpdateState();

  // Before `this` is destroyed, save any fields needed to schedule a call to
  // `AgentSchedulingGroupHost::DidUnloadRenderFrame()`. The acknowlegement
  // itself is asynchronous to ensure that any postMessage calls (which schedule
  // IPCs as well) made from unload handlers are routed to the browser process
  // before the corresponding `RenderFrameHostImpl` is torn down.
  auto& agent_scheduling_group = *agent_scheduling_group_;
  blink::LocalFrameToken frame_token = frame_->GetLocalFrameToken();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetTaskRunner(blink::TaskType::kInternalPostMessageForwarding);

  // Important: |this| is deleted after this call!
  if (!SwapOutAndDeleteThis(is_loading, std::move(replicated_frame_state),
                            proxy_frame_token,
                            std::move(remote_frame_interfaces),
                            std::move(remote_main_frame_interfaces))) {
    // The swap is cancelled because running the unload handlers ended up
    // detaching this frame.
    return;
  }

  // Notify the browser that this frame was swapped out. Use the cached
  // `AgentSchedulingGroup` because |this| is deleted. Post a task to send the
  // ACK, so that any postMessage IPCs scheduled from the unload handler are
  // sent before the ACK (see https://crbug.com/857274).
  auto send_unload_ack = base::BindOnce(
      [](AgentSchedulingGroup* agent_scheduling_group,
         const blink::LocalFrameToken& frame_token) {
        agent_scheduling_group->DidUnloadRenderFrame(frame_token);
      },
      &agent_scheduling_group, frame_token);
  task_runner->PostTask(FROM_HERE, std::move(send_unload_ack));
}

void RenderFrameImpl::Delete(mojom::FrameDeleteIntention intent) {
  TRACE_EVENT(
      "navigation", "RenderFrameImpl::Delete", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_render_frame_impl_deletion();
        data->set_intent(FrameDeleteIntentionToProto(intent));
      });
  base::ScopedUmaHistogramTimer histogram_timer("Navigation.RenderFrameDelete");
  // The main frame (when not provisional) is owned by the renderer's frame tree
  // via WebViewImpl. When a provisional main frame is swapped in, the ownership
  // moves from the browser to the renderer, but this happens in the renderer
  // process and is then the browser is informed.
  // If the provisional main frame is swapped in while the browser is destroying
  // it, the browser may request to delete |this|, thinking it has ownership
  // of it, but the renderer has already taken ownership via SwapIn().
  switch (intent) {
    case mojom::FrameDeleteIntention::kNotMainFrame:
      // The frame was not a main frame, so the browser should always have
      // ownership of it and we can just proceed with deleting it on
      // request.
      DCHECK(!is_main_frame_);
      break;
    case mojom::FrameDeleteIntention::kSpeculativeMainFrameForShutdown:
      // In this case the renderer has taken ownership of the provisional main
      // frame but the browser did not know yet and is shutting down. We can
      // ignore this request as the frame will be destroyed when the RenderView
      // is. This handles the shutdown case of https://crbug.com/957858.
      DCHECK(is_main_frame_);
      if (in_frame_tree_)
        return;
      break;
    case mojom::FrameDeleteIntention::
        kSpeculativeMainFrameForNavigationCancelled:
      // In this case the browser was navigating and cancelled the speculative
      // navigation. The renderer *should* undo the SwapIn() but the old state
      // has already been destroyed. Both ignoring the message or handling it
      // would leave the renderer in an inconsistent state now. If we ignore it
      // then the browser thinks the `blink::WebView` has a remote main frame,
      // but it is incorrect. If we handle it, then we are deleting a local main
      // frame out from under the `blink::WebView` and we will have bad pointers
      // in the renderer. So all we can do is crash. We should instead prevent
      // this scenario by blocking the browser from dropping the speculative
      // main frame when a commit (and ownership transfer) is imminent.
      // TODO(dcheng): This is the case of https://crbug.com/838348.
      DCHECK(is_main_frame_);
#if !BUILDFLAG(IS_ANDROID)
      // This check is not enabled on Android, since it seems like it's much
      // easier to trigger data races there.
      CHECK(!in_frame_tree_);
#endif  // !BUILDFLAG(IS_ANDROID)
      break;
  }

  // This will result in a call to RenderFrameImpl::FrameDetached, which
  // deletes the object. Do not access |this| after detach.
  frame_->Detach();
}

void RenderFrameImpl::UndoCommitNavigation(
    bool is_loading,
    blink::mojom::FrameReplicationStatePtr replicated_frame_state,
    const blink::RemoteFrameToken& proxy_frame_token,
    blink::mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
    blink::mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces) {
  // The browser process asked `this` to commit a navigation but has now decided
  // to discard the speculative RenderFrameHostImpl instead, since the
  // associated navigation was cancelled or replaced. However, the browser
  // process hasn't heard the `DidCommitNavigation()` yet, so pretend that the
  // commit never happened by immediately swapping `this` back to a proxy.
  //
  // This means that any state changes triggered by the already-swapped in
  // RenderFrame will simply be ignored, but that can't be helped: the
  // browser-side RFH will be gone before any outgoing IPCs from the renderer
  // for this RenderFrame (which by definition, are still in-flight) will be
  // processed by the browser process (as it has not yet seen the
  // `DidCommitNavigation()`).
  SwapOutAndDeleteThis(is_loading, std::move(replicated_frame_state),
                       proxy_frame_token, std::move(remote_frame_interfaces),
                       std::move(remote_main_frame_interfaces));
}

void RenderFrameImpl::SnapshotAccessibilityTree(
    mojom::SnapshotAccessibilityTreeParamsPtr params,
    SnapshotAccessibilityTreeCallback callback) {
  ui::AXTreeUpdate response;
  AXTreeSnapshotterImpl snapshotter(this, ui::AXMode(params->ax_mode));
  snapshotter.Snapshot(params->max_nodes, params->timeout, &response);
  std::move(callback).Run(response);
}

void RenderFrameImpl::GetSerializedHtmlWithLocalLinks(
    const base::flat_map<GURL, base::FilePath>& url_map,
    const base::flat_map<blink::FrameToken, base::FilePath>& frame_token_map,
    bool save_with_empty_url,
    mojo::PendingRemote<mojom::FrameHTMLSerializerHandler> handler_remote) {
  // Convert input to the canonical way of passing a map into a Blink API.
  LinkRewritingDelegate delegate(url_map, frame_token_map);
  RenderFrameWebFrameSerializerClient client(std::move(handler_remote));

  // Serialize the frame (without recursing into subframes).
  WebFrameSerializer::Serialize(GetWebFrame(), &client, &delegate,
                                save_with_empty_url);
}

void RenderFrameImpl::SetWantErrorMessageStackTrace() {
  want_error_message_stack_trace_ = true;
  GetAgentGroupScheduler().Isolate()->SetCaptureStackTraceForUncaughtExceptions(
      true);
}

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
#if BUILDFLAG(ENABLE_PPAPI)
  // Notify all instances that we painted. The same caveats apply as for
  // ViewFlushedPaint regarding instances closing themselves, so we take
  // similar precautions.
  PepperPluginSet plugins = active_pepper_instances_;
  for (PepperPluginInstanceImpl* plugin : plugins) {
    if (base::Contains(active_pepper_instances_, plugin)) {
      plugin->ViewInitiatedPaint();
    }
  }
#endif
}

RenderFrame* RenderFrameImpl::GetMainRenderFrame() {
  WebFrame* main_frame = GetWebView()->MainFrame();
  DCHECK(main_frame);
  if (!main_frame->IsWebLocalFrame())
    return nullptr;
  return RenderFrame::FromWebFrame(main_frame->ToWebLocalFrame());
}

RenderAccessibility* RenderFrameImpl::GetRenderAccessibility() {
  return render_accessibility_manager_->GetRenderAccessibilityImpl();
}

std::unique_ptr<AXTreeSnapshotter> RenderFrameImpl::CreateAXTreeSnapshotter(
    ui::AXMode ax_mode) {
  return std::make_unique<AXTreeSnapshotterImpl>(this, ax_mode);
}

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
int RenderFrameImpl::GetRoutingID() {
  return routing_id_;
}
#endif

blink::WebLocalFrame* RenderFrameImpl::GetWebFrame() {
  DCHECK(frame_);
  return frame_;
}

const blink::WebLocalFrame* RenderFrameImpl::GetWebFrame() const {
  DCHECK(frame_);
  return frame_;
}

blink::WebView* RenderFrameImpl::GetWebView() {
  blink::WebView* web_view = GetWebFrame()->View();
  DCHECK(web_view);
  return web_view;
}

const blink::WebView* RenderFrameImpl::GetWebView() const {
  const blink::WebView* web_view = GetWebFrame()->View();
  DCHECK(web_view);
  return web_view;
}

const blink::web_pref::WebPreferences& RenderFrameImpl::GetBlinkPreferences() {
  return GetWebView()->GetWebPreferences();
}

const blink::RendererPreferences& RenderFrameImpl::GetRendererPreferences()
    const {
  return GetWebView()->GetRendererPreferences();
}

void RenderFrameImpl::ShowVirtualKeyboard() {
  GetLocalRootWebFrameWidget()->ShowVirtualKeyboard();
}

blink::WebPlugin* RenderFrameImpl::CreatePlugin(
    const WebPluginInfo& info,
    const blink::WebPluginParams& params) {
#if BUILDFLAG(ENABLE_PPAPI)
  std::optional<url::Origin> origin_lock;
  if (GetContentClient()->renderer()->IsOriginIsolatedPepperPlugin(info.path)) {
    origin_lock = url::Origin::Create(GURL(params.url));
  }

  bool pepper_plugin_was_registered = false;
  scoped_refptr<PluginModule> pepper_module(PluginModule::Create(
      this, info, origin_lock, &pepper_plugin_was_registered,
      GetTaskRunner(blink::TaskType::kNetworking)));
  if (pepper_plugin_was_registered) {
    if (pepper_module.get()) {
      return new PepperWebPluginImpl(pepper_module.get(), params, this);
    }
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  LOG(WARNING) << "Pepper module/plugin creation failed.";
#endif
#endif  // BUILDFLAG(ENABLE_PPAPI)
  return nullptr;
}

void RenderFrameImpl::ExecuteJavaScript(const std::u16string& javascript) {
  v8::HandleScope handle_scope(GetAgentGroupScheduler().Isolate());
  frame_->ExecuteScript(WebScriptSource(WebString::FromUTF16(javascript)));
}

void RenderFrameImpl::BindLocalInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  GetInterface(interface_name, std::move(interface_pipe));
}

blink::AssociatedInterfaceRegistry*
RenderFrameImpl::GetAssociatedInterfaceRegistry() {
  return &associated_interfaces_;
}

blink::AssociatedInterfaceProvider*
RenderFrameImpl::GetRemoteAssociatedInterfaces() {
  if (!remote_associated_interfaces_) {
    DCHECK(pending_associated_interface_provider_remote_);
    remote_associated_interfaces_ =
        std::make_unique<blink::AssociatedInterfaceProvider>(
            std::move(pending_associated_interface_provider_remote_),
            GetTaskRunner(blink::TaskType::kInternalNavigationAssociated));
  }
  return remote_associated_interfaces_.get();
}

void RenderFrameImpl::SetSelectedText(const std::u16string& selection_text,
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

bool RenderFrameImpl::IsPasting() {
  return GetLocalRootWebFrameWidget()->IsPasting();
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

#if BUILDFLAG(IS_ANDROID)
void RenderFrameImpl::NotifyUpdateUserGestureCarryoverInfo() {
  GetFrameHost()->UpdateUserGestureCarryoverInfo();
}
#endif

void RenderFrameImpl::NotifyResourceRedirectReceived(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_response) {}

void RenderFrameImpl::NotifyResourceResponseReceived(
    int64_t request_id,
    const url::SchemeHostPort& final_response_url,
    network::mojom::URLResponseHeadPtr response_head,
    network::mojom::RequestDestination request_destination,
    bool is_ad_resource) {
  if (!blink::IsRequestDestinationFrame(request_destination)) {
    bool notify = ShouldNotifySubresourceResponseStarted(
        GetWebView()->GetRendererPreferences());
    UMA_HISTOGRAM_BOOLEAN(
        "Renderer.ReduceSubresourceResponseIPC.DidNotifyBrowser", notify);
    if (notify) {
      GetFrameHost()->SubresourceResponseStarted(final_response_url,
                                                 response_head->cert_status);
    }
  }
  DidStartResponse(final_response_url, request_id, std::move(response_head),
                   request_destination, is_ad_resource);
}

void RenderFrameImpl::NotifyResourceTransferSizeUpdated(
    int64_t request_id,
    int32_t transfer_size_diff) {
  DidReceiveTransferSizeUpdate(request_id, transfer_size_diff);
}

void RenderFrameImpl::NotifyResourceLoadCompleted(
    blink::mojom::ResourceLoadInfoPtr resource_load_info,
    const network::URLLoaderCompletionStatus& status) {
  DidCompleteResponse(resource_load_info->request_id, status);
  GetFrameHost()->ResourceLoadComplete(std::move(resource_load_info));
}

void RenderFrameImpl::NotifyResourceLoadCanceled(int64_t request_id) {
  DidCancelResponse(request_id);
}

void RenderFrameImpl::Clone(
    mojo::PendingReceiver<blink::mojom::ResourceLoadInfoNotifier>
        pending_resource_load_info_notifier) {
  resource_load_info_notifier_receivers_.Add(
      this, std::move(pending_resource_load_info_notifier),
      agent_scheduling_group_->agent_group_scheduler().DefaultTaskRunner());
}

void RenderFrameImpl::GetInterfaceProvider(
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver) {
  auto task_runner = GetTaskRunner(blink::TaskType::kInternalDefault);
  DCHECK(task_runner);
  interface_provider_receivers_.Add(this, std::move(receiver), task_runner);
}

void RenderFrameImpl::AllowBindings(int64_t enabled_bindings_flags) {
  auto new_bindings =
      BindingsPolicySet::FromEnumBitmask(enabled_bindings_flags);
  enabled_bindings_.PutAll(new_bindings);

  if (new_bindings.Has(BindingsPolicyValue::kMojoWebUi)) {
    // If mojo web UI is being enabled, update the protected memory bool to
    // allow MojoJS binding in this process.
    blink::WebV8Features::AllowMojoJSForProcess();
  }
}

void RenderFrameImpl::EnableMojoJsBindings(
    content::mojom::ExtraMojoJsFeaturesPtr features) {
  enable_mojo_js_bindings_ = true;
  mojo_js_features_ = std::move(features);

  // Update the protected memory bool to allow MojoJS binding in this process.
  blink::WebV8Features::AllowMojoJSForProcess();
}

void RenderFrameImpl::EnableMojoJsBindingsWithBroker(
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker) {
  mojo_js_interface_broker_ = std::move(broker);

  // Update the protected memory bool to allow MojoJS binding in this process.
  blink::WebV8Features::AllowMojoJSForProcess();
}

void RenderFrameImpl::BindWebUI(
    mojo::PendingAssociatedReceiver<mojom::WebUI> receiver,
    mojo::PendingAssociatedRemote<mojom::WebUIHost> remote) {
  DCHECK(enabled_bindings_.Has(BindingsPolicyValue::kWebUi));
  WebUIExtensionData::Create(this, std::move(receiver), std::move(remote));
}

void RenderFrameImpl::SetOldPageLifecycleStateFromNewPageCommitIfNeeded(
    const blink::mojom::OldPageInfo* old_page_info,
    const GURL& new_page_url) {
  if (!old_page_info)
    return;

  WebLocalFrame* old_main_web_frame = WebLocalFrame::FromFrameToken(
      old_page_info->frame_token_for_old_main_frame);
  if (!old_main_web_frame) {
    // Even if we sent a valid `frame_token_for_old_main_frame`, it might have
    // already been destroyed by the time we try to get the WebLocalFrame, so
    // we should check if it still exists.
    return;
  }
  RenderFrameImpl* old_main_render_frame =
      RenderFrameImpl::FromWebFrame(old_main_web_frame);
  if (!old_main_render_frame) {
    return;
  }
  if (!IsMainFrame() && !old_main_render_frame->IsMainFrame()) {
    // This shouldn't happen because `old_page_info` should only be set on
    // cross-BrowsingInstance navigations, which can only happen on main frames.
    // However, we got some reports of this happening (see
    // https://crbug.com/1207271).
    SCOPED_CRASH_KEY_BOOL("old_page_info", "new_is_main_frame", IsMainFrame());
    SCOPED_CRASH_KEY_STRING256("old_page_info", "new_url", new_page_url.spec());
    SCOPED_CRASH_KEY_BOOL("old_page_info", "old_is_main_frame",
                          old_main_render_frame->IsMainFrame());
    SCOPED_CRASH_KEY_STRING256("old_page_info", "old_url",
                               old_main_render_frame->GetLoadingUrl().spec());
    SCOPED_CRASH_KEY_BOOL(
        "old_page_info", "old_is_frozen",
        old_page_info->new_lifecycle_state_for_old_page->is_frozen);
    SCOPED_CRASH_KEY_BOOL("old_page_info", "old_is_in_bfcache",
                          old_page_info->new_lifecycle_state_for_old_page
                              ->is_in_back_forward_cache);
    SCOPED_CRASH_KEY_BOOL(
        "old_page_info", "old_is_hidden",
        old_page_info->new_lifecycle_state_for_old_page->visibility ==
            blink::mojom::PageVisibilityState::kHidden);
    SCOPED_CRASH_KEY_BOOL(
        "old_page_info", "old_pagehide_dispatch",
        old_page_info->new_lifecycle_state_for_old_page->pagehide_dispatch ==
            blink::mojom::PagehideDispatch::kNotDispatched);
    CaptureTraceForNavigationDebugScenario(
        DebugScenario::kDebugNonMainFrameWithOldPageInfo);
    return;
  }
  DCHECK_EQ(old_page_info->new_lifecycle_state_for_old_page->visibility,
            blink::mojom::PageVisibilityState::kHidden);
  DCHECK_NE(old_page_info->new_lifecycle_state_for_old_page->pagehide_dispatch,
            blink::mojom::PagehideDispatch::kNotDispatched);
  old_main_web_frame->View()->SetPageLifecycleStateFromNewPageCommit(
      old_page_info->new_lifecycle_state_for_old_page->visibility,
      old_page_info->new_lifecycle_state_for_old_page->pagehide_dispatch);
}

void RenderFrameImpl::CommitNavigation(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    std::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        subresource_proxying_loader_factory,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        keep_alive_loader_factory,
    mojo::PendingAssociatedRemote<blink::mojom::FetchLaterLoaderFactory>
        fetch_later_loader_factory,
    const blink::DocumentToken& document_token,
    const base::UnguessableToken& devtools_navigation_token,
    const base::Uuid& base_auction_nonce,
    const std::optional<blink::ParsedPermissionsPolicy>& permissions_policy,
    blink::mojom::PolicyContainerPtr policy_container,
    mojo::PendingRemote<blink::mojom::CodeCacheHost> code_cache_host,
    mojo::PendingRemote<blink::mojom::CodeCacheHost>
        code_cache_host_for_background,
    mojom::CookieManagerInfoPtr cookie_manager_info,
    mojom::StorageInfoPtr storage_info,
    mojom::NavigationClient::CommitNavigationCallback commit_callback) {
  base::ElapsedTimer timer;
  base::ScopedUmaHistogramTimer histogram_timer(kCommitRenderFrame);
  base::ScopedUmaHistogramTimer histogram_timer_frame(base::StrCat(
      {kCommitRenderFrame, IsMainFrame() ? ".MainFrame" : ".Subframe"}));
  TRACE_EVENT_WITH_FLOW0("navigation", "RenderFrameImpl::CommitNavigation",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(navigation_client_impl_);
  DCHECK(!blink::IsRendererDebugURL(common_params->url));
  DCHECK(!NavigationTypeUtils::IsSameDocument(common_params->navigation_type));
  // `origin_to_commit` must only be set on failed navigations or  data: URL
  // navigations, except when kUseBrowserCalculatedOrigin is enabled.
  CHECK(!commit_params->origin_to_commit ||
        common_params->url.SchemeIs(url::kDataScheme) ||
        base::FeatureList::IsEnabled(features::kUseBrowserCalculatedOrigin));
  LogCommitHistograms(commit_params->commit_sent, is_main_frame_,
                      common_params->url);

  bool is_new_navigation_in_outermost_main_frame_with_http_or_https = false;
  if (frame_->IsOutermostMainFrame() &&
      common_params->url.SchemeIsHTTPOrHTTPS()) {
    switch (common_params->navigation_type) {
      case blink::mojom::NavigationType::DIFFERENT_DOCUMENT:
        is_new_navigation_in_outermost_main_frame_with_http_or_https = true;
        break;
      case blink::mojom::NavigationType::RELOAD:
      case blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE:
      case blink::mojom::NavigationType::RESTORE:
      case blink::mojom::NavigationType::RESTORE_WITH_POST:
      case blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT:
      case blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT:
      case blink::mojom::NavigationType::SAME_DOCUMENT:
        break;
    }
  }

  AssertNavigationCommits assert_navigation_commits(
      this, kMayReplaceInitialEmptyDocument);

  SetOldPageLifecycleStateFromNewPageCommitIfNeeded(
      commit_params->old_page_info.get(), common_params->url);

  bool was_initiated_in_this_frame =
      navigation_client_impl_ &&
      navigation_client_impl_->was_initiated_in_this_frame();

  // Sanity check that the browser always sends us new loader factories on
  // cross-document navigations.
  DCHECK(common_params->url.SchemeIs(url::kJavaScriptScheme) ||
         subresource_loader_factories);

  int request_id = blink::GenerateRequestId();
  std::unique_ptr<DocumentState> document_state = BuildDocumentStateFromParams(
      *common_params, *commit_params, std::move(commit_callback),
      std::move(navigation_client_impl_), request_id,
      was_initiated_in_this_frame);

  // Check if the navigation being committed originated as a client redirect.
  bool is_client_redirect =
      !!(common_params->transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  auto navigation_params = std::make_unique<WebNavigationParams>(
      document_token, devtools_navigation_token, base_auction_nonce);
  navigation_params->navigation_delivery_type =
      commit_params->navigation_delivery_type;
  navigation_params->is_client_redirect = is_client_redirect;
  FillMiscNavigationParams(*common_params, *commit_params,
                           navigation_params.get());
  navigation_params->policy_container =
      ToWebPolicyContainer(std::move(policy_container));
  navigation_params->view_transition_state =
      std::move(commit_params->view_transition_state);

  if (frame_->IsOutermostMainFrame() && permissions_policy) {
    navigation_params->permissions_policy_override = permissions_policy;
  }

  auto commit_with_params = base::BindOnce(
      &RenderFrameImpl::CommitNavigationWithParams, weak_factory_.GetWeakPtr(),
      common_params.Clone(), commit_params.Clone(),
      std::move(subresource_loader_factories), std::move(subresource_overrides),
      std::move(controller_service_worker_info), std::move(container_info),
      std::move(subresource_proxying_loader_factory),
      std::move(keep_alive_loader_factory),
      std::move(fetch_later_loader_factory), std::move(code_cache_host),
      std::move(code_cache_host_for_background), std::move(cookie_manager_info),
      std::move(storage_info), std::move(document_state));

  // Handle a navigation that has a non-empty `data_url_as_string`, or perform
  // a "loadDataWithBaseURL" navigation, which is different from a normal data:
  // URL navigation in various ways:
  // - The "document URL" will use the supplied `base_url_for_data_url` if it's
  // not empty (otherwise it will fall back to the data: URL).
  // - The actual data: URL will be saved in the document's DocumentState to
  // later be returned as the `url` in DidCommitProvisionalLoadParams.
  bool should_handle_data_url_as_string = false;
#if BUILDFLAG(IS_ANDROID)
  should_handle_data_url_as_string |=
      is_main_frame_ && !commit_params->data_url_as_string.empty();
#endif

  if (should_handle_data_url_as_string ||
      commit_params->is_load_data_with_base_url) {
    std::string mime_type, charset, data;
    // `base_url` will be set to `base_url_for_data_url` from `common_params`,
    // unless it's empty (the `data_url_as_string` handling case), in which
    // case `url` from `common_params` will be used.
    GURL base_url;
    DecodeDataURL(*common_params, *commit_params, &mime_type, &charset, &data,
                  &base_url);
    // Note that even though we use the term "base URL", `base_url` is also
    // used as the "document URL" (unlike normal "base URL"s set through the
    // <base> element or other means, which would only be used to resolve
    // relative URLs, etc).
    navigation_params->url = base_url;
    WebNavigationParams::FillStaticResponse(navigation_params.get(),
                                            WebString::FromUTF8(mime_type),
                                            WebString::FromUTF8(charset), data);
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
    blink::WebNavigationBodyLoader::FillNavigationParamsResponseAndBodyLoader(
        std::move(common_params), std::move(commit_params), request_id,
        response_head.Clone(), std::move(response_body),
        std::move(url_loader_client_endpoints),
        GetTaskRunner(blink::TaskType::kInternalLoading),
        CreateResourceLoadInfoNotifierWrapper(), !frame_->Parent(),
        navigation_params.get(), frame_->IsAdFrame());
  }

  // The MHTML mime type should be same as the one we check in the browser
  // process's download_utils::MustDownload.
  bool is_mhtml_archive = base::EqualsCaseInsensitiveASCII(
                              response_head->mime_type, "multipart/related") ||
                          base::EqualsCaseInsensitiveASCII(
                              response_head->mime_type, "message/rfc822");
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

  if (is_new_navigation_in_outermost_main_frame_with_http_or_https) {
    base::UmaHistogramTimes(
        base::StrCat({kCommitRenderFrame,
                      ".OutermostMainFrame.NewNavigation.IsHTTPOrHTTPS"}),
        timer.Elapsed());
  }
}

void RenderFrameImpl::CommitNavigationWithParams(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    std::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        subresource_proxying_loader_factory,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        keep_alive_loader_factory,
    mojo::PendingAssociatedRemote<blink::mojom::FetchLaterLoaderFactory>
        fetch_later_loader_factory,
    mojo::PendingRemote<blink::mojom::CodeCacheHost> code_cache_host,
    mojo::PendingRemote<blink::mojom::CodeCacheHost>
        code_cache_host_for_background,
    mojom::CookieManagerInfoPtr cookie_manager_info,
    mojom::StorageInfoPtr storage_info,
    std::unique_ptr<DocumentState> document_state,
    std::unique_ptr<WebNavigationParams> navigation_params) {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "RenderFrameImpl::CommitNavigationWithParams",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  base::ElapsedTimer timer;
  if (common_params->url.IsAboutSrcdoc()) {
    WebNavigationParams::FillStaticResponse(navigation_params.get(),
                                            "text/html", "UTF-8",
                                            commit_params->srcdoc_value);
  }

  scoped_refptr<blink::ChildURLLoaderFactoryBundle> new_loader_factories =
      CreateLoaderFactoryBundle(std::move(subresource_loader_factories),
                                std::move(subresource_overrides),
                                std::move(subresource_proxying_loader_factory),
                                std::move(keep_alive_loader_factory),
                                std::move(fetch_later_loader_factory));

  DCHECK(new_loader_factories);
  DCHECK(new_loader_factories->HasBoundDefaultFactory());

  // If the navigation is for "view source", the WebLocalFrame needs to be put
  // in a special mode.
  if (commit_params->is_view_source)
    frame_->EnableViewSourceMode(true);

  if (frame_->IsOutermostMainFrame()) {
    // Save the Back/Forward Cache NotRestoredReasons struct to WebLocalFrame to
    // report for PerformanceNavigationTiming API.
    frame_->SetNotRestoredReasons(
        std::move(commit_params->not_restored_reasons));
  } else {
    // NotRestoredReasons are only set for the outermost main frame.
    CHECK(!commit_params->not_restored_reasons);
  }

  // |lcpp_hint| is set only when the frame is eligible (e.g. it's an outer
  // most main frame), which is checked in the browser process. Otherwise
  // nullptr.
  //
  // When there's a pre-existing LCPP hint on frame, we want to remove the
  // existing hint. Hence calling SetLCPPHint() is always required.
  CHECK(!commit_params->lcpp_hint || frame_->IsOutermostMainFrame());
  frame_->SetLCPPHint(std::move(commit_params->lcpp_hint));

  // Note: this intentionally does not call |Detach()| before |reset()|. If
  // there is an active |MHTMLBodyLoaderClient|, the browser-side navigation
  // code is explicitly replacing it with a new navigation commit request.
  // The check for |kWillCommit| in |~MHTMLBodyLoaderClient| covers this case.
  mhtml_body_loader_client_.reset();

  PrepareFrameForCommit(common_params->url, *commit_params);

  blink::WebFrameLoadType load_type =
      NavigationTypeToLoadType(common_params->navigation_type,
                               common_params->should_replace_current_entry);

  WebHistoryItem item_for_history_navigation;
  blink::mojom::CommitResult commit_status = blink::mojom::CommitResult::Ok;

  if (load_type == WebFrameLoadType::kBackForward ||
      load_type == WebFrameLoadType::kRestore) {
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

  navigation_params->load_with_storage_access =
      commit_params->load_with_storage_access;
  navigation_params->visited_link_salt = commit_params->visited_link_salt;

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
                new_loader_factories->Clone()));
  }

  DCHECK(!pending_loader_factories_);
  pending_loader_factories_ = std::move(new_loader_factories);
  pending_code_cache_host_ = std::move(code_cache_host);
  pending_code_cache_host_for_background_ =
      std::move(code_cache_host_for_background);
  pending_cookie_manager_info_ = std::move(cookie_manager_info);
  pending_storage_info_ = std::move(storage_info);
  original_storage_key_ = navigation_params->storage_key;

  base::WeakPtr<RenderFrameImpl> weak_self = weak_factory_.GetWeakPtr();
  frame_->CommitNavigation(std::move(navigation_params),
                           std::move(document_state));
  // The commit can result in this frame being removed.
  if (!weak_self)
    return;

  if (commit_params->local_surface_id) {
    CHECK(frame_->FrameWidget())
        << "Only local roots should get a SurfaceID update";
    frame_->FrameWidget()->ApplyLocalSurfaceIdUpdate(
        *commit_params->local_surface_id);
  }

  if (load_type == WebFrameLoadType::kStandard &&
      common_params->url.SchemeIsHTTPOrHTTPS()) {
    base::UmaHistogramMicrosecondsTimes(
        "Navigation.CommitNavigationWithParams.Time.IsStandardLoadType."
        "IsHTTPOrHTTPS",
        timer.Elapsed());
  }

  ResetMembersUsedForDurationOfCommit();
}

void RenderFrameImpl::CommitFailedNavigation(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    bool has_stale_copy_in_cache,
    int error_code,
    int extended_error_code,
    net::ResolveErrorInfo resolve_error_info,
    const std::optional<std::string>& error_page_content,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    const blink::DocumentToken& document_token,
    blink::mojom::PolicyContainerPtr policy_container,
    mojom::AlternativeErrorPageOverrideInfoPtr alternative_error_page_info,
    mojom::NavigationClient::CommitFailedNavigationCallback callback) {
  TRACE_EVENT1("navigation,benchmark,rail",
               "RenderFrameImpl::CommitFailedNavigation", "frame_token",
               frame_token_);
  DCHECK(navigation_client_impl_);
  DCHECK(!NavigationTypeUtils::IsSameDocument(common_params->navigation_type));
  // `origin_to_commit` must be set on failed navigations.
  CHECK(commit_params->origin_to_commit);

  // The browser process should not send us an initiator_base_url in a failed
  // navigation.
  DCHECK(!common_params->initiator_base_url);

  AssertNavigationCommits assert_navigation_commits(
      this, kMayReplaceInitialEmptyDocument);

  GetWebView()->SetHistoryListFromNavigation(
      commit_params->current_history_list_offset,
      commit_params->current_history_list_length);

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
  scoped_refptr<blink::ChildURLLoaderFactoryBundle> new_loader_factories =
      CreateLoaderFactoryBundle(
          std::move(subresource_loader_factories),
          std::nullopt /* subresource_overrides */,
          mojo::NullRemote() /* subresource_proxying_loader_factory */,
          mojo::NullRemote() /* keep_alive_loader_factory */,
          mojo::NullAssociatedRemote() /* fetch_later_loader_factory */);
  DCHECK(new_loader_factories->HasBoundDefaultFactory());

  // Send the provisional load failure.
  WebURLError error(
      error_code, extended_error_code, resolve_error_info,
      has_stale_copy_in_cache ? WebURLError::HasCopyInCache::kTrue
                              : WebURLError::HasCopyInCache::kFalse,
      WebURLError::IsWebSecurityViolation::kFalse, common_params->url,
      WebURLError::ShouldCollapseInitiator::kFalse);

  // Since the URL will be set to kUnreachableWebDataURL, use default content
  // settings.
  commit_params->content_settings =
      blink::CreateDefaultRendererContentSettings();
  auto navigation_params = std::make_unique<WebNavigationParams>(
      document_token,
      /*devtools_navigation_token=*/base::UnguessableToken::Create(),
      /*base_auction_nonce=*/base::Uuid::GenerateRandomV4());
  FillNavigationParamsRequest(*common_params, *commit_params,
                              navigation_params.get());
  // Use kUnreachableWebDataURL as the document URL (instead of the URL that
  // failed to load, which is saved separately as the "unreachable URL" below).
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

  if (commit_params->nav_entry_id == 0) {
    // For renderer initiated navigations, we send out a
    // DidFailProvisionalLoad() notification.
    NotifyObserversOfFailedProvisionalLoad();
  }

  std::string error_html;
  std::string* error_html_ptr = &error_html;
  if (error_code == net::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    DCHECK_NE(commit_params->http_response_code, -1);
    GetContentClient()->renderer()->PrepareErrorPageForHttpStatusError(
        this, error, navigation_params->http_method.Ascii(),
        commit_params->http_response_code, nullptr, error_html_ptr);
  } else {
    if (error_page_content) {
      error_html = error_page_content.value();
      error_html_ptr = nullptr;
    }
    // Prepare for the error page. Note that even if |error_html_ptr| is set to
    // null above, PrepareErrorPage might have other side effects e.g. setting
    // some error-related states, so we should still call it.
    GetContentClient()->renderer()->PrepareErrorPage(
        this, error, navigation_params->http_method.Ascii(),
        std::move(alternative_error_page_info), error_html_ptr);
  }

  // Make sure we never show errors in view source mode.
  frame_->EnableViewSourceMode(false);

  auto page_state =
      blink::PageState::CreateFromEncodedData(commit_params->page_state);
  if (page_state.IsValid())
    navigation_params->history_item = WebHistoryItem(page_state);
  if (!navigation_params->history_item.IsNull()) {
    if (common_params->navigation_type ==
            blink::mojom::NavigationType::RESTORE ||
        common_params->navigation_type ==
            blink::mojom::NavigationType::RESTORE_WITH_POST) {
      navigation_params->frame_load_type = WebFrameLoadType::kRestore;
    } else {
      navigation_params->frame_load_type = WebFrameLoadType::kBackForward;
    }
  } else if (common_params->should_replace_current_entry) {
    navigation_params->frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
  }

  navigation_params->service_worker_network_provider =
      ServiceWorkerNetworkProviderForFrame::CreateInvalidInstance();
  FillMiscNavigationParams(*common_params, *commit_params,
                           navigation_params.get());
  WebNavigationParams::FillStaticResponse(navigation_params.get(), "text/html",
                                          "UTF-8", error_html);
  // Save the URL that failed to load as the "unreachable URL" so that the
  // we can use that (instead of kUnreachableWebDataURL) for the HistoryItem for
  // this navigation, and also to send back with the DidCommitProvisionalLoad
  // message to the browser.
  // TODO(crbug.com/40150370): Stop sending the URL back with DidCommit.
  navigation_params->unreachable_url = error.url();
  if (commit_params->redirects.size()) {
    navigation_params->pre_redirect_url_for_failed_navigations =
        commit_params->redirects[0];
  } else {
    navigation_params->pre_redirect_url_for_failed_navigations = error.url();
  }

  navigation_params->policy_container =
      ToWebPolicyContainer(std::move(policy_container));

  navigation_params->view_transition_state =
      std::move(commit_params->view_transition_state);

  // The error page load (not to confuse with a failed load of original page)
  // was not initiated through BeginNavigation, therefore
  // |was_initiated_in_this_frame| is false.
  std::unique_ptr<DocumentState> document_state = BuildDocumentStateFromParams(
      *common_params, *commit_params, std::move(callback),
      std::move(navigation_client_impl_), blink::GenerateRequestId(),
      false /* was_initiated_in_this_frame */);

  DCHECK(!pending_loader_factories_);
  pending_loader_factories_ = std::move(new_loader_factories);

  // The load of the error page can result in this frame being removed.
  // Use a WeakPtr as an easy way to detect whether this has occurred. If so,
  // this method should return immediately and not touch any part of the object,
  // otherwise it will result in a use-after-free bug.
  base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();
  frame_->CommitNavigation(std::move(navigation_params),
                           std::move(document_state));
  if (!weak_this)
    return;

  ResetMembersUsedForDurationOfCommit();
}

void RenderFrameImpl::CommitSameDocumentNavigation(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    CommitSameDocumentNavigationCallback callback) {
  DCHECK(!blink::IsRendererDebugURL(common_params->url));
  DCHECK(!NavigationTypeUtils::IsReload(common_params->navigation_type));
  DCHECK(!commit_params->is_view_source);
  DCHECK(NavigationTypeUtils::IsSameDocument(common_params->navigation_type));

  CHECK(in_frame_tree_);
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
                               common_params->should_replace_current_entry);

  DocumentState* document_state =
      DocumentState::FromDocumentLoader(frame_->GetDocumentLoader());
  document_state->set_navigation_state(
      NavigationState::CreateForSameDocumentCommitFromBrowser(
          std::move(common_params), std::move(commit_params),
          std::move(callback)));
  NavigationState* navigation_state = document_state->navigation_state();

  blink::mojom::CommitResult commit_status = blink::mojom::CommitResult::Ok;
  WebHistoryItem item_for_history_navigation;

  if (navigation_state->common_params().navigation_type ==
      blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT) {
    DCHECK(blink::PageState::CreateFromEncodedData(
               navigation_state->commit_params().page_state)
               .IsValid());
    // We must know the nav entry ID of the page we are navigating back to,
    // which should be the case because history navigations are routed via the
    // browser.
    DCHECK_NE(0, navigation_state->commit_params().nav_entry_id);
    DCHECK(!navigation_state->common_params()
                .is_history_navigation_in_new_child_frame);
    commit_status = PrepareForHistoryNavigationCommit(
        navigation_state->common_params(), navigation_state->commit_params(),
        &item_for_history_navigation, &load_type);
  }

  if (commit_status == blink::mojom::CommitResult::Ok) {
    base::WeakPtr<RenderFrameImpl> weak_this = weak_factory_.GetWeakPtr();
    // Same-document navigations on data URLs loaded with a valid base URL
    // should keep the base URL as document URL.
    bool use_base_url_for_data_url =
        !navigation_state->common_params().base_url_for_data_url.is_empty();
#if BUILDFLAG(IS_ANDROID)
    use_base_url_for_data_url |=
        !navigation_state->commit_params().data_url_as_string.empty();
#endif

    GURL url;
    if (is_main_frame_ && use_base_url_for_data_url) {
      url = navigation_state->common_params().base_url_for_data_url;
    } else {
      url = navigation_state->common_params().url;
    }
    bool is_client_redirect = !!(navigation_state->common_params().transition &
                                 ui::PAGE_TRANSITION_CLIENT_REDIRECT);
    bool started_with_transient_activation =
        navigation_state->common_params().has_user_gesture;
    bool is_browser_initiated =
        navigation_state->commit_params().is_browser_initiated;
    bool has_ua_visual_transition =
        navigation_state->commit_params().has_ua_visual_transition;
    std::optional<blink::scheduler::TaskAttributionId>
        soft_navigation_heuristics_task_id =
            navigation_state->commit_params()
                .soft_navigation_heuristics_task_id;

    WebSecurityOrigin initiator_origin;
    if (navigation_state->common_params().initiator_origin) {
      initiator_origin =
          navigation_state->common_params().initiator_origin.value();
    }

    // Load the request.
    commit_status = frame_->CommitSameDocumentNavigation(
        url, load_type, item_for_history_navigation, is_client_redirect,
        started_with_transient_activation, initiator_origin,
        is_browser_initiated, has_ua_visual_transition,
        soft_navigation_heuristics_task_id);

    // If `commit_status` is Ok, RunCommitSameDocumentNavigationCallback() was
    // called in DidCommitNavigationInternal() or the NavigationApi deferred the
    // commit and will call DidCommitNavigationInternal() when the commit is
    // undeferred. Either way, no further work is needed here.
    if (commit_status == blink::mojom::CommitResult::Ok) {
      return;
    }

    // The load of the URL can result in this frame being removed. Use a
    // WeakPtr as an easy way to detect whether this has occured. If so, this
    // method should return immediately and not touch any part of the object,
    // otherwise it will result in a use-after-free bug.
    // Similarly, check whether `navigation_state` is still the state associated
    // with the WebDocumentLoader. It may have been preempted by a navigation
    // started by an event handler.
    if (!weak_this || document_state->navigation_state() != navigation_state) {
      return;
    }
  }

  DCHECK_NE(commit_status, blink::mojom::CommitResult::Ok);
  navigation_state->RunCommitSameDocumentNavigationCallback(commit_status);
  document_state->clear_navigation_state();

  // The browser expects the frame to be loading this navigation. Inform it
  // that the load stopped if needed.
  if (frame_ && !frame_->IsLoading()) {
    GetFrameHost()->DidStopLoading();
  }
}

void RenderFrameImpl::UpdateSubresourceLoaderFactories(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories) {
  if (loader_factories_->IsHostChildURLLoaderFactoryBundle()) {
    static_cast<blink::HostChildURLLoaderFactoryBundle*>(
        loader_factories_.get())
        ->UpdateThisAndAllClones(std::move(subresource_loader_factories));
  } else {
    // RFHI::recreate_default_url_loader_factory_after_network_service_crash_ is
    // only set for frames that directly get their factory bundle from the
    // browser (HostChildURLLoaderFactoryBundle) rather than inheriting the
    // bundle from another frame (TrackedChildURLLoaderFactoryBundle).
    // Therefore the default factory *may* be present in the `if`/true branch
    // and *must* be missing in the `else` branch.
    DCHECK(!subresource_loader_factories->pending_default_factory().is_valid());

    // If there is no `pending_default_factory` (see the DCHECK above) and no
    // empty-payload IPCs are sent, then the only way to get here should be
    // because of `pending_isolated_world_factories`.
    //
    // TODO(crbug.com/40158699): Remove the whole `else` branch once
    // Chrome Platform Apps and `pending_isolated_world_factories` are gone.
    DCHECK(!subresource_loader_factories->pending_isolated_world_factories()
                .empty());

    // `!IsHostChildURLLoaderFactoryBundle` should only happen if the frame
    // hosts a document that isn't related to a real navigation (i.e. if an
    // initial empty document should "inherit" the factories from its
    // opener/parent).
    DCHECK_EQ(NavigationCommitState::kInitialEmptyDocument,
              navigation_commit_state_);

    auto partial_bundle =
        base::MakeRefCounted<blink::ChildURLLoaderFactoryBundle>();
    static_cast<blink::URLLoaderFactoryBundle*>(partial_bundle.get())
        ->Update(std::move(subresource_loader_factories));
    loader_factories_->Update(partial_bundle->PassInterface());
  }

  // Resetting `background_resource_fetch_context_` here so it will be recreated
  // when MaybeGetBackgroundResourceFetchAssets() is called.
  background_resource_fetch_context_.reset();
}

// content::RenderFrame implementation
// ----------------------------------------
const blink::BrowserInterfaceBrokerProxy&
RenderFrameImpl::GetBrowserInterfaceBroker() {
  return frame_->GetBrowserInterfaceBroker();
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

bool RenderFrameImpl::IsDomStorageDisabled() const {
  return GetContentClient()->renderer()->IsDomStorageDisabled();
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

#if BUILDFLAG(ENABLE_PPAPI)
  WebPluginInfo info;
  std::string mime_type;
  bool found = false;
  GetPepperHost()->GetPluginInfo(params.url, params.mime_type.Utf8(), &found,
                                 &info, &mime_type);
  if (!found)
    return nullptr;

  WebPluginParams params_to_use = params;
  params_to_use.mime_type = WebString::FromUTF8(mime_type);
  return CreatePlugin(info, params_to_use);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_PPAPI)
}

std::unique_ptr<blink::WebMediaPlayer> RenderFrameImpl::CreateMediaPlayer(
    const blink::WebMediaPlayerSource& source,
    WebMediaPlayerClient* client,
    blink::MediaInspectorContext* inspector_context,
    WebMediaPlayerEncryptedMediaClient* encrypted_client,
    WebContentDecryptionModule* initial_cdm,
    const blink::WebString& sink_id,
    const cc::LayerTreeSettings* settings,
    scoped_refptr<base::TaskRunner> compositor_worker_task_runner) {
  // `settings` should be non-null since the WebView created for
  // RenderFrameImpl always composites.
  DCHECK(settings);
  return media_factory_.CreateMediaPlayer(
      source, client, inspector_context, encrypted_client, initial_cdm, sink_id,
      GetLocalRootWebFrameWidget()->GetFrameSinkId(), *settings,
      agent_scheduling_group_->agent_group_scheduler().CompositorTaskRunner(),
      std::move(compositor_worker_task_runner));
}

std::unique_ptr<blink::WebContentSettingsClient>
RenderFrameImpl::CreateWorkerContentSettingsClient() {
  if (!frame_ || !frame_->View())
    return nullptr;
  return GetContentClient()->renderer()->CreateWorkerContentSettingsClient(
      this);
}

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<media::SpeechRecognitionClient>
RenderFrameImpl::CreateSpeechRecognitionClient() {
  if (!frame_ || !frame_->View())
    return nullptr;
  return GetContentClient()->renderer()->CreateSpeechRecognitionClient(this);
}
#endif

scoped_refptr<blink::WebWorkerFetchContext>
RenderFrameImpl::CreateWorkerFetchContext() {
  ServiceWorkerNetworkProviderForFrame* provider =
      static_cast<ServiceWorkerNetworkProviderForFrame*>(
          frame_->GetDocumentLoader()->GetServiceWorkerNetworkProvider());
  DCHECK(provider);

  mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
      watcher_receiver;
  GetWebView()->RegisterRendererPreferenceWatcher(
      watcher_receiver.InitWithNewPipeAndPassRemote());

  mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
      pending_resource_load_info_notifier;
  resource_load_info_notifier_receivers_.Add(
      this,
      pending_resource_load_info_notifier.InitWithNewPipeAndPassReceiver(),
      agent_scheduling_group_->agent_group_scheduler().DefaultTaskRunner());

  std::vector<std::string> cors_exempt_header_list =
      RenderThreadImpl::current()->cors_exempt_header_list();
  blink::WebVector<blink::WebString> web_cors_exempt_header_list(
      cors_exempt_header_list.size());
  base::ranges::transform(
      cors_exempt_header_list, web_cors_exempt_header_list.begin(),
      [](const auto& header) { return blink::WebString::FromLatin1(header); });

  // |pending_subresource_loader_updater| and
  // |pending_resource_load_info_notifier| are not used for
  // non-PlzDedicatedWorker and worklets.
  scoped_refptr<blink::WebDedicatedOrSharedWorkerFetchContext>
      web_dedicated_or_shared_worker_fetch_context =
          blink::WebDedicatedOrSharedWorkerFetchContext::Create(
              provider->context(), GetWebView()->GetRendererPreferences(),
              std::move(watcher_receiver), GetLoaderFactoryBundle()->Clone(),
              GetLoaderFactoryBundle()->Clone(),
              /*pending_subresource_loader_updater=*/mojo::NullReceiver(),
              web_cors_exempt_header_list,
              std::move(pending_resource_load_info_notifier));

  web_dedicated_or_shared_worker_fetch_context->SetAncestorFrameToken(
      frame_->GetLocalFrameToken());
  web_dedicated_or_shared_worker_fetch_context->set_site_for_cookies(
      frame_->GetDocument().SiteForCookies());
  web_dedicated_or_shared_worker_fetch_context->set_top_frame_origin(
      frame_->GetDocument().TopFrameOrigin());

  for (auto& observer : observers_) {
    observer.WillCreateWorkerFetchContext(
        web_dedicated_or_shared_worker_fetch_context.get());
  }
  return web_dedicated_or_shared_worker_fetch_context;
}

scoped_refptr<blink::WebWorkerFetchContext>
RenderFrameImpl::CreateWorkerFetchContextForPlzDedicatedWorker(
    blink::WebDedicatedWorkerHostFactoryClient* factory_client) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  DCHECK(factory_client);

  mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
      watcher_receiver;
  GetWebView()->RegisterRendererPreferenceWatcher(
      watcher_receiver.InitWithNewPipeAndPassRemote());

  mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
      pending_resource_load_info_notifier;
  resource_load_info_notifier_receivers_.Add(
      this,
      pending_resource_load_info_notifier.InitWithNewPipeAndPassReceiver(),
      agent_scheduling_group_->agent_group_scheduler().DefaultTaskRunner());

  scoped_refptr<blink::WebDedicatedOrSharedWorkerFetchContext>
      web_dedicated_or_shared_worker_fetch_context =
          static_cast<DedicatedWorkerHostFactoryClient*>(factory_client)
              ->CreateWorkerFetchContext(
                  GetWebView()->GetRendererPreferences(),
                  std::move(watcher_receiver),
                  std::move(pending_resource_load_info_notifier));

  web_dedicated_or_shared_worker_fetch_context->SetAncestorFrameToken(
      frame_->GetLocalFrameToken());
  web_dedicated_or_shared_worker_fetch_context->set_site_for_cookies(
      frame_->GetDocument().SiteForCookies());
  web_dedicated_or_shared_worker_fetch_context->set_top_frame_origin(
      frame_->GetDocument().TopFrameOrigin());

  for (auto& observer : observers_) {
    observer.WillCreateWorkerFetchContext(
        web_dedicated_or_shared_worker_fetch_context.get());
  }
  return web_dedicated_or_shared_worker_fetch_context;
}

std::unique_ptr<blink::WebPrescientNetworking>
RenderFrameImpl::CreatePrescientNetworking() {
  return GetContentClient()->renderer()->CreatePrescientNetworking(this);
}

std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
RenderFrameImpl::CreateResourceLoadInfoNotifierWrapper() {
  return std::make_unique<blink::ResourceLoadInfoNotifierWrapper>(
      weak_wrapper_resource_load_info_notifier_->AsWeakPtr(),
      GetTaskRunner(blink::TaskType::kNetworking));
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

namespace {

// Emit the trace event using a helper as we:
// a) want to ensure that the trace event covers the entire function.
// b) we want to emit the new child routing id as an argument.
// c) child routing id becomes available only after a sync call.
struct CreateChildFrameTraceEvent {
  explicit CreateChildFrameTraceEvent(
      const blink::LocalFrameToken& frame_token) {
    TRACE_EVENT_BEGIN("navigation,rail", "RenderFrameImpl::createChildFrame",
                      "frame_token", frame_token);
  }

  ~CreateChildFrameTraceEvent() {
    TRACE_EVENT_END("navigation,rail", "child_frame_token", child_frame_token);
  }

  blink::LocalFrameToken child_frame_token;
};

}  // namespace

blink::WebLocalFrame* RenderFrameImpl::CreateChildFrame(
    blink::mojom::TreeScopeType scope,
    const blink::WebString& name,
    const blink::WebString& fallback_name,
    const blink::FramePolicy& frame_policy,
    const blink::WebFrameOwnerProperties& frame_owner_properties,
    blink::FrameOwnerElementType frame_owner_element_type,
    blink::WebPolicyContainerBindParams policy_container_bind_params,
    ukm::SourceId document_ukm_source_id,
    FinishChildFrameCreationFn finish_creation) {
  // Tracing analysis uses this to find main frames when this value is
  // MSG_ROUTING_NONE, and build the frame tree otherwise.
  CreateChildFrameTraceEvent trace_event(frame_token_);

  // Allocate child routing ID. This is a synchronous call.
  int child_routing_id;
  blink::LocalFrameToken frame_token;
  base::UnguessableToken devtools_frame_token;
  blink::DocumentToken document_token;
  if (!RenderThread::Get()->GenerateFrameRoutingID(
          child_routing_id, frame_token, devtools_frame_token,
          document_token)) {
    return nullptr;
  }
  trace_event.child_frame_token = frame_token;

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
  bool is_created_by_script = GetAgentGroupScheduler().Isolate()->InContext();
  std::string frame_unique_name =
      unique_name_helper_.GenerateNameForNewChildFrame(
          name.IsEmpty() ? fallback_name.Utf8() : name.Utf8(),
          is_created_by_script);

  mojo::PendingAssociatedReceiver<mojom::Frame> pending_frame_receiver;

  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker;

  mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
      associated_interface_provider;

  // Now create the child frame in the browser via an asynchronous call.
  GetFrameHost()->CreateChildFrame(
      frame_token, pending_frame_receiver.InitWithNewEndpointAndPassRemote(),
      browser_interface_broker.InitWithNewPipeAndPassReceiver(),
      blink::mojom::PolicyContainerBindParams::New(
          std::move(policy_container_bind_params.receiver)),
      associated_interface_provider.InitWithNewEndpointAndPassReceiver(), scope,
      name.Utf8(), frame_unique_name, is_created_by_script, frame_policy,
      blink::mojom::FrameOwnerProperties::From(frame_owner_properties),
      frame_owner_element_type, document_ukm_source_id);

  // Create the RenderFrame and WebLocalFrame, linking the two.
  RenderFrameImpl* child_render_frame = RenderFrameImpl::Create(
      *agent_scheduling_group_, frame_token, child_routing_id,
      std::move(pending_frame_receiver),
      std::move(associated_interface_provider), devtools_frame_token,
      /*is_for_nested_main_frame=*/false);
  child_render_frame->SetLoaderFactoryBundle(CloneLoaderFactories());
  child_render_frame->unique_name_helper_.set_propagated_name(
      frame_unique_name);
  if (is_created_by_script)
    child_render_frame->unique_name_helper_.Freeze();
  blink::WebLocalFrame* web_frame = frame_->CreateLocalChild(
      scope, child_render_frame,
      child_render_frame->blink_interface_registry_.get(), frame_token);
  finish_creation(web_frame, document_token,
                  std::move(browser_interface_broker));

  child_render_frame->in_frame_tree_ = true;
  child_render_frame->Initialize(/*parent=*/GetWebFrame());

  return web_frame;
}

void RenderFrameImpl::DidCreateFencedFrame(
    const blink::RemoteFrameToken& frame_token) {
  for (auto& observer : observers_)
    observer.DidCreateFencedFrame(frame_token);
}

blink::WebFrame* RenderFrameImpl::FindFrame(const blink::WebString& name) {
  if (GetBlinkPreferences().renderer_wide_named_frame_lookup) {
    for (const auto& it : g_frame_map.Get()) {
      WebLocalFrame* frame = it.second->GetWebFrame();
      if (frame->AssignedName() == name)
        return frame;
    }
  }

  return GetContentClient()->renderer()->FindFrame(this->GetWebFrame(),
                                                   name.Utf8());
}

void RenderFrameImpl::MaybeInitializeWidget(
    mojom::CreateFrameWidgetParamsPtr widget_params) {
  if (!PreviousWidgetForLazyCompositorInitialization(
          widget_params->previous_frame_token_for_compositor_reuse)) {
    InitializeFrameWidgetForFrame(*frame_, /*previous_widget=*/nullptr,
                                  std::move(widget_params),
                                  is_for_nested_main_frame_);
  } else {
    // Initializing the widget is deferred until commit if this RenderFrame
    // will be replacing a previous RenderFrame. This enables reuse of the
    // compositing setup which is expensive.
    // This step must be deferred until commit since this RenderFrame could be
    // speculative and the previous RenderFrame will continue to be visible
    // and animating until commit.
    //
    // TODO(khushalsagar): Ideal would be to move the widget initialization to
    // the commit stage for all cases. This shouldn't have any perf impact
    // since the expensive parts of compositing (setting up a connection to
    // the GPU process) is not done until the frame is made visible, which
    // happens at commit.
    widget_params_for_lazy_widget_creation_ = std::move(widget_params);
  }
}

void RenderFrameImpl::EnsureWidgetInitialized() {
  if (!widget_params_for_lazy_widget_creation_) {
    CHECK(GetLocalRootWebFrameWidget());
    return;
  }

  auto* previous_widget = PreviousWidgetForLazyCompositorInitialization(
      widget_params_for_lazy_widget_creation_
          ->previous_frame_token_for_compositor_reuse);
  CHECK(previous_widget);

  InitializeFrameWidgetForFrame(
      *frame_, previous_widget,
      std::move(widget_params_for_lazy_widget_creation_),
      is_for_nested_main_frame_);
}

blink::WebFrameWidget*
RenderFrameImpl::PreviousWidgetForLazyCompositorInitialization(
    const std::optional<blink::FrameToken>& previous_frame_token) const {
  if (!previous_frame_token) {
    return nullptr;
  }

  auto* previous_web_frame = WebFrame::FromFrameToken(*previous_frame_token);

  CHECK(previous_web_frame);
  CHECK(previous_web_frame->IsWebLocalFrame());

  auto* previous_render_frame =
      RenderFrameImpl::FromWebFrame(previous_web_frame);
  CHECK(previous_render_frame);
  CHECK_EQ(previous_render_frame->is_for_nested_main_frame_,
           is_for_nested_main_frame_);

  return previous_web_frame->ToWebLocalFrame()->FrameWidget();
}

void RenderFrameImpl::WillDetach(blink::DetachReason detach_reason) {
  if (detach_reason == blink::DetachReason::kNavigation) {
    if (navigation_client_impl_ &&
        ShouldQueueNavigationsWhenPendingCommitRFHExists()) {
      navigation_client_impl_->ResetWithoutCancelling();
    }

    // Defer initializing the new widget until the previous Document has been
    // torn down. Script handles like unload dispatched during tear down can
    // access the compositor.
    if (provisional_frame_for_local_root_swap_) {
      provisional_frame_for_local_root_swap_->EnsureWidgetInitialized();
      provisional_frame_for_local_root_swap_ = nullptr;
    }
  }

  for (auto& observer : observers_)
    observer.WillDetach(detach_reason);

  // blink::AudioOutputIPCFactory::io_task_runner_ may be null in tests.
  auto& factory = blink::AudioOutputIPCFactory::GetInstance();
  if (factory.io_task_runner())
    factory.MaybeDeregisterRemoteFactory(GetWebFrame()->GetLocalFrameToken());

  // Send a state update before the frame is detached.
  SendUpdateState();
}

void RenderFrameImpl::FrameDetached() {
  TRACE_EVENT0("navigation", "RenderFrameImpl::FrameDetached");
  base::ScopedUmaHistogramTimer histogram_timer(
      "Navigation.RenderFrameImpl.FrameDetached");
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

  if (mhtml_body_loader_client_) {
    mhtml_body_loader_client_->Detach();
    mhtml_body_loader_client_.reset();
  }

  delete this;
  // Object is invalid after this point.
}

void RenderFrameImpl::DidChangeName(const blink::WebString& name) {
  if (GetWebFrame()->GetCurrentHistoryItem().IsNull()) {
    // Once a navigation has committed, the unique name must no longer change to
    // avoid breaking back/forward navigations: https://crbug.com/607205
    unique_name_helper_.UpdateName(name.Utf8());
  }
  GetFrameHost()->DidChangeName(name.Utf8(), unique_name_helper_.value());
}

void RenderFrameImpl::DidMatchCSS(
    const blink::WebVector<blink::WebString>& newly_matching_selectors,
    const blink::WebVector<blink::WebString>& stopped_matching_selectors) {
  for (auto& observer : observers_)
    observer.DidMatchCSS(newly_matching_selectors, stopped_matching_selectors);
}

bool RenderFrameImpl::ShouldReportDetailedMessageForSourceAndSeverity(
    blink::mojom::ConsoleMessageLevel log_level,
    const blink::WebString& source) {
  if (want_error_message_stack_trace_ &&
      log_level == blink::mojom::ConsoleMessageLevel::kError) {
    return true;
  }
  return GetContentClient()->renderer()->ShouldReportDetailedMessageForSource(
      source.Utf16());
}

void RenderFrameImpl::DidAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  if (ShouldReportDetailedMessageForSourceAndSeverity(message.level,
                                                      source_name)) {
    for (auto& observer : observers_) {
      observer.DetailedConsoleMessageAdded(
          message.text.Utf16(), source_name.Utf16(), stack_trace.Utf16(),
          source_line, message.level);
    }
  }
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

  // Set the code cache host earlier to allow fetching the code cache as soon as
  // possible.
  document_loader->SetCodeCacheHost(
      std::move(pending_code_cache_host_),
      std::move(pending_code_cache_host_for_background_));
}

void RenderFrameImpl::DidCommitNavigation(
    blink::WebHistoryCommitType commit_type,
    bool should_reset_browser_interface_broker,
    const blink::ParsedPermissionsPolicy& permissions_policy_header,
    const blink::DocumentPolicyFeatureState& document_policy_header) {
  TRACE_EVENT_WITH_FLOW0("navigation", "RenderFrameImpl::DidCommitNavigation",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CHECK_EQ(NavigationCommitState::kWillCommit, navigation_commit_state_);
  navigation_commit_state_ = NavigationCommitState::kDidCommit;

  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();
  DocumentState* document_state =
      DocumentState::FromDocumentLoader(document_loader);
  NavigationState* navigation_state = document_state->navigation_state();
  DCHECK(!navigation_state->WasWithinSameDocument());

  TRACE_EVENT2("navigation,benchmark,rail",
               "RenderFrameImpl::didStartProvisionalLoad", "frame_token",
               frame_token_, "url",
               document_loader->GetUrl().GetString().Utf8());

  // Install factories as early as possible - it needs to happen before the
  // newly committed document starts any subresource fetches.  In particular,
  // this needs to happen before invoking
  // RenderFrameObserver::ReadyToCommitNavigation below.
  if (pending_loader_factories_) {
    // Commits triggered by the browser process should always provide
    // |pending_loader_factories_|.
    SetLoaderFactoryBundle(std::move(pending_loader_factories_));
  }
  DCHECK(loader_factories_);
  DCHECK(loader_factories_->HasBoundDefaultFactory());

  // TODO(dgozman): call DidStartNavigation in various places where we call
  // CommitNavigation() on the frame.
  if (!navigation_state->was_initiated_in_this_frame()) {
    // Navigation initiated in this frame has been already reported in
    // BeginNavigation.
    for (auto& observer : observers_)
      observer.DidStartNavigation(document_loader->GetUrl(), std::nullopt);
  }

  for (auto& observer : observers_)
    observer.ReadyToCommitNavigation(document_loader);

  for (auto& observer : observers_)
    observer.DidCreateNewDocument();

  DVLOG(1) << "Committed provisional load: "
           << TrimURL(GetLoadingUrl().possibly_invalid_spec());
  TRACE_EVENT2("navigation,rail", "RenderFrameImpl::didCommitProvisionalLoad",
               "frame_token", frame_token_, "url",
               GetLoadingUrl().possibly_invalid_spec());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWaitForDebuggerOnNavigation)) {
    std::string renderer =
        base::StrCat({"Renderer url=\"",
                      TrimURL(GetLoadingUrl().possibly_invalid_spec()), "\""});
    content::WaitForDebugger(renderer);
  }

  // Generate a new embedding token on each document change.
  GetWebFrame()->SetEmbeddingToken(base::UnguessableToken::Create());

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker_receiver;

  // blink passes true when the new pipe needs to be bound.
  if (should_reset_browser_interface_broker) {
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
    browser_interface_broker_receiver =
        frame_->GetBrowserInterfaceBroker().Reset(
            agent_scheduling_group_->agent_group_scheduler()
                .DefaultTaskRunner());

    // blink::AudioOutputIPCFactory::io_task_runner_ may be null in tests.
    auto& factory = blink::AudioOutputIPCFactory::GetInstance();
    if (factory.io_task_runner()) {
      // The RendererAudioOutputStreamFactory must be readily accessible on the
      // IO thread when it's needed, because the main thread may block while
      // waiting for the factory call to finish on the IO thread, so if we tried
      // to lazily initialize it, we could deadlock.
      //
      // TODO(crbug.com/40495144): Still, it is odd for one specific
      // factory to be registered here, make this a RenderFrameObserver.
      // code.
      factory.MaybeDeregisterRemoteFactory(GetWebFrame()->GetLocalFrameToken());
      factory.RegisterRemoteFactory(GetWebFrame()->GetLocalFrameToken(),
                                    frame_->GetBrowserInterfaceBroker());
    }

    // If the request for |audio_input_stream_factory_| is in flight when
    // |browser_interface_broker_proxy_| is reset, it will be silently dropped.
    // We reset |audio_input_stream_factory_| to force a new mojo request to be
    // sent the next time it's used. See https://crbug.com/795258 for
    // implementing a nicer solution.
    audio_input_stream_factory_.reset();

    render_accessibility_manager_->CloseConnection();
  }

  // Notify the MediaPermissionDispatcher that its connection will be closed
  // due to a navigation to a different document.
  if (media_permission_dispatcher_)
    media_permission_dispatcher_->OnNavigation();

  ui::PageTransition transition =
      GetTransitionType(frame_->GetDocumentLoader(), IsMainFrame(),
                        GetWebView()->IsFencedFrameRoot());

  // TODO(crbug.com/40092527): Turn this into a DCHECK for origin equality when
  // the linked bug is fixed. Currently sometimes the browser and renderer
  // disagree on the origin during commit navigation.
  if (pending_cookie_manager_info_ &&
      pending_cookie_manager_info_->origin ==
          url::Origin(frame_->GetDocument().GetSecurityOrigin())) {
    frame_->GetDocument().SetCookieManager(
        std::move(pending_cookie_manager_info_->cookie_manager));
  }

  // TODO(crbug.com/40092527): Turn this into a DCHECK for origin equality when
  // the linked bug is fixed. Currently sometimes the browser and renderer
  // disagree on the origin during commit navigation.
  if (pending_storage_info_ &&
      original_storage_key_.origin() ==
          url::Origin(frame_->GetDocument().GetSecurityOrigin())) {
    if (pending_storage_info_->local_storage_area) {
      frame_->SetLocalStorageArea(
          std::move(pending_storage_info_->local_storage_area));
    }
    if (pending_storage_info_->session_storage_area) {
      frame_->SetSessionStorageArea(
          std::move(pending_storage_info_->session_storage_area));
    }
  }

  DidCommitNavigationInternal(
      commit_type, transition, permissions_policy_header,
      document_policy_header,
      should_reset_browser_interface_broker
          ? mojom::DidCommitProvisionalLoadInterfaceParams::New(
                std::move(browser_interface_broker_receiver))
          : nullptr,
      nullptr /* same_document_params */, GetWebFrame()->GetEmbeddingToken());

  // If we end up reusing this WebRequest (for example, due to a #ref click),
  // we don't want the transition type to persist.  Just clear it.
  navigation_state->set_transition_type(ui::PAGE_TRANSITION_LINK);

  // Check whether we have new encoding name.
  UpdateEncoding(frame_, frame_->View()->PageEncoding().Utf8());

  NotifyObserversOfNavigationCommit(transition);

  document_state->clear_navigation_state();

  ResetMembersUsedForDurationOfCommit();
}

void RenderFrameImpl::DidCommitDocumentReplacementNavigation(
    blink::WebDocumentLoader* document_loader) {
  DocumentState::FromDocumentLoader(document_loader)
      ->set_navigation_state(NavigationState::CreateForSynchronousCommit());
  // TODO(crbug.com/40581836): figure out which of the following observer
  // calls are necessary, if any.
  for (auto& observer : observers_)
    observer.DidStartNavigation(document_loader->GetUrl(), std::nullopt);
  for (auto& observer : observers_)
    observer.ReadyToCommitNavigation(document_loader);
  for (auto& observer : observers_)
    observer.DidCreateNewDocument();
  ui::PageTransition transition = GetTransitionType(
      document_loader, IsMainFrame(), GetWebView()->IsFencedFrameRoot());
  NotifyObserversOfNavigationCommit(transition);
}

void RenderFrameImpl::DidClearWindowObject() {
  TRACE_EVENT_WITH_FLOW0("navigation", "RenderFrameImpl::DidClearWindowObject",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (enabled_bindings_.Has(BindingsPolicyValue::kWebUi)) {
    WebUIExtension::Install(frame_);
  }

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // DOM automation bindings that allows the JS content to send JSON-encoded
  // data back to automation in the browser process. By default this isn't
  // allowed unless the process has been started up with the --dom-automation
  // switch.
  if (command_line.HasSwitch(switches::kDomAutomationController))
    DomAutomationController::Install(this, frame_);

  // Bindings that allows the JS content to retrieve a variety of internal
  // metrics. By default this isn't allowed unless the process has been started
  // with the --enable-stats-collection-bindings switch.
  if (command_line.HasSwitch(switches::kStatsCollectionController))
    StatsCollectionController::Install(frame_);

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
  // Wait until any RenderFrameObservers for this frame have a chance to be
  // constructed.
  if (!initialized_)
    return;
  GetContentClient()->renderer()->RunScriptsAtDocumentStart(this);
  // Do not use |this|! ContentClient might have deleted them by now!
}

void RenderFrameImpl::DidReceiveTitle(const blink::WebString& title) {
  // Ignore all but top level navigations.
  if (!frame_->Parent() && !title.IsEmpty()) {
    base::trace_event::TraceLog::GetInstance()->UpdateProcessLabel(
        process_label_id_, title.Utf8());
  } else {
    // Set process title for sub-frames and title-less frames in traces.
    GURL loading_url = GetLoadingUrl();
    if (!loading_url.host().empty() &&
        loading_url.scheme() != url::kFileScheme) {
      std::string frame_title;
      if (frame_->Parent()) {
        frame_title += "Subframe: ";
      }
      frame_title += loading_url.DeprecatedGetOriginAsURL().spec();
      base::trace_event::TraceLog::GetInstance()->UpdateProcessLabel(
          process_label_id_, frame_title);
    }
  }

  // Also check whether we have new encoding name.
  UpdateEncoding(frame_, frame_->View()->PageEncoding().Utf8());
}

void RenderFrameImpl::DidDispatchDOMContentLoadedEvent() {
  TRACE_EVENT1("navigation,benchmark,rail",
               "RenderFrameImpl::DidDispatchDOMContentLoadedEvent",
               "frame_token", frame_token_);
  for (auto& observer : observers_)
    observer.DidDispatchDOMContentLoadedEvent();

  // Check whether we have new encoding name.
  UpdateEncoding(frame_, frame_->View()->PageEncoding().Utf8());
}

void RenderFrameImpl::RunScriptsAtDocumentReady() {
  DCHECK(initialized_);
  GetContentClient()->renderer()->RunScriptsAtDocumentEnd(this);
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
               "frame_token", frame_token_);
  if (!frame_->Parent()) {
    TRACE_EVENT_INSTANT1("WebCore,benchmark,rail", "LoadFinished",
                         TRACE_EVENT_SCOPE_PROCESS, "isOutermostMainFrame",
                         frame_->IsOutermostMainFrame());
  }

  for (auto& observer : observers_)
    observer.DidFinishLoad();
}

void RenderFrameImpl::DidFinishLoadForPrinting() {
  for (auto& observer : observers_)
    observer.DidFinishLoadForPrinting();
}

void RenderFrameImpl::DidFinishSameDocumentNavigation(
    blink::WebHistoryCommitType commit_type,
    bool is_synchronously_committed,
    blink::mojom::SameDocumentNavigationType same_document_navigation_type,
    bool is_client_redirect,
    const std::optional<blink::SameDocNavigationScreenshotDestinationToken>&
        screenshot_destination) {
  TRACE_EVENT1("navigation,rail",
               "RenderFrameImpl::didFinishSameDocumentNavigation",
               "frame_token", frame_token_);
  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();
  DocumentState* document_state =
      DocumentState::FromDocumentLoader(document_loader);
  if (is_synchronously_committed) {
    document_state->set_navigation_state(
        NavigationState::CreateForSynchronousCommit());
  }
  document_state->navigation_state()->set_was_within_same_document(true);

  ui::PageTransition transition = GetTransitionType(
      document_loader, IsMainFrame(), GetWebView()->IsFencedFrameRoot());
  auto same_document_params =
      mojom::DidCommitSameDocumentNavigationParams::New();
  same_document_params->same_document_navigation_type =
      same_document_navigation_type;
  same_document_params->is_client_redirect = is_client_redirect;
  same_document_params->started_with_transient_activation =
      document_loader->LastNavigationHadTransientUserActivation();
  same_document_params->should_replace_current_entry =
      document_loader->ReplacesCurrentHistoryItem();
  same_document_params->navigation_entry_screenshot_destination =
      screenshot_destination;

  DidCommitNavigationInternal(
      commit_type, transition,
      blink::ParsedPermissionsPolicy(),     // permissions_policy_header
      blink::DocumentPolicyFeatureState(),  // document_policy_header
      nullptr,                              // interface_params
      std::move(same_document_params),
      std::nullopt  // embedding_token
  );

  // If we end up reusing this WebRequest (for example, due to a #ref click),
  // we don't want the transition type to persist.  Just clear it.
  document_state->navigation_state()->set_transition_type(
      ui::PAGE_TRANSITION_LINK);

  for (auto& observer : observers_)
    observer.DidFinishSameDocumentNavigation();

  document_state->clear_navigation_state();
}

void RenderFrameImpl::DidFailAsyncSameDocumentCommit() {
  // This is called when the Navigation API deferred a same-document commit,
  // then fails the navigation without committing, so that we can run the
  // callback if this commit was browser-initiated. If the commit is aborted
  // due to frame detach or another navigation preempting it, NavigationState's
  // destructor will run the callback instead.
  DocumentState* document_state =
      DocumentState::FromDocumentLoader(frame_->GetDocumentLoader());
  if (NavigationState* navigation_state = document_state->navigation_state()) {
    navigation_state->RunCommitSameDocumentNavigationCallback(
        blink::mojom::CommitResult::Aborted);
    document_state->clear_navigation_state();
  }
}

void RenderFrameImpl::WillFreezePage() {
  // Make sure browser has the latest info before the page is frozen. If the
  // page goes into the back-forward cache it could be evicted and some of the
  // updates lost.
  SendUpdateState();
}

void RenderFrameImpl::DidOpenDocumentInputStream(const blink::WebURL& url) {
  GURL filtered_url(url);
  if (!IsValidCommitUrl(filtered_url)) {
    filtered_url = GURL(kBlockedURL);
  }
  GetFrameHost()->DidOpenDocumentInputStream(filtered_url);
}

void RenderFrameImpl::DidSetPageLifecycleState(bool restoring_from_bfcache) {
  for (auto& observer : observers_)
    observer.DidSetPageLifecycleState(restoring_from_bfcache);
}

void RenderFrameImpl::NotifyCurrentHistoryItemChanged() {
  SendUpdateState();
}

void RenderFrameImpl::DidUpdateCurrentHistoryItem() {
  StartDelayedSyncTimer();
}

void RenderFrameImpl::StartDelayedSyncTimer() {
  base::TimeDelta delay;
  if (send_content_state_immediately_) {
    SendUpdateState();
    return;
  } else if (GetWebView()->GetVisibilityState() !=
             blink::mojom::PageVisibilityState::kVisible)
    delay = kDelaySecondsForContentStateSyncHidden;
  else
    delay = kDelaySecondsForContentStateSync;

  if (delayed_state_sync_timer_.IsRunning()) {
    // The timer is already running. If the delay of the timer matches the
    // amount we want to delay by, then return. Otherwise stop the timer so that
    // it gets started with the right delay.
    if (delayed_state_sync_timer_.GetCurrentDelay() == delay)
      return;
    delayed_state_sync_timer_.Stop();
  }
  delayed_state_sync_timer_.Start(FROM_HERE, delay, this,
                                  &RenderFrameImpl::SendUpdateState);
}

bool RenderFrameImpl::SwapOutAndDeleteThis(
    bool is_loading,
    blink::mojom::FrameReplicationStatePtr replicated_frame_state,
    const blink::RemoteFrameToken& proxy_frame_token,
    blink::mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
    blink::mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces) {
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::SwapOutAndDeleteThis",
               "frame_token", frame_token_);
  DCHECK(!base::RunLoop::IsNestedOnCurrentThread());

  // Create a WebRemoteFrame so we can pass it into `Swap`.
  blink::WebRemoteFrame* remote_frame = blink::WebRemoteFrame::Create(
      frame_->GetTreeScopeType(), proxy_frame_token);

  blink::WebView* web_view = GetWebView();
  bool is_main_frame = is_main_frame_;

  // The swap call deletes this RenderFrame via FrameDetached.  Do not access
  // any members after this call.
  //
  // TODO(creis): WebFrame::swap() can return false.  Most of those cases
  // should be due to the frame being detached during unload (in which case
  // the necessary cleanup has happened anyway), but it might be possible for
  // it to return false without detaching.
  //
  // This executes the unload handlers on this frame and its local descendants.
  bool success =
      frame_->Swap(remote_frame, std::move(remote_frame_interfaces->frame_host),
                   std::move(remote_frame_interfaces->frame_receiver),
                   std::move(replicated_frame_state));

  // WARNING: Do not access 'this' past this point!

  if (is_main_frame) {
    // Main frames should always swap successfully because there is no parent
    // frame to cause them to become detached.
    DCHECK(success);

    // The `blink::RemoteFrame` being swapped in here has now been attached to
    // the Page as its main frame and properly initialized by the
    // WebFrame::Swap() call, so we can call WebView's
    // DidAttachRemoteMainFrame().
    web_view->DidAttachRemoteMainFrame(
        std::move(remote_main_frame_interfaces->main_frame_host),
        std::move(remote_main_frame_interfaces->main_frame));
  }

  if (!success) {
    // The swap can fail when the frame is detached during swap (this can
    // happen while running the unload handlers). When that happens, delete
    // the proxy.
    remote_frame->Close();
    return false;
  }

  if (is_loading)
    remote_frame->DidStartLoading();

  return true;
}

base::UnguessableToken RenderFrameImpl::GetDevToolsFrameToken() {
  return devtools_frame_token_;
}

void RenderFrameImpl::AbortClientNavigation(bool for_new_navigation) {
  CHECK(in_frame_tree_);
  is_requesting_navigation_ = false;
  if (mhtml_body_loader_client_) {
    mhtml_body_loader_client_->Detach();
    mhtml_body_loader_client_.reset();
  }
  NotifyObserversOfFailedProvisionalLoad();
  // See comment in header for more information of how navigation cleanup works.
  // Note: This might not actually cancel the navigation if the navigation is
  // already in the process of committing to a different RenderFrame.

  if (for_new_navigation) {
    navigation_client_impl_->ResetForNewNavigation(
        /*is_duplicate_navigation=*/false);
  } else {
    navigation_client_impl_->ResetForAbort();
  }
  navigation_client_impl_.reset();
}

void RenderFrameImpl::DidChangeSelection(bool is_empty_selection,
                                         blink::SyncCondition force_sync) {
  if (!GetLocalRootWebFrameWidget()->HandlingInputEvent() &&
      !GetLocalRootWebFrameWidget()->HandlingSelectRange())
    return;

  if (is_empty_selection)
    selection_text_.clear();

  // UpdateTextInputState should be called before SyncSelectionIfRequired.
  // UpdateTextInputState may send TextInputStateChanged to notify the focus
  // was changed, and SyncSelectionIfRequired may send SelectionChanged
  // to notify the selection was changed.  Focus change should be notified
  // before selection change.
  GetLocalRootWebFrameWidget()->UpdateTextInputState();
  SyncSelectionIfRequired(force_sync);
}

void RenderFrameImpl::OnMainFrameIntersectionChanged(
    const gfx::Rect& main_frame_intersection_rect) {
  if (main_frame_intersection_rect != main_frame_intersection_rect_) {
    main_frame_intersection_rect_ = main_frame_intersection_rect;
    for (auto& observer : observers_) {
      observer.OnMainFrameIntersectionChanged(main_frame_intersection_rect);
    }
  }
}

void RenderFrameImpl::OnMainFrameViewportRectangleChanged(
    const gfx::Rect& main_frame_viewport_rect) {
  if (main_frame_viewport_rect != main_frame_viewport_rect_) {
    main_frame_viewport_rect_ = main_frame_viewport_rect;
    for (auto& observer : observers_) {
      observer.OnMainFrameViewportRectangleChanged(main_frame_viewport_rect);
    }
  }
}

void RenderFrameImpl::OnMainFrameImageAdRectangleChanged(
    int element_id,
    const gfx::Rect& image_ad_rect) {
  for (auto& observer : observers_) {
    observer.OnMainFrameImageAdRectangleChanged(element_id, image_ad_rect);
  }
}

void RenderFrameImpl::OnOverlayPopupAdDetected() {
  for (auto& observer : observers_) {
    observer.OnOverlayPopupAdDetected();
  }
}

void RenderFrameImpl::OnLargeStickyAdDetected() {
  for (auto& observer : observers_) {
    observer.OnLargeStickyAdDetected();
  }
}

void RenderFrameImpl::FinalizeRequest(blink::WebURLRequest& request) {
  // This method is called for subresources, while transition type is
  // a navigation concept. We pass ui::PAGE_TRANSITION_LINK as default one.
  FinalizeRequestInternal(request, /*for_outermost_main_frame=*/false,
                          ui::PAGE_TRANSITION_LINK);
  for (auto& observer : observers_) {
    // TODO(sky): rename to FinalizeRequest.
    observer.WillSendRequest(request);
  }
}

std::optional<blink::WebURL> RenderFrameImpl::WillSendRequest(
    const blink::WebURL& target,
    const blink::WebSecurityOrigin& security_origin,
    const net::SiteForCookies& site_for_cookies,
    ForRedirect for_redirect,
    const blink::WebURL& upstream_url) {
  return WillSendRequestInternal(target, security_origin, site_for_cookies,
                                 for_redirect, upstream_url,
                                 ui::PAGE_TRANSITION_LINK);
}

std::optional<blink::WebURL> RenderFrameImpl::WillSendRequestInternal(
    const blink::WebURL& target,
    const blink::WebSecurityOrigin& security_origin,
    const net::SiteForCookies& site_for_cookies,
    ForRedirect for_redirect,
    const blink::WebURL& upstream_url,
    ui::PageTransition transition_type) {
  std::optional<blink::WebURL> adjusted = ApplyFilePathAlias(target);

  GURL new_url;
  std::optional<url::Origin> initiator_origin =
      security_origin.IsNull() ? std::optional<url::Origin>()
                               : std::optional<url::Origin>(security_origin);
  GetContentClient()->renderer()->WillSendRequest(
      frame_, transition_type, upstream_url,
      adjusted.has_value() ? *adjusted : target, site_for_cookies,
      base::OptionalToPtr(initiator_origin), &new_url);
  if (!new_url.is_empty()) {
    return WebURL(new_url);
  }
  return adjusted;
}

void RenderFrameImpl::FinalizeRequestInternal(
    blink::WebURLRequest& request,
    bool for_outermost_main_frame,
    ui::PageTransition transition_type) {
  if (GetWebView()->GetRendererPreferences().enable_do_not_track) {
    request.SetHttpHeaderField(
        blink::WebString::FromUTF8(blink::kDoNotTrackHeader), "1");
  }

  // The request's extra data may indicate that we should set a custom user
  // agent. This needs to be done here, after WebKit is through with setting the
  // user agent on its own.
  WebString custom_user_agent;
  if (request.GetURLRequestExtraData()) {
    blink::WebURLRequestExtraData* old_request_extra_data =
        static_cast<blink::WebURLRequestExtraData*>(
            request.GetURLRequestExtraData().get());

    custom_user_agent = old_request_extra_data->custom_user_agent();
    if (!custom_user_agent.IsNull()) {
      if (custom_user_agent.IsEmpty())
        request.ClearHttpHeaderField("User-Agent");
      else
        request.SetHttpHeaderField("User-Agent", custom_user_agent);
    }
  }

  if (!request.GetURLRequestExtraData()) {
    request.SetURLRequestExtraData(
        base::MakeRefCounted<blink::WebURLRequestExtraData>());
  }
  auto* url_request_extra_data = static_cast<blink::WebURLRequestExtraData*>(
      request.GetURLRequestExtraData().get());
  url_request_extra_data->set_custom_user_agent(custom_user_agent);
  url_request_extra_data->set_is_outermost_main_frame(IsMainFrame() &&
                                                      !IsInFencedFrameTree());
  url_request_extra_data->set_transition_type(transition_type);
  bool is_for_no_state_prefetch =
      GetContentClient()->renderer()->IsPrefetchOnly(this);
  url_request_extra_data->set_is_for_no_state_prefetch(
      is_for_no_state_prefetch);
  url_request_extra_data->set_allow_cross_origin_auth_prompt(
      GetWebView()->GetRendererPreferences().allow_cross_origin_auth_prompt);

  request.SetDownloadToNetworkCacheOnly(is_for_no_state_prefetch &&
                                        !for_outermost_main_frame);

  request.SetHasUserGesture(frame_->HasTransientUserActivation());

  if (!GetWebView()->GetRendererPreferences().enable_referrers) {
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
    const url::SchemeHostPort& final_response_url,
    int request_id,
    network::mojom::URLResponseHeadPtr response_head,
    network::mojom::RequestDestination request_destination,
    bool is_ad_resource) {
  for (auto& observer : observers_) {
    observer.DidStartResponse(final_response_url, request_id, *response_head,
                              request_destination, is_ad_resource);
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

void RenderFrameImpl::DidObserveUserInteraction(
    base::TimeTicks max_event_start,
    base::TimeTicks max_event_queued_main_thread,
    base::TimeTicks max_event_commit_finish,
    base::TimeTicks max_event_end,
    blink::UserInteractionType interaction_type,
    uint64_t interaction_offset) {
  for (auto& observer : observers_) {
    observer.DidObserveUserInteraction(
        max_event_start, max_event_queued_main_thread, max_event_commit_finish,
        max_event_end, interaction_type, interaction_offset);
  }
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

void RenderFrameImpl::DidObserveJavaScriptFrameworks(
    const blink::JavaScriptFrameworkDetectionResult& result) {
  for (auto& observer : observers_) {
    observer.DidObserveJavaScriptFrameworks(result);
  }
}

void RenderFrameImpl::DidObserveSubresourceLoad(
    const blink::SubresourceLoadMetrics& subresource_load_metrics) {
  for (auto& observer : observers_)
    observer.DidObserveSubresourceLoad(subresource_load_metrics);
}

void RenderFrameImpl::DidObserveNewFeatureUsage(
    const blink::UseCounterFeature& feature) {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "RenderFrameImpl::DidObserveNewFeatureUsage",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  for (auto& observer : observers_)
    observer.DidObserveNewFeatureUsage(feature);
}

void RenderFrameImpl::DidObserveSoftNavigation(
    blink::SoftNavigationMetrics metrics) {
  for (auto& observer : observers_) {
    observer.DidObserveSoftNavigation(metrics);
  }
}

void RenderFrameImpl::DidObserveLayoutShift(double score,
                                            bool after_input_or_scroll) {
  for (auto& observer : observers_)
    observer.DidObserveLayoutShift(score, after_input_or_scroll);
}

void RenderFrameImpl::DidCreateScriptContext(v8::Local<v8::Context> context,
                                             int world_id) {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "RenderFrameImpl::DidCreateScriptContext",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  v8::MicrotasksScope microtasks(GetAgentGroupScheduler().Isolate(),
                                 context->GetMicrotaskQueue(),
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  if (((enabled_bindings_.Has(BindingsPolicyValue::kMojoWebUi)) ||
       enable_mojo_js_bindings_) &&
      IsMainFrame() && world_id == ISOLATED_WORLD_ID_GLOBAL) {
    // We only allow these bindings to be installed when creating the main
    // world context of the main frame.
    blink::WebV8Features::EnableMojoJS(context, true);

    if (mojo_js_features_) {
      if (mojo_js_features_->file_system_access)
        blink::WebV8Features::EnableMojoJSFileSystemAccessHelper(context, true);
    }
  }

  if (world_id == ISOLATED_WORLD_ID_GLOBAL &&
      mojo_js_interface_broker_.is_valid()) {
    // MojoJS interface broker can be enabled on subframes, and will limit the
    // interfaces JavaScript can request to those provided in the broker.
    blink::WebV8Features::EnableMojoJSAndUseBroker(
        context, std::move(mojo_js_interface_broker_));
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
  StartDelayedSyncTimer();

  for (auto& observer : observers_)
    observer.DidChangeScrollOffset();
}

blink::WebMediaStreamDeviceObserver*
RenderFrameImpl::MediaStreamDeviceObserver() {
  if (!web_media_stream_device_observer_)
    InitializeMediaStreamDeviceObserver();
  return web_media_stream_device_observer_.get();
}

blink::WebEncryptedMediaClient* RenderFrameImpl::EncryptedMediaClient() {
  return media_factory_.EncryptedMediaClient();
}

blink::WebString RenderFrameImpl::UserAgentOverride() {
  if (ShouldUseUserAgentOverride()) {
    return WebString::FromUTF8(GetWebView()
                                   ->GetRendererPreferences()
                                   .user_agent_override.ua_string_override);
  }

  return blink::WebString();
}

std::optional<blink::UserAgentMetadata>
RenderFrameImpl::UserAgentMetadataOverride() {
  if (ShouldUseUserAgentOverride()) {
    return GetWebView()
        ->GetRendererPreferences()
        .user_agent_override.ua_metadata_override;
  }
  return std::nullopt;
}

bool RenderFrameImpl::ShouldUseUserAgentOverride() const {
  auto* web_view = GetWebView();
  // TODO(nasko): When the top-level frame is remote, there is no
  // WebDocumentLoader associated with it, so the checks below are not valid.
  // Temporarily return early and fix properly as part of
  // https://crbug.com/426555.
  if (web_view->MainFrame()->IsWebRemoteFrame())
    return false;
  const WebLocalFrame* main_frame = web_view->MainFrame()->ToWebLocalFrame();

  WebDocumentLoader* document_loader = main_frame->GetDocumentLoader();
  DocumentState* document_state =
      document_loader ? DocumentState::FromDocumentLoader(document_loader)
                      : nullptr;
  return document_state && document_state->is_overriding_user_agent();
}

blink::mojom::RendererAudioInputStreamFactory*
RenderFrameImpl::GetAudioInputStreamFactory() {
  if (!audio_input_stream_factory_)
    GetBrowserInterfaceBroker().GetInterface(
        audio_input_stream_factory_.BindNewPipeAndPassReceiver(
            agent_scheduling_group_->agent_group_scheduler()
                .DefaultTaskRunner()));
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

  render_accessibility_manager_->GetRenderAccessibilityImpl()->HandleAXEvent(
      event);
}

bool RenderFrameImpl::SendAccessibilitySerialization(
    std::vector<ui::AXTreeUpdate> updates,
    std::vector<ui::AXEvent> events,
    ui::AXLocationAndScrollUpdates location_and_scroll_updates,
    bool had_load_complete_messages) {
  // This function should never be called from a11y unless it's enabled.
  CHECK(IsAccessibilityEnabled());

  return render_accessibility_manager_->GetRenderAccessibilityImpl()
      ->SendAccessibilitySerialization(std::move(updates), std::move(events),
                                       std::move(location_and_scroll_updates),
                                       had_load_complete_messages);
}

void RenderFrameImpl::AddObserver(RenderFrameObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderFrameImpl::RemoveObserver(RenderFrameObserver* observer) {
  observer->RenderFrameGone();
  observers_.RemoveObserver(observer);
}

void RenderFrameImpl::OnDroppedNavigation() {
  is_requesting_navigation_ = false;
  frame_->DidDropNavigation();
}

void RenderFrameImpl::WasHidden() {
  frame_->WasHidden();
  for (auto& observer : observers_)
    observer.WasHidden();

#if BUILDFLAG(ENABLE_PPAPI)
  for (PepperPluginInstanceImpl* plugin : active_pepper_instances_) {
    plugin->PageVisibilityChanged(false);
  }
#endif  // BUILDFLAG(ENABLE_PPAPI)
}

void RenderFrameImpl::WasShown() {
  frame_->WasShown();
  for (auto& observer : observers_)
    observer.WasShown();

#if BUILDFLAG(ENABLE_PPAPI)
  for (PepperPluginInstanceImpl* plugin : active_pepper_instances_) {
    plugin->PageVisibilityChanged(true);
  }
#endif  // BUILDFLAG(ENABLE_PPAPI)
}

void RenderFrameImpl::OnFrameVisibilityChanged(
    blink::mojom::FrameVisibility render_status) {
  for (auto& observer : observers_) {
    observer.OnFrameVisibilityChanged(render_status);
  }
}

bool RenderFrameImpl::IsMainFrame() {
  return is_main_frame_;
}

bool RenderFrameImpl::IsInFencedFrameTree() const {
  return GetWebFrame()->IsInFencedFrameTree();
}

bool RenderFrameImpl::IsHidden() {
  CHECK(GetWebFrame()->IsProvisional() || GetLocalRootWebFrameWidget())
      << "Only provisional frames are created with no widget";
  if (!GetLocalRootWebFrameWidget()) {
    return true;
  }
  return GetLocalRootWebFrameWidget()->IsHidden();
}

bool RenderFrameImpl::IsLocalRoot() const {
  return !(frame_->Parent() && frame_->Parent()->IsWebLocalFrame());
}

const RenderFrameImpl* RenderFrameImpl::GetLocalRoot() const {
  return IsLocalRoot() ? this
                       : RenderFrameImpl::FromWebFrame(frame_->LocalRoot());
}

base::WeakPtr<RenderFrameImpl> RenderFrameImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

mojom::DidCommitProvisionalLoadParamsPtr
RenderFrameImpl::MakeDidCommitProvisionalLoadParams(
    blink::WebHistoryCommitType commit_type,
    ui::PageTransition transition,
    const blink::ParsedPermissionsPolicy& permissions_policy_header,
    const blink::DocumentPolicyFeatureState& document_policy_header,
    const std::optional<base::UnguessableToken>& embedding_token) {
  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();
  const WebURLResponse& response = document_loader->GetWebResponse();

  DocumentState* document_state =
      DocumentState::FromDocumentLoader(frame_->GetDocumentLoader());
  NavigationState* navigation_state = document_state->navigation_state();

  auto params = mojom::DidCommitProvisionalLoadParams::New();
  params->http_status_code = response.HttpStatusCode();
  params->url_is_unreachable = document_loader->HasUnreachableURL();
  params->method = "GET";
  params->post_id = -1;
  params->embedding_token = embedding_token;
  params->origin_calculation_debug_info =
      document_loader->OriginCalculationDebugInfo().Utf8();

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
       document_loader->ReplacesCurrentHistoryItem() &&
       !navigation_state->WasWithinSameDocument());

  WebDocument frame_document = frame_->GetDocument();
  // Set the origin of the frame.  This will be replicated to the corresponding
  // RenderFrameProxies in other processes.
  WebSecurityOrigin frame_origin = frame_document.GetSecurityOrigin();
  params->origin = frame_origin;

  params->permissions_policy_header = permissions_policy_header;
  params->document_policy_header = document_policy_header;

  params->insecure_request_policy = frame_->GetInsecureRequestPolicy();
  params->insecure_navigations_set =
      frame_->GetInsecureRequestToUpgrade().ReleaseVector();

  params->has_potentially_trustworthy_unique_origin =
      frame_origin.IsOpaque() && frame_origin.IsPotentiallyTrustworthy();

  // Set the URL to be displayed in the browser UI to the user. Note this might
  // be different than the URL actually used in the DocumentLoader (see comments
  // in GetLoadingUrl() and MaybeGetOverriddenURL()). This might not be the URL
  // actually shown to the user as well, since the browser has additional logic
  // for virtual URLs (e.g. the "history URL" is shown for loadDataWithBaseURL
  // instead of this URL).
  params->url = GetLoadingUrl();
  // Note: since we get the security origin from the `frame_document`, we also
  // get the base url from it too.
  if (params->url.IsAboutBlank() || params->url.IsAboutSrcdoc()) {
    GURL base_url = frame_document.BaseURL();
    // Only pass the base URL if it is valid and can be serialized by Mojo.
    if (base_url.is_valid() &&
        base_url.possibly_invalid_spec().length() <= url::kMaxURLChars) {
      params->initiator_base_url = base_url;
    }
  }

  // Don't send commit URLs to the browser that are known to be unsupported
  // (e.g., would not pass RenderProcessHostImpl::FilterURL). This is applied
  // after the initiator_base_url check above to avoid passing a base URL when
  // the URL was not about:blank but was rewritten to about:blank#blocked.
  if (!IsValidCommitUrl(params->url)) {
    params->url = GURL(kBlockedURL);
  }

  // TODO(crbug.com/40161149): Reconsider how we calculate
  // should_update_history.
  params->should_update_history =
      !document_loader->HasUnreachableURL() && response.HttpStatusCode() != 404;

  // Make navigation state a part of the DidCommitProvisionalLoad message so
  // that committed entry has it at all times.  Send a single HistoryItem for
  // this frame, rather than the whole tree.  It will be stored in the
  // corresponding FrameNavigationEntry.
  const WebHistoryItem& item = GetWebFrame()->GetCurrentHistoryItem();
  params->page_state = GetWebFrame()->CurrentHistoryItemToPageState();

  params->method = document_loader->HttpMethod().Latin1();
  if (params->method == "POST")
    params->post_id = ExtractPostId(item);

  params->item_sequence_number = item.ItemSequenceNumber();
  params->document_sequence_number = item.DocumentSequenceNumber();
  params->navigation_api_key = item.GetNavigationApiKey().Utf8();

  // Note that the value of `referrer` will be overwritten in the browser with a
  // browser-calculated value in most cases. The exceptions are
  // renderer-initated same-document navigations and the synchronous about:blank
  // commit (because the browser doesn't know anything about those navigations).
  // In those cases, the referrer policy component will still be overwritten in
  // the browser, because this navigation won't change it and the browser
  // already had access to the previous one. Send ReferrerPolicy::kDefault as a
  // placeholder.
  // TODO(crbug.com/40150370): Remove `referrer` from
  // DidCommitProvisionalLoadParams.
  params->referrer = blink::mojom::Referrer::New(
      blink::WebStringToGURL(document_loader->Referrer()),
      network::mojom::ReferrerPolicy::kDefault);

  if (!frame_->Parent()) {
    // Top-level navigation.

    // Update contents MIME type for main frame.
    params->contents_mime_type =
        document_loader->GetWebResponse().MimeType().Utf8();

    params->transition = transition;
    // Check that if we are in a fenced frame tree then we must have
    // PAGE_TRANSITION_AUTO_SUBFRAME. Otherwise we are a main frame
    // and should have valid main frame values.
    if (GetWebView()->IsFencedFrameRoot()) {
      DCHECK(ui::PageTransitionCoreTypeIs(params->transition,
                                          ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    } else {
      DCHECK(ui::PageTransitionIsMainFrame(params->transition));
    }

    // If the page contained a client redirect (meta refresh, document.loc...),
    // set the transition appropriately.
    if (document_loader->IsClientRedirect()) {
      params->transition = ui::PageTransitionFromInt(
          params->transition | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
    }

    // Send the user agent override back.
    params->is_overriding_user_agent =
        document_state->is_overriding_user_agent();

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

  bool requires_universal_access = false;
  const bool file_scheme_with_universal_access =
      params->origin.scheme() == url::kFileScheme &&
      GetBlinkPreferences().allow_universal_access_from_file_urls;

  // Standard URLs must match the reported origin, when it is not unique.
  // This check is very similar to RenderFrameHostImpl::CanCommitOrigin, but
  // adapted to the renderer process side.
  if (!params->origin.opaque() && params->url.IsStandard() &&
      GetBlinkPreferences().web_security_enabled) {
    if (!params->origin.IsSameOriginWith(params->url)) {
      // Exclude file: URLs when settings allow them access any origin.
      if (!file_scheme_with_universal_access) {
        SCOPED_CRASH_KEY_STRING256("MakeDCPLParams", "mismatched_url",
                                   params->url.possibly_invalid_spec());
        SCOPED_CRASH_KEY_STRING256("MakeDCPLParams", "mismatched_origin",
                                   params->origin.GetDebugString());
        CHECK(false) << " url:" << params->url << " origin:" << params->origin;
      } else {
        requires_universal_access = true;
      }
    }
    if (file_scheme_with_universal_access) {
      base::UmaHistogramBoolean(
          "Android.WebView.UniversalAccess.OriginUrlMismatchInRenderFrame",
          requires_universal_access);
    }
  }
  params->request_id = document_state->request_id();

  params->unload_start =
      GetWebFrame()->PerformanceMetricsForNestedContexts().UnloadStart();
  params->unload_end =
      GetWebFrame()->PerformanceMetricsForNestedContexts().UnloadEnd();
  params->commit_navigation_start = navigation_state->commit_start_time();
  params->commit_navigation_end = GetWebFrame()
                                      ->PerformanceMetricsForNestedContexts()
                                      .CommitNavigationEnd();

  return params;
}

void RenderFrameImpl::UpdateNavigationHistory(
    blink::WebHistoryCommitType commit_type) {
  NavigationState* navigation_state =
      DocumentState::FromDocumentLoader(frame_->GetDocumentLoader())
          ->navigation_state();
  const blink::mojom::CommitNavigationParams& commit_params =
      navigation_state->commit_params();

  GetWebFrame()->UpdateCurrentHistoryItem();
  GetWebFrame()->SetTargetToCurrentHistoryItem(
      blink::WebString::FromUTF8(unique_name_helper_.value()));

  bool is_new_navigation = commit_type == blink::kWebStandardCommit;
  blink::WebView* webview = GetWebView();
  if (commit_params.should_clear_history_list) {
    webview->SetHistoryListFromNavigation(/*history_offset*/ 0,
                                          /*history_length*/ 1);
  } else if (is_new_navigation) {
    DCHECK(!navigation_state->common_params().should_replace_current_entry ||
           (webview->HistoryBackListCount() +
            webview->HistoryForwardListCount() + 1) > 0);
    if (!navigation_state->common_params().should_replace_current_entry)
      webview->IncreaseHistoryListFromNavigation();
  } else if (commit_params.nav_entry_id != 0 &&
             !commit_params.intended_as_new_entry) {
    webview->SetHistoryListFromNavigation(
        navigation_state->commit_params().pending_history_list_offset, {});
  }
}

void RenderFrameImpl::NotifyObserversOfNavigationCommit(
    ui::PageTransition transition) {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "RenderFrameImpl::NotifyObserversOfNavigationCommit",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  for (auto& observer : observers_)
    observer.DidCommitProvisionalLoad(transition);
}

void RenderFrameImpl::UpdateStateForCommit(
    blink::WebHistoryCommitType commit_type,
    ui::PageTransition transition) {
  DocumentState* document_state =
      DocumentState::FromDocumentLoader(frame_->GetDocumentLoader());
  NavigationState* navigation_state = document_state->navigation_state();

  // We need to update the last committed session history entry with state for
  // the previous page. Do this before updating the current history item.
  SendUpdateState();

  UpdateNavigationHistory(commit_type);

  if (!frame_->Parent()) {  // Only for top frames.
    RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
    if (render_thread_impl) {  // Can be NULL in tests.
      render_thread_impl->histogram_customizer()->RenderViewNavigatedToHost(
          GetLoadingUrl().host(), blink::WebView::GetWebViewCount());
    }
  }

  if (IsLocalRoot()) {
    // This forces zoom factor to be propagated to the blink core frame.
    auto& widget = CHECK_DEREF(GetLocalRootWebFrameWidget());
    widget.SetZoomLevel(widget.GetZoomLevel());
  }

  // If we are a top frame navigation to another document we should clear any
  // existing autoplay flags on the Page. This is because flags are stored at
  // the page level so subframes would only add to them.
  if (!frame_->Parent() && !navigation_state->WasWithinSameDocument()) {
    GetWebView()->ClearAutoplayFlags();
  }

  // Set the correct autoplay flags on the Page and wipe the cached origin so
  // this will not be used incorrectly.
  if (url::Origin(frame_->GetSecurityOrigin()) == autoplay_flags_.first) {
    GetWebView()->AddAutoplayFlags(autoplay_flags_.second);
    autoplay_flags_.first = url::Origin();
  }
}

void RenderFrameImpl::DidCommitNavigationInternal(
    blink::WebHistoryCommitType commit_type,
    ui::PageTransition transition,
    const blink::ParsedPermissionsPolicy& permissions_policy_header,
    const blink::DocumentPolicyFeatureState& document_policy_header,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params,
    mojom::DidCommitSameDocumentNavigationParamsPtr same_document_params,
    const std::optional<base::UnguessableToken>& embedding_token) {
  DCHECK(!(same_document_params && interface_params));
  UpdateStateForCommit(commit_type, transition);

  if (GetBlinkPreferences().renderer_wide_named_frame_lookup)
    GetWebFrame()->SetAllowsCrossBrowsingInstanceFrameLookup();

  // This invocation must precede any calls to allowScripts(), allowImages(),
  // or allowPlugins() for the new page. This ensures that when these functions
  // call chrome::ContentSettingsManager::OnContentBlocked, those calls arrive
  // after the browser process has already been informed of the provisional
  // load committing.
  auto params = MakeDidCommitProvisionalLoadParams(
      commit_type, transition, permissions_policy_header,
      document_policy_header, embedding_token);

  NavigationState* navigation_state =
      DocumentState::FromDocumentLoader(frame_->GetDocumentLoader())
          ->navigation_state();
  if (same_document_params) {
    GetFrameHost()->DidCommitSameDocumentNavigation(
        std::move(params), std::move(same_document_params));
    // This will be a noop if this same document navigation is a synchronous
    // renderer-initiated commit.
    navigation_state->RunCommitSameDocumentNavigationCallback(
        blink::mojom::CommitResult::Ok);
  } else {
    if (navigation_state->has_navigation_client()) {
      navigation_state->RunCommitNavigationCallback(
          std::move(params), std::move(interface_params));
    } else {
      GetFrameHost()->DidCommitProvisionalLoad(std::move(params),
                                               std::move(interface_params));
    }
  }

  // Ensure we will propagate the main frame and viewport rect when the main
  // frame commits even if the rect does not change across navigations.
  if (IsMainFrame()) {
    main_frame_intersection_rect_.reset();
    main_frame_viewport_rect_.reset();
  }
}

void RenderFrameImpl::PrepareFrameForCommit(
    const GURL& url,
    const blink::mojom::CommitNavigationParams& commit_params) {
  is_requesting_navigation_ = false;
  GetContentClient()->SetActiveURL(
      url, frame_->Top()->GetSecurityOrigin().ToString().Utf8());

  GetWebView()->SetHistoryListFromNavigation(
      commit_params.current_history_list_offset,
      commit_params.current_history_list_length);
}

blink::mojom::CommitResult RenderFrameImpl::PrepareForHistoryNavigationCommit(
    const blink::mojom::CommonNavigationParams& common_params,
    const blink::mojom::CommitNavigationParams& commit_params,
    WebHistoryItem* item_for_history_navigation,
    blink::WebFrameLoadType* load_type) {
  blink::mojom::NavigationType navigation_type = common_params.navigation_type;
  DCHECK(navigation_type ==
             blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT ||
         navigation_type ==
             blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT ||
         navigation_type == blink::mojom::NavigationType::RESTORE ||
         navigation_type == blink::mojom::NavigationType::RESTORE_WITH_POST);
  *item_for_history_navigation = WebHistoryItem(
      blink::PageState::CreateFromEncodedData(commit_params.page_state));
  if (item_for_history_navigation->IsNull())
    return blink::mojom::CommitResult::Aborted;

  // The browser process sends a single WebHistoryItem for this frame.
  // TODO(creis): Change PageState to FrameState.  In the meantime, we
  // store the relevant frame's WebHistoryItem in the root of the
  // PageState.
  if (navigation_type == blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT ||
      navigation_type ==
          blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT) {
    *load_type = blink::WebFrameLoadType::kBackForward;
  } else {
    *load_type = blink::WebFrameLoadType::kRestore;
  }

  // Keep track of which subframes the browser process has history items
  // for during a history navigation.
  history_subframe_unique_names_ = commit_params.subframe_unique_names;

  if (navigation_type == blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT) {
    // If this is marked as a same document load but we haven't committed
    // anything, we can't proceed with the load. The browser shouldn't let this
    // happen.
    // TODO(crbug.com/41489044): A same-document history navigation was
    // performed but the renderer does not have a history item. Diagnose this,
    // make it a CHECK again, and drop the Restart.
    DCHECK(!GetWebFrame()->GetCurrentHistoryItem().IsNull());
    if (GetWebFrame()->GetCurrentHistoryItem().IsNull()) {
      SCOPED_CRASH_KEY_BOOL("history_no_item", "is_main_frame", IsMainFrame());
      SCOPED_CRASH_KEY_NUMBER("history_no_item", "renderer_commit_state",
                              (int)navigation_commit_state_);
      SCOPED_CRASH_KEY_NUMBER("history_no_item", "browser_history_offset",
                              commit_params.current_history_list_offset);
      SCOPED_CRASH_KEY_NUMBER("history_no_item", "browser_history_len",
                              commit_params.current_history_list_length);
      SCOPED_CRASH_KEY_NUMBER("history_no_item", "renderer_history_len",
                              GetWebView()->HistoryBackListCount() +
                                  GetWebView()->HistoryForwardListCount() + 1);
      base::debug::DumpWithoutCrashing();
      return blink::mojom::CommitResult::RestartCrossDocument;
    }

    // Additionally, if the current history item's document sequence number
    // doesn't match the one sent from the browser, it is possible that this
    // renderer has committed a different document. In such case, the navigation
    // cannot be loaded as a same-document navigation. The browser shouldn't let
    // this happen.
    // TODO(crbug.com/40055210): A same document history navigation was
    // performed but the renderer thinks there's a different document loaded.
    // Where did this bad state of a different document + same-document
    // navigation come from? Figure it out, make this a CHECK again, and drop
    // the Restart.
    DCHECK_EQ(GetWebFrame()->GetCurrentHistoryItem().DocumentSequenceNumber(),
              item_for_history_navigation->DocumentSequenceNumber());
    if (GetWebFrame()->GetCurrentHistoryItem().DocumentSequenceNumber() !=
        item_for_history_navigation->DocumentSequenceNumber()) {
      SCOPED_CRASH_KEY_NUMBER(
          "history_bad_seq", "browser_doc_seq_num",
          item_for_history_navigation->DocumentSequenceNumber());
      SCOPED_CRASH_KEY_NUMBER(
          "history_bad_seq", "renderer_doc_seq_num",
          GetWebFrame()->GetCurrentHistoryItem().DocumentSequenceNumber());
      base::debug::DumpWithoutCrashing();
      return blink::mojom::CommitResult::RestartCrossDocument;
    }
  }

  // Note: we used to check that initial history navigation in the child frame
  // was not canceled by a client redirect before committing it. However,
  // we now destroy the NavigationClient for initial history navigation, and
  // commit does not arrive to the renderer in this case.

  return blink::mojom::CommitResult::Ok;
}

bool RenderFrameImpl::SwapIn(WebFrame* previous_web_frame) {
  CHECK(!in_frame_tree_);

  // The unique name can still change in `WebFrame::Swap()` below (due to JS
  // changing the browsing context name), but in practice, this seems to be good
  // enough.
  unique_name_helper_.set_propagated_name(
      GetUniqueNameOfWebFrame(previous_web_frame));

  // Swapping out a frame can dispatch JS event handlers, causing `this` to be
  // deleted.
  bool is_main_frame = is_main_frame_;
  if (auto* render_frame = RenderFrameImpl::FromWebFrame(previous_web_frame)) {
    render_frame->provisional_frame_for_local_root_swap_ = GetWeakPtr();
  }
  if (!previous_web_frame->Swap(frame_)) {
    // Main frames should always swap successfully because there is no parent
    // frame to cause them to become detached.
    DCHECK(!is_main_frame);
    return false;
  }
  CHECK(GetLocalRootWebFrameWidget());

  // `previous_web_frame` is now detached, and should no longer be referenced.

  in_frame_tree_ = true;

  // If this is the main frame going from a remote frame to a local frame,
  // it needs to set RenderViewImpl's pointer for the main frame to itself.
  if (is_main_frame_) {
    // The WebFrame being swapped in here has now been attached to the Page as
    // its main frame, and the WebFrameWidget was previously initialized when
    // the frame was created so we can call WebView's DidAttachLocalMainFrame().
    GetWebView()->DidAttachLocalMainFrame();
  }

  return true;
}

void RenderFrameImpl::DidStartLoading() {
  // TODO(dgozman): consider removing this callback.
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::didStartLoading",
               "frame_token", frame_token_);
}

void RenderFrameImpl::DidStopLoading() {
  TRACE_EVENT1("navigation,rail", "RenderFrameImpl::didStopLoading",
               "frame_token", frame_token_);

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
  for (auto& observer : observers_)
    observer.FocusedElementChanged(element);
}

void RenderFrameImpl::BeginNavigation(
    std::unique_ptr<blink::WebNavigationInfo> info) {
  // A provisional frame should never make a renderer-initiated navigation: no
  // JS should be running in |this|, and no other frame should have a reference
  // to |this|.
  CHECK(in_frame_tree_);

  // This might be the first navigation in this RenderFrame.
  const bool first_navigation_in_render_frame = !had_started_any_navigation_;
  had_started_any_navigation_ = true;

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

  const GURL& url = info->url_request.Url();
  TRACE_EVENT2("navigation", "RenderFrameImpl::BeginNavigation", "url",
               url.possibly_invalid_spec(), "navigation_type",
               static_cast<int>(info->navigation_type));

  // When an MHTML Archive is present, it should be used to serve iframe
  // content instead of doing a network request. This should never be true for
  // the main frame.
  bool use_archive = (info->archive_status ==
                      blink::WebNavigationInfo::ArchiveStatus::Present) &&
                     !url.SchemeIs(url::kDataScheme);
  DCHECK(!(use_archive && IsMainFrame()));

#if BUILDFLAG(IS_ANDROID)
  // The handlenavigation API is deprecated and will be removed once
  // crbug.com/325351 is resolved.
  if (!url.is_empty() && !use_archive && !IsURLHandledByNetworkStack(url) &&
      GetContentClient()->renderer()->HandleNavigation(
          this, frame_, info->url_request, info->navigation_type,
          info->navigation_policy, false /* is_redirect */)) {
    return;
  }
#endif

  // TODO(crbug.com/40221940): Refactor _unfencedTop handling.
  if (info->is_unfenced_top_navigation) {
    OpenURL(std::move(info));
    return;
  }

  // If the browser is interested, then give it a chance to look at the request.
  if (IsTopLevelNavigation(frame_) &&
      GetWebView()
          ->GetRendererPreferences()
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
  if (IsTopLevelNavigation(frame_) && !url.SchemeIs(url::kAboutScheme) &&
      !url.is_empty()) {
    // All navigations to or from WebUI URLs or within WebUI-enabled
    // RenderProcesses must be handled by the browser process so that the
    // correct bindings and data sources can be registered.
    // All frames in a WebUI process must have the same enabled_bindings_, so
    // we can do a per-frame check here rather than a process-wide check.
    bool should_fork = HasWebUIScheme(url) || HasWebUIScheme(old_url) ||
                       enabled_bindings_.HasAny(kWebUIBindingsPolicySet);
    if (should_fork) {
      OpenURL(std::move(info));
      return;  // Suppress the load here.
    }
  }

  if (frame_->IsOutermostMainFrame() && url.is_valid() &&
      url.SchemeIsHTTPOrHTTPS() &&
      (base::FeatureList::IsEnabled(
           blink::features::kHttpDiskCachePrewarming) ||
       base::FeatureList::IsEnabled(
           blink::features::kSpeculativeServiceWorkerWarmUp) ||
       base::FeatureList::IsEnabled(
           features::kSpeculativeServiceWorkerStartup))) {
    frame_->MaybeStartOutermostMainFrameNavigation(WebVector<WebURL>({url}));
  }

  // Depending on navigation policy, send one of three IPCs to the browser
  // process: DownloadURL handles downloads, OpenURL handles all navigations
  // that will end up in a different tab/window, and BeginNavigation handles
  // everything else.
  if (info->navigation_policy == blink::kWebNavigationPolicyDownload) {
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token =
        CloneBlobURLToken(info->blob_url_token);

    frame_->DownloadURL(info->url_request,
                        network::mojom::RedirectMode::kFollow,
                        std::move(blob_url_token));
    return;
  }
  if (info->navigation_policy != blink::kWebNavigationPolicyCurrentTab) {
    OpenURL(std::move(info));
    return;
  }

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

  // In certain cases, Blink re-navigates to about:blank when creating a new
  // browsing context (when opening a new window or creating an iframe) and
  // expects the navigation to complete synchronously.
  // TODO(crbug.com/40184245): Remove the synchronous about:blank
  // navigation.
  bool should_do_synchronous_about_blank_navigation =
      // Mainly a proxy for checking about:blank, even though it can match
      // other things like about:srcdoc (or any empty document schemes that
      // are registered).
      // TODO(crbug.com/40184245): Tighten the condition to only accept
      // about:blank or an empty URL which defaults to about:blank, per the
      // spec:
      // https://html.spec.whatwg.org/multipage/iframe-embed-object.html#the-iframe-element:about:blank
      WebDocumentLoader::WillLoadUrlAsEmpty(url) &&
      // The navigation method must be "GET". This is to avoid issues like
      // https://crbug.com/1210653, where a form submits to about:blank
      // targeting a new window using a POST. The browser never expects this
      // to happen synchronously because it only expects the synchronous
      // about:blank navigation to originate from browsing context creation,
      // which will always be GET requests.
      info->url_request.HttpMethod().Equals("GET") &&
      // If the frame has committed or even started a navigation before, this
      // navigation can't possibly be triggered by browsing context creation,
      // which would have triggered the navigation synchronously as the first
      // navigation in this frame. Note that we check both
      // IsOnInitialEmptyDocument() and `first_navigation_in_render_frame`
      // here because `first_navigation_in_render_frame` only tracks the state
      // in this *RenderFrame*, so it will be true even if this navigation
      // happens on a frame that has existed before in another process (e.g.
      // an <iframe> pointing to a.com being navigated to a cross-origin
      // about:blank document that happens in a new frame). Meanwhile,
      // IsOnInitialEmptyDocument() tracks the state of the frame, so it will
      // be true in the aforementioned case and we would not do a synchronous
      // commit here.
      frame_->IsOnInitialEmptyDocument() && first_navigation_in_render_frame &&
      // If this is a subframe history navigation that should be sent to the
      // browser, don't commit it synchronously.
      !is_history_navigation_in_new_child_frame &&
      // Synchronous about:blank commits on iframes should only be triggered
      // when first creating the iframe with an unset/about:blank URL, which
      // means the origin should inherit from the parent.
      (IsMainFrame() || info->url_request.RequestorOrigin().IsSameOriginWith(
                            static_cast<WebLocalFrame*>(frame_->Parent())
                                ->GetDocument()
                                .GetSecurityOrigin()));

  if (should_do_synchronous_about_blank_navigation) {
    for (auto& observer : observers_)
      observer.DidStartNavigation(url, info->navigation_type);
    SynchronouslyCommitAboutBlankForBug778318(std::move(info));
    return;
  }

  // Everything else is handled asynchronously by the browser process through
  // BeginNavigation.
  BeginNavigationInternal(
      std::move(info), is_history_navigation_in_new_child_frame,
      renderer_before_unload_start, renderer_before_unload_end);
}

void RenderFrameImpl::SynchronouslyCommitAboutBlankForBug778318(
    std::unique_ptr<blink::WebNavigationInfo> info) {
  CHECK_EQ(NavigationCommitState::kInitialEmptyDocument,
           navigation_commit_state_);
  navigation_commit_state_ = NavigationCommitState::kNone;
  AssertNavigationCommits assert_navigation_commits(this);

  // TODO(dgozman): should we follow the RFI::CommitNavigation path instead?
  auto navigation_params = WebNavigationParams::CreateFromInfo(*info);
  // This quirk is internal to the renderer, so just reuse the previous
  // DocumentToken.
  navigation_params->document_token = frame_->GetDocument().Token();
  navigation_params->is_synchronous_commit_for_bug_778318 = true;
  // We need the provider to be non-null, otherwise Blink crashes, even
  // though the provider should not be used for any actual networking.
  navigation_params->service_worker_network_provider =
      ServiceWorkerNetworkProviderForFrame::CreateInvalidInstance();
  // The synchronous about:blank commit should only happen when the frame is
  // currently showing the initial empty document. For iframes, all navigations
  // that happen on the initial empty document should result in replacement, we
  // must have set the `frame_load_type` to kReplaceCurrentItem. For main frames
  // there are still cases where we will append instead of replace, but the
  // browser already expects this case.
  // TODO(crbug.com/40184245): Ensure main frame cases always do
  // replacement too.
  DCHECK(IsMainFrame() || navigation_params->frame_load_type ==
                              WebFrameLoadType::kReplaceCurrentItem);

  // This corresponds to steps 3 and 20 of
  // https://html.spec.whatwg.org/multipage/browsers.html#creating-a-new-browsing-context,
  // which sets the new Document's `referrer` member to the initiator frame's
  // full unredacted URL, in the case of new browsing context creation.
  //
  // The initiator might no longer exist however, in which case we cannot get
  // its document's full URL to use as the referrer.
  if (info->initiator_frame_token.has_value() &&
      WebFrame::FromFrameToken(info->initiator_frame_token.value())) {
    WebFrame* initiator =
        WebFrame::FromFrameToken(info->initiator_frame_token.value());
    DCHECK(initiator->IsWebLocalFrame());
    navigation_params->referrer =
        initiator->ToWebLocalFrame()->GetDocument().Url().GetString();
  }

  // To prevent pages from being able to abuse window.open() to determine the
  // system entropy, always set a fixed value of 'normal', for consistency with
  // other top-level navigations.
  if (IsMainFrame()) {
    navigation_params->navigation_timings.system_entropy_at_navigation_start =
        blink::mojom::SystemEntropy::kNormal;
  } else {
    // Sub frames always have an empty entropy state since they are generally
    // renderer-initiated. See
    // https://docs.google.com/document/d/1D6DqptsCEd3wPRsZ0q1iwVBAXXmhxZuLV-KKFI0ptCg/edit?usp=sharing
    // for background.
    DCHECK_EQ(blink::mojom::SystemEntropy::kEmpty,
              navigation_params->navigation_timings
                  .system_entropy_at_navigation_start);
  }

  frame_->CommitNavigation(std::move(navigation_params), BuildDocumentState());
}

// mojom::MhtmlFileWriter implementation
// ----------------------------------------

void RenderFrameImpl::SerializeAsMHTML(mojom::SerializeAsMHTMLParamsPtr params,
                                       SerializeAsMHTMLCallback callback) {
  TRACE_EVENT0("page-serialization", "RenderFrameImpl::SerializeAsMHTML");

  // Unpack payload.
  const WebString mhtml_boundary =
      WebString::FromUTF8(params->mhtml_boundary_marker);
  DCHECK(!mhtml_boundary.IsEmpty());

  // Holds WebThreadSafeData instances for some or all of header, contents and
  // footer.
  std::vector<WebThreadSafeData> mhtml_contents;
  auto delegate =
      std::make_unique<MHTMLPartsGenerationDelegateImpl>(std::move(params));

  // Generate MHTML header if needed.
  if (IsMainFrame()) {
    TRACE_EVENT0("page-serialization",
                 "RenderFrameImpl::SerializeAsMHTML header");
    // The returned data can be empty if the main frame should be skipped. If
    // the main frame is skipped, then the whole archive is bad.
    mhtml_contents.emplace_back(WebFrameSerializer::GenerateMHTMLHeader(
        mhtml_boundary, GetWebFrame(), delegate.get()));
  }

  // Generate MHTML parts.  Note that if this is not the main frame, then even
  // skipping the whole parts generation step is not an error - it simply
  // results in an omitted resource in the final file.
  TRACE_EVENT0("page-serialization",
               "RenderFrameImpl::SerializeAsMHTML parts serialization");
  MHTMLPartsGenerationDelegateImpl* delegate_ptr = delegate.get();
  WebFrameSerializer::GenerateMHTMLParts(
      mhtml_boundary, GetWebFrame(), delegate_ptr,
      base::BindOnce(&RenderFrameImpl::OnSerializeMHTMLComplete,
                     weak_factory_.GetWeakPtr(), std::move(delegate),
                     std::move(callback), std::move(mhtml_contents)));
}

void RenderFrameImpl::OnSerializeMHTMLComplete(
    std::unique_ptr<MHTMLPartsGenerationDelegateImpl> delegate,
    SerializeAsMHTMLCallback callback,
    std::vector<blink::WebThreadSafeData> mhtml_contents,
    blink::WebThreadSafeData frame_mhtml_data) {
  TRACE_EVENT0("page-serialization",
               "RenderFrameImpl::SerializeAsMHTML parts serialization");
  // The returned data can be empty if the frame should be skipped, but this
  // is OK.
  mhtml_contents.emplace_back(frame_mhtml_data);
  bool has_some_data = false;
  for (const auto& c : mhtml_contents) {
    if (!c.IsEmpty()) {
      has_some_data = true;
      break;
    }
  }

  // Note: the MHTML footer is written by the browser process, after the last
  // frame is serialized by a renderer process.

  MHTMLHandleWriterDelegate handle_delegate(
      *delegate->TakeParams(),
      base::BindOnce(&RenderFrameImpl::OnWriteMHTMLComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     delegate->TakeSerializedResourcesUriDigests()),
      GetTaskRunner(blink::TaskType::kInternalDefault));

  if (has_some_data) {
    handle_delegate.WriteContents(mhtml_contents);
  } else {
    handle_delegate.Finish(mojom::MhtmlSaveStatus::kSuccess);
  }
}

void RenderFrameImpl::OnWriteMHTMLComplete(
    SerializeAsMHTMLCallback callback,
    std::unordered_set<std::string> serialized_resources_uri_digests,
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
  std::move(callback).Run(save_status, std::move(digests_of_new_parts));
}

#ifndef STATIC_ASSERT_ENUM
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)
#undef STATIC_ASSERT_ENUM
#endif

void RenderFrameImpl::RequestOverlayRoutingToken(
    media::RoutingTokenCallback callback) {
  std::move(callback).Run(GetWebFrame()->GetFrameToken().value());
}

void RenderFrameImpl::OpenURL(std::unique_ptr<blink::WebNavigationInfo> info) {
  // A valid RequestorOrigin is always expected to be present.
  DCHECK(!info->url_request.RequestorOrigin().IsNull());

  WebNavigationPolicy policy = info->navigation_policy;
  auto params = blink::mojom::OpenURLParams::New();
  params->url = info->url_request.Url();
  params->initiator_origin = info->url_request.RequestorOrigin();
  if (info->requestor_base_url.IsValid()) {
    params->initiator_base_url = info->requestor_base_url;
  }
  params->post_body = blink::GetRequestBodyForWebURLRequest(info->url_request);
  DCHECK_EQ(!!params->post_body, IsHttpPost(info->url_request));
  params->extra_headers =
      blink::GetWebURLRequestHeadersAsString(info->url_request).Latin1();

  params->referrer = blink::mojom::Referrer::New(
      blink::WebStringToGURL(info->url_request.ReferrerString()),
      info->url_request.GetReferrerPolicy());
  params->disposition = NavigationPolicyToDisposition(policy);
  params->triggering_event_info = info->triggering_event_info;
  params->blob_url_token = CloneBlobURLToken(info->blob_url_token);
  params->should_replace_current_entry =
      info->frame_load_type == WebFrameLoadType::kReplaceCurrentItem &&
      GetWebView()->HistoryBackListCount() +
          GetWebView()->HistoryForwardListCount() + 1;
  params->user_gesture = info->has_transient_user_activation;
  params->is_unfenced_top_navigation = info->is_unfenced_top_navigation;
  params->initiator_navigation_state_keep_alive_handle =
      std::move(info->initiator_navigation_state_keep_alive_handle);

  params->initiator_frame_token = info->initiator_frame_token;

  // TODO(antoniosartori): Consider plumbing in the source location also for
  // navigations performed via OpenURL.
  params->source_location = network::mojom::SourceLocation::New();

  params->impression = info->impression;

  if (GetContentClient()->renderer()->AllowPopup())
    params->user_gesture = true;

  // A main frame navigation should already have consumed an activation in
  // FrameLoader::StartNavigation.
  DCHECK(!is_main_frame_ || !frame_->HasTransientUserActivation());
  if (!is_main_frame_ &&
      (policy == blink::kWebNavigationPolicyNewBackgroundTab ||
       policy == blink::kWebNavigationPolicyNewForegroundTab ||
       policy == blink::kWebNavigationPolicyNewWindow ||
       policy == blink::kWebNavigationPolicyNewPopup ||
       policy == blink::kWebNavigationPolicyPictureInPicture)) {
    frame_->ConsumeTransientUserActivation();
  }

  params->href_translate = info->href_translate.Latin1();

  bool current_frame_has_download_sandbox_flag = !frame_->IsAllowedToDownload();
  bool has_download_sandbox_flag =
      info->initiator_frame_has_download_sandbox_flag ||
      current_frame_has_download_sandbox_flag;
  bool from_ad = info->initiator_frame_is_ad || frame_->IsAdFrame();

  params->download_policy.ApplyDownloadFramePolicy(
      info->is_opener_navigation, info->url_request.HasUserGesture(),
      info->url_request.RequestorOrigin().CanAccess(
          frame_->GetSecurityOrigin()),
      has_download_sandbox_flag, from_ad);

  params->initiator_activation_and_ad_status =
      blink::GetNavigationInitiatorActivationAndAdStatus(
          info->url_request.HasUserGesture(), info->initiator_frame_is_ad,
          info->is_ad_script_in_stack);

  params->has_rel_opener = info->has_rel_opener;

  GetFrameHost()->OpenURL(std::move(params));
}

void RenderFrameImpl::SetLoaderFactoryBundle(
    scoped_refptr<blink::ChildURLLoaderFactoryBundle> loader_factories) {
  // `background_resource_fetch_context_` will be lazy initialized the first
  // time MaybeGetBackgroundResourceFetchAssets() is called if
  // BackgroundResourceFetch feature is enabled.
  background_resource_fetch_context_.reset();
  loader_factories_ = std::move(loader_factories);
}

blink::ChildURLLoaderFactoryBundle* RenderFrameImpl::GetLoaderFactoryBundle() {
  // GetLoaderFactoryBundle should not be called before `loader_factories_` have
  // been set up - before a document is committed (e.g. before a navigation
  // commits or the initial empty document commits) it is not possible to 1)
  // trigger subresource loads, or 2) trigger propagation of the factories into
  // a new frame.
  DCHECK(loader_factories_);

  return loader_factories_.get();
}

scoped_refptr<blink::ChildURLLoaderFactoryBundle>
RenderFrameImpl::CreateLoaderFactoryBundle(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> info,
    std::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        subresource_proxying_loader_factory,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        keep_alive_loader_factory,
    mojo::PendingAssociatedRemote<blink::mojom::FetchLaterLoaderFactory>
        fetch_later_loader_factory) {
  DCHECK(info);
  // We don't check `DCHECK(info->pending_default_factory())`, because it will
  // be missing for speculative frames (and in other cases where no subresource
  // loads are expected - e.g. in test frames created via RenderViewTest).  See
  // also the DCHECK in URLLoaderFactoryBundle::GetFactory.

  auto loader_factories =
      base::MakeRefCounted<blink::HostChildURLLoaderFactoryBundle>(
          GetTaskRunner(blink::TaskType::kInternalLoading));

  loader_factories->Update(
      std::make_unique<blink::ChildPendingURLLoaderFactoryBundle>(
          std::move(info)));

  if (subresource_overrides) {
    loader_factories->UpdateSubresourceOverrides(&*subresource_overrides);
  }
  if (subresource_proxying_loader_factory) {
    loader_factories->SetSubresourceProxyingLoaderFactory(
        std::move(subresource_proxying_loader_factory));
  }
  if (keep_alive_loader_factory) {
    loader_factories->SetKeepAliveLoaderFactory(
        std::move(keep_alive_loader_factory));
  }
  if (base::FeatureList::IsEnabled(blink::features::kFetchLaterAPI) &&
      fetch_later_loader_factory) {
    loader_factories->SetFetchLaterLoaderFactory(
        std::move(fetch_later_loader_factory));
  }

  return loader_factories;
}

void RenderFrameImpl::UpdateEncoding(WebFrame* frame,
                                     const std::string& encoding_name) {
  // Only update main frame's encoding_name.
  if (!frame->Parent()) {
    // TODO(crbug.com/40188381): Move `UpdateEncoding()` to the
    // `LocalMainFrameHost` interface where it makes sense. That is not a simple
    // as just migrating the method, since upon main frame creation we attempt
    // to send the encoding information before
    // `WebViewImpl::local_main_frame_host_remote_` is set-up, which breaks
    // things.`
    GetFrameHost()->UpdateEncoding(encoding_name);
  }
}

void RenderFrameImpl::SyncSelectionIfRequired(blink::SyncCondition force_sync) {
  std::u16string text;
  size_t offset;
  gfx::Range range;
#if BUILDFLAG(ENABLE_PPAPI)
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
  if (force_sync == blink::SyncCondition::kForced ||
      selection_text_offset_ != offset || selection_range_ != range ||
      selection_text_ != text) {
    selection_text_ = text;
    selection_text_offset_ = offset;
    selection_range_ = range;
    SetSelectedText(text, offset, range);
  }
  GetLocalRootWebFrameWidget()->UpdateSelectionBounds();
}

void RenderFrameImpl::CreateAudioInputStream(
    blink::CrossVariantMojoRemote<
        blink::mojom::RendererAudioInputStreamFactoryClientInterfaceBase>
        client,
    const base::UnguessableToken& session_id,
    const media::AudioParameters& params,
    bool automatic_gain_control,
    uint32_t shared_memory_count,
    blink::CrossVariantMojoReceiver<
        media::mojom::AudioProcessorControlsInterfaceBase> controls_receiver,
    const media::AudioProcessingSettings* settings) {
  DCHECK_EQ(!!settings, !!controls_receiver);
  media::mojom::AudioProcessingConfigPtr processing_config;
  if (controls_receiver) {
    DCHECK(settings);
    processing_config = media::mojom::AudioProcessingConfig::New(
        std::move(controls_receiver), *settings);
  }

  GetAudioInputStreamFactory()->CreateStream(
      std::move(client), session_id, params, automatic_gain_control,
      shared_memory_count, std::move(processing_config));
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

void RenderFrameImpl::BeginNavigationInternal(
    std::unique_ptr<blink::WebNavigationInfo> info,
    bool is_history_navigation_in_new_child_frame,
    base::TimeTicks renderer_before_unload_start,
    base::TimeTicks renderer_before_unload_end) {
  if (!frame_->WillStartNavigation(*info))
    return;

  for (auto& observer : observers_)
    observer.DidStartNavigation(info->url_request.Url(), info->navigation_type);
  is_requesting_navigation_ = true;

  // Set SiteForCookies.
  WebDocument frame_document = frame_->GetDocument();
  if (info->frame_type == blink::mojom::RequestContextFrameType::kTopLevel) {
    info->url_request.SetSiteForCookies(
        net::SiteForCookies::FromUrl(info->url_request.Url()));
  } else {
    info->url_request.SetSiteForCookies(frame_document.SiteForCookies());
  }

  ui::PageTransition transition_type = GetTransitionType(
      ui::PAGE_TRANSITION_LINK,
      info->frame_load_type == WebFrameLoadType::kReplaceCurrentItem,
      IsMainFrame(), GetWebView()->IsFencedFrameRoot(), info->navigation_type);
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
  bool for_outermost_main_frame = frame_->IsOutermostMainFrame();
  {
    // upstream_url can be empty here because ForRedirect(false) implies that
    // this isn't a browser intiated navigation.
    std::optional<blink::WebURL> adjusted_request_url = WillSendRequestInternal(
        info->url_request.Url(), info->url_request.RequestorOrigin(),
        info->url_request.SiteForCookies(), ForRedirect(false),
        /*upstream_url=*/GURL(), transition_type);
    if (adjusted_request_url.has_value()) {
      info->url_request.SetUrl(adjusted_request_url.value());
    }
  }
  FinalizeRequestInternal(info->url_request, for_outermost_main_frame,
                          transition_type);
  // The extra data was created in WillSendRequestInternal if it didn't exist.
  DCHECK(info->url_request.GetURLRequestExtraData());

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
      (info->navigation_type == blink::kWebNavigationTypeFormSubmitted ||
       info->navigation_type ==
           blink::kWebNavigationTypeFormResubmittedBackForward ||
       info->navigation_type == blink::kWebNavigationTypeFormResubmittedReload);

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
  std::optional<base::Value::Dict> devtools_initiator;
  if (!info->devtools_initiator_info.IsNull()) {
    std::optional<base::Value> devtools_initiator_value =
        base::JSONReader::Read(info->devtools_initiator_info.Utf8());
    if (devtools_initiator_value && devtools_initiator_value->is_dict()) {
      devtools_initiator = std::move(*devtools_initiator_value).TakeDict();
    }
  }

  blink::mojom::NavigationInitiatorActivationAndAdStatus
      initiator_activation_and_ad_status =
          blink::GetNavigationInitiatorActivationAndAdStatus(
              info->url_request.HasUserGesture(), info->initiator_frame_is_ad,
              info->is_ad_script_in_stack);

  blink::mojom::BeginNavigationParamsPtr begin_params =
      blink::mojom::BeginNavigationParams::New(
          info->initiator_frame_token,
          blink::GetWebURLRequestHeadersAsString(info->url_request).Latin1(),
          load_flags, info->url_request.GetSkipServiceWorker(),
          blink::GetRequestContextTypeForWebURLRequest(info->url_request),
          blink::GetMixedContentContextTypeForWebURLRequest(info->url_request),
          is_form_submission, was_initiated_by_link_click,
          info->force_history_push, searchable_form_url,
          searchable_form_encoding, client_side_redirect_url,
          std::move(devtools_initiator),
          info->url_request.TrustTokenParams()
              ? info->url_request.TrustTokenParams()->Clone()
              : nullptr,
          info->impression, renderer_before_unload_start,
          renderer_before_unload_end, initiator_activation_and_ad_status,
          info->is_container_initiated, info->storage_access_api_status,
          info->has_rel_opener);

  bool current_frame_has_download_sandbox_flag = !frame_->IsAllowedToDownload();
  bool has_download_sandbox_flag =
      info->initiator_frame_has_download_sandbox_flag ||
      current_frame_has_download_sandbox_flag;
  bool from_ad = info->initiator_frame_is_ad || frame_->IsAdFrame();

  mojo::PendingRemote<blink::mojom::NavigationStateKeepAliveHandle>
      initiator_navigation_state_keep_alive_handle =
          std::move(info->initiator_navigation_state_keep_alive_handle);

  network::mojom::RequestDestination request_destination =
      blink::GetRequestDestinationForWebURLRequest(info->url_request);

  blink::mojom::CommonNavigationParamsPtr common_params =
      MakeCommonNavigationParams(frame_->GetSecurityOrigin(), std::move(info),
                                 load_flags, has_download_sandbox_flag, from_ad,
                                 is_history_navigation_in_new_child_frame,
                                 request_destination);

  bool is_duplicate_navigation = false;
  if (navigation_client_impl_ &&
      navigation_client_impl_->HasBeginNavigationParams()) {
    // We ignore navigations that are identical to the ongoing navigation. This
    // is because the navigation is likely to be accidental (e.g. user clicked
    // the same link multiple times, etc).
    auto& prev_begin_params = navigation_client_impl_->begin_params();
    auto& prev_common_params = navigation_client_impl_->common_params();
    if (common_params->navigation_start - prev_common_params.navigation_start <=
            features::kDuplicateNavThreshold.Get() &&
        begin_params->was_initiated_by_link_click ==
            prev_begin_params.was_initiated_by_link_click &&
        common_params->url == prev_common_params.url &&
        common_params->method == "GET" && prev_common_params.method == "GET" &&
        common_params->initiator_origin ==
            prev_common_params.initiator_origin &&
        common_params->has_user_gesture ==
            prev_common_params.has_user_gesture &&
        common_params->referrer == prev_common_params.referrer &&
        common_params->transition == prev_common_params.transition &&
        common_params->should_replace_current_entry ==
            prev_common_params.should_replace_current_entry &&
        begin_params->headers == prev_begin_params.headers &&
        begin_params->has_rel_opener == prev_begin_params.has_rel_opener) {
      is_duplicate_navigation = true;
    }
  }
  base::UmaHistogramBoolean("Navigation.RendererInitiated.IsDuplicate",
                            is_duplicate_navigation);
  if (is_duplicate_navigation &&
      base::FeatureList::IsEnabled(features::kIgnoreDuplicateNavs)) {
    return;
  }

  mojo::PendingAssociatedRemote<mojom::NavigationClient>
      navigation_client_remote;
  BindNavigationClientWithParams(
      navigation_client_remote.InitWithNewEndpointAndPassReceiver(),
      begin_params.Clone(), common_params.Clone(), is_duplicate_navigation);
  mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
      renderer_cancellation_listener_receiver;
  navigation_client_impl_->SetUpRendererInitiatedNavigation(
      renderer_cancellation_listener_receiver.InitWithNewPipeAndPassRemote());
  GetFrameHost()->BeginNavigation(
      std::move(common_params), std::move(begin_params),
      std::move(blob_url_token), std::move(navigation_client_remote),
      std::move(initiator_navigation_state_keep_alive_handle),
      std::move(renderer_cancellation_listener_receiver));
}

void RenderFrameImpl::DecodeDataURL(
    const blink::mojom::CommonNavigationParams& common_params,
    const blink::mojom::CommitNavigationParams& commit_params,
    std::string* mime_type,
    std::string* charset,
    std::string* data,
    GURL* base_url) {
  // A loadData request with a specified base URL.
  GURL data_url = common_params.url;
#if BUILDFLAG(IS_ANDROID)
  if (!commit_params.data_url_as_string.empty()) {
#if DCHECK_IS_ON()
    {
      std::string mime_type_tmp, charset_tmp, data_tmp;
      DCHECK(net::DataURL::Parse(data_url, &mime_type_tmp, &charset_tmp,
                                 &data_tmp));
      DCHECK(data_tmp.empty());
    }
#endif
    // If `data_url_as_string` is set, the `url` in CommonNavigationParams will
    // only contain the data: URL header and won't contain any actual data (see
    // NavigationControllerAndroid::LoadUrl()), so we should use
    // `data_url_as_string` if possible.
    data_url = GURL(commit_params.data_url_as_string);
    if (!data_url.is_valid() || !data_url.SchemeIs(url::kDataScheme)) {
      // If the given data URL is invalid, use the data: URL header as a
      // fallback.
      data_url = common_params.url;
    }
  }
#endif
  // Parse the given data, then set the `base_url`, which is used as the
  // document URL.
  if (net::DataURL::Parse(data_url, mime_type, charset, data)) {
    // Since the base URL will also be used as the document URL, we should not
    // use an empty URL. If it's empty, use the data: URL as a fallback.
    // TODO(crbug.com/40187599): Maybe this should consider
    // `data_url_as_string` too. Otherwise, the base URL might be set to the
    // data: URL header with empty data, instead of the data: URL that contains
    // the actual data.
    *base_url = common_params.base_url_for_data_url.is_empty()
                    ? common_params.url
                    : common_params.base_url_for_data_url;
  } else {
    CHECK(false) << "Invalid URL passed: "
                 << common_params.url.possibly_invalid_spec();
  }
}

void RenderFrameImpl::SendUpdateState() {
  // Since we are sending immediately we can cancel any pending delayed sync
  // timer.
  delayed_state_sync_timer_.Stop();
  if (GetWebFrame()->GetCurrentHistoryItem().IsNull())
    return;

  GetFrameHost()->UpdateState(GetWebFrame()->CurrentHistoryItemToPageState());
}

blink::WebURL RenderFrameImpl::LastCommittedUrlForUKM() {
  return GetLoadingUrl();
}

// Returns the "loading URL", which might be different than the "document URL"
// used in the DocumentLoader in some cases:
// - For error pages, it will return the URL that failed to load, instead of the
// chrome-error:// URL used as the document URL in the DocumentLoader.
// - For loadDataWithBaseURL() navigations, it will return the data: URL
// used to commit, instead of the "base URL" used as the document URL in the
// DocumentLoader. See comments in  BuildDocumentStateFromParams() and
// in navigation_params.mojom's `is_load_data_with_base_url` for more details.
GURL RenderFrameImpl::GetLoadingUrl() const {
  WebDocumentLoader* document_loader = frame_->GetDocumentLoader();

  GURL overriden_url;
  if (MaybeGetOverriddenURL(document_loader, &overriden_url))
    return overriden_url;

  return document_loader->GetUrl();
}

media::MediaPermission* RenderFrameImpl::GetMediaPermission() {
  if (!media_permission_dispatcher_) {
    media_permission_dispatcher_ =
        std::make_unique<MediaPermissionDispatcher>(this);
  }
  return media_permission_dispatcher_.get();
}

void RenderFrameImpl::RegisterMojoInterfaces() {
  // TODO(dcheng): Fold this interface into mojom::Frame.
  GetAssociatedInterfaceRegistry()
      ->AddInterface<blink::mojom::AutoplayConfigurationClient>(
          base::BindRepeating(&RenderFrameImpl::BindAutoplayConfiguration,
                              weak_factory_.GetWeakPtr()));

  GetAssociatedInterfaceRegistry()->AddInterface<mojom::FrameBindingsControl>(
      base::BindRepeating(&RenderFrameImpl::BindFrameBindingsControl,
                          weak_factory_.GetWeakPtr()));

  GetAssociatedInterfaceRegistry()->AddInterface<mojom::NavigationClient>(
      base::BindRepeating(&RenderFrameImpl::BindNavigationClient,
                          weak_factory_.GetWeakPtr()));

  // TODO(dcheng): Fold this interface into mojom::Frame.
  GetAssociatedInterfaceRegistry()->AddInterface<mojom::MhtmlFileWriter>(
      base::BindRepeating(&RenderFrameImpl::BindMhtmlFileWriter,
                          base::Unretained(this)));

  GetAssociatedInterfaceRegistry()
      ->AddInterface<blink::mojom::RenderAccessibility>(base::BindRepeating(
          &RenderAccessibilityManager::BindReceiver,
          base::Unretained(render_accessibility_manager_.get())));

#if BUILDFLAG(IS_ANDROID)
  GetAssociatedInterfaceRegistry()->AddInterface<mojom::GinJavaBridge>(
      base::BindRepeating(&RenderFrameImpl::BindGinJavaBridge,
                          weak_factory_.GetWeakPtr()));
#endif
}

#if BUILDFLAG(IS_ANDROID)
void RenderFrameImpl::BindGinJavaBridge(
    mojo::PendingAssociatedReceiver<mojom::GinJavaBridge> receiver) {
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<GinJavaBridgeDispatcher>(this), std::move(receiver));
}
#endif

void RenderFrameImpl::BindMhtmlFileWriter(
    mojo::PendingAssociatedReceiver<mojom::MhtmlFileWriter> receiver) {
  mhtml_file_writer_receiver_.reset();
  mhtml_file_writer_receiver_.Bind(
      std::move(receiver), GetTaskRunner(blink::TaskType::kInternalDefault));
}

// TODO(crbug.com/40550966): Move this method to Blink, and eliminate
// the plumbing logic through blink::WebLocalFrameClient.
void RenderFrameImpl::CheckIfAudioSinkExistsAndIsAuthorized(
    const blink::WebString& sink_id,
    blink::WebSetSinkIdCompleteCallback completion_callback) {
  std::move(
      blink::ConvertToOutputDeviceStatusCB(std::move(completion_callback)))
      .Run(blink::AudioDeviceFactory::GetInstance()
               ->GetOutputDeviceInfo(GetWebFrame()->GetLocalFrameToken(),
                                     sink_id.Utf8())
               .device_status());
}

scoped_refptr<network::SharedURLLoaderFactory>
RenderFrameImpl::GetURLLoaderFactory() {
  if (!RenderThreadImpl::current()) {
    // Some tests (e.g. RenderViewTests) do not have RenderThreadImpl,
    // and must create a factory override instead.
    if (url_loader_factory_override_for_test_) {
      return url_loader_factory_override_for_test_;
    }

    // If the override does not exist, try looking in the ancestor chain since
    // we might have created child frames and asked them to create a URL loader
    // factory.
    for (auto* ancestor = GetWebFrame()->Parent(); ancestor;
         ancestor = ancestor->Parent()) {
      RenderFrameImpl* ancestor_frame = RenderFrameImpl::FromWebFrame(ancestor);
      if (ancestor_frame &&
          ancestor_frame->url_loader_factory_override_for_test_) {
        return ancestor_frame->url_loader_factory_override_for_test_;
      }
    }
    // At this point we can't create anything. We use CHECK(false) instead of
    // NOTREACHED() here to catch errors on clusterfuzz and production.
    CHECK(false);
    return nullptr;
  }
  return GetLoaderFactoryBundle();
}

blink::URLLoaderThrottleProvider*
RenderFrameImpl::GetURLLoaderThrottleProvider() {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // The RenderThreadImpl may not be valid in some tests.
  return render_thread ? render_thread->url_loader_throttle_provider()
                       : nullptr;
}

scoped_refptr<blink::WebBackgroundResourceFetchAssets>
RenderFrameImpl::MaybeGetBackgroundResourceFetchAssets() {
  if (!base::FeatureList::IsEnabled(
          blink::features::kBackgroundResourceFetch)) {
    return nullptr;
  }

  if (!background_resource_fetch_context_) {
    if (!background_resource_fetch_task_runner_) {
      background_resource_fetch_task_runner_ =
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::TaskPriority::USER_BLOCKING});
    }
    background_resource_fetch_context_ =
        base::MakeRefCounted<BackgroundResourceFetchAssets>(
            GetLoaderFactoryBundle()->Clone(),
            GetURLLoaderThrottleProvider()
                ? GetURLLoaderThrottleProvider()->Clone()
                : nullptr,
            background_resource_fetch_task_runner_,
            frame_->GetLocalFrameToken());
  }
  return background_resource_fetch_context_;
}

void RenderFrameImpl::OnStopLoading() {
  for (auto& observer : observers_)
    observer.OnStop();
}

bool RenderFrameImpl::IsRequestingNavigation() {
  return is_requesting_navigation_;
}

void RenderFrameImpl::LoadHTMLStringForTesting(std::string_view html,
                                               const GURL& base_url,
                                               const std::string& text_encoding,
                                               const GURL& unreachable_url,
                                               bool replace_current_item) {
  AssertNavigationCommits assert_navigation_commits(
      this, kMayReplaceInitialEmptyDocument);

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      url_loader_factory_remote;
  if (url_loader_factory_override_for_test_) {
    url_loader_factory_override_for_test_->Clone(
        url_loader_factory_remote.InitWithNewPipeAndPassReceiver());
  } else {
    url_loader_factory_remote =
        network::NotImplementedURLLoaderFactory::Create();
  }

  pending_loader_factories_ = CreateLoaderFactoryBundle(
      blink::ChildPendingURLLoaderFactoryBundle::CreateFromDefaultFactoryImpl(
          std::move(url_loader_factory_remote)),
      /*subresource_overrides=*/std::nullopt,
      /*subresource_proxying_loader_factory=*/{},
      /*keep_alive_loader_factory=*/{},
      /*fetch_later_loader_factory=*/{});

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

BindingsPolicySet RenderFrameImpl::GetEnabledBindings() {
  return enabled_bindings_;
}

void RenderFrameImpl::SetAccessibilityModeForTest(ui::AXMode new_mode) {
  render_accessibility_manager_->SetMode(new_mode, 1);
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

#if BUILDFLAG(ENABLE_PPAPI)

mojom::PepperHost* RenderFrameImpl::GetPepperHost() {
  if (!pepper_host_remote_.is_bound())
    GetRemoteAssociatedInterfaces()->GetInterface(&pepper_host_remote_);
  return pepper_host_remote_.get();
}

void RenderFrameImpl::PepperInstanceCreated(
    PepperPluginInstanceImpl* instance,
    mojo::PendingAssociatedRemote<mojom::PepperPluginInstance> mojo_instance,
    mojo::PendingAssociatedReceiver<mojom::PepperPluginInstanceHost>
        mojo_host) {
  active_pepper_instances_.insert(instance);
  GetPepperHost()->InstanceCreated(
      instance->pp_instance(), std::move(mojo_instance), std::move(mojo_host));
}

void RenderFrameImpl::PepperInstanceDeleted(
    PepperPluginInstanceImpl* instance) {
  active_pepper_instances_.erase(instance);

  if (focused_pepper_plugin_ == instance)
    PepperFocusChanged(instance, false);
}

void RenderFrameImpl::PepperFocusChanged(PepperPluginInstanceImpl* instance,
                                         bool focused) {
  if (focused)
    focused_pepper_plugin_ = instance;
  else if (focused_pepper_plugin_ == instance)
    focused_pepper_plugin_ = nullptr;

  GetLocalRootWebFrameWidget()->UpdateTextInputState();
  GetLocalRootWebFrameWidget()->UpdateSelectionBounds();
}

#endif  // BUILDFLAG(ENABLE_PPAPI)

blink::WebComputedAXTree* RenderFrameImpl::GetOrCreateWebComputedAXTree() {
  if (!computed_ax_tree_)
    computed_ax_tree_ = std::make_unique<AomContentAxTree>(this);
  return computed_ax_tree_.get();
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
RenderFrameImpl::CreateWebSocketHandshakeThrottle() {
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
      frame_->GetLocalFrameToken(),
      GetTaskRunner(blink::TaskType::kInternalDefault));
}

bool RenderFrameImpl::GetCaretBoundsFromFocusedPlugin(gfx::Rect& rect) {
#if BUILDFLAG(ENABLE_PPAPI)
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

void RenderFrameImpl::SetURLLoaderFactoryOverrideForTest(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  url_loader_factory_override_for_test_ = std::move(factory);
}

scoped_refptr<blink::ChildURLLoaderFactoryBundle>
RenderFrameImpl::CloneLoaderFactories() {
  auto pending_bundle = base::WrapUnique(
      static_cast<blink::TrackedChildPendingURLLoaderFactoryBundle*>(
          GetLoaderFactoryBundle()->Clone().release()));
  return base::MakeRefCounted<blink::TrackedChildURLLoaderFactoryBundle>(
      std::move(pending_bundle));
}

blink::scheduler::WebAgentGroupScheduler&
RenderFrameImpl::GetAgentGroupScheduler() {
  return agent_scheduling_group_->agent_group_scheduler();
}

url::Origin RenderFrameImpl::GetSecurityOriginOfTopFrame() {
  return frame_->Top()->GetSecurityOrigin();
}

base::WeakPtr<media::DecoderFactory> RenderFrameImpl::GetMediaDecoderFactory() {
  return media_factory_.GetDecoderFactory();
}

gfx::Rect RenderFrameImpl::ConvertViewportToWindow(const gfx::Rect& rect) {
  return GetLocalRootWebFrameWidget()->BlinkSpaceToEnclosedDIPs(rect);
}

float RenderFrameImpl::GetDeviceScaleFactor() {
  return GetLocalRootWebFrameWidget()->GetScreenInfo().device_scale_factor;
}

bool RenderFrameImpl::DeferMediaLoad(bool has_played_media_before,
                                     base::OnceClosure closure) {
  if (frame_->GetDocument().IsPrerendering()) {
    frame_->GetDocument().AddPostPrerenderingActivationStep(
        base::BindOnce(CallClientDeferMediaLoad, weak_factory_.GetWeakPtr(),
                       has_played_media_before, std::move(closure)));
    return true;
  }

  return GetContentClient()->renderer()->DeferMediaLoad(
      this, has_played_media_before, std::move(closure));
}

WebView* RenderFrameImpl::CreateNewWindow(
    const WebURLRequest& request,
    const blink::WebWindowFeatures& features,
    const WebString& frame_name,
    WebNavigationPolicy policy,
    network::mojom::WebSandboxFlags sandbox_flags,
    const blink::SessionStorageNamespaceId& session_storage_namespace_id,
    bool& consumed_user_gesture,
    const std::optional<blink::Impression>& impression,
    const std::optional<blink::WebPictureInPictureWindowOptions>& pip_options,
    const blink::WebURL& base_url) {
  consumed_user_gesture = false;
  mojom::CreateNewWindowParamsPtr params = mojom::CreateNewWindowParams::New();

  // The user activation check is done at the browser process through
  // |frame_host->CreateNewWindow()| call below.  But the extensions case
  // handled through the following |if| is an exception.
  params->allow_popup = false;
  if (GetContentClient()->renderer()->AllowPopup())
    params->allow_popup = true;

  params->window_container_type = WindowFeaturesToContainerType(features);

  params->session_storage_namespace_id = session_storage_namespace_id;
  if (!features.noopener) {
    params->clone_from_session_storage_namespace_id =
        GetWebView()->GetSessionStorageNamespaceId();
  }

  const std::string& frame_name_utf8 = frame_name.Utf8(
      WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);
  params->frame_name = frame_name_utf8;
  params->opener_suppressed = features.noopener;
  params->disposition = NavigationPolicyToDisposition(policy);
  if (!request.IsNull()) {
    params->target_url = request.Url();
    // The browser process does not consider empty URLs as valid (partly due to
    // a risk of treating them as a navigation to the privileged NTP in some
    // cases), so treat an attempt to create a window with an empty URL as
    // opening about:blank.
    //
    // Similarly, javascript: URLs should not be sent to the browser process,
    // since they are either handled within the renderer process (if a window is
    // created within the same browsing context group) or ignored (in the
    // noopener case). Use about:blank for the URL in that case as well, to
    // reduce the risk of running them incorrectly.
    if (params->target_url.is_empty() ||
        params->target_url.SchemeIs(url::kJavaScriptScheme)) {
      params->target_url = GURL(url::kAboutBlankURL);
    }

    params->referrer = blink::mojom::Referrer::New(
        blink::WebStringToGURL(request.ReferrerString()),
        request.GetReferrerPolicy());
  }
  params->features = ConvertWebWindowFeaturesToMojoWindowFeatures(features);

  params->is_form_submission = request.IsFormSubmission();
  params->form_submission_post_data =
      blink::GetRequestBodyForWebURLRequest(request);
  params->form_submission_post_content_type = request.HttpContentType().Utf8();

  params->impression = impression;

  if (pip_options) {
    CHECK_EQ(policy, blink::kWebNavigationPolicyPictureInPicture);
    auto pip_mojom_opts = blink::mojom::PictureInPictureWindowOptions::New();
    pip_mojom_opts->width = pip_options->width;
    pip_mojom_opts->height = pip_options->height;
    pip_mojom_opts->disallow_return_to_opener =
        pip_options->disallow_return_to_opener;
    pip_mojom_opts->prefer_initial_window_placement =
        pip_options->prefer_initial_window_placement;
    params->pip_options = std::move(pip_mojom_opts);
  }

  params->download_policy.ApplyDownloadFramePolicy(
      /*is_opener_navigation=*/false, request.HasUserGesture(),
      // `openee_can_access_opener_origin` only matters for opener navigations,
      // so its value here is irrelevant.
      /*openee_can_access_opener_origin=*/true,
      !GetWebFrame()->IsAllowedToDownload(), GetWebFrame()->IsAdFrame());

  params->initiator_activation_and_ad_status =
      blink::GetNavigationInitiatorActivationAndAdStatus(
          request.HasUserGesture(), GetWebFrame()->IsAdFrame(),
          GetWebFrame()->IsAdScriptInStack());

  // We preserve this information before sending the message since |params| is
  // moved on send.
  bool is_background_tab =
      params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;

  mojom::CreateNewWindowStatus status;
  mojom::CreateNewWindowReplyPtr reply;
  auto* frame_host = GetFrameHost();
  if (!frame_host->CreateNewWindow(std::move(params), &status, &reply)) {
    // The sync IPC failed, e.g. maybe the render process is in the middle of
    // shutting down. Can't create a new window without the browser process,
    // so just bail out.
    return nullptr;
  }

  // If creation of the window was blocked (e.g. because this frame doesn't
  // have user activation), return before consuming user activation. A frame
  // that isn't allowed to open a window  shouldn't be able to consume the
  // activation for the rest of the frame tree.
  if (status == mojom::CreateNewWindowStatus::kBlocked)
    return nullptr;

  // For Android WebView, we support a pop-up like behavior for window.open()
  // even if the embedding app doesn't support multiple windows. In this case,
  // window.open() will return "window" and navigate it to whatever URL was
  // passed. We also don't need to consume user gestures to protect against
  // multiple windows being opened, because, well, the app doesn't support
  // multiple windows.
  // TODO(dcheng): It's awkward that this is plumbed into Blink but not really
  // used much in Blink, except to enable web testing... perhaps this should
  // be checked directly in the browser side.
  if (status == mojom::CreateNewWindowStatus::kReuse) {
    // In this case, treat javascript: URLs as blocked rather than running them
    // in a reused main frame in Android WebView. See https://crbug.com/1083819.
    if (!request.IsNull() && request.Url().ProtocolIs(url::kJavaScriptScheme)) {
      return nullptr;
    }
    return GetWebView();
  }

  // Consume the transient user activation in the current renderer.
  consumed_user_gesture = GetWebFrame()->ConsumeTransientUserActivation(
      blink::UserActivationUpdateSource::kBrowser);

  // If we should ignore the new window (e.g. because of `noopener`), return
  // now that user activation was consumed.
  if (status == mojom::CreateNewWindowStatus::kIgnore)
    return nullptr;

  DCHECK(reply);
  DCHECK_NE(MSG_ROUTING_NONE, reply->main_frame_route_id);
  DCHECK_NE(MSG_ROUTING_NONE, reply->widget_params->routing_id);

  // While this view may be a background extension page, it can spawn a visible
  // render view. So we just assume that the new one is not another background
  // page instead of passing on our own value.
  // TODO(vangelis): Can we tell if the new view will be a background page?
  bool never_composited = false;

  // The initial hidden state for the RenderViewImpl here has to match what the
  // browser will eventually decide for the given disposition. Since we have to
  // return from this call synchronously, we just have to make our best guess
  // and rely on the browser sending a WasHidden / WasShown message if it
  // disagrees.
  mojom::CreateViewParamsPtr view_params = mojom::CreateViewParams::New();

  view_params->opener_frame_token = GetWebFrame()->GetFrameToken();
  view_params->window_was_opened_by_another_window = true;
  view_params->renderer_preferences = GetWebView()->GetRendererPreferences();
  view_params->web_preferences = GetWebView()->GetWebPreferences();

  view_params->replication_state = blink::mojom::FrameReplicationState::New();
  view_params->replication_state->frame_policy.sandbox_flags = sandbox_flags;
  view_params->replication_state->name = frame_name_utf8;
  view_params->devtools_main_frame_token = reply->devtools_main_frame_token;
  view_params->browsing_context_group_info = reply->browsing_context_group_info;
  view_params->color_provider_colors = reply->color_provider_colors;

  auto main_frame_params = mojom::CreateLocalMainFrameParams::New();
  main_frame_params->frame_token = reply->main_frame_token;
  main_frame_params->routing_id = reply->main_frame_route_id;
  main_frame_params->frame = std::move(reply->frame);
  main_frame_params->interface_broker =
      std::move(reply->main_frame_interface_broker);
  main_frame_params->document_token = reply->document_token;
  main_frame_params->policy_container = std::move(reply->policy_container);
  main_frame_params->associated_interface_provider_remote =
      std::move(reply->associated_interface_provider);
  main_frame_params->widget_params = std::move(reply->widget_params);
  main_frame_params->subresource_loader_factories =
      base::WrapUnique(static_cast<blink::PendingURLLoaderFactoryBundle*>(
          CloneLoaderFactories()->Clone().release()));

  view_params->main_frame =
      mojom::CreateMainFrameUnion::NewLocalParams(std::move(main_frame_params));
  view_params->blink_page_broadcast = std::move(reply->page_broadcast);
  view_params->session_storage_namespace_id =
      reply->cloned_session_storage_namespace_id;
  DCHECK(!view_params->session_storage_namespace_id.empty())
      << "Session storage namespace must be populated.";
  view_params->hidden = is_background_tab;
  view_params->never_composited = never_composited;
  view_params->partitioned_popin_params =
      std::move(reply->partitioned_popin_params);

  WebView* web_view = agent_scheduling_group_->CreateWebView(
      std::move(view_params),
      /*was_created_by_renderer=*/true, base_url);

  if (reply->wait_for_debugger) {
    blink::WebFrameWidget* frame_widget =
        web_view->MainFrame()->ToWebLocalFrame()->LocalRoot()->FrameWidget();
    frame_widget->WaitForDebuggerWhenShown();
  }

  return web_view;
}

std::unique_ptr<blink::WebLinkPreviewTriggerer>
RenderFrameImpl::CreateLinkPreviewTriggerer() {
  return GetContentClient()->renderer()->CreateLinkPreviewTriggerer();
}

void RenderFrameImpl::ResetMembersUsedForDurationOfCommit() {
  pending_loader_factories_ = nullptr;
  pending_code_cache_host_.reset();
  pending_cookie_manager_info_.reset();
  pending_storage_info_.reset();
  is_requesting_navigation_ = false;
}

}  // namespace content
