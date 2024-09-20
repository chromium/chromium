/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/frame/local_frame.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "services/network/public/mojom/source_location.mojom-blink.h"
#include "skia/public/mojom/skcolor.mojom-blink.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/common/loader/lcp_critical_path_predictor_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/reporting_observer.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-shared.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom-blink.h"
#include "third_party/blink/public/mojom/script_source_location.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_background_resource_fetch_assets.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_content_capture_client.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_link_preview_triggerer.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_producer.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/css/background_color_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/box_shadow_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/clip_path_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/child_frame_disconnector.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/ignore_opens_during_unload_count_incrementer.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/serializers/create_markup_options.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_controller.h"
#include "third_party/blink/renderer/core/editing/surrounding_text.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/frame_overlay.h"
#include "third_party/blink/renderer/core/frame/frame_serializer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_mojo_handler.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/pausable_script_executor.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/smart_clip.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/frame/virtual_keyboard_overlay_changed_observer.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/fullscreen/scoped_allow_fullscreen.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_reporter.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/layout/anchor_position_visibility_observer.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/loader/prerender_handle.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/drag_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/core/page/plugin_script_forbidden_scope.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/scroll/scroll_snapshot_client.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/back_forward_cache_utils.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_histogram_accumulator.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/mhtml/serialized_resource.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

#if BUILDFLAG(IS_MAC)
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/substring_util.h"
#include "third_party/blink/renderer/platform/fonts/mac/attributed_string_type_converter.h"
#include "ui/base/mojom/attributed_string.mojom-blink.h"
#include "ui/gfx/range/range.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "third_party/blink/renderer/core/frame/window_controls_overlay_changed_delegate.h"
#endif

namespace blink {

namespace {

// Max size in bytes of the Vector used in ForceSynchronousDocumentInstall to
// buffer data before sending it to the HTML parser.
constexpr unsigned kMaxDocumentChunkSize = 1000000;

// Maintain a global (statically-allocated) hash map indexed by the the result
// of hashing the |frame_token| passed on creation of a LocalFrame object.
using LocalFramesByTokenMap = HeapHashMap<uint64_t, WeakMember<LocalFrame>>;
static LocalFramesByTokenMap& GetLocalFramesMap() {
  DEFINE_STATIC_LOCAL(Persistent<LocalFramesByTokenMap>, map,
                      (MakeGarbageCollected<LocalFramesByTokenMap>()));
  return *map;
}

// Maximum number of burst download requests allowed.
const int kBurstDownloadLimit = 10;

inline float ParentLayoutZoomFactor(LocalFrame* frame) {
  auto* parent_local_frame = DynamicTo<LocalFrame>(frame->Tree().Parent());
  return parent_local_frame ? parent_local_frame->LayoutZoomFactor() : 1;
}

inline float ParentTextZoomFactor(LocalFrame* frame) {
  auto* parent_local_frame = DynamicTo<LocalFrame>(frame->Tree().Parent());
  return parent_local_frame ? parent_local_frame->TextZoomFactor() : 1;
}

// Convert a data url to a message pipe handle that corresponds to a remote
// blob, so that it can be passed across processes.
mojo::PendingRemote<mojom::blink::Blob> DataURLToBlob(const String& data_url) {
  auto blob_data = std::make_unique<BlobData>();
  StringUTF8Adaptor data_url_utf8(data_url);
  blob_data->AppendBytes(base::as_byte_span(data_url_utf8));
  scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create(std::move(blob_data), data_url_utf8.size());
  return blob_data_handle->CloneBlobRemote();
}

RemoteFrame* SourceFrameForOptionalToken(
    const std::optional<RemoteFrameToken>& source_frame_token) {
  if (!source_frame_token)
    return nullptr;
  return RemoteFrame::FromFrameToken(source_frame_token.value());
}

void SetViewportSegmentVariablesForRect(StyleEnvironmentVariables& vars,
                                        gfx::Rect segment_rect,
                                        unsigned first_dimension,
                                        unsigned second_dimension,
                                        const ExecutionContext* context) {
  vars.SetVariable(UADefinedTwoDimensionalVariable::kViewportSegmentTop,
                   first_dimension, second_dimension,
                   StyleEnvironmentVariables::FormatPx(segment_rect.y()),
                   context);
  vars.SetVariable(UADefinedTwoDimensionalVariable::kViewportSegmentRight,
                   first_dimension, second_dimension,
                   StyleEnvironmentVariables::FormatPx(segment_rect.right()),
                   context);
  vars.SetVariable(UADefinedTwoDimensionalVariable::kViewportSegmentBottom,
                   first_dimension, second_dimension,
                   StyleEnvironmentVariables::FormatPx(segment_rect.bottom()),
                   context);
  vars.SetVariable(UADefinedTwoDimensionalVariable::kViewportSegmentLeft,
                   first_dimension, second_dimension,
                   StyleEnvironmentVariables::FormatPx(segment_rect.x()),
                   context);
  vars.SetVariable(UADefinedTwoDimensionalVariable::kViewportSegmentWidth,
                   first_dimension, second_dimension,
                   StyleEnvironmentVariables::FormatPx(segment_rect.width()),
                   context);
  vars.SetVariable(UADefinedTwoDimensionalVariable::kViewportSegmentHeight,
                   first_dimension, second_dimension,
                   StyleEnvironmentVariables::FormatPx(segment_rect.height()),
                   context);
}

mojom::blink::BlockingDetailsPtr CreateBlockingDetailsMojom(
    const FeatureAndJSLocationBlockingBFCache& blocking_details) {
  auto feature_location_to_report = mojom::blink::BlockingDetails::New();
  feature_location_to_report->feature =
      static_cast<uint32_t>(blocking_details.Feature());
  // Zero line number and column number means no source location found.
  if (blocking_details.LineNumber() > 0 &&
      blocking_details.ColumnNumber() > 0) {
    // `Url()` and `Function()` may return nullptr.
    auto source_location = mojom::blink::ScriptSourceLocation::New(
        blocking_details.Url() ? KURL(blocking_details.Url()) : KURL(),
        blocking_details.Function() ? blocking_details.Function() : "",
        blocking_details.LineNumber(), blocking_details.ColumnNumber());
    feature_location_to_report->source = std::move(source_location);
  }
  return feature_location_to_report;
}

bool IsNavigationBlockedByCoopRestrictProperties(
    const LocalFrame& accessing_frame,
    const Frame& target_frame) {
  // If the two windows are not in the same CoopRelatedGroup, we should not
  // block one window from navigating the other. This prevents restricting
  // things that were not meant to. These are the cross browsing context group
  // accesses that already existed before COOP: restrict-properties.
  // TODO(https://crbug.com/1464618): Is there actually any scenario where cross
  // browsing context group was allowed before COOP: restrict-properties? Verify
  // that we need to have this check.
  if (accessing_frame.GetPage()->CoopRelatedGroupToken() !=
      target_frame.GetPage()->CoopRelatedGroupToken()) {
    return false;
  }

  // If we're dealing with an actual COOP: restrict-properties case, then
  // compare the browsing context group tokens. If they are different, the
  // navigation should not be permitted.
  if (accessing_frame.GetPage()->BrowsingContextGroupToken() !=
      target_frame.GetPage()->BrowsingContextGroupToken()) {
    return true;
  }

  return false;
}

// TODO: b/338175253 - remove the need for this conversion
mojom::blink::StorageTypeAccessed ToMojoStorageType(
    blink::WebContentSettingsClient::StorageType storage_type) {
  switch (storage_type) {
    case blink::WebContentSettingsClient::StorageType::kDatabase:
      return mojom::blink::StorageTypeAccessed::kDatabase;
    case blink::WebContentSettingsClient::StorageType::kCacheStorage:
      return mojom::blink::StorageTypeAccessed::kCacheStorage;
    case blink::WebContentSettingsClient::StorageType::kIndexedDB:
      return mojom::blink::StorageTypeAccessed::kIndexedDB;
    case blink::WebContentSettingsClient::StorageType::kFileSystem:
      return mojom::blink::StorageTypeAccessed::kFileSystem;
    case blink::WebContentSettingsClient::StorageType::kWebLocks:
      return mojom::blink::StorageTypeAccessed::kWebLocks;
    case blink::WebContentSettingsClient::StorageType::kLocalStorage:
      return mojom::blink::StorageTypeAccessed::kLocalStorage;
    case blink::WebContentSettingsClient::StorageType::kSessionStorage:
      return mojom::blink::StorageTypeAccessed::kSessionStorage;
  }
}

}  // namespace

template class CORE_TEMPLATE_EXPORT Supplement<LocalFrame>;

// static
LocalFrame* LocalFrame::FromFrameToken(const LocalFrameToken& frame_token) {
  LocalFramesByTokenMap& local_frames_map = GetLocalFramesMap();
  auto it = local_frames_map.find(LocalFrameToken::Hasher()(frame_token));
  return it == local_frames_map.end() ? nullptr : it->value.Get();
}

void LocalFrame::Init(Frame* opener,
                      const DocumentToken& document_token,
                      std::unique_ptr<PolicyContainer> policy_container,
                      const StorageKey& storage_key,
                      ukm::SourceId document_ukm_source_id,
                      const KURL& creator_base_url) {
  if (!policy_container)
    policy_container = PolicyContainer::CreateEmpty();

  CoreInitializer::GetInstance().InitLocalFrame(*this);

  GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &LocalFrame::BindTextFragmentReceiver, WrapWeakPersistent(this)));
  DCHECK(!mojo_handler_);
  mojo_handler_ = MakeGarbageCollected<LocalFrameMojoHandler>(*this);

  SetOpenerDoNotNotify(opener);
  loader_.Init(document_token, std::move(policy_container), storage_key,
               document_ukm_source_id, creator_base_url);
}

void LocalFrame::SetView(LocalFrameView* view) {
  DCHECK(!view_ || view_ != view);
  DCHECK(!GetDocument() || !GetDocument()->IsActive());
  if (view_)
    view_->WillBeRemovedFromFrame();
  view_ = view;
}

void LocalFrame::CreateView(const gfx::Size& viewport_size,
                            const Color& background_color) {
  DCHECK(this);
  DCHECK(GetPage());

  bool is_local_root = IsLocalRoot();

  if (is_local_root && View())
    View()->SetParentVisible(false);

  SetView(nullptr);

  LocalFrameView* frame_view = nullptr;
  if (is_local_root) {
    frame_view = MakeGarbageCollected<LocalFrameView>(*this, viewport_size);

    // The layout size is set by WebViewImpl to support meta viewport
    frame_view->SetLayoutSizeFixedToFrameSize(false);
  } else {
    frame_view = MakeGarbageCollected<LocalFrameView>(*this);
  }

  SetView(frame_view);

  frame_view->UpdateBaseBackgroundColorRecursively(background_color);

  if (is_local_root)
    frame_view->SetParentVisible(true);

  // FIXME: Not clear what the right thing for OOPI is here.
  if (OwnerLayoutObject()) {
    HTMLFrameOwnerElement* owner = DeprecatedLocalOwner();
    DCHECK(owner);
    // FIXME: OOPI might lead to us temporarily lying to a frame and telling it
    // that it's owned by a FrameOwner that knows nothing about it. If we're
    // lying to this frame, don't let it clobber the existing
    // EmbeddedContentView.
    if (owner->ContentFrame() == this)
      owner->SetEmbeddedContentView(frame_view);
  }

  if (Owner()) {
    View()->SetCanHaveScrollbars(Owner()->ScrollbarMode() !=
                                 mojom::blink::ScrollbarMode::kAlwaysOff);
  }
}

LocalFrame::~LocalFrame() {
  // Verify that the LocalFrameView has been cleared as part of detaching
  // the frame owner.
  DCHECK(!view_);
  DCHECK(!frame_color_overlay_);
  if (IsAdFrame())
    InstanceCounters::DecrementCounter(InstanceCounters::kAdSubframeCounter);

  // Before this destructor runs, `DetachImpl()` must have been run.
  CHECK(did_run_detach_impl_);
}

void LocalFrame::Trace(Visitor* visitor) const {
  visitor->Trace(ad_tracker_);
  visitor->Trace(script_observer_);
  visitor->Trace(attribution_src_loader_);
  visitor->Trace(probe_sink_);
  visitor->Trace(performance_monitor_);
  visitor->Trace(idleness_detector_);
  visitor->Trace(inspector_issue_reporter_);
  visitor->Trace(inspector_trace_events_);
  visitor->Trace(loader_);
  visitor->Trace(view_);
  visitor->Trace(dom_window_);
  visitor->Trace(page_popup_owner_);
  visitor->Trace(editor_);
  visitor->Trace(selection_);
  visitor->Trace(event_handler_);
  visitor->Trace(console_);
  visitor->Trace(smooth_scroll_sequencer_);
  visitor->Trace(content_capture_manager_);
  visitor->Trace(system_clipboard_);
  visitor->Trace(virtual_keyboard_overlay_changed_observers_);
  visitor->Trace(widget_creation_observers_);
  visitor->Trace(pause_handle_receivers_);
  visitor->Trace(frame_color_overlay_);
  visitor->Trace(mojo_handler_);
  visitor->Trace(text_fragment_handler_);
  visitor->Trace(scroll_snapshot_clients_);
  visitor->Trace(saved_scroll_offsets_);
  visitor->Trace(background_color_paint_image_generator_);
  visitor->Trace(box_shadow_paint_image_generator_);
  visitor->Trace(clip_path_paint_image_generator_);
  visitor->Trace(lcpp_);
  visitor->Trace(v8_local_compile_hints_producer_);
  visitor->Trace(browser_interface_broker_proxy_);
#if !BUILDFLAG(IS_ANDROID)
  visitor->Trace(window_controls_overlay_changed_delegate_);
#endif
  Frame::Trace(visitor);
  Supplementable<LocalFrame>::Trace(visitor);
}

bool LocalFrame::IsLocalRoot() const {
  if (!Tree().Parent())
    return true;

  return Tree().Parent()->IsRemoteFrame();
}

void LocalFrame::Navigate(FrameLoadRequest& request,
                          WebFrameLoadType frame_load_type) {
  if (HTMLFrameOwnerElement* element = DeprecatedLocalOwner())
    element->CancelPendingLazyLoad();

  if (!navigation_rate_limiter().CanProceed())
    return;

  TRACE_EVENT2("navigation", "LocalFrame::Navigate", "url",
               request.GetResourceRequest().Url().GetString().Utf8(),
               "load_type", static_cast<int>(frame_load_type));

  if (request.GetClientNavigationReason() != ClientNavigationReason::kNone &&
      request.GetClientNavigationReason() !=
          ClientNavigationReason::kInitialFrameNavigation) {
    probe::FrameScheduledNavigation(this, request.GetResourceRequest().Url(),
                                    base::TimeDelta(),
                                    request.GetClientNavigationReason());
  }

  if (NavigationShouldReplaceCurrentHistoryEntry(request, frame_load_type))
    frame_load_type = WebFrameLoadType::kReplaceCurrentItem;

  const ClientNavigationReason client_redirect_reason =
      request.GetClientNavigationReason();
  loader_.StartNavigation(request, frame_load_type);

  if (client_redirect_reason != ClientNavigationReason::kNone &&
      client_redirect_reason !=
          ClientNavigationReason::kInitialFrameNavigation) {
    probe::FrameClearedScheduledNavigation(this);
  }
}

// Much of this function is redundant with the browser process
// (NavigationRequest::ShouldReplaceCurrentEntryForSameUrlNavigation), but in
// the event that this navigation is handled synchronously because it is
// same-document, we need to apply it immediately. Also, we will synchronously
// fire the NavigateEvent, which exposes whether the navigation will push or
// replace to JS.
bool LocalFrame::ShouldReplaceForSameUrlNavigation(
    const FrameLoadRequest& request) {
  const KURL& request_url = request.GetResourceRequest().Url();
  if (request_url != GetDocument()->Url()) {
    return false;
  }

  // Forms should push even to the same URL.
  if (request.Form()) {
    return false;
  }

  // Don't replace if the navigation originated from a cross-origin iframe (so
  // that cross-origin iframes can't guess the URL of this frame based on
  // whether a history entry was added).
  if (request.GetOriginWindow() &&
      !request.GetOriginWindow()->GetSecurityOrigin()->CanAccess(
          DomWindow()->GetSecurityOrigin())) {
    return false;
  }

  // WebUI URLs and non-current-tab navigations go through the OpenURL path
  // rather than the BeginNavigation path, which converts same-URL navigations
  // to reloads if not already marked replacing. Defer to the browser process
  // in those cases.
  if (SchemeRegistry::IsWebUIScheme(request_url.Protocol()) ||
      request.GetNavigationPolicy() != kNavigationPolicyCurrentTab) {
    return false;
  }

  return true;
}

bool LocalFrame::NavigationShouldReplaceCurrentHistoryEntry(
    const FrameLoadRequest& request,
    WebFrameLoadType frame_load_type) {
  if (frame_load_type != WebFrameLoadType::kStandard) {
    return false;
  }

  // When a navigation is requested via the navigation API with
  // { history: "push" } specified, this should override all implicit
  // conversions to a replacing navigation.
  if (request.ForceHistoryPush() == mojom::blink::ForceHistoryPush::kYes) {
    CHECK(!ShouldMaintainTrivialSessionHistory());
    return false;
  }

  if (ShouldMaintainTrivialSessionHistory()) {
    // TODO(http://crbug.com/1197384): We may want to assert that
    // WebFrameLoadType is never kStandard in prerendered pages before
    // commit. DCHECK can be in FrameLoader::CommitNavigation or somewhere
    // similar.
    return true;
  }

  // In most cases, we will treat a navigation to the current URL as replacing.
  if (ShouldReplaceForSameUrlNavigation(request)) {
    return true;
  }

  // Form submissions targeting another window should not replace.
  if (request.Form() && request.GetOriginWindow() != DomWindow()) {
    return false;
  }

  // If the load event has finished or the user initiated the navigation,
  // don't replace.
  if (GetDocument()->LoadEventFinished() || HasTransientUserActivation(this)) {
    return false;
  }

  // Most non-user-initiated navigations before the load event replace. The
  // exceptions are "internal" navigations (e.g., drag-and-drop triggered
  // navigations), and anchor clicks.
  if (request.GetClientNavigationReason() == ClientNavigationReason::kNone ||
      request.GetClientNavigationReason() ==
          ClientNavigationReason::kAnchorClick) {
    return false;
  }
  return true;
}

bool LocalFrame::ShouldMaintainTrivialSessionHistory() const {
  // This should be kept in sync with
  // NavigationControllerImpl::ShouldMaintainTrivialSessionHistory.
  return GetDocument()->IsPrerendering() || IsInFencedFrameTree();
}

