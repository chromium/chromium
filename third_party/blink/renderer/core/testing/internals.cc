/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/testing/internals.h"

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "cc/layers/picture_layer.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-blink.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/css_unresolved_property.h"
#include "third_party/blink/renderer/core/css/select_rule_feature_set.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/iterator.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/shadow_root_v0.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/spell_check_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/frame/remote_dom_window.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/test_report_body.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_list.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/core/html/html_content_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/remote_playback_controller.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/keyboard_event_manager.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/import_map.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/scroll/programmatic_scroll_animator.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/callback_function_test.h"
#include "third_party/blink/renderer/core/testing/dictionary_test.h"
#include "third_party/blink/renderer/core/testing/gc_observation.h"
#include "third_party/blink/renderer/core/testing/hit_test_layer_rect.h"
#include "third_party/blink/renderer/core/testing/hit_test_layer_rect_list.h"
#include "third_party/blink/renderer/core/testing/internal_runtime_flags.h"
#include "third_party/blink/renderer/core/testing/internal_settings.h"
#include "third_party/blink/renderer/core/testing/mock_hyphenation.h"
#include "third_party/blink/renderer/core/testing/origin_trials_test.h"
#include "third_party/blink/renderer/core/testing/record_test.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/core/testing/sequence_test.h"
#include "third_party/blink/renderer/core/testing/static_selection.h"
#include "third_party/blink/renderer/core/testing/type_conversions.h"
#include "third_party/blink/renderer/core/testing/union_types_test.h"
#include "third_party/blink/renderer/core/timezone/timezone_controller.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-blink.h"
#include "ui/base/ui_base_features.h"
#include "v8/include/v8.h"

namespace blink {

using ui::mojom::ImeTextSpanThickness;
using ui::mojom::ImeTextSpanUnderlineStyle;

namespace {

std::unique_ptr<ScopedMockOverlayScrollbars> g_mock_overlay_scrollbars;

class UseCounterHelperObserverImpl final : public UseCounterHelper::Observer {
 public:
  UseCounterHelperObserverImpl(ScriptPromiseResolver* resolver,
                               WebFeature feature)
      : resolver_(resolver), feature_(feature) {}

  bool OnCountFeature(WebFeature feature) final {
    if (feature_ != feature)
      return false;
    resolver_->Resolve(static_cast<int>(feature));
    return true;
  }

  void Trace(Visitor* visitor) const override {
    UseCounterHelper::Observer::Trace(visitor);
    visitor->Trace(resolver_);
  }

 private:
  Member<ScriptPromiseResolver> resolver_;
  WebFeature feature_;
  DISALLOW_COPY_AND_ASSIGN(UseCounterHelperObserverImpl);
};

}  // namespace

static base::Optional<DocumentMarker::MarkerType> MarkerTypeFrom(
    const String& marker_type) {
  if (EqualIgnoringASCIICase(marker_type, "Spelling"))
    return DocumentMarker::kSpelling;
  if (EqualIgnoringASCIICase(marker_type, "Grammar"))
    return DocumentMarker::kGrammar;
  if (EqualIgnoringASCIICase(marker_type, "TextMatch"))
    return DocumentMarker::kTextMatch;
  if (EqualIgnoringASCIICase(marker_type, "Composition"))
    return DocumentMarker::kComposition;
  if (EqualIgnoringASCIICase(marker_type, "ActiveSuggestion"))
    return DocumentMarker::kActiveSuggestion;
  if (EqualIgnoringASCIICase(marker_type, "Suggestion"))
    return DocumentMarker::kSuggestion;
  return base::nullopt;
}

static base::Optional<DocumentMarker::MarkerTypes> MarkerTypesFrom(
    const String& marker_type) {
  if (marker_type.IsEmpty() || EqualIgnoringASCIICase(marker_type, "all"))
    return DocumentMarker::MarkerTypes::All();
  base::Optional<DocumentMarker::MarkerType> type = MarkerTypeFrom(marker_type);
  if (!type)
    return base::nullopt;
  return DocumentMarker::MarkerTypes(type.value());
}

static SpellCheckRequester* GetSpellCheckRequester(Document* document) {
  if (!document || !document->GetFrame())
    return nullptr;
  return &document->GetFrame()->GetSpellChecker().GetSpellCheckRequester();
}

static ScrollableArea* ScrollableAreaForNode(Node* node) {
  if (!node)
    return nullptr;

  if (auto* box = DynamicTo<LayoutBox>(node->GetLayoutObject()))
    return box->GetScrollableArea();
  return nullptr;
}

void Internals::ResetToConsistentState(Page* page) {
  DCHECK(page);

  page->SetIsCursorVisible(true);
  // Ensure the PageScaleFactor always stays within limits, if the test changed
  // the limits.
  page->SetDefaultPageScaleLimits(1, 4);
  page->SetPageScaleFactor(1);
  page->GetChromeClient().GetWebView()->DisableDeviceEmulation();

  // Ensure timers are reset so timers such as EventHandler's |hover_timer_| do
  // not cause additional lifecycle updates.
  for (Frame* frame = page->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (auto* local_frame = DynamicTo<LocalFrame>(frame))
      local_frame->GetEventHandler().Clear();
  }

  LocalFrame* frame = page->DeprecatedLocalMainFrame();
  frame->View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(), mojom::blink::ScrollType::kProgrammatic);
  OverrideUserPreferredLanguagesForTesting(Vector<AtomicString>());
  if (page->DeprecatedLocalMainFrame()->GetEditor().IsOverwriteModeEnabled())
    page->DeprecatedLocalMainFrame()->GetEditor().ToggleOverwriteModeEnabled();

  if (ScrollingCoordinator* scrolling_coordinator =
          page->GetScrollingCoordinator()) {
    scrolling_coordinator->Reset(frame);
  }

  KeyboardEventManager::SetCurrentCapsLockState(
      OverrideCapsLockState::kDefault);

  IntersectionObserver::SetThrottleDelayEnabledForTesting(true);
  g_mock_overlay_scrollbars.reset();

  Page::SetMaxNumberOfFramesToTenForTesting(false);
}

Internals::Internals(ExecutionContext* context)
    : runtime_flags_(InternalRuntimeFlags::create()),
      document_(To<LocalDOMWindow>(context)->document()) {
  document_->Fetcher()->EnableIsPreloadedForTest();
}

LocalFrame* Internals::GetFrame() const {
  if (!document_)
    return nullptr;
  return document_->GetFrame();
}

InternalSettings* Internals::settings() const {
  if (!document_)
    return nullptr;
  Page* page = document_->GetPage();
  if (!page)
    return nullptr;
  return InternalSettings::From(*page);
}

InternalRuntimeFlags* Internals::runtimeFlags() const {
  return runtime_flags_.Get();
}

unsigned Internals::workerThreadCount() const {
  return WorkerThread::WorkerThreadCount();
}

bool Internals::isFormControlsRefreshEnabled() const {
  return ::features::IsFormControlsRefreshEnabled();
}

GCObservation* Internals::observeGC(ScriptValue script_value) {
  v8::Local<v8::Value> observed_value = script_value.V8Value();
  DCHECK(!observed_value.IsEmpty());
  if (observed_value->IsNull() || observed_value->IsUndefined()) {
    V8ThrowException::ThrowTypeError(v8::Isolate::GetCurrent(),
                                     "value to observe is null or undefined");
    return nullptr;
  }

  return MakeGarbageCollected<GCObservation>(observed_value);
}

unsigned Internals::updateStyleAndReturnAffectedElementCount(
    ExceptionState& exception_state) const {
  if (!document_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No context document is available.");
    return 0;
  }

  unsigned before_count = document_->GetStyleEngine().StyleForElementCount();
  document_->UpdateStyleAndLayoutTree();
  return document_->GetStyleEngine().StyleForElementCount() - before_count;
}

unsigned Internals::needsLayoutCount(ExceptionState& exception_state) const {
  LocalFrame* context_frame = GetFrame();
  if (!context_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No context frame is available.");
    return 0;
  }

  bool is_partial;
  unsigned needs_layout_objects;
  unsigned total_objects;
  context_frame->View()->CountObjectsNeedingLayout(needs_layout_objects,
                                                   total_objects, is_partial);
  return needs_layout_objects;
}

unsigned Internals::hitTestCount(Document* doc,
                                 ExceptionState& exception_state) const {
  if (!doc) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "Must supply document to check");
    return 0;
  }

  if (!doc->GetLayoutView())
    return 0;

  return doc->GetLayoutView()->HitTestCount();
}

unsigned Internals::hitTestCacheHits(Document* doc,
                                     ExceptionState& exception_state) const {
  if (!doc) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "Must supply document to check");
    return 0;
  }

  if (!doc->GetLayoutView())
    return 0;

  return doc->GetLayoutView()->HitTestCacheHits();
}

Element* Internals::elementFromPoint(Document* doc,
                                     double x,
                                     double y,
                                     bool ignore_clipping,
                                     bool allow_child_frame_content,
                                     ExceptionState& exception_state) const {
  if (!doc) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "Must supply document to check");
    return nullptr;
  }

  if (!doc->GetLayoutView())
    return nullptr;

  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kReadOnly | HitTestRequest::kActive;
  if (ignore_clipping)
    hit_type |= HitTestRequest::kIgnoreClipping;
  if (allow_child_frame_content)
    hit_type |= HitTestRequest::kAllowChildFrameContent;

  HitTestRequest request(hit_type);

  return doc->HitTestPoint(x, y, request);
}

void Internals::clearHitTestCache(Document* doc,
                                  ExceptionState& exception_state) const {
  if (!doc) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "Must supply document to check");
    return;
  }

  if (!doc->GetLayoutView())
    return;

  doc->GetLayoutView()->ClearHitTestCache();
}

Element* Internals::innerEditorElement(Element* container,
                                       ExceptionState& exception_state) const {
  if (auto* control = ToTextControlOrNull(container))
    return control->InnerEditorElement();

  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not a text control element.");
  return nullptr;
}

bool Internals::isPreloaded(const String& url) {
  return isPreloadedBy(url, document_);
}

bool Internals::isPreloadedBy(const String& url, Document* document) {
  if (!document)
    return false;
  return document->Fetcher()->IsPreloadedForTest(document->CompleteURL(url));
}

bool Internals::isLoading(const String& url) {
  if (!document_)
    return false;
  const KURL full_url = document_->CompleteURL(url);
  const String cache_identifier =
      document_->Fetcher()->GetCacheIdentifier(full_url);
  Resource* resource =
      GetMemoryCache()->ResourceForURL(full_url, cache_identifier);
  // We check loader() here instead of isLoading(), because a multipart
  // ImageResource lies isLoading() == false after the first part is loaded.
  return resource && resource->Loader();
}

bool Internals::isLoadingFromMemoryCache(const String& url) {
  if (!document_)
    return false;
  const KURL full_url = document_->CompleteURL(url);
  const String cache_identifier =
      document_->Fetcher()->GetCacheIdentifier(full_url);
  Resource* resource =
      GetMemoryCache()->ResourceForURL(full_url, cache_identifier);
  return resource && resource->GetStatus() == ResourceStatus::kCached;
}

ScriptPromise Internals::getResourcePriority(ScriptState* script_state,
                                             const String& url,
                                             Document* document) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  KURL resource_url = url_test_helpers::ToKURL(url.Utf8());
  DCHECK(document);

  auto callback = WTF::Bind(&Internals::ResolveResourcePriority,
                            WTF::Passed(WrapPersistent(this)),
                            WTF::Passed(WrapPersistent(resolver)));
  ResourceFetcher::AddPriorityObserverForTesting(resource_url,
                                                 std::move(callback));

  return promise;
}

bool Internals::doesWindowHaveUrlFragment(DOMWindow* window) {
  if (IsA<RemoteDOMWindow>(window))
    return false;
  return To<LocalFrame>(window->GetFrame())
      ->GetDocument()
      ->Url()
      .HasFragmentIdentifier();
}

String Internals::getResourceHeader(const String& url,
                                    const String& header,
                                    Document* document) {
  if (!document)
    return String();
  Resource* resource = document->Fetcher()->AllResources().at(
      url_test_helpers::ToKURL(url.Utf8()));
  if (!resource)
    return String();
  return resource->GetResourceRequest().HttpHeaderField(AtomicString(header));
}

bool Internals::isValidContentSelect(Element* insertion_point,
                                     ExceptionState& exception_state) {
  DCHECK(insertion_point);
  if (!insertion_point->IsV0InsertionPoint()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The element is not an insertion point.");
    return false;
  }

  auto* html_content_element = DynamicTo<HTMLContentElement>(insertion_point);
  return html_content_element && html_content_element->IsSelectValid();
}

Node* Internals::treeScopeRootNode(Node* node) {
  DCHECK(node);
  return &node->GetTreeScope().RootNode();
}