bool LocalFrame::DetachImpl(FrameDetachType type) {
  TRACE_EVENT1("navigation", "LocalFrame::DetachImpl", "detach_type",
               static_cast<int>(type));
  std::string_view histogram_suffix =
      (type == FrameDetachType::kRemove) ? "Remove" : "Swap";
  base::ScopedUmaHistogramTimer histogram_timer(
      base::StrCat({"Navigation.LocalFrame.DetachImpl.", histogram_suffix}));
  absl::Cleanup check_post_condition = [this] {
    // This method must shutdown objects associated with it (such as
    // the `PerformanceMonitor` for local roots).
    CHECK(did_run_detach_impl_);
  };

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // BEGIN REENTRANCY SAFE BLOCK
  // Starting here, the code must be safe against reentrancy. Dispatching
  // events, et cetera can run Javascript, which can reenter Detach().
  //
  // Most cleanup code should *not* be in inside the reentrancy safe block.
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  if (IsProvisional()) {
    Frame* provisional_owner = GetProvisionalOwnerFrame();
    // Having multiple provisional frames somehow associated with the same frame
    // to potentially replace is a logic error.
    DCHECK_EQ(provisional_owner->ProvisionalFrame(), this);
    provisional_owner->SetProvisionalFrame(nullptr);
  }

  PluginScriptForbiddenScope forbid_plugin_destructor_scripting;
  // In a kSwap detach, if we have a navigation going, its moved to the frame
  // being swapped in, so we don't need to notify the client about the
  // navigation stopping here. That will be up to the provisional frame being
  // swapped in, which knows the actual state of the navigation.
  loader_.StopAllLoaders(/*abort_client=*/type == FrameDetachType::kRemove);
  // Don't allow any new child frames to load in this frame: attaching a new
  // child frame during or after detaching children results in an attached
  // frame on a detached DOM tree, which is bad.
  SubframeLoadingDisabler disabler(*GetDocument());
  // https://html.spec.whatwg.org/C/browsing-the-web.html#unload-a-document
  // The ignore-opens-during-unload counter of a Document must be incremented
  // both when unloading itself and when unloading its descendants.
  IgnoreOpensDuringUnloadCountIncrementer ignore_opens_during_unload(
      GetDocument());

  loader_.DispatchUnloadEventAndFillOldDocumentInfoIfNeeded(
      type == FrameDetachType::kSwap);
  if (evict_cached_session_storage_on_freeze_or_unload_) {
    // Evicts the cached data of Session Storage to avoid reusing old data in
    // the cache after the session storage has been modified by another renderer
    // process.
    CoreInitializer::GetInstance().EvictSessionStorageCachedData(
        GetDocument()->GetPage());
  }
  if (!Client())
    return false;

  if (!DetachChildren())
    return false;

  // Detach() needs to be called after detachChildren(), because
  // detachChildren() will trigger the unload event handlers of any child
  // frames, and those event handlers might start a new subresource load in this
  // frame which should be stopped by Detach.
  loader_.Detach();
  DomWindow()->FrameDestroyed();

  // Verify here that any LocalFrameView has been detached by now.
  if (view_ && view_->IsAttached()) {
    DCHECK(DeprecatedLocalOwner());
    DCHECK(DeprecatedLocalOwner()->OwnedEmbeddedContentView());
    DCHECK_EQ(view_, DeprecatedLocalOwner()->OwnedEmbeddedContentView());
  }
  DCHECK(!view_ || !view_->IsAttached());

  // This is the earliest that scripting can be disabled:
  // - FrameLoader::Detach() can fire XHR abort events
  // - Document::Shutdown() can dispose plugins which can run script.
  ScriptForbiddenScope forbid_script;
  if (!Client())
    return false;

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // END REENTRANCY SAFE BLOCK
  // Past this point, no script should be executed. If this method was
  // reentered, then a check for a null Client() above should have already
  // returned false.
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  DCHECK(!IsDetached());

  if (frame_color_overlay_)
    frame_color_overlay_.Release()->Destroy();

  if (IsLocalRoot()) {
    performance_monitor_->Shutdown();

    if (ad_tracker_)
      ad_tracker_->Shutdown();
    // Unregister only if this is LocalRoot because the paint_image_generator_
    // was created on LocalRoot.
    if (background_color_paint_image_generator_)
      background_color_paint_image_generator_->Shutdown();
    if (box_shadow_paint_image_generator_)
      box_shadow_paint_image_generator_->Shutdown();
    if (clip_path_paint_image_generator_)
      clip_path_paint_image_generator_->Shutdown();
    if (script_observer_) {
      script_observer_->Shutdown();
    }
  }
  idleness_detector_->Shutdown();
  if (inspector_issue_reporter_)
    probe_sink_->RemoveInspectorIssueReporter(inspector_issue_reporter_);
  if (inspector_trace_events_)
    probe_sink_->RemoveInspectorTraceEvents(inspector_trace_events_);
  inspector_task_runner_->Dispose();

  if (content_capture_manager_) {
    content_capture_manager_->Shutdown();
    content_capture_manager_ = nullptr;
  }

  if (text_fragment_handler_)
    text_fragment_handler_->DidDetachDocumentOrFrame();

  not_restored_reasons_.reset();

  DCHECK(!view_->IsAttached());
  Client()->WillBeDetached();

  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  CHECK(!view_->IsAttached());
  SetView(nullptr);

  GetEventHandlerRegistry().DidRemoveAllEventHandlers(*DomWindow());

  probe::FrameDetachedFromParent(this, type);

  supplements_.clear();
  frame_scheduler_.reset();
  mojo_handler_->DidDetachFrame();
  WeakIdentifierMap<LocalFrame>::NotifyObjectDestroyed(this);

  did_run_detach_impl_ = true;
  return true;
}

bool LocalFrame::DetachDocument() {
  return Loader().DetachDocument();
}

void LocalFrame::CheckCompleted() {
  GetDocument()->CheckCompleted();
}

BackgroundColorPaintImageGenerator*
LocalFrame::GetBackgroundColorPaintImageGenerator() {
  LocalFrame& local_root = LocalFrameRoot();
  // One background color paint worklet per root frame.
  // There is no compositor thread in certain testing environment, and we
  // should not composite background color animation in those cases.
  if (Thread::CompositorThread() &&
      !local_root.background_color_paint_image_generator_) {
    local_root.background_color_paint_image_generator_ =
        BackgroundColorPaintImageGenerator::Create(local_root);
  }
  return local_root.background_color_paint_image_generator_.Get();
}

void LocalFrame::SetBackgroundColorPaintImageGeneratorForTesting(
    BackgroundColorPaintImageGenerator* generator_for_testing) {
  LocalFrame& local_root = LocalFrameRoot();
  local_root.background_color_paint_image_generator_ = generator_for_testing;
}

BoxShadowPaintImageGenerator* LocalFrame::GetBoxShadowPaintImageGenerator() {
  // There is no compositor thread in certain testing environment, and we should
  // not composite background color animation in those cases.
  if (!Thread::CompositorThread())
    return nullptr;
  LocalFrame& local_root = LocalFrameRoot();
  // One box shadow paint worklet per root frame.
  if (!local_root.box_shadow_paint_image_generator_) {
    local_root.box_shadow_paint_image_generator_ =
        BoxShadowPaintImageGenerator::Create(local_root);
  }
  return local_root.box_shadow_paint_image_generator_.Get();
}

ClipPathPaintImageGenerator* LocalFrame::GetClipPathPaintImageGenerator() {
  LocalFrame& local_root = LocalFrameRoot();
  // One clip path paint worklet per root frame.
  if (!local_root.clip_path_paint_image_generator_) {
    local_root.clip_path_paint_image_generator_ =
        ClipPathPaintImageGenerator::Create(local_root);
  }
  return local_root.clip_path_paint_image_generator_.Get();
}

void LocalFrame::SetClipPathPaintImageGeneratorForTesting(
    ClipPathPaintImageGenerator* generator) {
  LocalFrame& local_root = LocalFrameRoot();
  local_root.clip_path_paint_image_generator_ = generator;
}

LCPCriticalPathPredictor* LocalFrame::GetLCPP() {
  if (!LcppEnabled()) {
    return nullptr;
  }

  // For now, we only attach LCPP to the outermost main frames.
  if (!IsOutermostMainFrame()) {
    return nullptr;
  }

  if (!lcpp_) {
    lcpp_ = MakeGarbageCollected<LCPCriticalPathPredictor>(*this);
  }
  return lcpp_.Get();
}

const SecurityContext* LocalFrame::GetSecurityContext() const {
  return DomWindow() ? &DomWindow()->GetSecurityContext() : nullptr;
}

// Provides a string description of the Frame as either its URL or origin if
// remote.
static String FrameDescription(const Frame& frame) {
  // URLs aren't available for RemoteFrames, so the error message uses their
  // origin instead.
  const LocalFrame* local_frame = DynamicTo<LocalFrame>(&frame);
  return local_frame
             ? "with URL '" +
                   local_frame->GetDocument()->Url().GetString().GetString() +
                   "'"
             : "with origin '" +
                   frame.GetSecurityContext()->GetSecurityOrigin()->ToString() +
                   "'";
}

void LocalFrame::PrintNavigationErrorMessage(const Frame& target_frame,
                                             const String& reason) {
  String message = "Unsafe attempt to initiate navigation for frame " +
                   FrameDescription(target_frame) + " from frame with URL '" +
                   GetDocument()->Url().GetString() + "'. " + reason + "\n";

  DomWindow()->PrintErrorMessage(message);
}

void LocalFrame::PrintNavigationWarning(const String& message) {
  console_->AddMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kWarning, message));
}

bool LocalFrame::ShouldClose() {
  // TODO(crbug.com/1407078): This should be fixed to dispatch beforeunload
  // events to both local and remote frames.
  return loader_.ShouldClose();
}

bool LocalFrame::DetachChildren() {
  DCHECK(GetDocument());
  ChildFrameDisconnector(
      *GetDocument(),
      ChildFrameDisconnector::DisconnectReason::kDisconnectParent)
      .Disconnect();
  return !!Client();
}

void LocalFrame::DidAttachDocument() {
  Document* document = GetDocument();
  DCHECK(document);
  GetEditor().Clear();
  // Clearing the event handler clears many events, but notably can ensure that
  // for a drag started on an element in a frame that was moved (likely via
  // appendChild()), the drag source will detach and stop firing drag events
  // even after the frame reattaches.
  GetEventHandler().Clear();
  Selection().DidAttachDocument(document);
  notified_color_scheme_ = false;

  smooth_scroll_sequencer_.Clear();

#if !BUILDFLAG(IS_ANDROID)
  // For PWAs with display_override "window-controls-overlay", titlebar area
  // rect bounds sent from the browser need to persist on navigation to keep the
  // UI consistent. The titlebar area rect values are set in |LocalFrame| before
  // the new document is attached. The css environment variables are needed to
  // be set for the new document.
  if (is_window_controls_overlay_visible_) {
    DocumentStyleEnvironmentVariables& vars =
        GetDocument()->GetStyleEngine().EnsureEnvironmentVariables();
    DCHECK(!vars.ResolveVariable(
        StyleEnvironmentVariables::GetVariableName(
            UADefinedVariable::kTitlebarAreaX, document->GetExecutionContext()),
        {}, false /* record_metrics */));
    SetTitlebarAreaDocumentStyleEnvironmentVariables();
  }
#endif
}

void LocalFrame::OnFirstPaint(bool text_painted, bool image_painted) {
  if (notified_color_scheme_)
    return;

  if (text_painted || image_painted) {
    // Infer the document's color scheme according to the background color, this
    // approach assumes that the background won't be changed after the first
    // text or image is painted, otherwise, the document will have a jarring
    // flash which should be avoid by most pages.
    double h, s, l;
    View()->DocumentBackgroundColor().GetHSL(h, s, l);
    GetLocalFrameHostRemote().DidInferColorScheme(
        l < 0.5 ? mojom::blink::PreferredColorScheme::kDark
                : mojom::blink::PreferredColorScheme::kLight);
    notified_color_scheme_ = true;
  }
}

bool LocalFrame::CanAccessEvent(
    const WebInputEventAttribution& attribution) const {
  switch (attribution.type()) {
    case WebInputEventAttribution::kTargetedFrame: {
      auto* frame_document = GetDocument();
      if (!frame_document)
        return false;

      Document* target_document = nullptr;
      if (auto* page = frame_document->GetPage()) {
        auto& pointer_lock_controller = page->GetPointerLockController();
        if (auto* element = pointer_lock_controller.GetElement()) {
          // If a pointer lock is held, we can expect all events to be
          // dispatched to the frame containing the locked element.
          target_document = &element->GetDocument();
        } else if (cc::ElementId element_id = attribution.target_frame_id()) {
          DOMNodeId target_document_id =
              DOMNodeIdFromCompositorElementId(element_id);
          target_document =
              DynamicTo<Document>(DOMNodeIds::NodeForId(target_document_id));
        }
      }

      if (!target_document || !target_document->domWindow())
        return false;

      return GetSecurityContext()->GetSecurityOrigin()->CanAccess(
          target_document->domWindow()->GetSecurityOrigin());
    }
    case WebInputEventAttribution::kFocusedFrame:
      return GetPage() ? GetPage()->GetFocusController().FocusedFrame() == this
                       : false;
    case WebInputEventAttribution::kUnknown:
      return false;
  }
}

void LocalFrame::Reload(WebFrameLoadType load_type) {
  DCHECK(IsReloadLoadType(load_type));
  if (!loader_.GetDocumentLoader()->GetHistoryItem())
    return;
  TRACE_EVENT1("navigation", "LocalFrame::Reload", "load_type",
               static_cast<int>(load_type));

  FrameLoadRequest request(
      DomWindow(), loader_.ResourceRequestForReload(
                       load_type, ClientRedirectPolicy::kClientRedirect));
  request.SetClientNavigationReason(ClientNavigationReason::kReload);
  probe::FrameScheduledNavigation(this, request.GetResourceRequest().Url(),
                                  base::TimeDelta(),
                                  ClientNavigationReason::kReload);
  loader_.StartNavigation(request, load_type);
  probe::FrameClearedScheduledNavigation(this);
}

LocalWindowProxy* LocalFrame::WindowProxy(DOMWrapperWorld& world) {
  return To<LocalWindowProxy>(Frame::GetWindowProxy(world));
}

LocalWindowProxy* LocalFrame::WindowProxyMaybeUninitialized(
    DOMWrapperWorld& world) {
  return To<LocalWindowProxy>(Frame::GetWindowProxyMaybeUninitialized(world));
}

LocalDOMWindow* LocalFrame::DomWindow() {
  return To<LocalDOMWindow>(dom_window_.Get());
}

const LocalDOMWindow* LocalFrame::DomWindow() const {
  return To<LocalDOMWindow>(dom_window_.Get());
}

void LocalFrame::SetDOMWindow(LocalDOMWindow* dom_window) {
  DCHECK(dom_window);
  if (DomWindow()) {
    DomWindow()->Reset();
    // SystemClipboard uses HeapMojo wrappers. HeapMojo
    // wrappers uses LocalDOMWindow (ExecutionContext) to reset the mojo
    // objects when the ExecutionContext was destroyed. So when new
    // LocalDOMWindow was set, we need to create new SystemClipboard.
    system_clipboard_ = nullptr;
  }
  GetWindowProxyManager()->ClearForNavigation();
  dom_window_ = dom_window;
  dom_window->Initialize();
  GetFrameScheduler()->SetAgentClusterId(GetAgentClusterId());
}

Document* LocalFrame::GetDocument() const {
  return DomWindow() ? DomWindow()->document() : nullptr;
}

void LocalFrame::DocumentDetached() {
  // Resets WebLinkPreviewTrigerer when the document detached as
  // WebLinkPreviewInitiator depends on document.
  is_link_preivew_triggerer_initialized_ = false;
  link_preview_triggerer_.reset();

  if (LocalFrameView* view = View()) {
    // Pagination layout may hold on to layout objects that are not part of the
    // Document's DOM. Destroy them now.
    view->DestroyPaginationLayout();
  }
}

void LocalFrame::SetPagePopupOwner(Element& owner) {
  page_popup_owner_ = &owner;
}

LayoutView* LocalFrame::ContentLayoutObject() const {
  return GetDocument() ? GetDocument()->GetLayoutView() : nullptr;
}

void LocalFrame::DidChangeVisibilityState() {
  if (GetDocument())
    GetDocument()->DidChangeVisibilityState();

  Frame::DidChangeVisibilityState();
}

void LocalFrame::AddWidgetCreationObserver(WidgetCreationObserver* observer) {
  CHECK(IsLocalRoot());
  CHECK(!GetWidgetForLocalRoot());

  widget_creation_observers_.insert(observer);
}

void LocalFrame::NotifyFrameWidgetCreated() {
  CHECK(IsLocalRoot());
  CHECK(GetWidgetForLocalRoot());

  // No need to copy `widget_creation_observers_` since we don't permit adding
  // new observers after this point.
  for (WidgetCreationObserver* observer : widget_creation_observers_) {
    observer->OnLocalRootWidgetCreated();
  }

  widget_creation_observers_.clear();
}

bool LocalFrame::IsCaretBrowsingEnabled() const {
  return GetSettings() ? GetSettings()->GetCaretBrowsingEnabled() : false;
}

void LocalFrame::HookBackForwardCacheEviction() {
  TRACE_EVENT0("blink", "LocalFrame::HookBackForwardCacheEviction");
  // Register a callback dispatched when JavaScript is executed on the frame.
  // The callback evicts the frame. If a frame is frozen by BackForwardCache,
  // the frame must not be mutated e.g., by JavaScript execution, then the
  // frame must be evicted in such cases.
  DCHECK(RuntimeEnabledFeatures::BackForwardCacheEnabled());
  static_cast<LocalWindowProxyManager*>(GetWindowProxyManager())
      ->SetAbortScriptExecution(
          [](v8::Isolate* isolate, v8::Local<v8::Context> context) {
            ScriptState* script_state = ScriptState::From(isolate, context);
            LocalDOMWindow* window = LocalDOMWindow::From(script_state);
            DCHECK(window);
            LocalFrame* frame = window->GetFrame();
            if (frame) {
              std::unique_ptr<SourceLocation> source_location = nullptr;
              if (base::FeatureList::IsEnabled(
                      features::kCaptureJSExecutionLocation)) {
                // Capture the source location of the JS execution if the flag
                // is enabled.
                source_location = CaptureSourceLocation();
              }
              frame->EvictFromBackForwardCache(
                  mojom::blink::RendererEvictionReason::kJavaScriptExecution,
                  std::move(source_location));
              if (base::FeatureList::IsEnabled(
                      features::kBackForwardCacheDWCOnJavaScriptExecution)) {
                // Adding |DumpWithoutCrashing()| here to make sure this is not
                // happening in any tests, except for when this is expected.
                base::debug::DumpWithoutCrashing();
              }
            }
          });
}

void LocalFrame::RemoveBackForwardCacheEviction() {
  TRACE_EVENT0("blink", "LocalFrame::RemoveBackForwardCacheEviction");
  DCHECK(RuntimeEnabledFeatures::BackForwardCacheEnabled());
  static_cast<LocalWindowProxyManager*>(GetWindowProxyManager())
      ->SetAbortScriptExecution(nullptr);

  // The page is being restored, and from this point eviction should not happen
  // for any reason. Change the deferring state from |kBufferIncoming| to
  // |kStrict| so that network related eviction cannot happen.
  GetDocument()->Fetcher()->SetDefersLoading(LoaderFreezeMode::kStrict);
}

void LocalFrame::SetTextDirection(base::i18n::TextDirection direction) {
  // The Editor::SetBaseWritingDirection() function checks if we can change
  // the text direction of the selected node and updates its DOM "dir"
  // attribute and its CSS "direction" property.
  // So, we just call the function as Safari does.
  Editor& editor = GetEditor();
  if (!editor.CanEdit())
    return;

  switch (direction) {
    case base::i18n::TextDirection::UNKNOWN_DIRECTION:
      editor.SetBaseWritingDirection(
          mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION);
      break;

    case base::i18n::TextDirection::LEFT_TO_RIGHT:
      editor.SetBaseWritingDirection(
          mojo_base::mojom::blink::TextDirection::LEFT_TO_RIGHT);
      break;

    case base::i18n::TextDirection::RIGHT_TO_LEFT:
      editor.SetBaseWritingDirection(
          mojo_base::mojom::blink::TextDirection::RIGHT_TO_LEFT);
      break;

    default:
      NOTIMPLEMENTED();
      break;
  }
}

void LocalFrame::SetIsInert(bool inert) {
  if (is_inert_ == inert)
    return;
  is_inert_ = inert;

  // Propagate inert to child frames
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    child->UpdateInertIfPossible();
  }

  // Nodes all over the accessibility tree can change inertness which means they
  // must be added or removed from the tree.
  if (GetDocument()) {
    GetDocument()->RefreshAccessibilityTree();
  }
}

void LocalFrame::SetInheritedEffectiveTouchAction(TouchAction touch_action) {
  if (inherited_effective_touch_action_ == touch_action)
    return;
  inherited_effective_touch_action_ = touch_action;
  GetDocument()->GetStyleEngine().MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create(
          style_change_reason::kInheritedStyleChangeFromParentFrame));
}

bool LocalFrame::BubbleLogicalScrollInParentFrame(
    mojom::blink::ScrollDirection direction,
    ui::ScrollGranularity granularity) {
  bool is_embedded_main_frame = IsMainFrame() && !IsOutermostMainFrame();
  if (is_embedded_main_frame || IsA<RemoteFrame>(Parent())) {
    GetLocalFrameHostRemote().BubbleLogicalScrollInParentFrame(direction,
                                                               granularity);
    return false;
  } else if (auto* local_parent = DynamicTo<LocalFrame>(Parent())) {
    return local_parent->BubbleLogicalScrollFromChildFrame(direction,
                                                           granularity, this);
  }

  DCHECK(IsOutermostMainFrame());
  return false;
}

bool LocalFrame::BubbleLogicalScrollFromChildFrame(
    mojom::blink::ScrollDirection direction,
    ui::ScrollGranularity granularity,
    Frame* child) {
  FrameOwner* owner = child->Owner();
  auto* owner_element = DynamicTo<HTMLFrameOwnerElement>(owner);
  DCHECK(owner_element);

  return GetEventHandler().BubblingScroll(direction, granularity,
                                          owner_element);
}

mojom::blink::SuddenTerminationDisablerType
SuddenTerminationDisablerTypeForEventType(const AtomicString& event_type) {
  if (event_type == event_type_names::kUnload) {
    return mojom::blink::SuddenTerminationDisablerType::kUnloadHandler;
  }
  if (event_type == event_type_names::kBeforeunload) {
    return mojom::blink::SuddenTerminationDisablerType::kBeforeUnloadHandler;
  }
  if (event_type == event_type_names::kPagehide) {
    return mojom::blink::SuddenTerminationDisablerType::kPageHideHandler;
  }
  if (event_type == event_type_names::kVisibilitychange) {
    return mojom::blink::SuddenTerminationDisablerType::
        kVisibilityChangeHandler;
  }
  NOTREACHED_IN_MIGRATION();
  return mojom::blink::SuddenTerminationDisablerType::kUnloadHandler;
}

int NumberOfSuddenTerminationEventListeners(const EventTarget& event_target,
                                            const AtomicString& event_type) {
  if (event_type != event_type_names::kVisibilitychange)
    return event_target.NumberOfEventListeners(event_type);
  // For visibilitychange, we need to count the number of event listeners that
  // are registered on the document and the window, as the event is initially
  // dispatched on the document but might bubble up to the window.
  // The other events (beforeunload, unload, pagehide) are dispatched on the
  // window and won't bubble up anywhere, so we don't need to check for
  // listeners the document for those events.
  int total_listeners_count = event_target.NumberOfEventListeners(event_type);
  if (auto* dom_window = event_target.ToLocalDOMWindow()) {
    // |event_target| is the window, so get the count for listeners registered
    // on the document.
    total_listeners_count +=
        dom_window->document()->NumberOfEventListeners(event_type);
  } else {
    auto* node = const_cast<EventTarget*>(&event_target)->ToNode();
    DCHECK(node);
    DCHECK(node->IsDocumentNode());
    DCHECK(node->GetDocument().domWindow());
    // |event_target| is the document, so get the count for listeners registered
    // on the window.
    total_listeners_count +=
        node->GetDocument().domWindow()->NumberOfEventListeners(event_type);
  }
  return total_listeners_count;
}

void LocalFrame::UpdateSuddenTerminationStatus(
    bool added_listener,
    mojom::blink::SuddenTerminationDisablerType disabler_type) {
  Platform::Current()->SuddenTerminationChanged(!added_listener);
  if (features::IsUnloadBlocklisted()) {
    // Block BFCache for using the unload handler. Originally unload handler was
    // not a blocklisted feature, but we make them blocklisted so the source
    // location will be captured. See https://crbug.com/1513120 for details.
    if (disabler_type ==
        mojom::blink::SuddenTerminationDisablerType::kUnloadHandler) {
      if (added_listener) {
        if (feature_handle_for_scheduler_) {
          return;
        }
        feature_handle_for_scheduler_ = GetFrameScheduler()->RegisterFeature(
            SchedulingPolicy::Feature::kUnloadHandler,
            {SchedulingPolicy::DisableBackForwardCache()});
      } else {
        feature_handle_for_scheduler_.reset();
      }
    }
  }
  GetLocalFrameHostRemote().SuddenTerminationDisablerChanged(added_listener,
                                                             disabler_type);
}

void LocalFrame::AddedSuddenTerminationDisablerListener(
    const EventTarget& event_target,
    const AtomicString& event_type) {
  if (NumberOfSuddenTerminationEventListeners(event_target, event_type) == 1) {
    // The first handler of this type was added.
    UpdateSuddenTerminationStatus(
        true, SuddenTerminationDisablerTypeForEventType(event_type));
  }
}

void LocalFrame::RemovedSuddenTerminationDisablerListener(
    const EventTarget& event_target,
    const AtomicString& event_type) {
  if (NumberOfSuddenTerminationEventListeners(event_target, event_type) == 0) {
    // The last handler of this type was removed.
    UpdateSuddenTerminationStatus(
        false, SuddenTerminationDisablerTypeForEventType(event_type));
  }
}

void LocalFrame::DidFocus() {
  GetLocalFrameHostRemote().DidFocusFrame();
}

void LocalFrame::DidChangeThemeColor(bool update_theme_color_cache) {
  if (Tree().Parent())
    return;

  if (update_theme_color_cache)
    GetDocument()->UpdateThemeColorCache();

  std::optional<Color> color = GetDocument()->ThemeColor();
  std::optional<SkColor> sk_color;
  if (color)
    sk_color = color->Rgb();

  GetLocalFrameHostRemote().DidChangeThemeColor(sk_color);
}

void LocalFrame::DidChangeBackgroundColor(SkColor4f background_color,
                                          bool color_adjust) {
  DCHECK(!Tree().Parent());
  GetLocalFrameHostRemote().DidChangeBackgroundColor(background_color,
                                                     color_adjust);
}

LocalFrame& LocalFrame::LocalFrameRoot() const {
  const LocalFrame* cur_frame = this;
  while (cur_frame && IsA<LocalFrame>(cur_frame->Parent()))
    cur_frame = To<LocalFrame>(cur_frame->Parent());

  return const_cast<LocalFrame&>(*cur_frame);
}

scoped_refptr<InspectorTaskRunner> LocalFrame::GetInspectorTaskRunner() {
  return inspector_task_runner_;
}

void LocalFrame::StartPrinting(const WebPrintParams& print_params,
                               float maximum_shrink_ratio) {
  DCHECK(!saved_scroll_offsets_);
  print_params_ = print_params;

  if (!print_params_.use_paginated_layout) {
    // Not laying out for pagination (e.g. this is a subframe, or a special
    // headers/footers document, which is generated once per page). Just set the
    // initial containing block to the default page size from print parameters.
    if (LayoutView* layout_view = View()->GetLayoutView()) {
      auto size = PhysicalSize::FromSizeFRound(
          print_params_.default_page_description.size);
      layout_view->SetInitialContainingBlockSizeForPrinting(size);
    }
  }

  SetPrinting(true, maximum_shrink_ratio);
}

void LocalFrame::StartPrintingSubLocalFrame() {
  gfx::SizeF page_size;
  // This is a subframe. Use the non-printing layout size as "pagination" size.
  if (const LayoutView* layout_view = View()->GetLayoutView()) {
    page_size =
        gfx::SizeF(layout_view->GetNonPrintingLayoutSize(kIncludeScrollbars));
  }
  WebPrintParams print_params(page_size);

  // Only the root frame is paginated.
  print_params.use_paginated_layout = false;

  StartPrinting(print_params);
}

void LocalFrame::EndPrinting() {
  RestoreScrollOffsets();
  SetPrinting(false, 0);
}

void LocalFrame::SetPrinting(bool printing, float maximum_shrink_ratio) {
  // In setting printing, we should not validate resources already cached for
  // the document.  See https://bugs.webkit.org/show_bug.cgi?id=43704
  ResourceCacheValidationSuppressor validation_suppressor(
      GetDocument()->Fetcher());

  GetDocument()->SetPrinting(printing ? Document::kPrinting
                                      : Document::kFinishingPrinting);
  View()->AdjustMediaTypeForPrinting(printing);

  if (TextAutosizer* text_autosizer = GetDocument()->GetTextAutosizer())
    text_autosizer->UpdatePageInfo();

  if (ShouldUsePaginatedLayout()) {
    View()->ForceLayoutForPagination(maximum_shrink_ratio);
  } else {
    if (LayoutView* layout_view = View()->GetLayoutView()) {
      layout_view->SetIntrinsicLogicalWidthsDirty();
      layout_view->SetNeedsLayout(layout_invalidation_reason::kPrintingChanged);
      layout_view->InvalidatePaintForViewAndDescendants();
    }
    GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kPrinting);
    View()->AdjustViewSize();

    View()->DestroyPaginationLayout();
  }

  // Subframes of the one we're printing don't lay out to the page size.
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child)) {
      if (printing) {
        child_local_frame->StartPrintingSubLocalFrame();
      } else {
        child_local_frame->EndPrinting();
      }
    }
  }

  if (auto* layout_view = View()->GetLayoutView()) {
    layout_view->AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kPrinting);
  }

  if (!printing)
    GetDocument()->SetPrinting(Document::kNotPrinting);
}

bool LocalFrame::ShouldUsePaginatedLayout() const {
  if (!GetDocument()->Printing())
    return false;

  // Only the top frame being printed may be fitted to page size.
  // Subframes should be constrained by parents only.
  // This function considers the following two kinds of frames as top frames:
  // -- frame with no parent;
  // -- frame's parent is not in printing mode.
  // For the second type, it is a bit complicated when its parent is a remote
  // frame. In such case, we can not check its document or other internal
  // status. However, if the parent is in printing mode, this frame's printing
  // must have started with |use_paginated_layout| as false in print context.
  if (auto* local_parent = DynamicTo<LocalFrame>(Tree().Parent())) {
    return !local_parent->GetDocument()->Printing();
  }
  return print_params_.use_paginated_layout;
}

void LocalFrame::StartPaintPreview() {
  SetInvalidationForCapture(true);
}

void LocalFrame::EndPaintPreview() {
  SetInvalidationForCapture(false);
}

void LocalFrame::SetInvalidationForCapture(bool capturing) {
  if (!capturing)
    RestoreScrollOffsets();

  ResourceCacheValidationSuppressor validation_suppressor(
      GetDocument()->Fetcher());

  // Subframes of the captured content should be updated.
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child)) {
      child_local_frame->SetInvalidationForCapture(capturing);
    }
  }

  auto* layout_view = View()->GetLayoutView();
  if (!layout_view) {
    return;
  }

  // Trigger a paint property update to ensure the unclipped behavior is
  // applied to the frame level scroller.
  layout_view->SetNeedsPaintPropertyUpdate();

  if (!GetPage()->GetScrollbarTheme().UsesOverlayScrollbars()) {
    // During CapturePaintPreview, the LayoutView thinks it should not have
    // scrollbars. So if scrollbars affect layout, we should force relayout
    // when entering and exiting paint preview.
    layout_view->SetNeedsLayout(layout_invalidation_reason::kPaintPreview);
  }
}

void LocalFrame::EnsureSaveScrollOffset(Node& node) {
  const auto* scrollable_area = PaintLayerScrollableArea::FromNode(node);
  if (!scrollable_area)
    return;
  if (!saved_scroll_offsets_)
    saved_scroll_offsets_ = MakeGarbageCollected<SavedScrollOffsets>();
  // Retain the first scroll offset saved for each scrollable area.
  if (!saved_scroll_offsets_->Contains(&node))
    saved_scroll_offsets_->Set(&node, scrollable_area->GetScrollOffset());
}

void LocalFrame::RestoreScrollOffsets() {
  if (!saved_scroll_offsets_)
    return;

  // Restore scroll offsets unconditionally (i.e. without clamping) in case
  // layout or view sizes haven't been updated yet.
  for (auto& entry : *saved_scroll_offsets_) {
    auto* scrollable_area = PaintLayerScrollableArea::FromNode(*entry.key);
    if (!scrollable_area)
      continue;
    scrollable_area->SetScrollOffsetUnconditionally(
        entry.value, mojom::blink::ScrollType::kProgrammatic);
  }
  saved_scroll_offsets_ = nullptr;
}

void LocalFrame::SetLayoutZoomFactor(float factor) {
  SetLayoutAndTextZoomFactors(factor, text_zoom_factor_);
}

void LocalFrame::SetTextZoomFactor(float factor) {
  SetLayoutAndTextZoomFactors(layout_zoom_factor_, factor);
}

void LocalFrame::SetLayoutAndTextZoomFactors(float layout_zoom_factor,
                                             float text_zoom_factor) {
  if (layout_zoom_factor_ == layout_zoom_factor &&
      text_zoom_factor_ == text_zoom_factor) {
    return;
  }

  Page* page = GetPage();
  if (!page)
    return;

  Document* document = GetDocument();
  if (!document)
    return;

  // Respect SVGs zoomAndPan="disabled" property in standalone SVG documents.
  // FIXME: How to handle compound documents + zoomAndPan="disabled"? Needs SVG
  // WG clarification.
  if (document->IsSVGDocument()) {
    if (!document->AccessSVGExtensions().ZoomAndPanEnabled())
      return;
  }

  bool layout_zoom_changed = (layout_zoom_factor != layout_zoom_factor_);

  layout_zoom_factor_ = layout_zoom_factor;
  text_zoom_factor_ = text_zoom_factor;

  if (!GetDocument()->StandardizedBrowserZoomEnabled()) {
    // Zoom factor will not be propagated via style resolution, it must be
    // propagated here.
    for (Frame* child = Tree().FirstChild(); child;
         child = child->Tree().NextSibling()) {
      if (auto* child_local_frame = DynamicTo<LocalFrame>(child)) {
        child_local_frame->SetLayoutAndTextZoomFactors(layout_zoom_factor_,
                                                       text_zoom_factor_);
      } else {
        DynamicTo<RemoteFrame>(child)->ZoomFactorChanged(layout_zoom_factor);
      }
    }
  }

  if (layout_zoom_changed) {
#if !BUILDFLAG(IS_ANDROID)
    MaybeUpdateWindowControlsOverlayWithNewZoomLevel();
#endif
    document->LayoutViewportWasResized();
    document->MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  }
  document->GetStyleEngine().MarkViewportStyleDirty();
  document->GetStyleEngine().MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create(style_change_reason::kZoom));
  if (View())
    View()->SetNeedsLayout();
}

void LocalFrame::MediaQueryAffectingValueChangedForLocalSubtree(
    MediaValueChange value) {
  GetDocument()->MediaQueryAffectingValueChanged(value);
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child))
      child_local_frame->MediaQueryAffectingValueChangedForLocalSubtree(value);
  }
}

void LocalFrame::ViewportSegmentsChanged(
    const WebVector<gfx::Rect>& viewport_segments) {
  if (!RuntimeEnabledFeatures::ViewportSegmentsEnabled(
          GetDocument()->GetExecutionContext())) {
    return;
  }

  DCHECK(IsLocalRoot());

  // A change in the viewport segments requires re-evaluation of media queries
  // for the local frame subtree (the segments affect the
  // "horizontal-viewport-segments" and "vertical-viewport-segments" features).
  MediaQueryAffectingValueChangedForLocalSubtree(MediaValueChange::kOther);

  // Fullscreen element has its own document and uses the viewport media queries,
  // so we need to make sure the media queries are re-evaluated.
  if (Element* fullscreen = Fullscreen::FullscreenElementFrom(*GetDocument())) {
    GetDocument()->GetStyleEngine().MarkAllElementsForStyleRecalc(
        StyleChangeReasonForTracing::Create(style_change_reason::kFullscreen));
    CSSDefaultStyleSheets::Instance()
        .RebuildFullscreenRuleSetIfMediaQueriesChanged(*fullscreen);
  }

  // Also need to update the environment variables related to viewport segments.
  UpdateViewportSegmentCSSEnvironmentVariables(viewport_segments);
}

void LocalFrame::UpdateViewportSegmentCSSEnvironmentVariables(
    const WebVector<gfx::Rect>& viewport_segments) {
  DCHECK(RuntimeEnabledFeatures::ViewportSegmentsEnabled(
      GetDocument()->GetExecutionContext()));

  // Update the variable values on the root instance so that documents that
  // are created after the values change automatically have the right values.
  UpdateViewportSegmentCSSEnvironmentVariables(
      StyleEnvironmentVariables::GetRootInstance(), viewport_segments);

  if (Element* fullscreen = Fullscreen::FullscreenElementFrom(*GetDocument())) {
    // Fullscreen has its own document so we need to update its variables as
    // well.
    UpdateViewportSegmentCSSEnvironmentVariables(
        fullscreen->GetDocument().GetStyleEngine().EnsureEnvironmentVariables(),
        viewport_segments);
  }
}