Node* Internals::parentTreeScope(Node* node) {
  DCHECK(node);
  const TreeScope* parent_tree_scope = node->GetTreeScope().ParentTreeScope();
  return parent_tree_scope ? &parent_tree_scope->RootNode() : nullptr;
}

uint16_t Internals::compareTreeScopePosition(
    const Node* node1,
    const Node* node2,
    ExceptionState& exception_state) const {
  DCHECK(node1 && node2);
  const TreeScope* tree_scope1 =
      IsA<Document>(node1)
          ? static_cast<const TreeScope*>(To<Document>(node1))
          : IsA<ShadowRoot>(node1)
                ? static_cast<const TreeScope*>(To<ShadowRoot>(node1))
                : nullptr;
  const TreeScope* tree_scope2 =
      IsA<Document>(node2)
          ? static_cast<const TreeScope*>(To<Document>(node2))
          : IsA<ShadowRoot>(node2)
                ? static_cast<const TreeScope*>(To<ShadowRoot>(node2))
                : nullptr;
  if (!tree_scope1 || !tree_scope2) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        String::Format(
            "The %s node is neither a document node, nor a shadow root.",
            tree_scope1 ? "second" : "first"));
    return 0;
  }
  return tree_scope1->ComparePosition(*tree_scope2);
}

void Internals::pauseAnimations(double pause_time,
                                ExceptionState& exception_state) {
  if (pause_time < 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        ExceptionMessages::IndexExceedsMinimumBound("pauseTime", pause_time,
                                                    0.0));
    return;
  }

  if (!GetFrame())
    return;

  GetFrame()->View()->UpdateAllLifecyclePhasesForTest();
  GetFrame()->GetDocument()->Timeline().PauseAnimationsForTesting(pause_time);
}

bool Internals::isCompositedAnimation(Animation* animation) {
  return animation->HasActiveAnimationsOnCompositor();
}

void Internals::disableCompositedAnimation(Animation* animation) {
  animation->DisableCompositedAnimationForTesting();
}

void Internals::advanceImageAnimation(Element* image,
                                      ExceptionState& exception_state) {
  DCHECK(image);

  ImageResourceContent* content = nullptr;
  if (auto* html_image = DynamicTo<HTMLImageElement>(*image)) {
    content = html_image->CachedImage();
  } else if (auto* svg_image = DynamicTo<SVGImageElement>(*image)) {
    content = svg_image->CachedImage();
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The element provided is not a image element.");
    return;
  }

  if (!content || !content->HasImage()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The image resource is not available.");
    return;
  }

  Image* image_data = content->GetImage();
  image_data->AdvanceAnimationForTesting();
}

bool Internals::hasShadowInsertionPoint(const Node* root,
                                        ExceptionState& exception_state) const {
  DCHECK(root);
  if (!IsA<ShadowRoot>(root)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node argument is not a shadow root.");
    return false;
  }
  return To<ShadowRoot>(root)->V0().ContainsShadowElements();
}

bool Internals::hasContentElement(const Node* root,
                                  ExceptionState& exception_state) const {
  DCHECK(root);
  if (!IsA<ShadowRoot>(root)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node argument is not a shadow root.");
    return false;
  }
  return To<ShadowRoot>(root)->V0().ContainsContentElements();
}

uint32_t Internals::countElementShadow(const Node* root,
                                       ExceptionState& exception_state) const {
  DCHECK(root);
  if (!IsA<ShadowRoot>(root)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node argument is not a shadow root.");
    return 0;
  }
  return To<ShadowRoot>(root)->ChildShadowRootCount();
}

Node* Internals::nextSiblingInFlatTree(Node* node,
                                       ExceptionState& exception_state) {
  DCHECK(node);
  if (!node->CanParticipateInFlatTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node argument doesn't particite in the flat tree.");
    return nullptr;
  }
  return FlatTreeTraversal::NextSibling(*node);
}

Node* Internals::firstChildInFlatTree(Node* node,
                                      ExceptionState& exception_state) {
  DCHECK(node);
  if (!node->CanParticipateInFlatTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node argument doesn't particite in the flat tree");
    return nullptr;
  }
  return FlatTreeTraversal::FirstChild(*node);
}

Node* Internals::lastChildInFlatTree(Node* node,
                                     ExceptionState& exception_state) {
  DCHECK(node);
  if (!node->CanParticipateInFlatTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node argument doesn't particite in the flat tree.");
    return nullptr;
  }
  return FlatTreeTraversal::LastChild(*node);
}

Node* Internals::nextInFlatTree(Node* node, ExceptionState& exception_state) {
  DCHECK(node);
  if (!node->CanParticipateInFlatTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node argument doesn't particite in the flat tree.");
    return nullptr;
  }
  return FlatTreeTraversal::Next(*node);
}

Node* Internals::previousInFlatTree(Node* node,
                                    ExceptionState& exception_state) {
  DCHECK(node);
  if (!node->CanParticipateInFlatTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node argument doesn't particite in the flat tree.");
    return nullptr;
  }
  return FlatTreeTraversal::Previous(*node);
}

String Internals::elementLayoutTreeAsText(Element* element,
                                          ExceptionState& exception_state) {
  DCHECK(element);
  element->GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  String representation = ExternalRepresentation(element);
  if (representation.IsEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The element provided has no external representation.");
    return String();
  }

  return representation;
}

CSSStyleDeclaration* Internals::computedStyleIncludingVisitedInfo(
    Node* node) const {
  DCHECK(node);
  bool allow_visited_style = true;
  return MakeGarbageCollected<CSSComputedStyleDeclaration>(node,
                                                           allow_visited_style);
}

ShadowRoot* Internals::createUserAgentShadowRoot(Element* host) {
  DCHECK(host);
  return &host->EnsureUserAgentShadowRoot();
}

void Internals::setBrowserControlsState(float top_height,
                                        float bottom_height,
                                        bool shrinks_layout) {
  document_->GetPage()->GetChromeClient().SetBrowserControlsState(
      top_height, bottom_height, shrinks_layout);
}

void Internals::setBrowserControlsShownRatio(float top_ratio,
                                             float bottom_ratio) {
  document_->GetPage()->GetChromeClient().SetBrowserControlsShownRatio(
      top_ratio, bottom_ratio);
}

Node* Internals::effectiveRootScroller(Document* document) {
  if (!document)
    document = document_;

  return &document->GetRootScrollerController().EffectiveRootScroller();
}

ShadowRoot* Internals::shadowRoot(Element* host) {
  DCHECK(host);
  return host->GetShadowRoot();
}

String Internals::shadowRootType(const Node* root,
                                 ExceptionState& exception_state) const {
  DCHECK(root);
  auto* shadow_root = DynamicTo<ShadowRoot>(root);
  if (!shadow_root) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node provided is not a shadow root.");
    return String();
  }

  switch (shadow_root->GetType()) {
    case ShadowRootType::kUserAgent:
      return String("UserAgentShadowRoot");
    case ShadowRootType::V0:
      return String("V0ShadowRoot");
    case ShadowRootType::kOpen:
      return String("OpenShadowRoot");
    case ShadowRootType::kClosed:
      return String("ClosedShadowRoot");
    default:
      NOTREACHED();
      return String("Unknown");
  }
}

const AtomicString& Internals::shadowPseudoId(Element* element) {
  DCHECK(element);
  return element->ShadowPseudoId();
}

String Internals::visiblePlaceholder(Element* element) {
  if (auto* text_control_element = ToTextControlOrNull(element)) {
    if (!text_control_element->IsPlaceholderVisible())
      return String();
    if (HTMLElement* placeholder_element =
            text_control_element->PlaceholderElement())
      return placeholder_element->textContent();
  }

  return String();
}

bool Internals::isValidationMessageVisible(Element* element) {
  DCHECK(element);
  if (auto* page = element->GetDocument().GetPage()) {
    return page->GetValidationMessageClient().IsValidationMessageVisible(
        *element);
  }
  return false;
}

void Internals::selectColorInColorChooser(Element* element,
                                          const String& color_value) {
  DCHECK(element);
  Color color;
  if (!color.SetFromString(color_value))
    return;
  if (auto* input = DynamicTo<HTMLInputElement>(*element))
    input->SelectColorInColorChooser(color);
}

void Internals::endColorChooser(Element* element) {
  DCHECK(element);
  if (auto* input = DynamicTo<HTMLInputElement>(*element))
    input->EndColorChooserForTesting();
}

bool Internals::hasAutofocusRequest(Document* document) {
  if (!document)
    document = document_;
  return document->HasAutofocusCandidates();
}

bool Internals::hasAutofocusRequest() {
  return hasAutofocusRequest(nullptr);
}

Vector<String> Internals::formControlStateOfHistoryItem(
    ExceptionState& exception_state) {
  HistoryItem* main_item = nullptr;
  if (GetFrame())
    main_item = GetFrame()->Loader().GetDocumentLoader()->GetHistoryItem();
  if (!main_item) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No history item is available.");
    return Vector<String>();
  }
  return main_item->GetDocumentState();
}

void Internals::setFormControlStateOfHistoryItem(
    const Vector<String>& state,
    ExceptionState& exception_state) {
  HistoryItem* main_item = nullptr;
  if (GetFrame())
    main_item = GetFrame()->Loader().GetDocumentLoader()->GetHistoryItem();
  if (!main_item) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No history item is available.");
    return;
  }
  main_item->ClearDocumentState();
  main_item->SetDocumentState(state);
}

DOMWindow* Internals::pagePopupWindow() const {
  if (!document_)
    return nullptr;
  if (Page* page = document_->GetPage()) {
    return To<LocalDOMWindow>(
        page->GetChromeClient().PagePopupWindowForTesting());
  }
  return nullptr;
}

DOMRectReadOnly* Internals::absoluteCaretBounds(
    ExceptionState& exception_state) {
  if (!GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The document's frame cannot be retrieved.");
    return nullptr;
  }

  document_->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  return DOMRectReadOnly::FromIntRect(
      GetFrame()->Selection().AbsoluteCaretBounds());
}

String Internals::textAffinity() {
  if (GetFrame() && GetFrame()
                            ->GetPage()
                            ->GetFocusController()
                            .FocusedFrame()
                            ->Selection()
                            .GetSelectionInDOMTree()
                            .Affinity() == TextAffinity::kUpstream) {
    return "Upstream";
  }
  return "Downstream";
}

DOMRectReadOnly* Internals::boundingBox(Element* element) {
  DCHECK(element);

  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object)
    return DOMRectReadOnly::Create(0, 0, 0, 0);
  return DOMRectReadOnly::FromIntRect(layout_object->AbsoluteBoundingBoxRect());
}

void Internals::setMarker(Document* document,
                          const Range* range,
                          const String& marker_type,
                          ExceptionState& exception_state) {
  if (!document) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No context document is available.");
    return;
  }

  base::Optional<DocumentMarker::MarkerType> type = MarkerTypeFrom(marker_type);
  if (!type) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The marker type provided ('" + marker_type + "') is invalid.");
    return;
  }

  if (type != DocumentMarker::kSpelling && type != DocumentMarker::kGrammar) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "internals.setMarker() currently only "
                                      "supports spelling and grammar markers; "
                                      "attempted to add marker of type '" +
                                          marker_type + "'.");
    return;
  }

  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  if (type == DocumentMarker::kSpelling)
    document->Markers().AddSpellingMarker(EphemeralRange(range));
  else
    document->Markers().AddGrammarMarker(EphemeralRange(range));
}

unsigned Internals::markerCountForNode(Node* node,
                                       const String& marker_type,
                                       ExceptionState& exception_state) {
  DCHECK(node);
  base::Optional<DocumentMarker::MarkerTypes> marker_types =
      MarkerTypesFrom(marker_type);
  if (!marker_types) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The marker type provided ('" + marker_type + "') is invalid.");
    return 0;
  }

  return node->GetDocument()
      .Markers()
      .MarkersFor(To<Text>(*node), marker_types.value())
      .size();
}

unsigned Internals::activeMarkerCountForNode(Node* node) {
  DCHECK(node);

  // Only TextMatch markers can be active.
  DocumentMarkerVector markers = node->GetDocument().Markers().MarkersFor(
      To<Text>(*node), DocumentMarker::MarkerTypes::TextMatch());

  unsigned active_marker_count = 0;
  for (const auto& marker : markers) {
    if (To<TextMatchMarker>(marker.Get())->IsActiveMatch())
      active_marker_count++;
  }

  return active_marker_count;
}

DocumentMarker* Internals::MarkerAt(Node* node,
                                    const String& marker_type,
                                    unsigned index,
                                    ExceptionState& exception_state) {
  DCHECK(node);
  base::Optional<DocumentMarker::MarkerTypes> marker_types =
      MarkerTypesFrom(marker_type);
  if (!marker_types) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The marker type provided ('" + marker_type + "') is invalid.");
    return nullptr;
  }

  DocumentMarkerVector markers = node->GetDocument().Markers().MarkersFor(
      To<Text>(*node), marker_types.value());
  if (markers.size() <= index)
    return nullptr;
  return markers[index];
}

Range* Internals::markerRangeForNode(Node* node,
                                     const String& marker_type,
                                     unsigned index,
                                     ExceptionState& exception_state) {
  DCHECK(node);
  DocumentMarker* marker = MarkerAt(node, marker_type, index, exception_state);
  if (!marker)
    return nullptr;
  return MakeGarbageCollected<Range>(node->GetDocument(), node,
                                     marker->StartOffset(), node,
                                     marker->EndOffset());
}

String Internals::markerDescriptionForNode(Node* node,
                                           const String& marker_type,
                                           unsigned index,
                                           ExceptionState& exception_state) {
  DocumentMarker* marker = MarkerAt(node, marker_type, index, exception_state);
  if (!marker || !IsSpellCheckMarker(*marker))
    return String();
  return To<SpellCheckMarker>(marker)->Description();
}

unsigned Internals::markerBackgroundColorForNode(
    Node* node,
    const String& marker_type,
    unsigned index,
    ExceptionState& exception_state) {
  DocumentMarker* marker = MarkerAt(node, marker_type, index, exception_state);
  auto* style_marker = DynamicTo<StyleableMarker>(marker);
  if (!style_marker)
    return 0;
  return style_marker->BackgroundColor().Rgb();
}

unsigned Internals::markerUnderlineColorForNode(
    Node* node,
    const String& marker_type,
    unsigned index,
    ExceptionState& exception_state) {
  DocumentMarker* marker = MarkerAt(node, marker_type, index, exception_state);
  auto* style_marker = DynamicTo<StyleableMarker>(marker);
  if (!style_marker)
    return 0;
  return style_marker->UnderlineColor().Rgb();
}

static base::Optional<TextMatchMarker::MatchStatus> MatchStatusFrom(
    const String& match_status) {
  if (EqualIgnoringASCIICase(match_status, "kActive"))
    return TextMatchMarker::MatchStatus::kActive;
  if (EqualIgnoringASCIICase(match_status, "kInactive"))
    return TextMatchMarker::MatchStatus::kInactive;
  return base::nullopt;
}

void Internals::addTextMatchMarker(const Range* range,
                                   const String& match_status,
                                   ExceptionState& exception_state) {
  DCHECK(range);
  if (!range->OwnerDocument().View())
    return;

  base::Optional<TextMatchMarker::MatchStatus> match_status_enum =
      MatchStatusFrom(match_status);
  if (!match_status_enum) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The match status provided ('" + match_status + "') is invalid.");
    return;
  }

  range->OwnerDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  range->OwnerDocument().Markers().AddTextMatchMarker(
      EphemeralRange(range), match_status_enum.value());

  // This simulates what the production code does after
  // DocumentMarkerController::addTextMatchMarker().
  range->OwnerDocument().GetLayoutView()->InvalidatePaintForTickmarks();
}

static bool ParseColor(const String& value,
                       Color& color,
                       ExceptionState& exception_state,
                       String error_message) {
  if (!color.SetFromString(value)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      error_message);
    return false;
  }
  return true;
}

static base::Optional<ImeTextSpanThickness> ThicknessFrom(
    const String& thickness) {
  if (EqualIgnoringASCIICase(thickness, "none"))
    return ImeTextSpanThickness::kNone;
  if (EqualIgnoringASCIICase(thickness, "thin"))
    return ImeTextSpanThickness::kThin;
  if (EqualIgnoringASCIICase(thickness, "thick"))
    return ImeTextSpanThickness::kThick;
  return base::nullopt;
}

static base::Optional<ImeTextSpanUnderlineStyle> UnderlineStyleFrom(
    const String& underline_style) {
  if (EqualIgnoringASCIICase(underline_style, "none"))
    return ImeTextSpanUnderlineStyle::kNone;
  if (EqualIgnoringASCIICase(underline_style, "solid"))
    return ImeTextSpanUnderlineStyle::kSolid;
  if (EqualIgnoringASCIICase(underline_style, "dot"))
    return ImeTextSpanUnderlineStyle::kDot;
  if (EqualIgnoringASCIICase(underline_style, "dash"))
    return ImeTextSpanUnderlineStyle::kDash;
  if (EqualIgnoringASCIICase(underline_style, "squiggle"))
    return ImeTextSpanUnderlineStyle::kSquiggle;
  return base::nullopt;
}

namespace {

void addStyleableMarkerHelper(const Range* range,
                              const String& underline_color_value,
                              const String& thickness_value,
                              const String& underline_style_value,
                              const String& text_color_value,
                              const String& background_color_value,
                              ExceptionState& exception_state,
                              std::function<void(const EphemeralRange&,
                                                 Color,
                                                 ImeTextSpanThickness,
                                                 ImeTextSpanUnderlineStyle,
                                                 Color,
                                                 Color)> create_marker) {
  DCHECK(range);
  range->OwnerDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  base::Optional<ImeTextSpanThickness> thickness =
      ThicknessFrom(thickness_value);
  if (!thickness) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The thickness provided ('" + thickness_value + "') is invalid.");
    return;
  }

  base::Optional<ImeTextSpanUnderlineStyle> underline_style =
      UnderlineStyleFrom(underline_style_value);
  if (!underline_style_value) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The underline style provided ('" +
                                          underline_style_value +
                                          "') is invalid.");
    return;
  }

  Color underline_color;
  Color background_color;
  Color text_color;
  if (ParseColor(underline_color_value, underline_color, exception_state,
                 "Invalid underline color.") &&
      ParseColor(text_color_value, text_color, exception_state,
                 "Invalid text color.") &&
      ParseColor(background_color_value, background_color, exception_state,
                 "Invalid background color.")) {
    create_marker(EphemeralRange(range), underline_color, thickness.value(),
                  underline_style.value(), text_color, background_color);
  }
}

}  // namespace

void Internals::addCompositionMarker(const Range* range,
                                     const String& underline_color_value,
                                     const String& thickness_value,
                                     const String& underline_style_value,
                                     const String& text_color_value,
                                     const String& background_color_value,
                                     ExceptionState& exception_state) {
  DocumentMarkerController& document_marker_controller =
      range->OwnerDocument().Markers();
  addStyleableMarkerHelper(
      range, underline_color_value, thickness_value, underline_style_value,
      text_color_value, background_color_value, exception_state,
      [&document_marker_controller](const EphemeralRange& range,
                                    Color underline_color,
                                    ImeTextSpanThickness thickness,
                                    ImeTextSpanUnderlineStyle underline_style,
                                    Color text_color, Color background_color) {
        document_marker_controller.AddCompositionMarker(
            range, underline_color, thickness, underline_style, text_color,
            background_color);
      });
}

void Internals::addActiveSuggestionMarker(const Range* range,
                                          const String& underline_color_value,
                                          const String& thickness_value,
                                          const String& background_color_value,
                                          ExceptionState& exception_state) {
  // Underline style and text color aren't really supported for suggestions so
  // providing default values for now.
  String underline_style_value = "solid";
  String text_color_value = "transparent";
  DocumentMarkerController& document_marker_controller =
      range->OwnerDocument().Markers();
  addStyleableMarkerHelper(
      range, underline_color_value, thickness_value, underline_style_value,
      text_color_value, background_color_value, exception_state,
      [&document_marker_controller](const EphemeralRange& range,
                                    Color underline_color,
                                    ImeTextSpanThickness thickness,
                                    ImeTextSpanUnderlineStyle underline_style,
                                    Color text_color, Color background_color) {
        document_marker_controller.AddActiveSuggestionMarker(
            range, underline_color, thickness, underline_style, text_color,
            background_color);
      });
}

void Internals::addSuggestionMarker(
    const Range* range,
    const Vector<String>& suggestions,
    const String& suggestion_highlight_color_value,
    const String& underline_color_value,
    const String& thickness_value,
    const String& background_color_value,
    ExceptionState& exception_state) {
  // Underline style and text color aren't really supported for suggestions so
  // providing default values for now.
  String underline_style_value = "solid";
  String text_color_value = "transparent";
  Color suggestion_highlight_color;
  if (!ParseColor(suggestion_highlight_color_value, suggestion_highlight_color,
                  exception_state, "Invalid suggestion highlight color."))
    return;

  DocumentMarkerController& document_marker_controller =
      range->OwnerDocument().Markers();
  addStyleableMarkerHelper(
      range, underline_color_value, thickness_value, underline_style_value,
      text_color_value, background_color_value, exception_state,
      [&document_marker_controller, &suggestions, &suggestion_highlight_color](
          const EphemeralRange& range, Color underline_color,
          ImeTextSpanThickness thickness,
          ImeTextSpanUnderlineStyle underline_style, Color text_color,
          Color background_color) {
        document_marker_controller.AddSuggestionMarker(
            range,
            SuggestionMarkerProperties::Builder()
                .SetType(SuggestionMarker::SuggestionType::kNotMisspelling)
                .SetSuggestions(suggestions)
                .SetHighlightColor(suggestion_highlight_color)
                .SetUnderlineColor(underline_color)
                .SetThickness(thickness)
                .SetUnderlineStyle(underline_style)
                .SetTextColor(text_color)
                .SetBackgroundColor(background_color)
                .Build());
      });
}

void Internals::setTextMatchMarkersActive(Node* node,
                                          unsigned start_offset,
                                          unsigned end_offset,
                                          bool active) {
  DCHECK(node);
  node->GetDocument().Markers().SetTextMatchMarkersActive(
      To<Text>(*node), start_offset, end_offset, active);
}

void Internals::setMarkedTextMatchesAreHighlighted(Document* document,
                                                   bool highlight) {
  if (!document || !document->GetFrame())
    return;

  document->GetFrame()->GetEditor().SetMarkedTextMatchesAreHighlighted(
      highlight);
}

String Internals::viewportAsText(Document* document,
                                 float,
                                 int available_width,
                                 int available_height,
                                 ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->GetPage()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return String();
  }

  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Page* page = document->GetPage();

  // Update initial viewport size.
  IntSize initial_viewport_size(available_width, available_height);
  document->GetPage()->DeprecatedLocalMainFrame()->View()->SetFrameRect(
      IntRect(IntPoint::Zero(), initial_viewport_size));

  ViewportDescription description = page->GetViewportDescription();
  PageScaleConstraints constraints =
      description.Resolve(FloatSize(initial_viewport_size), Length());

  constraints.FitToContentsWidth(constraints.layout_size.Width(),
                                 available_width);
  constraints.ResolveAutoInitialScale();

  StringBuilder builder;

  builder.Append("viewport size ");
  builder.Append(String::Number(constraints.layout_size.Width()));
  builder.Append('x');
  builder.Append(String::Number(constraints.layout_size.Height()));

  builder.Append(" scale ");
  builder.Append(String::Number(constraints.initial_scale));
  builder.Append(" with limits [");
  builder.Append(String::Number(constraints.minimum_scale));
  builder.Append(", ");
  builder.Append(String::Number(constraints.maximum_scale));

  builder.Append("] and userScalable ");
  builder.Append(description.user_zoom ? "true" : "false");

  return builder.ToString();
}

bool Internals::elementShouldAutoComplete(Element* element,
                                          ExceptionState& exception_state) {
  DCHECK(element);
  if (auto* input = DynamicTo<HTMLInputElement>(*element))
    return input->ShouldAutocomplete();

  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidNodeTypeError,
                                    "The element provided is not an INPUT.");
  return false;
}

String Internals::suggestedValue(Element* element,
                                 ExceptionState& exception_state) {
  DCHECK(element);
  if (!element->IsFormControlElement()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidNodeTypeError,
        "The element provided is not a form control element.");
    return String();
  }

  String suggested_value;
  if (auto* input = DynamicTo<HTMLInputElement>(*element))
    return input->SuggestedValue();

  if (auto* textarea = DynamicTo<HTMLTextAreaElement>(*element))
    return textarea->SuggestedValue();

  if (auto* select = DynamicTo<HTMLSelectElement>(*element))
    return select->SuggestedValue();

  return suggested_value;
}

void Internals::setSuggestedValue(Element* element,
                                  const String& value,
                                  ExceptionState& exception_state) {
  DCHECK(element);
  if (!element->IsFormControlElement()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidNodeTypeError,
        "The element provided is not a form control element.");
    return;
  }

  if (auto* input = DynamicTo<HTMLInputElement>(*element))
    input->SetSuggestedValue(value);

  if (auto* textarea = DynamicTo<HTMLTextAreaElement>(*element))
    textarea->SetSuggestedValue(value);

  if (auto* select = DynamicTo<HTMLSelectElement>(*element))
    select->SetSuggestedValue(value);
}