void LocalFrame::UpdateViewportSegmentCSSEnvironmentVariables(
    StyleEnvironmentVariables& vars,
    const WebVector<gfx::Rect>& viewport_segments) {
  // Unset all variables, since they will be set as a whole by the code below.
  // Since the number and configurations of the segments can change, and
  // removing variables clears all values that have previously been set,
  // we will recalculate all the values on each change.
  const UADefinedTwoDimensionalVariable vars_to_remove[] = {
      UADefinedTwoDimensionalVariable::kViewportSegmentTop,
      UADefinedTwoDimensionalVariable::kViewportSegmentRight,
      UADefinedTwoDimensionalVariable::kViewportSegmentBottom,
      UADefinedTwoDimensionalVariable::kViewportSegmentLeft,
      UADefinedTwoDimensionalVariable::kViewportSegmentWidth,
      UADefinedTwoDimensionalVariable::kViewportSegmentHeight,
  };

  ExecutionContext* context = GetDocument()->GetExecutionContext();
  for (auto var : vars_to_remove) {
    vars.RemoveVariable(var, context);
  }

  // Per [css-env-1], only set the segment variables if there is more than one.
  if (viewport_segments.size() >= 2) {
    // Iterate the segments in row-major order, setting the segment variables
    // based on x and y index.
    int current_y_position = viewport_segments[0].y();
    unsigned x_index = 0;
    unsigned y_index = 0;
    SetViewportSegmentVariablesForRect(vars, viewport_segments[0], x_index,
                                       y_index, context);
    for (size_t i = 1; i < viewport_segments.size(); i++) {
      if (viewport_segments[i].y() == current_y_position) {
        x_index++;
        SetViewportSegmentVariablesForRect(vars, viewport_segments[i], x_index,
                                           y_index, context);
      } else {
        // If there is a different y value, this is the next row so increase
        // y index and start again from 0 for x.
        y_index++;
        x_index = 0;
        current_y_position = viewport_segments[i].y();
        SetViewportSegmentVariablesForRect(vars, viewport_segments[i], x_index,
                                           y_index, context);
      }
    }
  }
}

void LocalFrame::OverrideDevicePostureForEmulation(
    mojom::blink::DevicePostureType device_posture_param) {
  mojo_handler_->OverrideDevicePostureForEmulation(device_posture_param);
}

void LocalFrame::DisableDevicePostureOverrideForEmulation() {
  mojo_handler_->DisableDevicePostureOverrideForEmulation();
}

mojom::blink::DevicePostureType LocalFrame::GetDevicePosture() {
  return mojo_handler_->GetDevicePosture();
}

double LocalFrame::DevicePixelRatio() const {
  if (!page_)
    return 0;

  double ratio = page_->InspectorDeviceScaleFactorOverride();
  ratio *= LayoutZoomFactor();
  return ratio;
}

String LocalFrame::SelectedText() const {
  return Selection().SelectedText();
}

String LocalFrame::SelectedText(const TextIteratorBehavior& behavior) const {
  return Selection().SelectedText(behavior);
}

String LocalFrame::SelectedTextForClipboard() const {
  if (!GetDocument())
    return g_empty_string;
  DCHECK(!GetDocument()->NeedsLayoutTreeUpdate());
  return Selection().SelectedTextForClipboard();
}

void LocalFrame::TextSelectionChanged(const WTF::String& selection_text,
                                      uint32_t offset,
                                      const gfx::Range& range) const {
  GetLocalFrameHostRemote().TextSelectionChanged(selection_text, offset, range);
}

PositionWithAffinity LocalFrame::PositionForPoint(
    const PhysicalOffset& frame_point) {
  HitTestLocation location(frame_point);
  HitTestResult result = GetEventHandler().HitTestResultAtLocation(location);
  return result.GetPositionForInnerNodeOrImageMapImage();
}

Document* LocalFrame::DocumentAtPoint(
    const PhysicalOffset& point_in_root_frame) {
  if (!View())
    return nullptr;

  HitTestLocation location(View()->ConvertFromRootFrame(point_in_root_frame));

  if (!ContentLayoutObject())
    return nullptr;
  HitTestResult result = GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  return result.InnerNode() ? &result.InnerNode()->GetDocument() : nullptr;
}

void LocalFrame::RemoveSpellingMarkersUnderWords(const Vector<String>& words) {
  GetSpellChecker().RemoveSpellingMarkersUnderWords(words);
}

String LocalFrame::GetLayerTreeAsTextForTesting(unsigned flags) const {
  if (!ContentLayoutObject())
    return String();

  std::unique_ptr<JSONObject> layers;
  if (!(flags & kOutputAsLayerTree)) {
    layers = View()->CompositedLayersAsJSON(static_cast<LayerTreeFlags>(flags));
  }
  return layers ? layers->ToPrettyJSONString() : String();
}

bool LocalFrame::ShouldThrottleRendering() const {
  return View() && View()->ShouldThrottleRendering();
}

LocalFrame::LocalFrame(
    LocalFrameClient* client,
    Page& page,
    FrameOwner* owner,
    Frame* parent,
    Frame* previous_sibling,
    FrameInsertType insert_type,
    const LocalFrameToken& frame_token,
    WindowAgentFactory* inheriting_agent_factory,
    InterfaceRegistry* interface_registry,
    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker> interface_broker,
    const base::TickClock* clock)
    : Frame(client,
            page,
            owner,
            parent,
            previous_sibling,
            insert_type,
            frame_token,
            client->GetDevToolsFrameToken(),
            MakeGarbageCollected<LocalWindowProxyManager>(
                page.GetAgentGroupScheduler().Isolate(),
                *this),
            inheriting_agent_factory),
      frame_scheduler_(page.GetPageScheduler()->CreateFrameScheduler(
          this,
          IsInFencedFrameTree(),
          IsMainFrame() ? FrameScheduler::FrameType::kMainFrame
                        : FrameScheduler::FrameType::kSubframe)),
      loader_(this),
      editor_(MakeGarbageCollected<Editor>(*this)),
      selection_(MakeGarbageCollected<FrameSelection>(*this)),
      event_handler_(MakeGarbageCollected<EventHandler>(*this)),
      console_(MakeGarbageCollected<FrameConsole>(*this)),
      navigation_disable_count_(0),
      in_view_source_mode_(false),
      frozen_(false),
      paused_(false),
      hidden_(false),
      layout_zoom_factor_(ParentLayoutZoomFactor(this)),
      text_zoom_factor_(ParentTextZoomFactor(this)),
      inspector_task_runner_(InspectorTaskRunner::Create(
          GetTaskRunner(TaskType::kInternalInspector))),
      interface_registry_(interface_registry
                              ? interface_registry
                              : InterfaceRegistry::GetEmptyInterfaceRegistry()),
      v8_local_compile_hints_producer_(
          MakeGarbageCollected<v8_compile_hints::V8LocalCompileHintsProducer>(
              this)),
      // TODO(https://crbug.com/352165586): Give non-null context to the proxy.
      browser_interface_broker_proxy_(nullptr /* No LocalDOMWindow yet... */) {
  auto frame_tracking_result =
      GetLocalFramesMap().insert(FrameToken::Hasher()(GetFrameToken()), this);
  CHECK(frame_tracking_result.stored_value) << "Inserting a duplicate item.";
  v8::Isolate* isolate = page.GetAgentGroupScheduler().Isolate();

  if (interface_broker.is_valid()) {  // This may be invalid in unit tests.
    browser_interface_broker_proxy_.Bind(
        std::move(interface_broker),
        page.GetAgentGroupScheduler().DefaultTaskRunner());
  }

  // There is generally one probe sink per local frame tree, so for root frames
  // we create a new child sink and for child frames we propagate one from root.
  // However, if local frame swap is performed, we don't want both frames to be
  // active at once, so a dummy probe sink is created for provisional frame and
  // swapped for that of the frame being swapped on in `SwapIn()`. Since we can
  // only know whether the frame is provisional upon `Initialize()` call which
  // does a lot of things that may potentially lead to instrumentation calls,
  // we set provisional probe sink unconditionally here, then possibly replace
  // it with that of the local root after `Initialize()`.
  probe_sink_ = MakeGarbageCollected<CoreProbeSink>();
  if (IsLocalRoot()) {
    performance_monitor_ =
        MakeGarbageCollected<PerformanceMonitor>(this, isolate);

    inspector_issue_reporter_ = MakeGarbageCollected<InspectorIssueReporter>(
        &page.GetInspectorIssueStorage());
    probe_sink_->AddInspectorIssueReporter(inspector_issue_reporter_);
    inspector_trace_events_ = MakeGarbageCollected<InspectorTraceEvents>();
    probe_sink_->AddInspectorTraceEvents(inspector_trace_events_);
    if (RuntimeEnabledFeatures::AdTaggingEnabled()) {
      ad_tracker_ = MakeGarbageCollected<AdTracker>(this);
    }
    if (blink::LcppScriptObserverEnabled()) {
      script_observer_ = MakeGarbageCollected<LCPScriptObserver>(this);
    }
  } else {
    // Inertness only needs to be updated if this frame might inherit the
    // inert state from a higher-level frame. If this is an OOPIF local root,
    // it will be updated later.
    UpdateInertIfPossible();
    UpdateInheritedEffectiveTouchActionIfPossible();
    ad_tracker_ = LocalFrameRoot().ad_tracker_;
    performance_monitor_ = LocalFrameRoot().performance_monitor_;
    script_observer_ = LocalFrameRoot().script_observer_;
  }
  idleness_detector_ = MakeGarbageCollected<IdlenessDetector>(this, clock);
  attribution_src_loader_ = MakeGarbageCollected<AttributionSrcLoader>(this);
  inspector_task_runner_->InitIsolate(isolate);

  if (IsOutermostMainFrame()) {
    intersection_state_.occlusion_state =
        mojom::blink::FrameOcclusionState::kGuaranteedNotOccluded;
  }

  DCHECK(ad_tracker_ ? RuntimeEnabledFeatures::AdTaggingEnabled()
                     : !RuntimeEnabledFeatures::AdTaggingEnabled());

  // See SubresourceFilterAgent::Initialize for why we don't set this here for
  // fenced frames.
  is_frame_created_by_ad_script_ =
      !IsMainFrame() && ad_tracker_ &&
      ad_tracker_->IsAdScriptInStack(
          AdTracker::StackType::kBottomAndTop,
          /*out_ad_script=*/&ad_script_from_frame_creation_stack_);

  Initialize();
  // Now that we know whether the frame is provisional, inherit the probe
  // sink from parent if appropriate. See comment above for more details.
  if (!IsLocalRoot() && !IsProvisional()) {
    probe_sink_ = LocalFrameRoot().probe_sink_;
    probe::FrameAttachedToParent(this, ad_script_from_frame_creation_stack_);
  }
}

FrameScheduler* LocalFrame::GetFrameScheduler() {
  return frame_scheduler_.get();
}

EventHandlerRegistry& LocalFrame::GetEventHandlerRegistry() const {
  return event_handler_->GetEventHandlerRegistry();
}

scoped_refptr<base::SingleThreadTaskRunner> LocalFrame::GetTaskRunner(
    TaskType type) {
  DCHECK(IsMainThread());
  return frame_scheduler_->GetTaskRunner(type);
}

void LocalFrame::ScheduleVisualUpdateUnlessThrottled() {
  if (ShouldThrottleRendering())
    return;
  GetPage()->Animator().ScheduleVisualUpdate(this);
}

static bool CanAccessAncestor(const SecurityOrigin& active_security_origin,
                              const Frame* target_frame) {
  // targetFrame can be 0 when we're trying to navigate a top-level frame
  // that has a 0 opener.
  if (!target_frame)
    return false;

  const bool is_local_active_origin = active_security_origin.IsLocal();
  for (const Frame* ancestor_frame = target_frame; ancestor_frame;
       ancestor_frame = ancestor_frame->Tree().Parent()) {
    const SecurityOrigin* ancestor_security_origin =
        ancestor_frame->GetSecurityContext()->GetSecurityOrigin();
    if (active_security_origin.CanAccess(ancestor_security_origin))
      return true;

    // Allow file URL descendant navigation even when
    // allowFileAccessFromFileURLs is false.
    // FIXME: It's a bit strange to special-case local origins here. Should we
    // be doing something more general instead?
    if (is_local_active_origin && ancestor_security_origin->IsLocal())
      return true;
  }

  return false;
}

bool LocalFrame::CanNavigate(const Frame& target_frame,
                             const KURL& destination_url) {
  // https://html.spec.whatwg.org/multipage/browsers.html#allowed-to-navigate
  // If source is target, then return true.
  if (&target_frame == this)
    return true;

  // Navigating window.opener cross origin, without user activation. See
  // https://crbug.com/813643.
  if (Opener() == target_frame && !HasTransientUserActivation(this) &&
      !target_frame.GetSecurityContext()->GetSecurityOrigin()->CanAccess(
          SecurityOrigin::Create(destination_url).get())) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kOpenerNavigationWithoutGesture);
  }

  // Frames from different browsing context groups in the same CoopRelatedGroup
  // should not be able navigate one another.
  if (IsNavigationBlockedByCoopRestrictProperties(*this, target_frame)) {
    return false;
  }

  if (destination_url.ProtocolIsJavaScript() &&
      (!GetSecurityContext()->GetSecurityOrigin()->CanAccess(
          target_frame.GetSecurityContext()->GetSecurityOrigin()))) {
    PrintNavigationErrorMessage(
        target_frame,
        "The frame attempting navigation must be same-origin with the target "
        "if navigating to a javascript: url");
    return false;
  }

  if (GetSecurityContext()->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kNavigation)) {
    // 'allow-top-navigation' and 'allow-top-navigation-by-user-activation'
    // allow the outermost frame navigations. They don't allow root fenced frame
    // navigations from the descendant frames.
    const bool target_is_outermost_frame =
        target_frame.IsMainFrame() &&
        !target_frame.GetPage()->IsMainFrameFencedFrameRoot();

    if (!target_frame.Tree().IsDescendantOf(this) &&
        !target_is_outermost_frame) {
      PrintNavigationErrorMessage(
          target_frame,
          IsInFencedFrameTree()
              ? "The frame attempting navigation is in a fenced frame tree, "
                "and is therefore disallowed from navigating its ancestors."
              : "The frame attempting navigation is sandboxed, and is "
                "therefore "
                "disallowed from navigating its ancestors.");
      return false;
    }

    // Sandboxed frames can also navigate popups, if the
    // 'allow-sandbox-escape-via-popup' flag is specified, or if
    // 'allow-popups' flag is specified and the popup's opener is the frame.
    if (target_is_outermost_frame && target_frame != Tree().Top() &&
        GetSecurityContext()->IsSandboxed(
            network::mojom::blink::WebSandboxFlags::
                kPropagatesToAuxiliaryBrowsingContexts) &&
        (GetSecurityContext()->IsSandboxed(
             network::mojom::blink::WebSandboxFlags::kPopups) ||
         target_frame.Opener() != this)) {
      PrintNavigationErrorMessage(
          target_frame,
          "The frame attempting navigation is sandboxed and is trying "
          "to navigate a popup, but is not the popup's opener and is not "
          "set to propagate sandboxing to popups.");
      return false;
    }

    // Top navigation is forbidden in sandboxed frames unless opted-in, and only
    // then if the ancestor chain allowed to navigate the top frame.
    // Note: We don't check root fenced frames for kTop* flags since the kTop*
    // flags imply the actual top-level page.
    if ((target_frame == Tree().Top()) &&
        !target_frame.GetPage()->IsMainFrameFencedFrameRoot()) {
      if (GetSecurityContext()->IsSandboxed(
              network::mojom::blink::WebSandboxFlags::kTopNavigation) &&
          GetSecurityContext()->IsSandboxed(
              network::mojom::blink::WebSandboxFlags::
                  kTopNavigationByUserActivation)) {
        PrintNavigationErrorMessage(
            target_frame,
            "The frame attempting navigation of the top-level window is "
            "sandboxed, but the flag of 'allow-top-navigation' or "
            "'allow-top-navigation-by-user-activation' is not set.");
        return false;
      }

      // With only 'allow-top-navigation-by-user-activation' (but not
      // 'allow-top-navigation'), top navigation requires a user gesture.
      if (GetSecurityContext()->IsSandboxed(
              network::mojom::blink::WebSandboxFlags::kTopNavigation) &&
          !GetSecurityContext()->IsSandboxed(
              network::mojom::blink::WebSandboxFlags::
                  kTopNavigationByUserActivation)) {
        // If there is no user activation, fail.
        if (!HasTransientUserActivation(this)) {
          GetLocalFrameHostRemote().DidBlockNavigation(
              destination_url, GetDocument()->Url(),
              mojom::blink::NavigationBlockedReason::
                  kRedirectWithNoUserGestureSandbox);
          PrintNavigationErrorMessage(
              target_frame,
              "The frame attempting navigation of the top-level window is "
              "sandboxed with the 'allow-top-navigation-by-user-activation' "
              "flag, but has no user activation (aka gesture). See "
              "https://www.chromestatus.com/feature/5629582019395584.");
          return false;
        }
      }

      // With only 'allow-top-navigation':
      // This is a "last line of defense" to prevent a cross-origin document
      // from escalating its own top-navigation privileges. See
      // `PolicyContainerPolicies::can_navigate_top_without_user_gesture`
      // for the cases where this would be allowed or disallowed.
      // See (crbug.com/1145553) and (crbug.com/1251790).
      if (!DomWindow()
               ->GetExecutionContext()
               ->GetPolicyContainer()
               ->GetPolicies()
               .can_navigate_top_without_user_gesture &&
          !HasStickyUserActivation()) {
        String message =
            "The frame attempting to navigate the top-level window is "
            "cross-origin and either it or one of its ancestors is not "
            "allowed to navigate the top frame.\n";
        PrintNavigationErrorMessage(target_frame, message);
        return false;
      }
      return true;
    }
  }

  DCHECK(GetSecurityContext()->GetSecurityOrigin());
  const SecurityOrigin& origin = *GetSecurityContext()->GetSecurityOrigin();

  // This is the normal case. A document can navigate its decendant frames,
  // or, more generally, a document can navigate a frame if the document is
  // in the same origin as any of that frame's ancestors (in the frame
  // hierarchy).
  //
  // See http://www.adambarth.com/papers/2008/barth-jackson-mitchell.pdf for
  // historical information about this security check.
  if (CanAccessAncestor(origin, &target_frame))
    return true;

  // Top-level frames are easier to navigate than other frames because they
  // display their URLs in the address bar (in most browsers). However, there
  // are still some restrictions on navigation to avoid nuisance attacks.
  // Specifically, a document can navigate a top-level frame if that frame
  // opened the document or if the document is the same-origin with any of
  // the top-level frame's opener's ancestors (in the frame hierarchy).
  //
  // In both of these cases, the document performing the navigation is in
  // some way related to the frame being navigate (e.g., by the "opener"
  // and/or "parent" relation). Requiring some sort of relation prevents a
  // document from navigating arbitrary, unrelated top-level frames.
  if (!target_frame.Tree().Parent()) {
    if (target_frame == Opener())
      return true;
    if (CanAccessAncestor(origin, target_frame.Opener()))
      return true;
  }

  if (target_frame == Tree().Top()) {
    // A frame navigating its top may blocked if the document initiating
    // the navigation has never received a user gesture and the navigation
    // isn't same-origin with the target.
    if (HasStickyUserActivation() ||
        target_frame.GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            SecurityOrigin::Create(destination_url).get())) {
      return true;
    }

    String target_domain = network_utils::GetDomainAndRegistry(
        target_frame.GetSecurityContext()->GetSecurityOrigin()->Domain(),
        network_utils::kIncludePrivateRegistries);
    String destination_domain = network_utils::GetDomainAndRegistry(
        destination_url.Host(), network_utils::kIncludePrivateRegistries);
    if (!target_domain.empty() && !destination_domain.empty() &&
        target_domain == destination_domain &&
        (target_frame.GetSecurityContext()->GetSecurityOrigin()->Protocol() ==
             destination_url.Protocol())) {
      return true;
    }

    if (loader_.GetDocumentLoader()->GetContentSettings()->allow_popup) {
      return true;
    }
    PrintNavigationErrorMessage(
        target_frame,
        "The frame attempting navigation is targeting its top-level window, "
        "but is neither same-origin with its target nor has it received a "
        "user gesture. See "
        "https://www.chromestatus.com/feature/5851021045661696.");
    GetLocalFrameHostRemote().DidBlockNavigation(
        destination_url, GetDocument()->Url(),
        mojom::blink::NavigationBlockedReason::kRedirectWithNoUserGesture);

  } else {
    PrintNavigationErrorMessage(
        target_frame,
        "The frame attempting navigation is neither same-origin with the "
        "target, nor is it the target's parent or opener.");
  }
  return false;
}