void Internals::setAutofilledValue(Element* element,
                                   const String& value,
                                   ExceptionState& exception_state) {
  DCHECK(element);
  if (!element->IsFormControlElement()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidNodeTypeError,
        "The element provided is not a form control element.");
    return;
  }

  if (auto* input = DynamicTo<HTMLInputElement>(*element)) {
    input->DispatchScopedEvent(
        *Event::CreateBubble(event_type_names::kKeydown));
    input->SetAutofillValue(value);
    input->DispatchScopedEvent(*Event::CreateBubble(event_type_names::kKeyup));
  }

  if (auto* textarea = DynamicTo<HTMLTextAreaElement>(*element)) {
    textarea->DispatchScopedEvent(
        *Event::CreateBubble(event_type_names::kKeydown));
    textarea->SetAutofillValue(value);
    textarea->DispatchScopedEvent(
        *Event::CreateBubble(event_type_names::kKeyup));
  }

  if (auto* select = DynamicTo<HTMLSelectElement>(*element))
    select->setValue(value, true /* send_events */);

  To<HTMLFormControlElement>(element)->SetAutofillState(
      blink::WebAutofillState::kAutofilled);
}

void Internals::setEditingValue(Element* element,
                                const String& value,
                                ExceptionState& exception_state) {
  DCHECK(element);
  auto* html_input_element = DynamicTo<HTMLInputElement>(element);
  if (!html_input_element) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidNodeTypeError,
                                      "The element provided is not an INPUT.");
    return;
  }

  html_input_element->SetEditingValue(value);
}

void Internals::setAutofilled(Element* element,
                              bool enabled,
                              ExceptionState& exception_state) {
  DCHECK(element);
  auto* form_control_element = DynamicTo<HTMLFormControlElement>(element);
  if (!form_control_element) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidNodeTypeError,
        "The element provided is not a form control element.");
    return;
  }
  form_control_element->SetAutofillState(
      enabled ? WebAutofillState::kAutofilled : WebAutofillState::kNotFilled);
}

void Internals::setSelectionRangeForNumberType(
    Element* input_element,
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  DCHECK(input_element);
  auto* html_input_element = DynamicTo<HTMLInputElement>(input_element);
  if (!html_input_element) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidNodeTypeError,
        "The element provided is not an input element.");
    return;
  }

  html_input_element->SetSelectionRangeForTesting(start, end, exception_state);
}

Range* Internals::rangeFromLocationAndLength(Element* scope,
                                             int range_location,
                                             int range_length) {
  DCHECK(scope);

  // TextIterator depends on Layout information, make sure layout it up to date.
  scope->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  return CreateRange(
      PlainTextRange(range_location, range_location + range_length)
          .CreateRange(*scope));
}

unsigned Internals::locationFromRange(Element* scope, const Range* range) {
  DCHECK(scope && range);
  // PlainTextRange depends on Layout information, make sure layout it up to
  // date.
  scope->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  return PlainTextRange::Create(*scope, *range).Start();
}

unsigned Internals::lengthFromRange(Element* scope, const Range* range) {
  DCHECK(scope && range);
  // PlainTextRange depends on Layout information, make sure layout it up to
  // date.
  scope->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  return PlainTextRange::Create(*scope, *range).length();
}

String Internals::rangeAsText(const Range* range) {
  DCHECK(range);
  // Clean layout is required by plain text extraction.
  range->OwnerDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  return range->GetText();
}

// FIXME: The next four functions are very similar - combine them once
// bestClickableNode/bestContextMenuNode have been combined..

void Internals::HitTestRect(HitTestLocation& location,
                            HitTestResult& result,
                            int x,
                            int y,
                            int width,
                            int height,
                            Document* document) {
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EventHandler& event_handler = document->GetFrame()->GetEventHandler();
  PhysicalRect rect{LayoutUnit(x), LayoutUnit(y), LayoutUnit(width),
                    LayoutUnit(height)};
  rect.offset = document->GetFrame()->View()->ConvertFromRootFrame(rect.offset);
  location = HitTestLocation(rect);
  result = event_handler.HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive |
                    HitTestRequest::kListBased);
}

DOMPoint* Internals::touchPositionAdjustedToBestClickableNode(
    int x,
    int y,
    int width,
    int height,
    Document* document,
    ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return nullptr;
  }

  HitTestLocation location;
  HitTestResult result;
  HitTestRect(location, result, x, y, width, height, document);
  Node* target_node = nullptr;
  IntPoint adjusted_point;

  EventHandler& event_handler = document->GetFrame()->GetEventHandler();
  bool found_node = event_handler.BestClickableNodeForHitTestResult(
      location, result, adjusted_point, target_node);
  if (found_node)
    return DOMPoint::Create(adjusted_point.X(), adjusted_point.Y());

  return nullptr;
}

Node* Internals::touchNodeAdjustedToBestClickableNode(
    int x,
    int y,
    int width,
    int height,
    Document* document,
    ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return nullptr;
  }

  HitTestLocation location;
  HitTestResult result;
  HitTestRect(location, result, x, y, width, height, document);
  Node* target_node = nullptr;
  IntPoint adjusted_point;
  document->GetFrame()->GetEventHandler().BestClickableNodeForHitTestResult(
      location, result, adjusted_point, target_node);
  return target_node;
}

DOMPoint* Internals::touchPositionAdjustedToBestContextMenuNode(
    int x,
    int y,
    int width,
    int height,
    Document* document,
    ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return nullptr;
  }

  HitTestLocation location;
  HitTestResult result;
  HitTestRect(location, result, x, y, width, height, document);
  Node* target_node = nullptr;
  IntPoint adjusted_point;

  EventHandler& event_handler = document->GetFrame()->GetEventHandler();
  bool found_node = event_handler.BestContextMenuNodeForHitTestResult(
      location, result, adjusted_point, target_node);
  if (found_node)
    return DOMPoint::Create(adjusted_point.X(), adjusted_point.Y());

  return DOMPoint::Create(x, y);
}

Node* Internals::touchNodeAdjustedToBestContextMenuNode(
    int x,
    int y,
    int width,
    int height,
    Document* document,
    ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return nullptr;
  }

  HitTestLocation location;
  HitTestResult result;
  HitTestRect(location, result, x, y, width, height, document);
  Node* target_node = nullptr;
  IntPoint adjusted_point;
  document->GetFrame()->GetEventHandler().BestContextMenuNodeForHitTestResult(
      location, result, adjusted_point, target_node);
  return target_node;
}

int Internals::lastSpellCheckRequestSequence(Document* document,
                                             ExceptionState& exception_state) {
  SpellCheckRequester* requester = GetSpellCheckRequester(document);

  if (!requester) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No spell check requestor can be obtained for the provided document.");
    return -1;
  }

  return requester->LastRequestSequence();
}

int Internals::lastSpellCheckProcessedSequence(
    Document* document,
    ExceptionState& exception_state) {
  SpellCheckRequester* requester = GetSpellCheckRequester(document);

  if (!requester) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No spell check requestor can be obtained for the provided document.");
    return -1;
  }

  return requester->LastProcessedSequence();
}

void Internals::cancelCurrentSpellCheckRequest(
    Document* document,
    ExceptionState& exception_state) {
  SpellCheckRequester* requester = GetSpellCheckRequester(document);

  if (!requester) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No spell check requestor can be obtained for the provided document.");
    return;
  }

  requester->CancelCheck();
}

String Internals::idleTimeSpellCheckerState(Document* document,
                                            ExceptionState& exception_state) {
  static const char* const kTexts[] = {
#define V(state) #state,
      FOR_EACH_IDLE_SPELL_CHECK_CONTROLLER_STATE(V)
#undef V
  };

  if (!document || !document->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No frame can be obtained from the provided document.");
    return String();
  }

  IdleSpellCheckController::State state = document->GetFrame()
                                              ->GetSpellChecker()
                                              .GetIdleSpellCheckController()
                                              .GetState();
  auto* const* const it = std::begin(kTexts) + static_cast<size_t>(state);
  DCHECK_GE(it, std::begin(kTexts)) << "Unknown state value";
  DCHECK_LT(it, std::end(kTexts)) << "Unknown state value";
  return *it;
}

void Internals::runIdleTimeSpellChecker(Document* document,
                                        ExceptionState& exception_state) {
  if (!document || !document->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No frame can be obtained from the provided document.");
    return;
  }

  document->GetFrame()
      ->GetSpellChecker()
      .GetIdleSpellCheckController()
      .ForceInvocationForTesting();
}

Vector<AtomicString> Internals::userPreferredLanguages() const {
  return blink::UserPreferredLanguages();
}

// Optimally, the bindings generator would pass a Vector<AtomicString> here but
// this is not supported yet.
void Internals::setUserPreferredLanguages(const Vector<String>& languages) {
  Vector<AtomicString> atomic_languages;
  for (const String& language : languages)
    atomic_languages.push_back(AtomicString(language));
  OverrideUserPreferredLanguagesForTesting(atomic_languages);
}

void Internals::setSystemTimeZone(const String& timezone) {
  blink::TimeZoneController::ChangeTimeZoneForTesting(timezone);
}

unsigned Internals::mediaKeysCount() {
  return InstanceCounters::CounterValue(InstanceCounters::kMediaKeysCounter);
}

unsigned Internals::mediaKeySessionCount() {
  return InstanceCounters::CounterValue(
      InstanceCounters::kMediaKeySessionCounter);
}

static unsigned EventHandlerCount(
    Document& document,
    EventHandlerRegistry::EventHandlerClass handler_class) {
  if (!document.GetPage())
    return 0;
  EventHandlerRegistry* registry =
      &document.GetFrame()->GetEventHandlerRegistry();
  unsigned count = 0;
  const EventTargetSet* targets = registry->EventHandlerTargets(handler_class);
  if (targets) {
    for (const auto& target : *targets)
      count += target.value;
  }
  return count;
}

unsigned Internals::wheelEventHandlerCount(Document* document) const {
  DCHECK(document);
  return EventHandlerCount(*document,
                           EventHandlerRegistry::kWheelEventBlocking) +
         EventHandlerCount(*document, EventHandlerRegistry::kWheelEventPassive);
}

unsigned Internals::scrollEventHandlerCount(Document* document) const {
  DCHECK(document);
  return EventHandlerCount(*document, EventHandlerRegistry::kScrollEvent);
}

unsigned Internals::touchStartOrMoveEventHandlerCount(
    Document* document) const {
  DCHECK(document);
  return EventHandlerCount(*document, EventHandlerRegistry::kTouchAction) +
         EventHandlerCount(
             *document, EventHandlerRegistry::kTouchStartOrMoveEventBlocking) +
         EventHandlerCount(
             *document,
             EventHandlerRegistry::kTouchStartOrMoveEventBlockingLowLatency) +
         EventHandlerCount(*document,
                           EventHandlerRegistry::kTouchStartOrMoveEventPassive);
}

unsigned Internals::touchEndOrCancelEventHandlerCount(
    Document* document) const {
  DCHECK(document);
  return EventHandlerCount(
             *document, EventHandlerRegistry::kTouchEndOrCancelEventBlocking) +
         EventHandlerCount(*document,
                           EventHandlerRegistry::kTouchEndOrCancelEventPassive);
}

unsigned Internals::pointerEventHandlerCount(Document* document) const {
  DCHECK(document);
  return EventHandlerCount(*document, EventHandlerRegistry::kPointerEvent) +
         EventHandlerCount(*document,
                           EventHandlerRegistry::kPointerRawUpdateEvent);
}

// Given a vector of rects, merge those that are adjacent, leaving empty rects
// in the place of no longer used slots. This is intended to simplify the list
// of rects returned by an SkRegion (which have been split apart for sorting
// purposes). No attempt is made to do this efficiently (eg. by relying on the
// sort criteria of SkRegion).
static void MergeRects(Vector<IntRect>& rects) {
  for (wtf_size_t i = 0; i < rects.size(); ++i) {
    if (rects[i].IsEmpty())
      continue;
    bool updated;
    do {
      updated = false;
      for (wtf_size_t j = i + 1; j < rects.size(); ++j) {
        if (rects[j].IsEmpty())
          continue;
        // Try to merge rects[j] into rects[i] along the 4 possible edges.
        if (rects[i].Y() == rects[j].Y() &&
            rects[i].Height() == rects[j].Height()) {
          if (rects[i].X() + rects[i].Width() == rects[j].X()) {
            rects[i].Expand(rects[j].Width(), 0);
            rects[j] = IntRect();
            updated = true;
          } else if (rects[i].X() == rects[j].X() + rects[j].Width()) {
            rects[i].SetX(rects[j].X());
            rects[i].Expand(rects[j].Width(), 0);
            rects[j] = IntRect();
            updated = true;
          }
        } else if (rects[i].X() == rects[j].X() &&
                   rects[i].Width() == rects[j].Width()) {
          if (rects[i].Y() + rects[i].Height() == rects[j].Y()) {
            rects[i].Expand(0, rects[j].Height());
            rects[j] = IntRect();
            updated = true;
          } else if (rects[i].Y() == rects[j].Y() + rects[j].Height()) {
            rects[i].SetY(rects[j].Y());
            rects[i].Expand(0, rects[j].Height());
            rects[j] = IntRect();
            updated = true;
          }
        }
      }
    } while (updated);
  }
}

HitTestLayerRectList* Internals::touchEventTargetLayerRects(
    Document* document,
    ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->View() || !document->GetPage() || document != document_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return nullptr;
  }

  document->View()->UpdateAllLifecyclePhasesForTest();

  auto* hit_test_rects = MakeGarbageCollected<HitTestLayerRectList>();
  for (const auto& layer : document->View()->RootCcLayer()->children()) {
    const cc::TouchActionRegion& touch_action_region =
        layer->touch_action_region();
    if (!touch_action_region.GetAllRegions().IsEmpty()) {
      const auto& offset = layer->offset_to_transform_parent();
      IntRect layer_rect(RoundedIntPoint(FloatPoint(offset.x(), offset.y())),
                         IntSize(layer->bounds()));

      Vector<IntRect> layer_hit_test_rects;
      for (const auto& hit_test_rect : touch_action_region.GetAllRegions())
        layer_hit_test_rects.push_back(IntRect(hit_test_rect));
      MergeRects(layer_hit_test_rects);

      for (const IntRect& hit_test_rect : layer_hit_test_rects) {
        if (!hit_test_rect.IsEmpty()) {
          hit_test_rects->Append(DOMRectReadOnly::FromIntRect(layer_rect),
                                 DOMRectReadOnly::FromIntRect(hit_test_rect));
        }
      }
    }
  }
  return hit_test_rects;
}

bool Internals::executeCommand(Document* document,
                               const String& name,
                               const String& value,
                               ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return false;
  }

  LocalFrame* frame = document->GetFrame();
  return frame->GetEditor().ExecuteCommand(name, value);
}

void Internals::triggerTestInspectorIssue(Document* document) {
  DCHECK(document);
  auto info = mojom::blink::InspectorIssueInfo::New(
      mojom::InspectorIssueCode::kSameSiteCookieIssue,
      mojom::blink::InspectorIssueDetails::New());
  document->GetFrame()->AddInspectorIssue(std::move(info));
}

AtomicString Internals::htmlNamespace() {
  return html_names::xhtmlNamespaceURI;
}

Vector<AtomicString> Internals::htmlTags() {
  Vector<AtomicString> tags(html_names::kTagsCount);
  std::unique_ptr<const HTMLQualifiedName* []> qualified_names =
      html_names::GetTags();
  for (wtf_size_t i = 0; i < html_names::kTagsCount; ++i)
    tags[i] = qualified_names[i]->LocalName();
  return tags;
}

AtomicString Internals::svgNamespace() {
  return svg_names::kNamespaceURI;
}

Vector<AtomicString> Internals::svgTags() {
  Vector<AtomicString> tags(svg_names::kTagsCount);
  std::unique_ptr<const SVGQualifiedName* []> qualified_names =
      svg_names::GetTags();
  for (wtf_size_t i = 0; i < svg_names::kTagsCount; ++i)
    tags[i] = qualified_names[i]->LocalName();
  return tags;
}

StaticNodeList* Internals::nodesFromRect(
    Document* document,
    int x,
    int y,
    int width,
    int height,
    bool ignore_clipping,
    bool allow_child_frame_content,
    ExceptionState& exception_state) const {
  DCHECK(document);
  if (!document->GetFrame() || !document->GetFrame()->View()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No view can be obtained from the provided document.");
    return nullptr;
  }

  HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kReadOnly |
                                                HitTestRequest::kActive |
                                                HitTestRequest::kListBased;
  LocalFrame* frame = document->GetFrame();
  PhysicalRect rect{LayoutUnit(x), LayoutUnit(y), LayoutUnit(width),
                    LayoutUnit(height)};
  if (ignore_clipping) {
    hit_type |= HitTestRequest::kIgnoreClipping;
  } else if (!IntRect(IntPoint(), frame->View()->Size())
                  .Intersects(EnclosingIntRect(rect))) {
    return nullptr;
  }
  if (allow_child_frame_content)
    hit_type |= HitTestRequest::kAllowChildFrameContent;

  HeapVector<Member<Node>> matches;
  HitTestRequest request(hit_type);
  HitTestLocation location(rect);
  HitTestResult result(request, location);
  frame->ContentLayoutObject()->HitTest(location, result);
  CopyToVector(result.ListBasedTestResult(), matches);

  return StaticNodeList::Adopt(matches);
}

bool Internals::hasSpellingMarker(Document* document,
                                  int from,
                                  int length,
                                  ExceptionState& exception_state) {
  if (!document || !document->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No frame can be obtained from the provided document.");
    return false;
  }

  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  return document->GetFrame()->GetSpellChecker().SelectionStartHasMarkerFor(
      DocumentMarker::kSpelling, from, length);
}

void Internals::replaceMisspelled(Document* document,
                                  const String& replacement,
                                  ExceptionState& exception_state) {
  if (!document || !document->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No frame can be obtained from the provided document.");
    return;
  }

  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  document->GetFrame()->GetSpellChecker().ReplaceMisspelledRange(replacement);
}

bool Internals::canHyphenate(const AtomicString& locale) {
  return LayoutLocale::ValueOrDefault(LayoutLocale::Get(locale))
      .GetHyphenation();
}

void Internals::setMockHyphenation(const AtomicString& locale) {
  LayoutLocale::SetHyphenationForTesting(locale,
                                         base::AdoptRef(new MockHyphenation));
}

bool Internals::isOverwriteModeEnabled(Document* document) {
  DCHECK(document);
  if (!document->GetFrame())
    return false;

  return document->GetFrame()->GetEditor().IsOverwriteModeEnabled();
}

void Internals::toggleOverwriteModeEnabled(Document* document) {
  DCHECK(document);
  if (!document->GetFrame())
    return;

  document->GetFrame()->GetEditor().ToggleOverwriteModeEnabled();
}

unsigned Internals::numberOfLiveNodes() const {
  return InstanceCounters::CounterValue(InstanceCounters::kNodeCounter);
}

unsigned Internals::numberOfLiveDocuments() const {
  return InstanceCounters::CounterValue(InstanceCounters::kDocumentCounter);
}

bool Internals::hasGrammarMarker(Document* document,
                                 int from,
                                 int length,
                                 ExceptionState& exception_state) {
  if (!document || !document->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No frame can be obtained from the provided document.");
    return false;
  }

  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  return document->GetFrame()->GetSpellChecker().SelectionStartHasMarkerFor(
      DocumentMarker::kGrammar, from, length);
}

unsigned Internals::numberOfScrollableAreas(Document* document) {
  DCHECK(document);
  if (!document->GetFrame())
    return 0;

  unsigned count = 0;
  LocalFrame* frame = document->GetFrame();
  if (frame->View()->ScrollableAreas()) {
    for (const auto& scrollable_area : *frame->View()->ScrollableAreas()) {
      if (scrollable_area->ScrollsOverflow())
        count++;
    }
  }

  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (child_local_frame && child_local_frame->View() &&
        child_local_frame->View()->ScrollableAreas()) {
      for (const auto& scrollable_area :
           *child_local_frame->View()->ScrollableAreas()) {
        if (scrollable_area->ScrollsOverflow())
          count++;
      }
    }
  }

  return count;
}

bool Internals::isPageBoxVisible(Document* document, int page_number) {
  DCHECK(document);
  return document->IsPageBoxVisible(page_number);
}

String Internals::layerTreeAsText(Document* document,
                                  ExceptionState& exception_state) const {
  return layerTreeAsText(document, 0, exception_state);
}

bool Internals::scrollsWithRespectTo(Element* element1,
                                     Element* element2,
                                     ExceptionState& exception_state) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK(element1 && element2);
  element1->GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  LayoutObject* layout_object1 = element1->GetLayoutObject();
  LayoutObject* layout_object2 = element2->GetLayoutObject();
  if (!layout_object1 || !layout_object1->IsBox()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        layout_object1
            ? "The first provided element's layoutObject is not a box."
            : "The first provided element has no layoutObject.");
    return false;
  }
  if (!layout_object2 || !layout_object2->IsBox()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        layout_object2
            ? "The second provided element's layoutObject is not a box."
            : "The second provided element has no layoutObject.");
    return false;
  }

  PaintLayer* layer1 = To<LayoutBox>(layout_object1)->Layer();
  PaintLayer* layer2 = To<LayoutBox>(layout_object2)->Layer();
  if (!layer1 || !layer2) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        String::Format(
            "No PaintLayer can be obtained from the %s provided element.",
            layer1 ? "second" : "first"));
    return false;
  }

  return layer1->ScrollsWithRespectTo(layer2);
}

String Internals::layerTreeAsText(Document* document,
                                  unsigned flags,
                                  ExceptionState& exception_state) const {
  DCHECK(document);
  if (!document->GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return String();
  }

  document->View()->UpdateAllLifecyclePhasesForTest();

  return document->GetFrame()->GetLayerTreeAsTextForTesting(flags);
}

String Internals::scrollingStateTreeAsText(Document*) const {
  return String();
}

String Internals::mainThreadScrollingReasons(
    Document* document,
    ExceptionState& exception_state) const {
  DCHECK(document);
  if (!document->GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return String();
  }

  document->GetFrame()->View()->UpdateAllLifecyclePhasesForTest();

  return document->GetFrame()->View()->MainThreadScrollingReasonsAsText();
}

DOMRectList* Internals::nonFastScrollableRects(
    Document* document,
    ExceptionState& exception_state) const {
  DCHECK(document);
  const LocalFrame* frame = document->GetFrame();
  if (!frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return nullptr;
  }

  frame->View()->UpdateAllLifecyclePhasesForTest();

  auto* pac = document->View()->GetPaintArtifactCompositor();
  auto* layer_tree_host = pac->RootLayer()->layer_tree_host();
  // Ensure |cc::TransformTree| has updated the correct ToScreen transforms.
  layer_tree_host->UpdateLayers();

  Vector<IntRect> layer_non_fast_scrollable_rects;
  for (auto* layer : *layer_tree_host) {
    const cc::Region& non_fast_region = layer->non_fast_scrollable_region();
    for (const gfx::Rect& non_fast_rect : non_fast_region) {
      gfx::RectF layer_rect(non_fast_rect);

      // Map |layer_rect| into screen space.
      layer_rect.Offset(layer->offset_to_transform_parent());
      auto& transform_tree =
          layer->layer_tree_host()->property_trees()->transform_tree;
      transform_tree.UpdateTransforms(layer->transform_tree_index());
      const gfx::Transform& to_screen =
          transform_tree.ToScreen(layer->transform_tree_index());
      to_screen.TransformRect(&layer_rect);

      layer_non_fast_scrollable_rects.push_back(
          IntRect(ToEnclosingRect(layer_rect)));
    }
  }

  return MakeGarbageCollected<DOMRectList>(layer_non_fast_scrollable_rects);
}

void Internals::evictAllResources() const {
  GetMemoryCache()->EvictResources();
}

String Internals::counterValue(Element* element) {
  if (!element)
    return String();

  return CounterValueForElement(element);
}

int Internals::pageNumber(Element* element,
                          float page_width,
                          float page_height,
                          ExceptionState& exception_state) {
  if (!element)
    return 0;

  if (page_width <= 0 || page_height <= 0) {
    exception_state.ThrowTypeError(
        "Page width and height must be larger than 0.");
    return 0;
  }

  return PrintContext::PageNumberForElement(element,
                                            FloatSize(page_width, page_height));
}

Vector<String> Internals::IconURLs(Document* document,
                                   int icon_types_mask) const {
  Vector<IconURL> icon_urls = document->IconURLs(icon_types_mask);
  Vector<String> array;

  for (auto& icon_url : icon_urls)
    array.push_back(icon_url.icon_url_.GetString());

  return array;
}

Vector<String> Internals::shortcutIconURLs(Document* document) const {
  int icon_types_mask =
      1 << static_cast<int>(mojom::blink::FaviconIconType::kFavicon);
  return IconURLs(document, icon_types_mask);
}

Vector<String> Internals::allIconURLs(Document* document) const {
  int icon_types_mask =
      1 << static_cast<int>(mojom::blink::FaviconIconType::kFavicon) |
      1 << static_cast<int>(mojom::blink::FaviconIconType::kTouchIcon) |
      1 << static_cast<int>(
          mojom::blink::FaviconIconType::kTouchPrecomposedIcon);
  return IconURLs(document, icon_types_mask);
}

int Internals::numberOfPages(float page_width,
                             float page_height,
                             ExceptionState& exception_state) {
  if (!GetFrame())
    return -1;

  if (page_width <= 0 || page_height <= 0) {
    exception_state.ThrowTypeError(
        "Page width and height must be larger than 0.");
    return -1;
  }

  return PrintContext::NumberOfPages(GetFrame(),
                                     FloatSize(page_width, page_height));
}