void LocalFrame::MaybeStartOutermostMainFrameNavigation(
    const Vector<KURL>& urls) const {
  TRACE_EVENT0("navigation",
               "LocalFrame::MaybeStartOutermostMainFrameNavigation");
  mojo_handler_->NonAssociatedLocalFrameHostRemote()
      .MaybeStartOutermostMainFrameNavigation(urls);
}

ContentCaptureManager* LocalFrame::GetOrResetContentCaptureManager() {
  DCHECK(Client());
  if (!IsLocalRoot())
    return nullptr;

  // WebContentCaptureClient is set on each navigation and it could become null
  // because the url is in disallowed list, so ContentCaptureManager
  // is created or released as needed to save the resources.
  // It is a little bit odd that ContentCaptureManager is created or released on
  // demand, and that this is something that could be improved with an explicit
  // signal for creating / destroying content capture managers.
  if (Client()->GetWebContentCaptureClient()) {
    if (!content_capture_manager_) {
      content_capture_manager_ =
          MakeGarbageCollected<ContentCaptureManager>(*this);
    }
  } else if (content_capture_manager_) {
    content_capture_manager_->Shutdown();
    content_capture_manager_ = nullptr;
  }
  return content_capture_manager_.Get();
}

BrowserInterfaceBrokerProxy& LocalFrame::GetBrowserInterfaceBroker() {
  if (!browser_interface_broker_proxy_.is_bound()) {
    // This branch is taken in unit tests.
    return GetEmptyBrowserInterfaceBroker();
  }
  return browser_interface_broker_proxy_;
}

AssociatedInterfaceProvider*
LocalFrame::GetRemoteNavigationAssociatedInterfaces() {
  DCHECK(Client());
  return Client()->GetRemoteNavigationAssociatedInterfaces();
}

LocalFrameClient* LocalFrame::Client() const {
  return static_cast<LocalFrameClient*>(Frame::Client());
}

FrameWidget* LocalFrame::GetWidgetForLocalRoot() {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(this);
  if (!web_frame)
    return nullptr;
  // This WebFrameWidgetImpl upcasts to a FrameWidget which is the interface
  // exposed to Blink core.
  return web_frame->LocalRootFrameWidget();
}

WebContentSettingsClient* LocalFrame::GetContentSettingsClient() {
  return Client() ? Client()->GetContentSettingsClient() : nullptr;
}

PluginData* LocalFrame::GetPluginData() const {
  if (!Loader().AllowPlugins())
    return nullptr;
  return GetPage()->GetPluginData();
}

void LocalFrame::SetAdTrackerForTesting(AdTracker* ad_tracker) {
  if (ad_tracker_)
    ad_tracker_->Shutdown();
  ad_tracker_ = ad_tracker;
}

DEFINE_WEAK_IDENTIFIER_MAP(LocalFrame)

FrameNavigationDisabler::FrameNavigationDisabler(LocalFrame& frame)
    : frame_(&frame) {
  frame_->DisableNavigation();
}

FrameNavigationDisabler::~FrameNavigationDisabler() {
  frame_->EnableNavigation();
}

LocalFrame::LazyLoadImageSetting LocalFrame::GetLazyLoadImageSetting() const {
  DCHECK(GetSettings());
  if (!GetSettings()->GetLazyLoadEnabled()) {
    return LocalFrame::LazyLoadImageSetting::kDisabled;
  }

  // Disable explicit and automatic lazyload for backgrounded pages including
  // NoStatePrefetch and Prerender.
  if (!GetDocument()->IsPageVisible()) {
    return LocalFrame::LazyLoadImageSetting::kDisabled;
  }

  return LocalFrame::LazyLoadImageSetting::kEnabledExplicit;
}

scoped_refptr<network::SharedURLLoaderFactory>
LocalFrame::GetURLLoaderFactory() {
  return Client()->GetURLLoaderFactory();
}

std::unique_ptr<URLLoader> LocalFrame::CreateURLLoaderForTesting() {
  return Client()->CreateURLLoaderForTesting();
}

scoped_refptr<WebBackgroundResourceFetchAssets>
LocalFrame::MaybeGetBackgroundResourceFetchAssets() {
  return Client()->MaybeGetBackgroundResourceFetchAssets();
}

WebPluginContainerImpl* LocalFrame::GetWebPluginContainer(Node* node) const {
  if (auto* plugin_document = DynamicTo<PluginDocument>(GetDocument())) {
    return plugin_document->GetPluginView();
  }
  if (!node) {
    DCHECK(GetDocument());
    node = GetDocument()->FocusedElement();
  }

  if (node) {
    return node->GetWebPluginContainer();
  }
  return nullptr;
}

void LocalFrame::WasHidden() {
  if (hidden_)
    return;
  hidden_ = true;

  if (auto* content_capture_manager = GetOrResetContentCaptureManager()) {
    content_capture_manager->OnFrameWasHidden();
  }

  // An iframe may get a "was hidden" notification before it has been attached
  // to the frame tree; in that case, skip further processing.
  if (!Owner() || IsProvisional())
    return;

  // Mark intersections as dirty, so that child frames will reevaluate their
  // render throttling status on the next lifecycle update.
  LocalFrameView* frame_view = View();
  if (frame_view)
    frame_view->SetIntersectionObservationState(LocalFrameView::kDesired);

  // If we are tracking occlusion for this frame, and it was not previously
  // known to be occluded, then we need to force "not visible" notifications to
  // be sent, since it's unknown whether this frame will run lifecycle updates.

  // Frame was already occluded, nothing more to do.
  if (intersection_state_.occlusion_state ==
      mojom::blink::FrameOcclusionState::kPossiblyOccluded) {
    return;
  }

  Document* document = GetDocument();
  if (frame_view && document && document->IsActive()) {
    if (auto* controller = GetDocument()->GetIntersectionObserverController()) {
      if (controller->NeedsOcclusionTracking()) {
        View()->ForceUpdateViewportIntersections();
      }
    }
  }
}

void LocalFrame::WasShown() {
  if (!hidden_)
    return;
  hidden_ = false;
  if (LocalFrameView* frame_view = View())
    frame_view->ScheduleAnimation();

  if (auto* content_capture_manager = GetOrResetContentCaptureManager()) {
    content_capture_manager->OnFrameWasShown();
  }
}

bool LocalFrame::ClipsContent() const {
  // A paint preview shouldn't clip to the viewport. Each frame paints to a
  // separate canvas in full to allow scrolling.
  if (GetDocument()->GetPaintPreviewState() != Document::kNotPaintingPreview) {
    return false;
  }

  if (ShouldUsePaginatedLayout()) {
    return false;
  }

  if (IsOutermostMainFrame()) {
    return GetSettings()->GetMainFrameClipsContent();
  }
  // By default clip to viewport.
  return true;
}

void LocalFrame::SetViewportIntersectionFromParent(
    const mojom::blink::ViewportIntersectionState& intersection_state) {
  DCHECK(IsLocalRoot());
  DCHECK(!IsOutermostMainFrame());
  // Notify the render frame observers when the main frame intersection or the
  // transform changes.
  if (intersection_state_.main_frame_intersection !=
          intersection_state.main_frame_intersection ||
      intersection_state_.main_frame_transform !=
          intersection_state.main_frame_transform) {
    gfx::Rect rect = intersection_state.main_frame_transform.MapRect(
        intersection_state.main_frame_intersection);

    // Return <0, 0, 0, 0> if there is no area.
    if (rect.IsEmpty())
      rect.set_origin(gfx::Point(0, 0));
    Client()->OnMainFrameIntersectionChanged(rect);
  }

  // Viewport intersection state needs to be updated when remote ancestor
  // frames and their respective scroll positions, clips, etc change.
  if (intersection_state_.viewport_intersection !=
          intersection_state.viewport_intersection ||
      intersection_state_.outermost_main_frame_size !=
          intersection_state.outermost_main_frame_size) {
    int viewport_intersect_area =
        intersection_state.viewport_intersection.size()
            .GetCheckedArea()
            .ValueOrDefault(INT_MAX);
    int outermost_main_frame_area =
        intersection_state.outermost_main_frame_size.GetCheckedArea()
            .ValueOrDefault(INT_MAX);
    float ratio = 1.0f * viewport_intersect_area / outermost_main_frame_area;
    const float ratio_threshold =
        1.0f * features::kLargeFrameSizePercentThreshold.Get() / 100;
    GetFrameScheduler()->SetVisibleAreaLarge(ratio > ratio_threshold);
  }

  // We only schedule an update if the viewport intersection or occlusion state
  // has changed; neither the viewport offset nor the compositing bounds will
  // affect IntersectionObserver.
  bool needs_update =
      intersection_state_.viewport_intersection !=
          intersection_state.viewport_intersection ||
      intersection_state_.occlusion_state != intersection_state.occlusion_state;
  intersection_state_ = intersection_state;
  if (needs_update) {
    if (LocalFrameView* frame_view = View()) {
      frame_view->SetIntersectionObservationState(LocalFrameView::kRequired);
      frame_view->ScheduleAnimation();
    }
  }
}

gfx::Size LocalFrame::GetOutermostMainFrameSize() const {
  LocalFrame& local_root = LocalFrameRoot();
  return local_root.IsOutermostMainFrame()
             ? local_root.View()->LayoutViewport()->VisibleContentRect().size()
             : local_root.intersection_state_.outermost_main_frame_size;
}

gfx::Point LocalFrame::GetOutermostMainFrameScrollPosition() const {
  LocalFrame& local_root = LocalFrameRoot();
  return local_root.IsOutermostMainFrame()
             ? gfx::ToFlooredPoint(
                   local_root.View()->LayoutViewport()->ScrollPosition())
             : local_root.intersection_state_
                   .outermost_main_frame_scroll_position;
}

void LocalFrame::SetOpener(Frame* opener_frame) {
  // Only a local frame should be able to update another frame's opener.
  DCHECK(!opener_frame || opener_frame->IsLocalFrame());

  auto* web_frame = WebFrame::FromCoreFrame(this);
  if (web_frame && Opener() != opener_frame) {
    GetLocalFrameHostRemote().DidChangeOpener(
        opener_frame
            ? std::optional<blink::LocalFrameToken>(
                  opener_frame->GetFrameToken().GetAs<LocalFrameToken>())
            : std::nullopt);
  }
  SetOpenerDoNotNotify(opener_frame);
}

mojom::blink::FrameOcclusionState LocalFrame::GetOcclusionState() const {
  if (hidden_)
    return mojom::blink::FrameOcclusionState::kPossiblyOccluded;
  if (IsLocalRoot())
    return intersection_state_.occlusion_state;
  return LocalFrameRoot().GetOcclusionState();
}

bool LocalFrame::NeedsOcclusionTracking() const {
  if (Document* document = GetDocument()) {
    if (IntersectionObserverController* controller =
            document->GetIntersectionObserverController()) {
      return controller->NeedsOcclusionTracking();
    }
  }
  return false;
}

void LocalFrame::ForceSynchronousDocumentInstall(const AtomicString& mime_type,
                                                 const SegmentedBuffer& data) {
  CHECK(GetDocument()->IsInitialEmptyDocument());
  DCHECK(!Client()->IsLocalFrameClientImpl());
  DCHECK(GetPage());

  // Any Document requires Shutdown() before detach, even the initial empty
  // document.
  GetDocument()->Shutdown();
  DomWindow()->ClearForReuse();

  Document* document = DomWindow()->InstallNewDocument(
      DocumentInit::Create()
          .WithWindow(DomWindow(), nullptr)
          .WithTypeFrom(mime_type)
          .ForPrerendering(GetPage()->IsPrerendering()));
  DCHECK_EQ(document, GetDocument());
  DocumentParser* parser = document->OpenForNavigation(
      kForceSynchronousParsing, mime_type, AtomicString("UTF-8"));

  if (RuntimeEnabledFeatures::DocumentInstallChunkingEnabled()) {
    // Some code creates a very large number of tiny chunks that show up in
    // |data|, such as InternalPopupMenu. Calling parser->AppendBytes() with
    // each tiny piece dramatically slows down document loading. By combining
    // these chunks in a Vector before passing it to parser->AppendBytes() gets
    // around this problem.
    Vector<char> current_chunk;
    for (const auto& segment : data) {
      current_chunk.AppendSpan(base::span(segment));
      if (current_chunk.size() > kMaxDocumentChunkSize) {
        parser->AppendBytes(base::as_byte_span(current_chunk));
        current_chunk.clear();
      }
    }
    parser->AppendBytes(base::as_byte_span(current_chunk));
    current_chunk.clear();
  } else {
    for (const auto& segment : data) {
      parser->AppendBytes(base::as_bytes(segment));
    }
  }

  parser->Finish();

  // Upon loading of SVGImages, log PageVisits in UseCounter if we did not
  // replace the document in `parser->Finish()`, which may happen when XSLT
  // finishes processing.
  // Do not track PageVisits for inspector, web page popups, and validation
  // message overlays (the other callers of this method).
  if (document == GetDocument() && document->IsSVGDocument())
    loader_.GetDocumentLoader()->GetUseCounter().DidCommitLoad(this);
}

bool LocalFrame::IsProvisional() const {
  // Calling this after the frame is marked as completely detached is a bug, as
  // this state can no longer be accurately calculated.
  CHECK(!IsDetached());

  if (IsMainFrame()) {
    return GetPage()->MainFrame() != this;
  }

  DCHECK(Owner());
  return Owner()->ContentFrame() != this;
}

bool LocalFrame::IsAdFrame() const {
  return ad_evidence_ && ad_evidence_->IndicatesAdFrame();
}

bool LocalFrame::IsAdRoot() const {
  return IsAdFrame() && !ad_evidence_->parent_is_ad();
}