String Internals::pageProperty(String property_name,
                               unsigned page_number,
                               ExceptionState& exception_state) const {
  if (!GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No frame is available.");
    return String();
  }

  return PrintContext::PageProperty(GetFrame(), property_name.Utf8().c_str(),
                                    page_number);
}

String Internals::pageSizeAndMarginsInPixels(
    unsigned page_number,
    int width,
    int height,
    int margin_top,
    int margin_right,
    int margin_bottom,
    int margin_left,
    ExceptionState& exception_state) const {
  if (!GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No frame is available.");
    return String();
  }

  return PrintContext::PageSizeAndMarginsInPixels(
      GetFrame(), page_number, width, height, margin_top, margin_right,
      margin_bottom, margin_left);
}

float Internals::pageScaleFactor(ExceptionState& exception_state) {
  if (!document_->GetPage()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The document's page cannot be retrieved.");
    return 0;
  }
  Page* page = document_->GetPage();
  return page->GetVisualViewport().Scale();
}

void Internals::setPageScaleFactor(float scale_factor,
                                   ExceptionState& exception_state) {
  if (scale_factor <= 0)
    return;
  if (!document_->GetPage()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The document's page cannot be retrieved.");
    return;
  }
  Page* page = document_->GetPage();
  page->GetVisualViewport().SetScale(scale_factor);
}

void Internals::setPageScaleFactorLimits(float min_scale_factor,
                                         float max_scale_factor,
                                         ExceptionState& exception_state) {
  if (!document_->GetPage()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The document's page cannot be retrieved.");
    return;
  }

  Page* page = document_->GetPage();
  page->SetDefaultPageScaleLimits(min_scale_factor, max_scale_factor);
}

float Internals::pageZoomFactor(ExceptionState& exception_state) {
  if (!document_->GetPage()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The document's page cannot be retrieved.");
    return 0;
  }
  // Page zoom without Device Scale Factor.
  return document_->GetPage()->GetChromeClient().UserZoomFactor();
}

void Internals::setIsCursorVisible(Document* document,
                                   bool is_visible,
                                   ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->GetPage()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No context document can be obtained.");
    return;
  }
  document->GetPage()->SetIsCursorVisible(is_visible);
}

void Internals::setMaxNumberOfFramesToTen(bool enabled) {
  // This gets reset by Internals::ResetToConsistentState
  Page::SetMaxNumberOfFramesToTenForTesting(enabled);
}

String Internals::effectivePreload(HTMLMediaElement* media_element) {
  DCHECK(media_element);
  return media_element->EffectivePreload();
}

void Internals::mediaPlayerRemoteRouteAvailabilityChanged(
    HTMLMediaElement* media_element,
    bool available) {
  DCHECK(media_element);

  RemotePlaybackController::From(*media_element)
      ->AvailabilityChangedForTesting(available);
}

void Internals::mediaPlayerPlayingRemotelyChanged(
    HTMLMediaElement* media_element,
    bool remote) {
  DCHECK(media_element);

  RemotePlaybackController::From(*media_element)
      ->StateChangedForTesting(remote);
}

void Internals::setPersistent(HTMLVideoElement* video_element,
                              bool persistent) {
  DCHECK(video_element);
  video_element->OnBecamePersistentVideo(persistent);
}

void Internals::forceStaleStateForMediaElement(HTMLMediaElement* media_element,
                                               int target_state) {
  DCHECK(media_element);
  // Even though this is an internals method, the checks are necessary to
  // prevent fuzzers from taking this path and generating useless noise.
  if (target_state < static_cast<int>(WebMediaPlayer::kReadyStateHaveNothing) ||
      target_state >
          static_cast<int>(WebMediaPlayer::kReadyStateHaveEnoughData)) {
    return;
  }

  if (auto* wmp = media_element->GetWebMediaPlayer()) {
    wmp->ForceStaleStateForTesting(
        static_cast<WebMediaPlayer::ReadyState>(target_state));
  }
}

bool Internals::isMediaElementSuspended(HTMLMediaElement* media_element) {
  DCHECK(media_element);
  if (auto* wmp = media_element->GetWebMediaPlayer())
    return wmp->IsSuspendedForTesting();
  return false;
}

void Internals::setMediaControlsTestMode(HTMLMediaElement* media_element,
                                         bool enable) {
  DCHECK(media_element);
  MediaControls* media_controls = media_element->GetMediaControls();
  DCHECK(media_controls);
  media_controls->SetTestMode(enable);
}

void Internals::registerURLSchemeAsBypassingContentSecurityPolicy(
    const String& scheme) {
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy(scheme);
}

void Internals::registerURLSchemeAsBypassingContentSecurityPolicy(
    const String& scheme,
    const Vector<String>& policy_areas) {
  uint32_t policy_areas_enum = SchemeRegistry::kPolicyAreaNone;
  for (const auto& policy_area : policy_areas) {
    if (policy_area == "img")
      policy_areas_enum |= SchemeRegistry::kPolicyAreaImage;
    else if (policy_area == "style")
      policy_areas_enum |= SchemeRegistry::kPolicyAreaStyle;
  }
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy(
      scheme, static_cast<SchemeRegistry::PolicyAreas>(policy_areas_enum));
}

void Internals::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
    const String& scheme) {
  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      scheme);
}

TypeConversions* Internals::typeConversions() const {
  return MakeGarbageCollected<TypeConversions>();
}

DictionaryTest* Internals::dictionaryTest() const {
  return MakeGarbageCollected<DictionaryTest>();
}

RecordTest* Internals::recordTest() const {
  return MakeGarbageCollected<RecordTest>();
}

SequenceTest* Internals::sequenceTest() const {
  return MakeGarbageCollected<SequenceTest>();
}

UnionTypesTest* Internals::unionTypesTest() const {
  return MakeGarbageCollected<UnionTypesTest>();
}

OriginTrialsTest* Internals::originTrialsTest() const {
  return MakeGarbageCollected<OriginTrialsTest>();
}

CallbackFunctionTest* Internals::callbackFunctionTest() const {
  return MakeGarbageCollected<CallbackFunctionTest>();
}

Vector<String> Internals::getReferencedFilePaths() const {
  if (!GetFrame())
    return Vector<String>();

  return GetFrame()
      ->Loader()
      .GetDocumentLoader()
      ->GetHistoryItem()
      ->GetReferencedFilePaths();
}

void Internals::startTrackingRepaints(Document* document,
                                      ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->View()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return;
  }

  LocalFrameView* frame_view = document->View();
  frame_view->UpdateAllLifecyclePhasesForTest();
  frame_view->SetTracksRasterInvalidations(true);
}

void Internals::stopTrackingRepaints(Document* document,
                                     ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->View()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return;
  }

  LocalFrameView* frame_view = document->View();
  frame_view->UpdateAllLifecyclePhasesForTest();
  frame_view->SetTracksRasterInvalidations(false);
}

void Internals::updateLayoutAndRunPostLayoutTasks(
    Node* node,
    ExceptionState& exception_state) {
  Document* document = nullptr;
  if (!node) {
    document = document_;
  } else if (IsA<Document>(node)) {
    document = To<Document>(node);
  } else if (auto* iframe = DynamicTo<HTMLIFrameElement>(*node)) {
    document = iframe->contentDocument();
  }

  if (!document) {
    exception_state.ThrowTypeError(
        "The node provided is neither a document nor an IFrame.");
    return;
  }
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  if (auto* view = document->View())
    view->FlushAnyPendingPostLayoutTasks();
}

void Internals::forceFullRepaint(Document* document,
                                 ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->View()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return;
  }

  if (auto* layout_view = document->GetLayoutView())
    layout_view->InvalidatePaintForViewAndDescendants();
}

DOMRectList* Internals::draggableRegions(Document* document,
                                         ExceptionState& exception_state) {
  return AnnotatedRegions(document, true, exception_state);
}

DOMRectList* Internals::nonDraggableRegions(Document* document,
                                            ExceptionState& exception_state) {
  return AnnotatedRegions(document, false, exception_state);
}

DOMRectList* Internals::AnnotatedRegions(Document* document,
                                         bool draggable,
                                         ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->View()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return MakeGarbageCollected<DOMRectList>();
  }

  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  document->View()->UpdateDocumentAnnotatedRegions();
  Vector<AnnotatedRegionValue> regions = document->AnnotatedRegions();

  Vector<FloatQuad> quads;
  for (const AnnotatedRegionValue& region : regions) {
    if (region.draggable == draggable)
      quads.push_back(FloatQuad(FloatRect(region.bounds)));
  }
  return MakeGarbageCollected<DOMRectList>(quads);
}

static const char* CursorTypeToString(
    ui::mojom::blink::CursorType cursor_type) {
  switch (cursor_type) {
    case ui::mojom::blink::CursorType::kPointer:
      return "Pointer";
    case ui::mojom::blink::CursorType::kCross:
      return "Cross";
    case ui::mojom::blink::CursorType::kHand:
      return "Hand";
    case ui::mojom::blink::CursorType::kIBeam:
      return "IBeam";
    case ui::mojom::blink::CursorType::kWait:
      return "Wait";
    case ui::mojom::blink::CursorType::kHelp:
      return "Help";
    case ui::mojom::blink::CursorType::kEastResize:
      return "EastResize";
    case ui::mojom::blink::CursorType::kNorthResize:
      return "NorthResize";
    case ui::mojom::blink::CursorType::kNorthEastResize:
      return "NorthEastResize";
    case ui::mojom::blink::CursorType::kNorthWestResize:
      return "NorthWestResize";
    case ui::mojom::blink::CursorType::kSouthResize:
      return "SouthResize";
    case ui::mojom::blink::CursorType::kSouthEastResize:
      return "SouthEastResize";
    case ui::mojom::blink::CursorType::kSouthWestResize:
      return "SouthWestResize";
    case ui::mojom::blink::CursorType::kWestResize:
      return "WestResize";
    case ui::mojom::blink::CursorType::kNorthSouthResize:
      return "NorthSouthResize";
    case ui::mojom::blink::CursorType::kEastWestResize:
      return "EastWestResize";
    case ui::mojom::blink::CursorType::kNorthEastSouthWestResize:
      return "NorthEastSouthWestResize";
    case ui::mojom::blink::CursorType::kNorthWestSouthEastResize:
      return "NorthWestSouthEastResize";
    case ui::mojom::blink::CursorType::kColumnResize:
      return "ColumnResize";
    case ui::mojom::blink::CursorType::kRowResize:
      return "RowResize";
    case ui::mojom::blink::CursorType::kMiddlePanning:
      return "MiddlePanning";
    case ui::mojom::blink::CursorType::kMiddlePanningVertical:
      return "MiddlePanningVertical";
    case ui::mojom::blink::CursorType::kMiddlePanningHorizontal:
      return "MiddlePanningHorizontal";
    case ui::mojom::blink::CursorType::kEastPanning:
      return "EastPanning";
    case ui::mojom::blink::CursorType::kNorthPanning:
      return "NorthPanning";
    case ui::mojom::blink::CursorType::kNorthEastPanning:
      return "NorthEastPanning";
    case ui::mojom::blink::CursorType::kNorthWestPanning:
      return "NorthWestPanning";
    case ui::mojom::blink::CursorType::kSouthPanning:
      return "SouthPanning";
    case ui::mojom::blink::CursorType::kSouthEastPanning:
      return "SouthEastPanning";
    case ui::mojom::blink::CursorType::kSouthWestPanning:
      return "SouthWestPanning";
    case ui::mojom::blink::CursorType::kWestPanning:
      return "WestPanning";
    case ui::mojom::blink::CursorType::kMove:
      return "Move";
    case ui::mojom::blink::CursorType::kVerticalText:
      return "VerticalText";
    case ui::mojom::blink::CursorType::kCell:
      return "Cell";
    case ui::mojom::blink::CursorType::kContextMenu:
      return "ContextMenu";
    case ui::mojom::blink::CursorType::kAlias:
      return "Alias";
    case ui::mojom::blink::CursorType::kProgress:
      return "Progress";
    case ui::mojom::blink::CursorType::kNoDrop:
      return "NoDrop";
    case ui::mojom::blink::CursorType::kCopy:
      return "Copy";
    case ui::mojom::blink::CursorType::kNone:
      return "None";
    case ui::mojom::blink::CursorType::kNotAllowed:
      return "NotAllowed";
    case ui::mojom::blink::CursorType::kZoomIn:
      return "ZoomIn";
    case ui::mojom::blink::CursorType::kZoomOut:
      return "ZoomOut";
    case ui::mojom::blink::CursorType::kGrab:
      return "Grab";
    case ui::mojom::blink::CursorType::kGrabbing:
      return "Grabbing";
    case ui::mojom::blink::CursorType::kCustom:
      return "Custom";
    case ui::mojom::blink::CursorType::kNull:
      return "Null";
    case ui::mojom::blink::CursorType::kDndNone:
      return "DragAndDropNone";
    case ui::mojom::blink::CursorType::kDndMove:
      return "DragAndDropMove";
    case ui::mojom::blink::CursorType::kDndCopy:
      return "DragAndDropCopy";
    case ui::mojom::blink::CursorType::kDndLink:
      return "DragAndDropLink";
  }

  NOTREACHED();
  return "UNKNOWN";
}

String Internals::getCurrentCursorInfo() {
  if (!GetFrame())
    return String();

  ui::Cursor cursor =
      GetFrame()->GetPage()->GetChromeClient().LastSetCursorForTesting();

  StringBuilder result;
  result.Append("type=");
  result.Append(CursorTypeToString(cursor.type()));
  if (cursor.type() == ui::mojom::blink::CursorType::kCustom) {
    result.Append(" hotSpot=");
    result.AppendNumber(cursor.custom_hotspot().x());
    result.Append(',');
    result.AppendNumber(cursor.custom_hotspot().y());

    SkBitmap bitmap = cursor.custom_bitmap();
    DCHECK(!bitmap.isNull());
    result.Append(" image=");
    result.AppendNumber(bitmap.width());
    result.Append('x');
    result.AppendNumber(bitmap.height());
  }
  if (cursor.image_scale_factor() != 1) {
    result.Append(" scale=");
    result.AppendNumber(cursor.image_scale_factor(), 8);
  }

  return result.ToString();
}

bool Internals::cursorUpdatePending() const {
  if (!GetFrame())
    return false;

  return GetFrame()->GetEventHandler().CursorUpdatePending();
}

DOMArrayBuffer* Internals::serializeObject(
    v8::Isolate* isolate,
    const ScriptValue& value,
    ExceptionState& exception_state) const {
  scoped_refptr<SerializedScriptValue> serialized_value =
      SerializedScriptValue::Serialize(
          isolate, value.V8Value(),
          SerializedScriptValue::SerializeOptions(
              SerializedScriptValue::kNotForStorage),
          exception_state);
  if (exception_state.HadException())
    return nullptr;

  base::span<const uint8_t> span = serialized_value->GetWireData();
  DOMArrayBuffer* buffer = DOMArrayBuffer::CreateUninitializedOrNull(
      SafeCast<uint32_t>(span.size()), sizeof(uint8_t));
  if (buffer)
    memcpy(buffer->Data(), span.data(), span.size());
  return buffer;
}

ScriptValue Internals::deserializeBuffer(v8::Isolate* isolate,
                                         DOMArrayBuffer* buffer) const {
  scoped_refptr<SerializedScriptValue> serialized_value =
      SerializedScriptValue::Create(static_cast<const char*>(buffer->Data()),
                                    buffer->ByteLength());
  return ScriptValue(isolate, serialized_value->Deserialize(isolate));
}

void Internals::forceReload(bool bypass_cache) {
  if (!GetFrame())
    return;

  GetFrame()->Reload(bypass_cache ? WebFrameLoadType::kReloadBypassingCache
                                  : WebFrameLoadType::kReload);
}

StaticSelection* Internals::getDragCaret() {
  SelectionInDOMTree::Builder builder;
  if (GetFrame()) {
    const DragCaret& caret = GetFrame()->GetPage()->GetDragCaret();
    const PositionWithAffinity& position = caret.CaretPosition();
    if (position.GetDocument() == GetFrame()->GetDocument())
      builder.Collapse(caret.CaretPosition());
  }
  return StaticSelection::FromSelectionInDOMTree(builder.Build());
}

StaticSelection* Internals::getSelectionInFlatTree(
    DOMWindow* window,
    ExceptionState& exception_state) {
  Frame* const frame = window->GetFrame();
  auto* local_frame = DynamicTo<LocalFrame>(frame);
  if (!local_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "Must supply local window");
    return nullptr;
  }
  return StaticSelection::FromSelectionInFlatTree(ConvertToSelectionInFlatTree(
      local_frame->Selection().GetSelectionInDOMTree()));
}

Node* Internals::visibleSelectionAnchorNode() {
  if (!GetFrame())
    return nullptr;
  Position position = GetFrame()
                          ->Selection()
                          .ComputeVisibleSelectionInDOMTreeDeprecated()
                          .Base();
  return position.IsNull() ? nullptr : position.ComputeContainerNode();
}

unsigned Internals::visibleSelectionAnchorOffset() {
  if (!GetFrame())
    return 0;
  Position position = GetFrame()
                          ->Selection()
                          .ComputeVisibleSelectionInDOMTreeDeprecated()
                          .Base();
  return position.IsNull() ? 0 : position.ComputeOffsetInContainerNode();
}

Node* Internals::visibleSelectionFocusNode() {
  if (!GetFrame())
    return nullptr;
  Position position = GetFrame()
                          ->Selection()
                          .ComputeVisibleSelectionInDOMTreeDeprecated()
                          .Extent();
  return position.IsNull() ? nullptr : position.ComputeContainerNode();
}

unsigned Internals::visibleSelectionFocusOffset() {
  if (!GetFrame())
    return 0;
  Position position = GetFrame()
                          ->Selection()
                          .ComputeVisibleSelectionInDOMTreeDeprecated()
                          .Extent();
  return position.IsNull() ? 0 : position.ComputeOffsetInContainerNode();
}

DOMRect* Internals::selectionBounds(ExceptionState& exception_state) {
  if (!GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The document's frame cannot be retrieved.");
    return nullptr;
  }

  GetFrame()->View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kSelection);
  return DOMRect::FromFloatRect(
      FloatRect(GetFrame()->Selection().AbsoluteUnclippedBounds()));
}

String Internals::markerTextForListItem(Element* element) {
  DCHECK(element);
  return blink::MarkerTextForListItem(element);
}

String Internals::getImageSourceURL(Element* element) {
  DCHECK(element);
  return element->ImageSourceURL();
}

void Internals::forceImageReload(Element* element,
                                 ExceptionState& exception_state) {
  auto* html_image_element = DynamicTo<HTMLImageElement>(element);
  if (!html_image_element) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The element should be HTMLImageElement.");
  }

  html_image_element->ForceReload();
}

String Internals::selectMenuListText(HTMLSelectElement* select) {
  DCHECK(select);
  if (!select->UsesMenuList())
    return String();
  return select->InnerElement().innerText();
}

bool Internals::isSelectPopupVisible(Node* node) {
  DCHECK(node);
  if (auto* select = DynamicTo<HTMLSelectElement>(*node))
    return select->PopupIsVisible();
  return false;
}

bool Internals::selectPopupItemStyleIsRtl(Node* node, int item_index) {
  auto* select = DynamicTo<HTMLSelectElement>(node);
  if (!select)
    return false;

  if (item_index < 0 ||
      static_cast<wtf_size_t>(item_index) >= select->GetListItems().size())
    return false;
  const ComputedStyle* item_style =
      select->ItemComputedStyle(*select->GetListItems()[item_index]);
  return item_style && item_style->Direction() == TextDirection::kRtl;
}

int Internals::selectPopupItemStyleFontHeight(Node* node, int item_index) {
  auto* select = DynamicTo<HTMLSelectElement>(node);
  if (!select)
    return false;

  if (item_index < 0 ||
      static_cast<wtf_size_t>(item_index) >= select->GetListItems().size())
    return false;
  const ComputedStyle* item_style =
      select->ItemComputedStyle(*select->GetListItems()[item_index]);

  if (item_style) {
    const SimpleFontData* font_data = item_style->GetFont().PrimaryFont();
    DCHECK(font_data);
    return font_data ? font_data->GetFontMetrics().Height() : 0;
  }
  return 0;
}

void Internals::resetTypeAheadSession(HTMLSelectElement* select) {
  DCHECK(select);
  select->ResetTypeAheadSessionForTesting();
}

bool Internals::loseSharedGraphicsContext3D() {
  std::unique_ptr<WebGraphicsContext3DProvider> shared_provider =
      Platform::Current()->CreateSharedOffscreenGraphicsContext3DProvider();
  if (!shared_provider)
    return false;
  gpu::gles2::GLES2Interface* shared_gl = shared_provider->ContextGL();
  if (!shared_gl)
    return false;
  shared_gl->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_EXT,
                                 GL_INNOCENT_CONTEXT_RESET_EXT);
  // To prevent tests that call loseSharedGraphicsContext3D from being
  // flaky, we call finish so that the context is guaranteed to be lost
  // synchronously (i.e. before returning).
  shared_gl->Finish();
  return true;
}

void Internals::forceCompositingUpdate(Document* document,
                                       ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->GetLayoutView()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return;
  }

  document->GetFrame()->View()->UpdateAllLifecyclePhasesForTest();
}

void Internals::setShouldRevealPassword(Element* element,
                                        bool reveal,
                                        ExceptionState& exception_state) {
  DCHECK(element);
  auto* html_input_element = DynamicTo<HTMLInputElement>(element);
  if (!html_input_element) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidNodeTypeError,
                                      "The element provided is not an INPUT.");
    return;
  }

  return html_input_element->SetShouldRevealPassword(reveal);
}

namespace {

class AddOneFunction : public NewScriptFunction::Callable {
 public:
  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    v8::Local<v8::Value> v8_value = value.V8Value();
    DCHECK(v8_value->IsNumber());
    int32_t int_value =
        static_cast<int32_t>(v8_value.As<v8::Integer>()->Value());
    return ScriptValue(
        script_state->GetIsolate(),
        v8::Integer::New(script_state->GetIsolate(), int_value + 1));
  }
};

}  // namespace

ScriptPromise Internals::createResolvedPromise(ScriptState* script_state,
                                               ScriptValue value) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(value);
  return promise;
}

ScriptPromise Internals::createRejectedPromise(ScriptState* script_state,
                                               ScriptValue value) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Reject(value);
  return promise;
}

ScriptPromise Internals::addOneToPromise(ScriptState* script_state,
                                         ScriptPromise promise) {
  return promise.Then(MakeGarbageCollected<NewScriptFunction>(
      script_state, MakeGarbageCollected<AddOneFunction>()));
}

ScriptPromise Internals::promiseCheck(ScriptState* script_state,
                                      int32_t arg1,
                                      bool arg2,
                                      const ScriptValue& arg3,
                                      const String& arg4,
                                      const Vector<String>& arg5,
                                      ExceptionState& exception_state) {
  if (arg2)
    return ScriptPromise::Cast(script_state,
                               V8String(script_state->GetIsolate(), "done"));
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Thrown from the native implementation.");
  return ScriptPromise();
}

ScriptPromise Internals::promiseCheckWithoutExceptionState(
    ScriptState* script_state,
    const ScriptValue& arg1,
    const String& arg2,
    const Vector<String>& arg3) {
  return ScriptPromise::Cast(script_state,
                             V8String(script_state->GetIsolate(), "done"));
}

ScriptPromise Internals::promiseCheckRange(ScriptState* script_state,
                                           int32_t arg1) {
  return ScriptPromise::Cast(script_state,
                             V8String(script_state->GetIsolate(), "done"));
}

ScriptPromise Internals::promiseCheckOverload(ScriptState* script_state,
                                              Location*) {
  return ScriptPromise::Cast(script_state,
                             V8String(script_state->GetIsolate(), "done"));
}

ScriptPromise Internals::promiseCheckOverload(ScriptState* script_state,
                                              Document*) {
  return ScriptPromise::Cast(script_state,
                             V8String(script_state->GetIsolate(), "done"));
}

ScriptPromise Internals::promiseCheckOverload(ScriptState* script_state,
                                              Location*,
                                              int32_t,
                                              int32_t) {
  return ScriptPromise::Cast(script_state,
                             V8String(script_state->GetIsolate(), "done"));
}

void Internals::Trace(Visitor* visitor) const {
  visitor->Trace(runtime_flags_);
  visitor->Trace(document_);
  ScriptWrappable::Trace(visitor);
}

void Internals::setValueForUser(HTMLInputElement* element,
                                const String& value) {
  element->SetValueForUser(value);
}

void Internals::setFocused(bool focused) {
  if (!GetFrame())
    return;

  GetFrame()->GetPage()->GetFocusController().SetFocused(focused);
}

void Internals::setInitialFocus(bool reverse) {
  if (!GetFrame())
    return;

  GetFrame()->GetDocument()->ClearFocusedElement();
  GetFrame()->GetPage()->GetFocusController().SetInitialFocus(
      reverse ? mojom::blink::FocusType::kBackward
              : mojom::blink::FocusType::kForward);
}

Element* Internals::interestedElement() {
  if (!GetFrame() || !GetFrame()->GetPage())
    return nullptr;

  if (!RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled()) {
    return To<LocalFrame>(
               GetFrame()->GetPage()->GetFocusController().FocusedOrMainFrame())
        ->GetDocument()
        ->ActiveElement();
  }

  return GetFrame()
      ->GetPage()
      ->GetSpatialNavigationController()
      .GetInterestedElement();
}