void LocalFrame::SetAdEvidence(const FrameAdEvidence& ad_evidence) {
  DCHECK(!IsMainFrame() || IsInFencedFrameTree());
  DCHECK(ad_evidence.is_complete());

  // Once set, `is_frame_created_by_ad_script_` should not be unset.
  DCHECK(!is_frame_created_by_ad_script_ ||
         ad_evidence.created_by_ad_script() ==
             blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
  is_frame_created_by_ad_script_ =
      ad_evidence.created_by_ad_script() ==
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript;

  if (ad_evidence_.has_value()) {
    // Check that replacing with the new ad evidence doesn't violate invariants.
    // The parent frame's ad status should not change as it can only change due
    // to a cross-document commit, which would remove this child frame.
    DCHECK_EQ(ad_evidence_->parent_is_ad(), ad_evidence.parent_is_ad());

    // The most restrictive filter list result cannot become less restrictive,
    // by definition.
    DCHECK_LE(ad_evidence_->most_restrictive_filter_list_result(),
              ad_evidence.most_restrictive_filter_list_result());
  }

  bool was_ad_frame = IsAdFrame();
  bool is_ad_frame = ad_evidence.IndicatesAdFrame();
  ad_evidence_ = ad_evidence;

  if (was_ad_frame == is_ad_frame)
    return;

  if (auto* document = GetDocument()) {
    // TODO(fdoray): It is possible for the document not to be installed when
    // this method is called. Consider inheriting frame bit in the graph instead
    // of sending an IPC.
    auto* document_resource_coordinator = document->GetResourceCoordinator();
    if (document_resource_coordinator)
      document_resource_coordinator->SetIsAdFrame(is_ad_frame);
  }

  UpdateAdHighlight();
  frame_scheduler_->SetIsAdFrame(is_ad_frame);

  if (is_ad_frame) {
    UseCounter::Count(DomWindow(), WebFeature::kAdFrameDetected);
    InstanceCounters::IncrementCounter(InstanceCounters::kAdSubframeCounter);
  } else {
    InstanceCounters::DecrementCounter(InstanceCounters::kAdSubframeCounter);
  }
}

bool LocalFrame::IsAdScriptInStack() const {
  return ad_tracker_ &&
         ad_tracker_->IsAdScriptInStack(AdTracker::StackType::kBottomAndTop);
}

void LocalFrame::UpdateAdHighlight() {
  if (IsMainFrame() && !IsInFencedFrameTree())
    return;

  // TODO(bokan): Fenced frames may need some work to propagate the ad
  // highlighting setting to the inner tree.
  if (IsAdRoot() && GetPage()->GetSettings().GetHighlightAds())
    SetSubframeColorOverlay(SkColorSetARGB(128, 255, 0, 0));
  else
    SetSubframeColorOverlay(SK_ColorTRANSPARENT);
}

void LocalFrame::PauseSubresourceLoading(
    mojo::PendingReceiver<mojom::blink::PauseSubresourceLoadingHandle>
        receiver) {
  auto handle = GetFrameScheduler()->GetPauseSubresourceLoadingHandle();
  if (!handle)
    return;
  pause_handle_receivers_.Add(std::move(handle), std::move(receiver),
                              GetTaskRunner(blink::TaskType::kInternalDefault));
}

void LocalFrame::ResumeSubresourceLoading() {
  pause_handle_receivers_.Clear();
}

SmoothScrollSequencer* LocalFrame::CreateNewSmoothScrollSequence() {
  if (RuntimeEnabledFeatures::MultiSmoothScrollIntoViewEnabled()) {
    // If MultiSmoothScrollIntoView is enabled, we run smooth scrolls in
    // parallel, not in sequence.
    return nullptr;
  }
  if (!IsLocalRoot()) {
    return LocalFrameRoot().CreateNewSmoothScrollSequence();
  }

  SmoothScrollSequencer* old_sequencer = smooth_scroll_sequencer_;
  smooth_scroll_sequencer_ = MakeGarbageCollected<SmoothScrollSequencer>(*this);
  return old_sequencer;
}

void LocalFrame::ReinstateSmoothScrollSequence(
    SmoothScrollSequencer* sequencer) {
  if (!IsLocalRoot()) {
    LocalFrameRoot().ReinstateSmoothScrollSequence(sequencer);
    return;
  }

  smooth_scroll_sequencer_ = sequencer;
}

void LocalFrame::FinishedScrollSequence() {
  if (!IsLocalRoot()) {
    LocalFrameRoot().FinishedScrollSequence();
    return;
  }

  smooth_scroll_sequencer_.Clear();
}

SmoothScrollSequencer* LocalFrame::GetSmoothScrollSequencer() const {
  if (!IsLocalRoot())
    return LocalFrameRoot().GetSmoothScrollSequencer();
  return smooth_scroll_sequencer_.Get();
}

ukm::UkmRecorder* LocalFrame::GetUkmRecorder() {
  Document* document = GetDocument();
  if (!document)
    return nullptr;
  return document->UkmRecorder();
}

int64_t LocalFrame::GetUkmSourceId() {
  Document* document = GetDocument();
  if (!document)
    return ukm::kInvalidSourceId;
  return document->UkmSourceID();
}

void LocalFrame::UpdateTaskTime(base::TimeDelta time) {
  Client()->DidChangeCpuTiming(time);
}

void LocalFrame::UpdateBackForwardCacheDisablingFeatures(
    BlockingDetails details) {
  auto mojom_details = ConvertFeatureAndLocationToMojomStruct(
      *details.non_sticky_features_and_js_locations,
      *details.sticky_features_and_js_locations);
  GetBackForwardCacheControllerHostRemote()
      .DidChangeBackForwardCacheDisablingFeatures(std::move(mojom_details));
}

using BlockingDetailsList = Vector<mojom::blink::BlockingDetailsPtr>;
BlockingDetailsList LocalFrame::ConvertFeatureAndLocationToMojomStruct(
    const BFCacheBlockingFeatureAndLocations& non_sticky,
    const BFCacheBlockingFeatureAndLocations& sticky) {
  BlockingDetailsList blocking_details_list;
  for (auto feature : non_sticky.details_list) {
    auto blocking_details = CreateBlockingDetailsMojom(feature);
    blocking_details_list.push_back(std::move(blocking_details));
  }
  for (auto feature : sticky.details_list) {
    auto blocking_details = CreateBlockingDetailsMojom(feature);
    blocking_details_list.push_back(std::move(blocking_details));
  }
  return blocking_details_list;
}

const base::UnguessableToken& LocalFrame::GetAgentClusterId() const {
  if (const LocalDOMWindow* window = DomWindow()) {
    return window->GetAgentClusterID();
  }
  return base::UnguessableToken::Null();
}

void LocalFrame::OnTaskCompleted(base::TimeTicks start_time,
                                 base::TimeTicks end_time) {
  if (FrameWidget* widget = GetWidgetForLocalRoot()) {
    widget->OnTaskCompletedForFrame(start_time, end_time, this);
  }
}

void LocalFrame::MainFrameInteractive() {
  if (Page* page = GetPage()) {
    page->GetV8CrowdsourcedCompileHintsProducer().GenerateData();
  }
  constexpr bool kIsFinalData = true;
  v8_local_compile_hints_producer_->GenerateData(kIsFinalData);

  V8HistogramAccumulator::GetInstance()->GenerateDataInteractive();
}

void LocalFrame::MainFrameFirstMeaningfulPaint() {
  // Generate local compile hints early (the user might navigate away before the
  // page turns interactive). If we still reach interactive, we replace the
  // compile hints with new data.
  constexpr bool kIsFinalData = false;
  v8_local_compile_hints_producer_->GenerateData(kIsFinalData);
}

DocumentResourceCoordinator* LocalFrame::GetDocumentResourceCoordinator() {
  return CHECK_DEREF(GetDocument()).GetResourceCoordinator();
}

mojom::blink::ReportingServiceProxy* LocalFrame::GetReportingService() {
  return mojo_handler_->ReportingService();
}

mojom::blink::DevicePostureProvider* LocalFrame::GetDevicePostureProvider() {
  return mojo_handler_->DevicePostureProvider();
}

// static
void LocalFrame::NotifyUserActivation(
    LocalFrame* frame,
    mojom::blink::UserActivationNotificationType notification_type,
    bool need_browser_verification) {
  if (frame) {
    frame->NotifyUserActivation(notification_type, need_browser_verification);
  }
}

// static
bool LocalFrame::HasTransientUserActivation(LocalFrame* frame) {
  return frame ? frame->Frame::HasTransientUserActivation() : false;
}

// static
bool LocalFrame::ConsumeTransientUserActivation(
    LocalFrame* frame,
    UserActivationUpdateSource update_source) {
  return frame ? frame->ConsumeTransientUserActivation(update_source) : false;
}

void LocalFrame::NotifyUserActivation(
    mojom::blink::UserActivationNotificationType notification_type,
    bool need_browser_verification) {
  mojom::blink::UserActivationUpdateType update_type =
      need_browser_verification
          ? mojom::blink::UserActivationUpdateType::
                kNotifyActivationPendingBrowserVerification
          : mojom::blink::UserActivationUpdateType::kNotifyActivation;

  GetLocalFrameHostRemote().UpdateUserActivationState(update_type,
                                                      notification_type);
  Client()->NotifyUserActivation();
  NotifyUserActivationInFrameTree(notification_type);
}

bool LocalFrame::ConsumeTransientUserActivation(
    UserActivationUpdateSource update_source) {
  if (update_source == UserActivationUpdateSource::kRenderer) {
    GetLocalFrameHostRemote().UpdateUserActivationState(
        mojom::blink::UserActivationUpdateType::kConsumeTransientActivation,
        mojom::blink::UserActivationNotificationType::kNone);
  }
  return ConsumeTransientUserActivationInFrameTree();
}

void LocalFrame::ConsumeHistoryUserActivation() {
  // Notify the frame in the browser process, which will consume the activation
  // in all frames of the page (consistent with the loop below).
  GetLocalFrameHostRemote().DidConsumeHistoryUserActivation();
  for (Frame* node = &Tree().Top(); node; node = node->Tree().TraverseNext()) {
    if (LocalFrame* local_frame_node = DynamicTo<LocalFrame>(node)) {
      local_frame_node->history_user_activation_state_.Consume();
    }
  }
}

void LocalFrame::SetHadUserInteraction(bool had_user_interaction) {
  if (had_user_interaction) {
    history_user_activation_state_.Activate();
  } else {
    history_user_activation_state_.Clear();
  }

  DomWindow()->closewatcher_stack()->SetHadUserInteraction(
      had_user_interaction);

  GetFrameScheduler()->SetHadUserActivation(had_user_interaction);
}

namespace {

class FrameColorOverlay final : public FrameOverlay::Delegate {
 public:
  explicit FrameColorOverlay(LocalFrame* frame, SkColor color)
      : color_(color), frame_(frame) {}
  SkColor GetColorForTesting() const { return color_; }

 private:
  void PaintFrameOverlay(const FrameOverlay& frame_overlay,
                         GraphicsContext& graphics_context,
                         const gfx::Size&) const override {
    const auto* view = frame_->View();
    DCHECK(view);
    if (view->Width() == 0 || view->Height() == 0)
      return;
    ScopedPaintChunkProperties properties(
        graphics_context.GetPaintController(),
        view->GetLayoutView()->FirstFragment().LocalBorderBoxProperties(),
        frame_overlay, DisplayItem::kFrameOverlay);
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            graphics_context, frame_overlay, DisplayItem::kFrameOverlay))
      return;
    DrawingRecorder recorder(graphics_context, frame_overlay,
                             DisplayItem::kFrameOverlay,
                             gfx::Rect(view->Size()));
    gfx::RectF rect(0, 0, view->Width(), view->Height());
    graphics_context.FillRect(
        rect, Color::FromSkColor(color_),
        PaintAutoDarkMode(view->GetLayoutView()->StyleRef(),
                          DarkModeFilter::ElementRole::kBackground));
  }

  // TODO(https://crbug.com/1351544): This should be an SkColor4f or a Color.
  SkColor color_;
  Persistent<LocalFrame> frame_;
};

}  // namespace

void LocalFrame::SetReducedAcceptLanguage(
    const AtomicString& reduced_accept_language) {
  reduced_accept_language_ = reduced_accept_language;
}

template <>
struct DowncastTraits<FrameColorOverlay> {
  static bool AllowFrom(const FrameOverlay::Delegate& frame_overlay) {
    return true;
  }
};

std::optional<SkColor> LocalFrame::GetFrameOverlayColorForTesting() const {
  if (!frame_color_overlay_)
    return std::nullopt;
  return DynamicTo<FrameColorOverlay>(frame_color_overlay_->GetDelegate())
      ->GetColorForTesting();
}

void LocalFrame::SetMainFrameColorOverlay(SkColor color) {
  DCHECK(IsMainFrame() && !IsInFencedFrameTree());
  SetFrameColorOverlay(color);
}

void LocalFrame::SetSubframeColorOverlay(SkColor color) {
  DCHECK(!IsMainFrame() || IsInFencedFrameTree());
  SetFrameColorOverlay(color);
}

void LocalFrame::SetFrameColorOverlay(SkColor color) {
  if (frame_color_overlay_)
    frame_color_overlay_.Release()->Destroy();

  if (color == SK_ColorTRANSPARENT)
    return;

  frame_color_overlay_ = MakeGarbageCollected<FrameOverlay>(
      this, std::make_unique<FrameColorOverlay>(this, color));
}

void LocalFrame::UpdateFrameColorOverlayPrePaint() {
  if (frame_color_overlay_)
    frame_color_overlay_->UpdatePrePaint();
}

void LocalFrame::PaintFrameColorOverlay(GraphicsContext& context) {
  if (frame_color_overlay_)
    frame_color_overlay_->Paint(context);
}

void LocalFrame::ForciblyPurgeV8Memory() {
  DomWindow()->NotifyContextDestroyed();

  WindowProxyManager* window_proxy_manager = GetWindowProxyManager();
  window_proxy_manager->ClearForV8MemoryPurge();
  Loader().StopAllLoaders(/*abort_client=*/true);
}

void LocalFrame::OnPageLifecycleStateUpdated() {
  if (frozen_ != GetPage()->Frozen()) {
    frozen_ = GetPage()->Frozen();
    if (frozen_) {
      DidFreeze();
    } else {
      DidResume();
    }
    // The event handlers might have detached the frame.
    if (!IsAttached())
      return;
  }
  SetContextPaused(GetPage()->Paused());

  mojom::blink::FrameLifecycleState frame_lifecycle_state =
      mojom::blink::FrameLifecycleState::kRunning;
  if (GetPage()->Paused()) {
    frame_lifecycle_state = mojom::blink::FrameLifecycleState::kPaused;
  } else if (GetPage()->Frozen()) {
    frame_lifecycle_state = mojom::blink::FrameLifecycleState::kFrozen;
  }

  DomWindow()->SetLifecycleState(frame_lifecycle_state);
}

void LocalFrame::SetContextPaused(bool is_paused) {
  TRACE_EVENT0("blink", "LocalFrame::SetContextPaused");
  if (is_paused == paused_)
    return;
  paused_ = is_paused;

  if (IsLocalRoot() && (!is_paused || GetPage()->ShowPausedHudOverlay())) {
    auto* widget = GetWidgetForLocalRoot();
    if (widget) {
      const auto* debug_state = widget->GetLayerTreeDebugState();
      if (debug_state) {
        cc::LayerTreeDebugState new_debug_state = *debug_state;
        new_debug_state.debugger_paused = is_paused;
        widget->SetLayerTreeDebugState(new_debug_state);
      }
    }
  }

  GetDocument()->Fetcher()->SetDefersLoading(GetLoaderFreezeMode());
  Loader().SetDefersLoading(GetLoaderFreezeMode());
  // TODO(altimin): Move this to PageScheduler level.
  GetFrameScheduler()->SetPaused(is_paused);
}

LocalFrame* LocalFrame::GetPreviousLocalFrameForLocalSwap() {
  CHECK(IsProvisional());
  if (auto* previous_main_frame =
          GetPage()->GetPreviousMainFrameForLocalSwap()) {
    return previous_main_frame;
  }
  return DynamicTo<LocalFrame>(GetProvisionalOwnerFrame());
}

bool LocalFrame::SwapIn() {
  TRACE_EVENT0("navigation", "LocalFrame::SwapIn");
  base::ScopedUmaHistogramTimer histogram_timer("Navigation.LocalFrame.SwapIn");
  DCHECK(IsProvisional());
  WebLocalFrameClient* client = Client()->GetWebFrame()->Client();
  // Swap in `this`, which is a provisional frame to an existing frame.
  Frame* provisional_owner_frame = GetProvisionalOwnerFrame();


  // First, check if there's a previous main frame to be used for a main frame
  // LocalFrame <-> LocalFrame swap.
  Frame* previous_local_main_frame =
      GetPage()->GetPreviousMainFrameForLocalSwap();
  if (previous_local_main_frame && !previous_local_main_frame->IsDetached()) {
    // We're about to do a LocalFrame <-> LocalFrame swap for a provisional
    // main frame, where the previous main frame and the provisional main frame
    // are in different Pages. The provisional frame's owner is set to the
    // placeholder main RemoteFrame for the new Page, but we should trigger the
    // swapping out of the previous Page's main frame instead here.
    // This is because we want to preserve the behavior before RenderDocument,
    // where we would unload the previous document before the next document on
    // same-LocalFrame cross-document navigation, and also transfer some state
    // from the previous document to the new one.
    // The placeholder main RemoteFrame for the new Page will also get detached
    // so that the new main LocalFrame can be swapped in, but that will be done
    // a bit later on in `Frame::SwapImpl()`, as we don't need to transfer any
    // data from the placeholder RemoteFrame.
    CHECK(IsMainFrame());
    CHECK(previous_local_main_frame->IsLocalFrame());
    CHECK_NE(previous_local_main_frame->GetPage(), GetPage());
    CHECK(provisional_owner_frame->IsRemoteFrame());
    CHECK(!DynamicTo<RemoteFrame>(provisional_owner_frame)
               ->IsRemoteFrameHostRemoteBound());
    GetPage()->SetPreviousMainFrameForLocalSwap(nullptr);
    return client->SwapIn(WebFrame::FromCoreFrame(previous_local_main_frame));
  }

  // In all other cases, the LocalFrame would be swapped in with the provisional
  // owner frame which belongs to the same Page as `this`. The provisional owner
  // frame can be a RemoteFrame or a LocalFrame (for non-main frame
  // LocalFrame <-> LocalFrame swap cases).
  CHECK_EQ(provisional_owner_frame->GetPage(), GetPage());

  // When creating a provisional LocalFrame, a new provisional probe sink is
  // created. Whether that probe sink is going to be used differs depending
  // on the situation:
  // - For local roots, that provisional probe sink should be used, as
  //   local roots needs new probe sinks. So nothing needs to be done here.
  // - For non-local-root LocalFrame <-> LocalFrame swap, reuse the previous
  //   LocalFrame's probe sink.
  // - For other cases, reuse the local root's probe sink.
  // Note that the probes dispatched to provisional sink are lost, so no
  // events are sent before swap in or after swap out.
  if (!IsLocalRoot()) {
    if (auto* local_provisional_owner =
            DynamicTo<LocalFrame>(provisional_owner_frame)) {
      // This is doing a LocalFrame <-> LocalFrame swap, so reuse the previous
      // LocalFrame's probe sink through swapping below. The detaching/unloading
      // of the previous document is done before we swap the probe sinks. This
      // is to ensure that resources from the old document won't stay around and
      // thus won't be be captured in the newly committed document's probe sink.
      bool swap_result =
          client->SwapIn(WebFrame::FromCoreFrame(provisional_owner_frame));
      std::swap(probe_sink_, local_provisional_owner->probe_sink_);
      return swap_result;
    }

    // This is a remote -> local swap, so just use the local root's probe sink.
    probe_sink_ = LocalFrameRoot().probe_sink_;
    // For remote -> local swap, Send a frameAttached event to keep the legacy
    // behavior where we fire the frameAttached event on cross-site navigations.
    probe::FrameAttachedToParent(this, ad_script_from_frame_creation_stack_);
  }

  return client->SwapIn(WebFrame::FromCoreFrame(provisional_owner_frame));
}

void LocalFrame::Discard() {
  DomWindow()->GetScriptController().DiscardFrame();
}

void LocalFrame::LoadJavaScriptURL(const KURL& url) {
  // Protect privileged pages against bookmarklets and other JavaScript
  // manipulations.
  if (SchemeRegistry::ShouldTreatURLSchemeAsNotAllowingJavascriptURLs(
          GetSecurityContext()
              ->GetSecurityOrigin()
              ->GetOriginOrPrecursorOriginIfOpaque()
              ->Protocol())) {
    return;
  }

  // TODO(mustaq): This is called only through the user typing a javascript URL
  // into the omnibox.  See https://crbug.com/1082900
  NotifyUserActivation(
      mojom::blink::UserActivationNotificationType::kInteraction, false);
  auto* window = DomWindow();
  window->GetScriptController().ExecuteJavaScriptURL(
      url, network::mojom::CSPDisposition::DO_NOT_CHECK,
      &DOMWrapperWorld::MainWorld(window->GetIsolate()));
}

void LocalFrame::RequestExecuteScript(
    int32_t world_id,
    base::span<const WebScriptSource> sources,
    mojom::blink::UserActivationOption user_gesture,
    mojom::blink::EvaluationTiming evaluation_timing,
    mojom::blink::LoadEventBlockingOption blocking_option,
    WebScriptExecutionCallback callback,
    BackForwardCacheAware back_forward_cache_aware,
    mojom::blink::WantResultOption want_result_option,
    mojom::blink::PromiseResultOption promise_behavior) {
  DOMWrapperWorld* world;
  ExecuteScriptPolicy execute_script_policy;
  CHECK(!IsProvisional());
  if (world_id == DOMWrapperWorld::kMainWorldId) {
    world = &DOMWrapperWorld::MainWorld(ToIsolate(this));
    execute_script_policy =
        ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled;
  } else {
    world = DOMWrapperWorld::EnsureIsolatedWorld(ToIsolate(this), world_id);

    // This is to preserve the existing behavior.
    execute_script_policy =
        ExecuteScriptPolicy::kExecuteScriptWhenScriptsDisabled;
  }

  if (back_forward_cache_aware == BackForwardCacheAware::kPossiblyDisallow) {
    GetFrameScheduler()->RegisterStickyFeature(
        SchedulingPolicy::Feature::kInjectedJavascript,
        {SchedulingPolicy::DisableBackForwardCache()});
  }

  Vector<WebScriptSource> script_sources;
  script_sources.AppendSpan(sources);

  ScriptState* script_state = ToScriptState(this, *world);
  CHECK(script_state);
  PausableScriptExecutor::CreateAndRun(
      script_state, std::move(script_sources), execute_script_policy,
      user_gesture, evaluation_timing, blocking_option, want_result_option,
      promise_behavior, std::move(callback));
}

void LocalFrame::SetEvictCachedSessionStorageOnFreezeOrUnload() {
  DCHECK(RuntimeEnabledFeatures::Prerender2Enabled(
      GetDocument()->GetExecutionContext()));
  evict_cached_session_storage_on_freeze_or_unload_ = true;
}