bool Internals::isActivated() {
  if (!GetFrame())
    return false;

  return GetFrame()->GetPage()->GetFocusController().IsActive();
}

bool Internals::isInCanvasFontCache(Document* document,
                                    const String& font_string) {
  return document->GetCanvasFontCache()->IsInCache(font_string);
}

unsigned Internals::canvasFontCacheMaxFonts() {
  return CanvasFontCache::MaxFonts();
}

void Internals::setScrollChain(ScrollState* scroll_state,
                               const HeapVector<Member<Element>>& elements,
                               ExceptionState&) {
  Deque<DOMNodeId> scroll_chain;
  for (wtf_size_t i = 0; i < elements.size(); ++i)
    scroll_chain.push_back(DOMNodeIds::IdForNode(elements[i].Get()));
  scroll_state->SetScrollChain(scroll_chain);
}

String Internals::selectedHTMLForClipboard() {
  if (!GetFrame())
    return String();

  // Selection normalization and markup generation require clean layout.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  return GetFrame()->Selection().SelectedHTMLForClipboard();
}

String Internals::selectedTextForClipboard() {
  if (!GetFrame() || !GetFrame()->GetDocument())
    return String();

  // Clean layout is required for extracting plain text from selection.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  return GetFrame()->Selection().SelectedTextForClipboard();
}

void Internals::setVisualViewportOffset(int x, int y) {
  if (!GetFrame())
    return;

  GetFrame()->GetPage()->GetVisualViewport().SetLocation(FloatPoint(x, y));
}

bool Internals::isUseCounted(Document* document, uint32_t feature) {
  if (feature >= static_cast<int32_t>(WebFeature::kNumberOfFeatures))
    return false;
  return document->IsUseCounted(static_cast<WebFeature>(feature));
}

bool Internals::isCSSPropertyUseCounted(Document* document,
                                        const String& property_name) {
  return document->IsPropertyCounted(
      unresolvedCSSPropertyID(document->GetExecutionContext(), property_name));
}

bool Internals::isAnimatedCSSPropertyUseCounted(Document* document,
                                                const String& property_name) {
  return document->IsAnimatedPropertyCounted(
      unresolvedCSSPropertyID(document->GetExecutionContext(), property_name));
}

void Internals::clearUseCounter(Document* document, uint32_t feature) {
  if (feature >= static_cast<int32_t>(WebFeature::kNumberOfFeatures))
    return;
  document->ClearUseCounterForTesting(static_cast<WebFeature>(feature));
}

Vector<String> Internals::getCSSPropertyLonghands() const {
  Vector<String> result;
  for (CSSPropertyID property : CSSPropertyIDList()) {
    const CSSProperty& property_class = CSSProperty::Get(property);
    if (property_class.IsLonghand()) {
      result.push_back(property_class.GetPropertyNameString());
    }
  }
  return result;
}

Vector<String> Internals::getCSSPropertyShorthands() const {
  Vector<String> result;
  for (CSSPropertyID property : CSSPropertyIDList()) {
    const CSSProperty& property_class = CSSProperty::Get(property);
    if (property_class.IsShorthand()) {
      result.push_back(property_class.GetPropertyNameString());
    }
  }
  return result;
}

Vector<String> Internals::getCSSPropertyAliases() const {
  Vector<String> result;
  for (CSSPropertyID alias : kCSSPropertyAliasList) {
    DCHECK(isPropertyAlias(alias));
    result.push_back(CSSUnresolvedProperty::GetAliasProperty(alias)
                         ->GetPropertyNameString());
  }
  return result;
}

ScriptPromise Internals::observeUseCounter(ScriptState* script_state,
                                           Document* document,
                                           uint32_t feature) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (feature >= static_cast<int32_t>(WebFeature::kNumberOfFeatures)) {
    resolver->Reject();
    return promise;
  }

  WebFeature use_counter_feature = static_cast<WebFeature>(feature);
  if (document->IsUseCounted(use_counter_feature)) {
    resolver->Resolve();
    return promise;
  }

  DocumentLoader* loader = document->Loader();
  if (!loader) {
    resolver->Reject();
    return promise;
  }

  loader->GetUseCounterHelper().AddObserver(
      MakeGarbageCollected<UseCounterHelperObserverImpl>(
          resolver, static_cast<WebFeature>(use_counter_feature)));
  return promise;
}

String Internals::unscopableAttribute() {
  return "unscopableAttribute";
}

String Internals::unscopableMethod() {
  return "unscopableMethod";
}

void Internals::setCapsLockState(bool enabled) {
  KeyboardEventManager::SetCurrentCapsLockState(
      enabled ? OverrideCapsLockState::kOn : OverrideCapsLockState::kOff);
}

void Internals::setPseudoClassState(Element* element,
                                    const String& pseudo,
                                    bool matches,
                                    ExceptionState& exception_state) {
  if (!element->GetDocument().SetPseudoStateForTesting(*element, pseudo,
                                                       matches)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      pseudo + " is not supported");
  }
}

bool Internals::setScrollbarVisibilityInScrollableArea(Node* node,
                                                       bool visible) {
  if (ScrollableArea* scrollable_area = ScrollableAreaForNode(node)) {
    scrollable_area->SetScrollbarsHiddenForTesting(!visible);
    scrollable_area->GetScrollAnimator().SetScrollbarsVisibleForTesting(
        visible);
    return scrollable_area->GetPageScrollbarTheme().UsesOverlayScrollbars();
  }
  return false;
}

double Internals::monotonicTimeToZeroBasedDocumentTime(
    double platform_time,
    ExceptionState& exception_state) {
  return document_->Loader()
      ->GetTiming()
      .MonotonicTimeToZeroBasedDocumentTime(
          base::TimeTicks() + base::TimeDelta::FromSecondsD(platform_time))
      .InSecondsF();
}

int64_t Internals::zeroBasedDocumentTimeToMonotonicTime(double dom_event_time) {
  return document_->Loader()->GetTiming().ZeroBasedDocumentTimeToMonotonicTime(
      dom_event_time);
}

int64_t Internals::currentTimeTicks() {
  return base::TimeTicks::Now().since_origin().InMicroseconds();
}

String Internals::getScrollAnimationState(Node* node) const {
  if (ScrollableArea* scrollable_area = ScrollableAreaForNode(node))
    return scrollable_area->GetScrollAnimator().RunStateAsText();
  return String();
}

String Internals::getProgrammaticScrollAnimationState(Node* node) const {
  if (ScrollableArea* scrollable_area = ScrollableAreaForNode(node))
    return scrollable_area->GetProgrammaticScrollAnimator().RunStateAsText();
  return String();
}

void Internals::crash() {
  CHECK(false) << "Intentional crash";
}

String Internals::evaluateInInspectorOverlay(const String& script) {
  LocalFrame* frame = GetFrame();
  if (frame && frame->Client())
    return frame->Client()->evaluateInInspectorOverlayForTesting(script);
  return g_empty_string;
}

void Internals::setIsLowEndDevice(bool is_low_end_device) {
  MemoryPressureListenerRegistry::SetIsLowEndDeviceForTesting(
      is_low_end_device);
}

bool Internals::isLowEndDevice() const {
  return MemoryPressureListenerRegistry::IsLowEndDevice();
}

Vector<String> Internals::supportedTextEncodingLabels() const {
  return WTF::TextEncodingAliasesForTesting();
}

void Internals::simulateRasterUnderInvalidations(bool enable) {
  RasterInvalidationTracking::SimulateRasterUnderInvalidations(enable);
}

unsigned Internals::LifecycleUpdateCount() const {
  return document_->View()->LifecycleUpdateCountForTesting();
}

void Internals::DisableIntersectionObserverThrottleDelay() const {
  // This gets reset by Internals::ResetToConsistentState
  IntersectionObserver::SetThrottleDelayEnabledForTesting(false);
}

bool Internals::isSiteIsolated(HTMLIFrameElement* iframe) const {
  return iframe->ContentFrame() && iframe->ContentFrame()->IsRemoteFrame();
}

bool Internals::isTrackingOcclusionForIFrame(HTMLIFrameElement* iframe) const {
  if (!iframe->ContentFrame() || !iframe->ContentFrame()->IsRemoteFrame())
    return false;
  RemoteFrame* remote_frame = To<RemoteFrame>(iframe->ContentFrame());
  return remote_frame->View()->NeedsOcclusionTracking();
}

void Internals::DisableFrequencyCappingForOverlayPopupDetection() const {
  OverlayInterstitialAdDetector::DisableFrequencyCappingForTesting();
}

void Internals::addEmbedderCustomElementName(const AtomicString& name,
                                             ExceptionState& exception_state) {
  CustomElement::AddEmbedderCustomElementNameForTesting(name, exception_state);
}

String Internals::resolveModuleSpecifier(const String& specifier,
                                         const String& base_url_string,
                                         Document* document,
                                         ExceptionState& exception_state) {
  Modulator* modulator =
      Modulator::From(ToScriptStateForMainWorld(document->GetFrame()));

  if (!modulator) {
    V8ThrowException::ThrowTypeError(
        v8::Isolate::GetCurrent(),
        "Failed to resolve module specifier " + specifier + ": No modulator");
    return NullURL();
  }

  const KURL base_url = document->CompleteURL(base_url_string);
  String failure_reason = "Failed";
  const KURL result =
      modulator->ResolveModuleSpecifier(specifier, base_url, &failure_reason);

  if (!result.IsValid()) {
    V8ThrowException::ThrowTypeError(v8::Isolate::GetCurrent(),
                                     "Failed to resolve module specifier " +
                                         specifier + ": " + failure_reason);
    return NullURL();
  }

  return result.GetString();
}

String Internals::getParsedImportMap(Document* document,
                                     ExceptionState& exception_state) {
  Modulator* modulator =
      Modulator::From(ToScriptStateForMainWorld(document->GetFrame()));

  if (!modulator) {
    V8ThrowException::ThrowTypeError(v8::Isolate::GetCurrent(), "No modulator");
    return String();
  }

  const ImportMap* import_map = modulator->GetImportMapForTest();
  if (!import_map)
    return "{}";

  return import_map->ToString();
}

void Internals::setDeviceEmulationScale(float scale,
                                        ExceptionState& exception_state) {
  if (scale <= 0)
    return;
  auto* page = document_->GetPage();
  if (!page) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The document's page cannot be retrieved.");
    return;
  }
  DeviceEmulationParams params;
  params.scale = scale;
  page->GetChromeClient().GetWebView()->EnableDeviceEmulation(params);
}

void Internals::ResolveResourcePriority(ScriptPromiseResolver* resolver,
                                        int resource_load_priority) {
  resolver->Resolve(resource_load_priority);
}

String Internals::getAgentId(DOMWindow* window) {
  if (!window->IsLocalDOMWindow())
    return String();

  // Sounds like there's no notion of "process ID" in Blink, but the main
  // thread's thread ID serves for that purpose.
  PlatformThreadId process_id = Thread::MainThread()->ThreadId();

  uintptr_t agent_address =
      reinterpret_cast<uintptr_t>(To<LocalDOMWindow>(window)->GetAgent());

  // This serializes a pointer as a decimal number, which is a bit ugly, but
  // it works. Is there any utility to dump a number in a hexadecimal form?
  // I couldn't find one in WTF.
  return String::Number(process_id) + ":" + String::Number(agent_address);
}

void Internals::useMockOverlayScrollbars() {
  g_mock_overlay_scrollbars =
      std::make_unique<ScopedMockOverlayScrollbars>(true);
}

bool Internals::overlayScrollbarsEnabled() const {
  return ScrollbarThemeSettings::OverlayScrollbarsEnabled();
}

void Internals::generateTestReport(const String& message) {
  // Construct the test report.
  TestReportBody* body = MakeGarbageCollected<TestReportBody>(message);
  Report* report =
      MakeGarbageCollected<Report>("test", document_->Url().GetString(), body);

  // Send the test report to any ReportingObservers.
  ReportingContext::From(document_->domWindow())->QueueReport(report);
}

void Internals::setIsAdSubframe(HTMLIFrameElement* iframe,
                                ExceptionState& exception_state) {
  if (!iframe->ContentFrame() || !iframe->ContentFrame()->IsLocalFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Frame cannot be accessed.");
    return;
  }
  LocalFrame* parent_frame = iframe->GetDocument().GetFrame();
  LocalFrame* child_frame = To<LocalFrame>(iframe->ContentFrame());
  bool parent_is_ad = parent_frame && parent_frame->IsAdSubframe();
  child_frame->SetIsAdSubframe(parent_is_ad
                                   ? blink::mojom::AdFrameType::kChildAd
                                   : blink::mojom::AdFrameType::kRootAd);
}

}  // namespace blink