LocalFrameToken LocalFrame::GetLocalFrameToken() const {
  return GetFrameToken().GetAs<LocalFrameToken>();
}

LoaderFreezeMode LocalFrame::GetLoaderFreezeMode() {
  if (paused_ || frozen_) {
    if (GetPage()->GetPageScheduler()->IsInBackForwardCache() &&
        IsInflightNetworkRequestBackForwardCacheSupportEnabled()) {
      return LoaderFreezeMode::kBufferIncoming;
    }
    return LoaderFreezeMode::kStrict;
  }
  return LoaderFreezeMode::kNone;
}

void LocalFrame::DidFreeze() {
  TRACE_EVENT0("blink", "LocalFrame::DidFreeze");
  DCHECK(IsAttached());
  GetDocument()->DispatchFreezeEvent();
  if (evict_cached_session_storage_on_freeze_or_unload_) {
    // Evicts the cached data of Session Storage to avoid reusing old data in
    // the cache after the session storage has been modified by another renderer
    // process.
    CoreInitializer::GetInstance().EvictSessionStorageCachedData(
        GetDocument()->GetPage());
  }
  // DispatchFreezeEvent dispatches JS events, which may detach |this|.
  if (!IsAttached())
    return;
  // TODO(fmeawad): Move the following logic to the page once we have a
  // PageResourceCoordinator in Blink. http://crbug.com/838415
  if (auto* document_resource_coordinator =
          GetDocument()->GetResourceCoordinator()) {
    document_resource_coordinator->SetLifecycleState(
        performance_manager::mojom::LifecycleState::kFrozen);
  }

  if (GetPage()->GetPageScheduler()->IsInBackForwardCache()) {
    DomWindow()->SetIsInBackForwardCache(true);
  }

  LoaderFreezeMode freeze_mode = GetLoaderFreezeMode();
  GetDocument()->Fetcher()->SetDefersLoading(freeze_mode);
  Loader().SetDefersLoading(freeze_mode);
}

void LocalFrame::DidResume() {
  TRACE_EVENT0("blink", "LocalFrame::DidResume");
  DCHECK(IsAttached());
  // Before doing anything, set the "is in BFCache" state to false. This might
  // affect calculations of other states triggered by the code below, e.g. the
  // LoaderFreezeMode.
  DomWindow()->SetIsInBackForwardCache(false);

  // TODO(yuzus): Figure out if we should call GetLoaderFreezeMode().
  GetDocument()->Fetcher()->SetDefersLoading(LoaderFreezeMode::kNone);
  Loader().SetDefersLoading(LoaderFreezeMode::kNone);

  GetDocument()->DispatchEvent(*Event::Create(event_type_names::kResume));
  // TODO(fmeawad): Move the following logic to the page once we have a
  // PageResourceCoordinator in Blink
  if (auto* document_resource_coordinator =
          GetDocument()->GetResourceCoordinator()) {
    document_resource_coordinator->SetLifecycleState(
        performance_manager::mojom::LifecycleState::kRunning);
  }

  // TODO(yuzus): Figure out where these calls should really belong.
  GetDocument()->DispatchHandleLoadStart();
  GetDocument()->DispatchHandleLoadComplete();
}

void LocalFrame::CountUseIfFeatureWouldBeBlockedByPermissionsPolicy(
    mojom::WebFeature blocked_cross_origin,
    mojom::WebFeature blocked_same_origin) {
  // Get the origin of the top-level document
  const SecurityOrigin* topOrigin =
      Tree().Top().GetSecurityContext()->GetSecurityOrigin();

  // Check if this frame is same-origin with the top-level or is in
  // a fenced frame tree.
  if (!GetSecurityContext()->GetSecurityOrigin()->CanAccess(topOrigin) ||
      IsInFencedFrameTree()) {
    // This frame is cross-origin with the top-level frame, and so would be
    // blocked without a permissions policy.
    UseCounter::Count(GetDocument(), blocked_cross_origin);
    return;
  }

  // Walk up the frame tree looking for any cross-origin embeds. Even if this
  // frame is same-origin with the top-level, if it is embedded by a cross-
  // origin frame (like A->B->A) it would be blocked without a permissions
  // policy.
  const Frame* f = this;
  while (!f->IsMainFrame()) {
    if (!f->GetSecurityContext()->GetSecurityOrigin()->CanAccess(topOrigin)) {
      UseCounter::Count(GetDocument(), blocked_same_origin);
      return;
    }
    f = f->Tree().Parent();
  }
}

void LocalFrame::FinishedLoading(FrameLoader::NavigationFinishState state) {
  DomWindow()->FinishedLoading(state);
}

void LocalFrame::UpdateFaviconURL() {
  if (!IsMainFrame())
    return;

  // The URL to the icon may be in the header. As such, only
  // ask the loader for the icon if it's finished loading.
  if (!GetDocument()->LoadEventFinished())
    return;

  int icon_types_mask =
      1 << static_cast<int>(mojom::blink::FaviconIconType::kFavicon) |
      1 << static_cast<int>(mojom::blink::FaviconIconType::kTouchIcon) |
      1 << static_cast<int>(
          mojom::blink::FaviconIconType::kTouchPrecomposedIcon);
  Vector<IconURL> icon_urls = GetDocument()->IconURLs(icon_types_mask);
  if (icon_urls.empty())
    return;

  Vector<mojom::blink::FaviconURLPtr> urls;
  urls.reserve(icon_urls.size());
  for (const auto& icon_url : icon_urls) {
    urls.push_back(mojom::blink::FaviconURL::New(
        icon_url.icon_url_, icon_url.icon_type_, icon_url.sizes_,
        icon_url.is_default_icon_));
  }
  DCHECK_EQ(icon_urls.size(), urls.size());

  GetLocalFrameHostRemote().UpdateFaviconURL(std::move(urls));

  if (GetPage())
    GetPage()->GetPageScheduler()->OnTitleOrFaviconUpdated();
}

void LocalFrame::SetIsCapturingMediaCallback(
    IsCapturingMediaCallback callback) {
  is_capturing_media_callback_ = std::move(callback);
}

bool LocalFrame::IsCapturingMedia() const {
  return is_capturing_media_callback_ ? is_capturing_media_callback_.Run()
                                      : false;
}

SystemClipboard* LocalFrame::GetSystemClipboard() {
  if (!system_clipboard_)
    system_clipboard_ = MakeGarbageCollected<SystemClipboard>(this);

  return system_clipboard_.Get();
}

void LocalFrame::WasAttachedAsLocalMainFrame() {
  mojo_handler_->WasAttachedAsLocalMainFrame();
}

void LocalFrame::EvictFromBackForwardCache(
    mojom::blink::RendererEvictionReason reason,
    std::unique_ptr<SourceLocation> source_location) {
  if (!GetPage()->GetPageScheduler()->IsInBackForwardCache())
    return;
  UMA_HISTOGRAM_ENUMERATION("BackForwardCache.Eviction.Renderer", reason);
  mojom::blink::ScriptSourceLocationPtr source = nullptr;
  if (source_location) {
    source = mojom::blink::ScriptSourceLocation::New(
        source_location->Url() ? KURL(source_location->Url()) : KURL(),
        source_location->Function() ? source_location->Function() : "",
        source_location->LineNumber(), source_location->ColumnNumber());
  }
  GetBackForwardCacheControllerHostRemote().EvictFromBackForwardCache(
      std::move(reason), std::move(source));
}

void LocalFrame::DidBufferLoadWhileInBackForwardCache(
    bool update_process_wide_count,
    size_t num_bytes) {
  DomWindow()->DidBufferLoadWhileInBackForwardCache(update_process_wide_count,
                                                    num_bytes);
}

void LocalFrame::SetScaleFactor(float scale_factor) {
  DCHECK(!GetDocument() || !GetDocument()->Printing());
  DCHECK(IsMainFrame());

  const PageScaleConstraints& constraints =
      GetPage()->GetPageScaleConstraintsSet().FinalConstraints();
  scale_factor = constraints.ClampToConstraints(scale_factor);
  if (scale_factor == GetPage()->GetVisualViewport().Scale())
    return;
  GetPage()->GetVisualViewport().SetScale(scale_factor);
}

void LocalFrame::ClosePageForTesting() {
  mojo_handler_->ClosePageForTesting();
}

void LocalFrame::SetInitialFocus(bool reverse) {
  GetDocument()->ClearFocusedElement();
  GetPage()->GetFocusController().SetInitialFocus(
      reverse ? mojom::blink::FocusType::kBackward
              : mojom::blink::FocusType::kForward);
}

#if BUILDFLAG(IS_MAC)
void LocalFrame::GetCharacterIndexAtPoint(const gfx::Point& point) {
  HitTestLocation location(View()->ViewportToFrame(gfx::Point(point)));
  HitTestResult result = GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  uint32_t index =
      Selection().CharacterIndexForPoint(result.RoundedPointInInnerNodeFrame());
  mojo_handler_->TextInputHost().GotCharacterIndexAtPoint(index);
}
#endif

#if !BUILDFLAG(IS_ANDROID)
void LocalFrame::UpdateWindowControlsOverlay(
    const gfx::Rect& bounding_rect_in_dips) {
  // The rect passed to us from content is in DIP screen space, relative to the
  // main frame, and needs to move to CSS space. This doesn't take the page's
  // zoom factor into account so we must scale by the inverse of the page zoom
  // in order to get correct CSS space coordinates. Note that when
  // use-zoom-for-dsf is enabled, WindowToViewportScalar will be the true device
  // scale factor, and LayoutZoomFactor will be the combination of the device
  // scale factor and the zoom percent of the page. It is preferable to compute
  // a rect that is slightly larger than one that would render smaller than the
  // window control overlay.
  LocalFrame& local_frame_root = LocalFrameRoot();
  const float window_to_viewport_factor =
      GetPage()->GetChromeClient().WindowToViewportScalar(&local_frame_root,
                                                          1.0f);
  const float zoom_factor = local_frame_root.LayoutZoomFactor();
  const float scale_factor = zoom_factor / window_to_viewport_factor;
  gfx::Rect window_controls_overlay_rect =
      gfx::ScaleToEnclosingRect(bounding_rect_in_dips, 1.0f / scale_factor);

  bool fire_event =
      (window_controls_overlay_rect != window_controls_overlay_rect_);
  is_window_controls_overlay_visible_ = !window_controls_overlay_rect.IsEmpty();
  window_controls_overlay_rect_ = window_controls_overlay_rect;
  window_controls_overlay_rect_in_dips_ = bounding_rect_in_dips;

  DocumentStyleEnvironmentVariables& vars =
      GetDocument()->GetStyleEngine().EnsureEnvironmentVariables();

  if (is_window_controls_overlay_visible_) {
    SetTitlebarAreaDocumentStyleEnvironmentVariables();
  } else {
    const UADefinedVariable vars_to_remove[] = {
        UADefinedVariable::kTitlebarAreaX,
        UADefinedVariable::kTitlebarAreaY,
        UADefinedVariable::kTitlebarAreaWidth,
        UADefinedVariable::kTitlebarAreaHeight,
    };
    for (auto var_to_remove : vars_to_remove) {
      vars.RemoveVariable(var_to_remove);
    }
  }

  if (fire_event && window_controls_overlay_changed_delegate_) {
    window_controls_overlay_changed_delegate_->WindowControlsOverlayChanged(
        window_controls_overlay_rect_);
  }
}

void LocalFrame::RegisterWindowControlsOverlayChangedDelegate(
    WindowControlsOverlayChangedDelegate* delegate) {
  window_controls_overlay_changed_delegate_ = delegate;
}
#endif

HitTestResult LocalFrame::HitTestResultForVisualViewportPos(
    const gfx::Point& pos_in_viewport) {
  gfx::Point root_frame_point(
      GetPage()->GetVisualViewport().ViewportToRootFrame(pos_in_viewport));
  HitTestLocation location(View()->ConvertFromRootFrame(root_frame_point));
  HitTestResult result = GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  result.SetToShadowHostIfInUAShadowRoot();
  return result;
}

void LocalFrame::DidChangeVisibleToHitTesting() {
  // LayoutEmbeddedContent does not propagate style updates to descendants.
  // Need to update the field manually.
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    child->UpdateVisibleToHitTesting();
  }

  // The transform property tree node depends on visibility.
  if (auto* view = View()->GetLayoutView()) {
    view->SetNeedsPaintPropertyUpdate();
  }
}

WebPrescientNetworking* LocalFrame::PrescientNetworking() {
  if (!prescient_networking_) {
    WebLocalFrameImpl* web_local_frame = WebLocalFrameImpl::FromFrame(this);
    // There is no valid WebLocalFrame, return a nullptr to ignore pre* hints.
    if (!web_local_frame)
      return nullptr;
    prescient_networking_ =
        web_local_frame->Client()->CreatePrescientNetworking();
  }
  return prescient_networking_.get();
}

void LocalFrame::SetPrescientNetworkingForTesting(
    std::unique_ptr<WebPrescientNetworking> prescient_networking) {
  prescient_networking_ = std::move(prescient_networking);
}

mojom::blink::LocalFrameHost& LocalFrame::GetLocalFrameHostRemote() const {
  return mojo_handler_->LocalFrameHostRemote();
}

mojom::blink::BackForwardCacheControllerHost&
LocalFrame::GetBackForwardCacheControllerHostRemote() {
  return mojo_handler_->BackForwardCacheControllerHostRemote();
}

void LocalFrame::NotifyUserActivation(
    mojom::blink::UserActivationNotificationType notification_type) {
  NotifyUserActivation(notification_type, false);
}

void LocalFrame::RegisterVirtualKeyboardOverlayChangedObserver(
    VirtualKeyboardOverlayChangedObserver* observer) {
  virtual_keyboard_overlay_changed_observers_.insert(observer);
}

void LocalFrame::NotifyVirtualKeyboardOverlayRectObservers(
    const gfx::Rect& rect) const {
  HeapVector<Member<VirtualKeyboardOverlayChangedObserver>, 32> observers(
      virtual_keyboard_overlay_changed_observers_);
  for (VirtualKeyboardOverlayChangedObserver* observer : observers)
    observer->VirtualKeyboardOverlayChanged(rect);
}

void LocalFrame::AddInspectorIssue(AuditsIssue info) {
  if (GetPage()) {
    GetPage()->GetInspectorIssueStorage().AddInspectorIssue(DomWindow(),
                                                            std::move(info));
  }
}

void LocalFrame::CopyImageAtViewportPoint(const gfx::Point& viewport_point) {
  HitTestResult result = HitTestResultForVisualViewportPos(viewport_point);
  if (!IsA<HTMLCanvasElement>(result.InnerNodeOrImageMapImage()) &&
      result.AbsoluteImageURL().IsEmpty()) {
    // There isn't actually an image at these coordinates.  Might be because
    // the window scrolled while the context menu was open or because the page
    // changed itself between when we thought there was an image here and when
    // we actually tried to retrieve the image.
    //
    // FIXME: implement a cache of the most recent HitTestResult to avoid having
    //        to do two hit tests.
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  GetEditor().CopyImage(result);
}

void LocalFrame::SaveImageAt(const gfx::Point& window_point) {
  gfx::Point viewport_position =
      GetWidgetForLocalRoot()->DIPsToRoundedBlinkSpace(window_point);
  Node* node = HitTestResultForVisualViewportPos(viewport_position)
                   .InnerNodeOrImageMapImage();
  if (!node || !(IsA<HTMLCanvasElement>(*node) || IsA<HTMLImageElement>(*node)))
    return;

  String url = To<Element>(*node).ImageSourceURL();
  if (!KURL(NullURL(), url).ProtocolIsData())
    return;

  auto params = mojom::blink::DownloadURLParams::New();
  params->is_context_menu_save = true;
  params->data_url_blob = DataURLToBlob(url);
  GetLocalFrameHostRemote().DownloadURL(std::move(params));
}

void LocalFrame::MediaPlayerActionAtViewportPoint(
    const gfx::Point& viewport_position,
    const mojom::blink::MediaPlayerActionType type,
    bool enable) {
  HitTestResult result = HitTestResultForVisualViewportPos(viewport_position);
  Node* node = result.InnerNode();
  if (!IsA<HTMLVideoElement>(*node) && !IsA<HTMLAudioElement>(*node))
    return;

  auto* media_element = To<HTMLMediaElement>(node);
  switch (type) {
    case mojom::blink::MediaPlayerActionType::kLoop:
      media_element->SetLoop(enable);
      break;
    case mojom::blink::MediaPlayerActionType::kControls:
      media_element->SetUserWantsControlsVisible(enable);
      break;
    case mojom::blink::MediaPlayerActionType::kSaveVideoFrameAs:
      if (auto* video = DynamicTo<HTMLVideoElement>(media_element); video) {
        auto image = video->CreateStaticBitmapImage();
        if (!image) {
          return;
        }
        auto data_buffer = ImageDataBuffer::Create(image);
        if (!data_buffer) {
          return;
        }

        ImageEncodingMimeType encoding_mime_type =
            ImageEncoderUtils::ToEncodingMimeType(
                "image/png", ImageEncoderUtils::kEncodeReasonToDataURL);
        String data_url =
            data_buffer->ToDataURL(encoding_mime_type, /*quality=*/0);

        auto params = mojom::blink::DownloadURLParams::New();
        params->is_context_menu_save = true;
        // Suggested name always starts with "videoframe_", plus the timestamp
        // of the video frame in milliseconds.
        auto timestamp_ms = base::saturated_cast<uint32_t>(
            media_element->currentTime() * base::Time::kMillisecondsPerSecond);
        params->suggested_name = "videoframe_" + String::Number(timestamp_ms);
        params->data_url_blob = DataURLToBlob(data_url);
        GetLocalFrameHostRemote().DownloadURL(std::move(params));
      }
      break;
    case mojom::blink::MediaPlayerActionType::kCopyVideoFrame:
      if (auto* video = DynamicTo<HTMLVideoElement>(media_element); video) {
        auto image = video->CreateStaticBitmapImage();
        if (image) {
          GetEditor().CopyImage(result, image);
        }
      }
      break;
    case mojom::blink::MediaPlayerActionType::kPictureInPicture:
      if (auto* video = DynamicTo<HTMLVideoElement>(media_element); video) {
        if (enable) {
          PictureInPictureController::From(node->GetDocument())
              .EnterPictureInPicture(video, /*promise=*/nullptr);
        } else {
          PictureInPictureController::From(node->GetDocument())
              .ExitPictureInPicture(video, nullptr);
        }
      }
      break;
  }
}

void LocalFrame::RequestVideoFrameAtWithBoundsHint(
    const gfx::Point& viewport_position,
    const gfx::Size& max_size,
    int max_area,
    base::OnceCallback<void(const SkBitmap&, const gfx::Rect&)> callback) {
  HitTestResult result = HitTestResultForVisualViewportPos(viewport_position);
  Node* node = result.InnerNode();
  auto* video = DynamicTo<HTMLVideoElement>(node);

  if (!video) {
    std::move(callback).Run(SkBitmap(), gfx::Rect());
    return;
  }

  // Scale to match the max dimensions if needed, to reduce data sent over IPC.
  // This is to match the algorithm in gfx::ResizedImageForMaxDimensions().
  // TODO(crbug.com/1508722): Revisit to see whether we need both `max_size` and
  // `max_area`, which seems redundant.
  auto size = video->BitmapSourceSize();
  if ((size.width() > max_size.width() || size.height() > max_size.height()) &&
      size.GetArea() > max_area) {
    double scale =
        std::min(static_cast<double>(max_size.width()) / size.width(),
                 static_cast<double>(max_size.height()) / size.height());
    int width = std::clamp<int>(scale * size.width(), 1, max_size.width());
    int height = std::clamp<int>(scale * size.height(), 1, max_size.height());
    size = gfx::Size(width, height);
  }

  auto image =
      video->CreateStaticBitmapImage(/*allow_accelerated_images=*/true, size);
  if (!image) {
    std::move(callback).Run(SkBitmap(), gfx::Rect());
    return;
  }

  auto bitmap = image->AsSkBitmapForCurrentFrame(kRespectImageOrientation);

  // Only kN32_SkColorType bitmaps can be sent across IPC, so convert if
  // necessary.
  SkBitmap converted_bitmap;
  if (bitmap.colorType() == kN32_SkColorType) {
    converted_bitmap = bitmap;
  } else {
    SkImageInfo info = bitmap.info().makeColorType(kN32_SkColorType);
    if (converted_bitmap.tryAllocPixels(info)) {
      bitmap.readPixels(info, converted_bitmap.getPixels(),
                        converted_bitmap.rowBytes(), 0, 0);
    }
  }

  // Get the bounds of the video element.
  WebNode web_node(node);
  WebElement web_element = web_node.To<WebElement>();
  auto bounds = web_element.BoundsInWidget();

  std::move(callback).Run(converted_bitmap, bounds);
}

void LocalFrame::DownloadURL(
    const ResourceRequest& request,
    network::mojom::blink::RedirectMode cross_origin_redirect_behavior) {
  mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token;
  if (request.Url().ProtocolIs("blob")) {
    DomWindow()->GetPublicURLManager().Resolve(
        request.Url(), blob_url_token.InitWithNewPipeAndPassReceiver());
  }

  DownloadURL(request, cross_origin_redirect_behavior,
              std::move(blob_url_token));
}

void LocalFrame::DownloadURL(
    const ResourceRequest& request,
    network::mojom::blink::RedirectMode cross_origin_redirect_behavior,
    mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token) {
  if (ShouldThrottleDownload())
    return;

  auto params = mojom::blink::DownloadURLParams::New();
  const KURL& url = request.Url();
  // Pass data URL through blob.
  if (url.ProtocolIs("data")) {
    params->url = KURL();
    params->data_url_blob = DataURLToBlob(url.GetString());
  } else {
    params->url = url;
  }

  params->referrer = mojom::blink::Referrer::New();
  params->referrer->url = KURL(request.ReferrerString());
  params->referrer->policy = request.GetReferrerPolicy();
  params->initiator_origin = request.RequestorOrigin();
  if (request.GetSuggestedFilename().has_value())
    params->suggested_name = *request.GetSuggestedFilename();
  params->cross_origin_redirects = cross_origin_redirect_behavior;
  params->blob_url_token = std::move(blob_url_token);
  params->has_user_gesture = request.HasUserGesture();

  GetLocalFrameHostRemote().DownloadURL(std::move(params));
}

void LocalFrame::AdvanceFocusForIME(mojom::blink::FocusType focus_type) {
  auto* focused_frame = GetPage()->GetFocusController().FocusedFrame();
  if (focused_frame != this)
    return;

  DCHECK(GetDocument());
  Element* element = GetDocument()->FocusedElement();
  if (!element)
    return;

  Element* next_element =
      GetPage()->GetFocusController().NextFocusableElementForImeAndAutofill(
          element, focus_type);
  if (!next_element)
    return;

  next_element->scrollIntoViewIfNeeded(true /*centerIfNeeded*/);
  next_element->Focus(FocusParams(FocusTrigger::kUserGesture));
}

void LocalFrame::PostMessageEvent(
    const std::optional<RemoteFrameToken>& source_frame_token,
    const String& source_origin,
    const String& target_origin,
    BlinkTransferableMessage message) {
  TRACE_EVENT0("blink", "LocalFrame::PostMessageEvent");
  RemoteFrame* source_frame = SourceFrameForOptionalToken(source_frame_token);

  // We must pass in the target_origin to do the security check on this side,
  // since it may have changed since the original postMessage call was made.
  scoped_refptr<SecurityOrigin> target_security_origin;
  if (!target_origin.empty()) {
    target_security_origin = SecurityOrigin::CreateFromString(target_origin);
  }

  // Preparation of the MessageEvent.
  MessageEvent* message_event = MessageEvent::Create();
  DOMWindow* window = nullptr;
  if (source_frame)
    window = source_frame->DomWindow();
  MessagePortArray* ports = nullptr;
  if (GetDocument()) {
    ports = MessagePort::EntanglePorts(*GetDocument()->GetExecutionContext(),
                                       std::move(message.ports));
  }

  // The |message.user_activation| only conveys the sender |Frame|'s user
  // activation state to receiver JS.  This is never used for activating the
  // receiver (or any other) |Frame|.
  UserActivation* user_activation = nullptr;
  if (message.user_activation) {
    user_activation = MakeGarbageCollected<UserActivation>(
        message.user_activation->has_been_active,
        message.user_activation->was_active);
  }

  message_event->initMessageEvent(
      event_type_names::kMessage, false, false, std::move(message.message),
      source_origin, "" /*lastEventId*/, window, ports, user_activation,
      message.delegated_capability);

  // If the agent cluster id had a value it means this was locked when it
  // was serialized.
  if (message.locked_to_sender_agent_cluster)
    message_event->LockToAgentCluster();

  // Finally dispatch the message to the DOM Window.
  DomWindow()->DispatchMessageEventWithOriginCheck(
      target_security_origin.get(), message_event,
      std::make_unique<SourceLocation>(String(), String(), 0, 0, nullptr),
      message.sender_agent_cluster_id);
}

bool LocalFrame::ShouldThrottleDownload() {
  const auto now = base::TimeTicks::Now();
  if (num_burst_download_requests_ == 0) {
    burst_download_start_time_ = now;
  } else if (num_burst_download_requests_ >= kBurstDownloadLimit) {
    static constexpr auto kBurstDownloadLimitResetInterval = base::Seconds(1);
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

#if BUILDFLAG(IS_MAC)
void LocalFrame::ResetTextInputHostForTesting() {
  mojo_handler_->ResetTextInputHostForTesting();
}

void LocalFrame::RebindTextInputHostForTesting() {
  mojo_handler_->RebindTextInputHostForTesting();
}
#endif

Frame* LocalFrame::GetProvisionalOwnerFrame() {
  DCHECK(IsProvisional());
  if (Owner()) {
    // Since `this` is a provisional frame, its owner's `ContentFrame()` will
    // be the old LocalFrame.
    return Owner()->ContentFrame();
  }
  return GetPage()->MainFrame();
}

namespace {

// TODO(editing-dev): We should move |CreateMarkupInRect()| to
// "core/editing/serializers/Serialization.cpp".
String CreateMarkupInRect(LocalFrame* frame,
                          const gfx::Point& start_point,
                          const gfx::Point& end_point) {
  VisiblePosition start_visible_position = CreateVisiblePosition(
      PositionForContentsPointRespectingEditingBoundary(start_point, frame));
  VisiblePosition end_visible_position = CreateVisiblePosition(
      PositionForContentsPointRespectingEditingBoundary(end_point, frame));

  Position start_position = start_visible_position.DeepEquivalent();
  Position end_position = end_visible_position.DeepEquivalent();

  // document() will return null if -webkit-user-select is set to none.
  if (!start_position.GetDocument() || !end_position.GetDocument())
    return String();

  const CreateMarkupOptions create_markup_options =
      CreateMarkupOptions::Builder()
          .SetShouldAnnotateForInterchange(true)
          .SetShouldResolveURLs(kResolveNonLocalURLs)
          .Build();
  if (start_position.CompareTo(end_position) <= 0) {
    return CreateMarkup(start_position, end_position, create_markup_options);
  }
  return CreateMarkup(end_position, start_position, create_markup_options);
}

}  // namespace

void LocalFrame::ExtractSmartClipDataInternal(const gfx::Rect& rect_in_viewport,
                                              String& clip_text,
                                              String& clip_html,
                                              gfx::Rect& clip_rect) {
  // TODO(mahesh.ma): Check clip_data even after use-zoom-for-dsf is enabled.
  SmartClipData clip_data = SmartClip(this).DataForRect(rect_in_viewport);
  clip_text = clip_data.ClipData();
  clip_rect = clip_data.RectInViewport();

  gfx::Point start_point(rect_in_viewport.x(), rect_in_viewport.y());
  gfx::Point end_point(rect_in_viewport.x() + rect_in_viewport.width(),
                       rect_in_viewport.y() + rect_in_viewport.height());
  clip_html = CreateMarkupInRect(this, View()->ViewportToFrame(start_point),
                                 View()->ViewportToFrame(end_point));
}

void LocalFrame::CreateTextFragmentHandler() {
  text_fragment_handler_ = MakeGarbageCollected<TextFragmentHandler>(this);
}

void LocalFrame::BindTextFragmentReceiver(
    mojo::PendingReceiver<mojom::blink::TextFragmentReceiver> receiver) {
  if (IsDetached())
    return;

  if (!text_fragment_handler_)
    CreateTextFragmentHandler();

  text_fragment_handler_->BindTextFragmentReceiver(std::move(receiver));
}

SpellChecker& LocalFrame::GetSpellChecker() const {
  DCHECK(DomWindow());
  return DomWindow()->GetSpellChecker();
}

InputMethodController& LocalFrame::GetInputMethodController() const {
  DCHECK(DomWindow());
  return DomWindow()->GetInputMethodController();
}

TextSuggestionController& LocalFrame::GetTextSuggestionController() const {
  DCHECK(DomWindow());
  return DomWindow()->GetTextSuggestionController();
}

void LocalFrame::WriteIntoTrace(perfetto::TracedValue ctx) const {
  perfetto::TracedDictionary dict = std::move(ctx).WriteDictionary();
  dict.Add("document", GetDocument());
  dict.Add("is_main_frame", IsMainFrame());
  dict.Add("is_outermost_main_frame", IsOutermostMainFrame());
  dict.Add("is_cross_origin_to_parent", IsCrossOriginToParentOrOuterDocument());
  dict.Add("is_cross_origin_to_outermost_main_frame",
           IsCrossOriginToOutermostMainFrame());
}

mojo::PendingRemote<mojom::blink::BlobURLStore>
LocalFrame::GetBlobUrlStorePendingRemote() {
  mojo::PendingRemote<mojom::blink::BlobURLStore> pending_remote;
  GetBrowserInterfaceBroker().GetInterface(
      pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

#if !BUILDFLAG(IS_ANDROID)
void LocalFrame::SetTitlebarAreaDocumentStyleEnvironmentVariables() const {
  DCHECK(is_window_controls_overlay_visible_);
  DocumentStyleEnvironmentVariables& vars =
      GetDocument()->GetStyleEngine().EnsureEnvironmentVariables();
  vars.SetVariable(
      UADefinedVariable::kTitlebarAreaX,
      StyleEnvironmentVariables::FormatPx(window_controls_overlay_rect_.x()));
  vars.SetVariable(
      UADefinedVariable::kTitlebarAreaY,
      StyleEnvironmentVariables::FormatPx(window_controls_overlay_rect_.y()));
  vars.SetVariable(UADefinedVariable::kTitlebarAreaWidth,
                   StyleEnvironmentVariables::FormatPx(
                       window_controls_overlay_rect_.width()));
  vars.SetVariable(UADefinedVariable::kTitlebarAreaHeight,
                   StyleEnvironmentVariables::FormatPx(
                       window_controls_overlay_rect_.height()));
}

void LocalFrame::MaybeUpdateWindowControlsOverlayWithNewZoomLevel() {
  // |window_controls_overlay_rect_| is only set for local root.
  if (!is_window_controls_overlay_visible_ || !IsLocalRoot())
    return;

  DCHECK(!window_controls_overlay_rect_in_dips_.IsEmpty());

  UpdateWindowControlsOverlay(window_controls_overlay_rect_in_dips_);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void LocalFrame::SetNotRestoredReasons(
    mojom::blink::BackForwardCacheNotRestoredReasonsPtr not_restored_reasons) {
  // Back/forward cache is only enabled for outermost main frame.
  DCHECK(IsOutermostMainFrame());
  not_restored_reasons_ = mojo::Clone(not_restored_reasons);
}

const mojom::blink::BackForwardCacheNotRestoredReasonsPtr&
LocalFrame::GetNotRestoredReasons() {
  // Back/forward cache is only enabled for the outermost main frames, and the
  // web exposed API returns non-null values only for the outermost main frames.
  DCHECK(IsOutermostMainFrame());
  return not_restored_reasons_;
}

void LocalFrame::AddScrollSnapshotClient(ScrollSnapshotClient& client) {
  scroll_snapshot_clients_.insert(&client);
}

void LocalFrame::UpdateScrollSnapshots() {
  // TODO(xiaochengh): Can we DCHECK that is is done at the beginning of a frame
  // and is done exactly once?
  for (auto& client : scroll_snapshot_clients_)
    client->UpdateSnapshot();
}

bool LocalFrame::ValidateScrollSnapshotClients() {
  bool valid = true;
  for (auto& client : scroll_snapshot_clients_) {
    valid &= client->ValidateSnapshot();
  }
  return valid;
}

void LocalFrame::ClearScrollSnapshotClients() {
  scroll_snapshot_clients_.clear();
}

void LocalFrame::ScheduleNextServiceForScrollSnapshotClients() {
  for (auto& client : scroll_snapshot_clients_) {
    if (client->ShouldScheduleNextService()) {
      View()->ScheduleAnimation();
      return;
    }
  }
}

void LocalFrame::CheckPositionAnchorsForCssVisibilityChanges() {
  for (auto& client : scroll_snapshot_clients_) {
    if (AnchorPositionScrollData* scroll_data =
            DynamicTo<AnchorPositionScrollData>(client.Get())) {
      if (auto* observer = scroll_data->GetAnchorPositionVisibilityObserver()) {
        observer->UpdateForCssAnchorVisibility();
      }
    }
  }
}

void LocalFrame::CheckPositionAnchorsForChainedVisibilityChanges() {
  AnchorPositionVisibilityObserver::UpdateForChainedAnchorVisibility(
      scroll_snapshot_clients_);
}

bool LocalFrame::IsSameOrigin() {
  const SecurityOrigin* security_origin =
      GetSecurityContext()->GetSecurityOrigin();
  const SecurityOrigin* top_security_origin =
      Tree().Top().GetSecurityContext()->GetSecurityOrigin();

  return security_origin->IsSameOriginWith(top_security_origin);
}

bool LocalFrame::ImagesEnabled() {
  DCHECK(!IsDetached());
  // If this is called in the middle of detach, GetDocumentLoader() might
  // already be nullptr.
  if (!loader_.GetDocumentLoader()) {
    return false;
  }
  bool allow_image_renderer = GetSettings()->GetImagesEnabled();
  bool allow_image_content_setting =
      loader_.GetDocumentLoader()->GetContentSettings()->allow_image;
  return allow_image_renderer && allow_image_content_setting;
}

bool LocalFrame::ScriptEnabled() {
  DCHECK(!IsDetached());
  // If this is called in the middle of detach, GetDocumentLoader() might
  // already be nullptr.
  if (!loader_.GetDocumentLoader()) {
    return false;
  }
  bool allow_script_renderer = GetSettings()->GetScriptEnabled();
  bool allow_script_content_setting =
      loader_.GetDocumentLoader()->GetContentSettings()->allow_script;
  return allow_script_renderer && allow_script_content_setting;
}

const WebPrintParams& LocalFrame::GetPrintParams() const {
  // If this fails, it's probably because nobody called StartPrinting().
  DCHECK(GetDocument()->Printing());

  return print_params_;
}

mojo::PendingRemote<mojom::blink::NavigationStateKeepAliveHandle>
LocalFrame::IssueKeepAliveHandle() {
  mojo::PendingRemote<mojom::blink::NavigationStateKeepAliveHandle>
      keep_alive_remote;
  GetLocalFrameHostRemote().IssueKeepAliveHandle(
      keep_alive_remote.InitWithNewPipeAndPassReceiver());
  return keep_alive_remote;
}

WebLinkPreviewTriggerer* LocalFrame::GetOrCreateLinkPreviewTriggerer() {
  EnsureLinkPreviewTriggererInitialized();
  return link_preview_triggerer_.get();
}

void LocalFrame::EnsureLinkPreviewTriggererInitialized() {
  if (is_link_preivew_triggerer_initialized_) {
    return;
  }

  CHECK(!link_preview_triggerer_);

  WebLocalFrameImpl* web_local_frame = WebLocalFrameImpl::FromFrame(this);
  if (!web_local_frame) {
    return;
  }

  link_preview_triggerer_ =
      web_local_frame->Client()->CreateLinkPreviewTriggerer();
  is_link_preivew_triggerer_initialized_ = true;
}

void LocalFrame::SetLinkPreviewTriggererForTesting(
    std::unique_ptr<WebLinkPreviewTriggerer> trigger) {
  link_preview_triggerer_ = std::move(trigger);
  is_link_preivew_triggerer_initialized_ = true;
}

void LocalFrame::AllowStorageAccessAndNotify(
    blink::WebContentSettingsClient::StorageType storage_type,
    base::OnceCallback<void(bool)> callback) {
  mojom::blink::StorageTypeAccessed mojo_storage_type =
      ToMojoStorageType(storage_type);
  auto wrapped_callback = WTF::BindOnce(&LocalFrame::OnStorageAccessCallback,
                                        WrapWeakPersistent(this),
                                        std::move(callback), mojo_storage_type);
  if (WebContentSettingsClient* content_settings_client =
          GetContentSettingsClient()) {
    content_settings_client->AllowStorageAccess(storage_type,
                                                std::move(wrapped_callback));
  } else {
    std::move(wrapped_callback).Run(true);
  }
}

bool LocalFrame::AllowStorageAccessSyncAndNotify(
    blink::WebContentSettingsClient::StorageType storage_type) {
  bool allowed = true;
  if (WebContentSettingsClient* content_settings_client =
          GetContentSettingsClient()) {
    allowed = content_settings_client->AllowStorageAccessSync(storage_type);
  }
  GetLocalFrameHostRemote().NotifyStorageAccessed(
      ToMojoStorageType(storage_type), !allowed);
  return allowed;
}

void LocalFrame::OnStorageAccessCallback(
    base::OnceCallback<void(bool)> callback,
    mojom::blink::StorageTypeAccessed storage_type,
    bool is_allowed) {
  GetLocalFrameHostRemote().NotifyStorageAccessed(storage_type, !is_allowed);
  std::move(callback).Run(is_allowed);
}

}  // namespace blink
