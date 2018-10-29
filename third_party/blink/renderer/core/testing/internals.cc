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

#include <deque>
#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "cc/layers/picture_layer.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/modules/remoteplayback/web_remote_playback_availability.h"
#include "third_party/blink/public/platform/modules/remoteplayback/web_remote_playback_client.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_connection_type.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/css/properties/css_unresolved_property.h"
#include "third_party/blink/renderer/core/css/select_rule_feature_set.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css_property_names.h"
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
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/frame/settings.h"
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
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/keyboard_event_manager.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/layout/layout_menu_list.h"
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
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator_context.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_tree_as_text.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/programmatic_scroll_animator.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/callback_function_test.h"
#include "third_party/blink/renderer/core/testing/dictionary_test.h"
#include "third_party/blink/renderer/core/testing/gc_observation.h"
#include "third_party/blink/renderer/core/testing/internal_runtime_flags.h"
#include "third_party/blink/renderer/core/testing/internal_settings.h"
#include "third_party/blink/renderer/core/testing/layer_rect.h"
#include "third_party/blink/renderer/core/testing/layer_rect_list.h"
#include "third_party/blink/renderer/core/testing/mock_hyphenation.h"
#include "third_party/blink/renderer/core/testing/origin_trials_test.h"
#include "third_party/blink/renderer/core/testing/record_test.h"
#include "third_party/blink/renderer/core/testing/sequence_test.h"
#include "third_party/blink/renderer/core/testing/static_selection.h"
#include "third_party/blink/renderer/core/testing/type_conversions.h"
#include "third_party/blink/renderer/core/testing/union_types_test.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/cursor.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "v8/include/v8.h"

namespace blink {

using ws::mojom::ImeTextSpanThickness;

namespace {

class UseCounterObserverImpl final : public UseCounter::Observer {

 public:
  UseCounterObserverImpl(ScriptPromiseResolver* resolver, WebFeature feature)
      : resolver_(resolver), feature_(feature) {}

  bool OnCountFeature(WebFeature feature) final {
    if (feature_ != feature)
      return false;
    resolver_->Resolve(static_cast<int>(feature));
    return true;
  }

  void Trace(blink::Visitor* visitor) override {
    UseCounter::Observer::Trace(visitor);
    visitor->Trace(resolver_);
  }

 private:
  Member<ScriptPromiseResolver> resolver_;
  WebFeature feature_;
  DISALLOW_COPY_AND_ASSIGN(UseCounterObserverImpl);
};

}  // namespace

static base::Optional<DocumentMarker::MarkerType> MarkerTypeFrom(
    const String& marker_type) {
  if (DeprecatedEqualIgnoringCase(marker_type, "Spelling"))
    return DocumentMarker::kSpelling;
  if (DeprecatedEqualIgnoringCase(marker_type, "Grammar"))
    return DocumentMarker::kGrammar;
  if (DeprecatedEqualIgnoringCase(marker_type, "TextMatch"))
    return DocumentMarker::kTextMatch;
  if (DeprecatedEqualIgnoringCase(marker_type, "Composition"))
    return DocumentMarker::kComposition;
  if (DeprecatedEqualIgnoringCase(marker_type, "ActiveSuggestion"))
    return DocumentMarker::kActiveSuggestion;
  if (DeprecatedEqualIgnoringCase(marker_type, "Suggestion"))
    return DocumentMarker::kSuggestion;
  return base::nullopt;
}

static base::Optional<DocumentMarker::MarkerTypes> MarkerTypesFrom(
    const String& marker_type) {
  if (marker_type.IsEmpty() || DeprecatedEqualIgnoringCase(marker_type, "all"))
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

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return nullptr;

  return ToLayoutBox(layout_object)->GetScrollableArea();
}

static RuntimeEnabledFeatures::Backup* g_s_features_backup = nullptr;

void Internals::ResetToConsistentState(Page* page) {
  DCHECK(page);

  if (!g_s_features_backup)
    g_s_features_backup = new RuntimeEnabledFeatures::Backup;
  g_s_features_backup->Restore();
  page->SetIsCursorVisible(true);
  // Ensure the PageScaleFactor always stays within limits, if the test changed
  // the limits. BlinkTestRunner will reset the limits to those set by
  // LayoutTestDefaultPreferences when preferences are reapplied after this
  // call.
  page->SetDefaultPageScaleLimits(1, 4);
  page->SetPageScaleFactor(1);
  LocalFrame* frame = page->DeprecatedLocalMainFrame();
  frame->View()->LayoutViewport()->SetScrollOffset(ScrollOffset(),
                                                   kProgrammaticScroll);
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
}

Internals::Internals(ExecutionContext* context)
    : runtime_flags_(InternalRuntimeFlags::create()),
      document_(To<Document>(context)) {
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

GCObservation* Internals::observeGC(ScriptValue script_value) {
  v8::Local<v8::Value> observed_value = script_value.V8Value();
  DCHECK(!observed_value.IsEmpty());
  if (observed_value->IsNull() || observed_value->IsUndefined()) {
    V8ThrowException::ThrowTypeError(v8::Isolate::GetCurrent(),
                                     "value to observe is null or undefined");
    return nullptr;
  }

  return GCObservation::Create(observed_value);
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

  return doc->GetLayoutView()->HitTestCount();
}

unsigned Internals::hitTestCacheHits(Document* doc,
                                     ExceptionState& exception_state) const {
  if (!doc) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "Must supply document to check");
    return 0;
  }

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
  const String cache_identifier = document_->Fetcher()->GetCacheIdentifier();
  Resource* resource = GetMemoryCache()->ResourceForURL(
      document_->CompleteURL(url), cache_identifier);
  // We check loader() here instead of isLoading(), because a multipart
  // ImageResource lies isLoading() == false after the first part is loaded.
  return resource && resource->Loader();
}

bool Internals::isLoadingFromMemoryCache(const String& url) {
  if (!document_)
    return false;
  const String cache_identifier = document_->Fetcher()->GetCacheIdentifier();
  Resource* resource = GetMemoryCache()->ResourceForURL(
      document_->CompleteURL(url), cache_identifier);
  return resource && resource->GetStatus() == ResourceStatus::kCached;
}

int Internals::getResourcePriority(const String& url, Document* document) {
  if (!document)
    return static_cast<int>(ResourceLoadPriority::kUnresolved);

  Resource* resource = document->Fetcher()->AllResources().at(
      url_test_helpers::ToKURL(url.Utf8().data()));

  if (!resource)
    return static_cast<int>(ResourceLoadPriority::kUnresolved);

  return static_cast<int>(resource->GetResourceRequest().Priority());
}

String Internals::getResourceHeader(const String& url,
                                    const String& header,
                                    Document* document) {
  if (!document)
    return String();
  Resource* resource = document->Fetcher()->AllResources().at(
      url_test_helpers::ToKURL(url.Utf8().data()));
  if (!resource)
    return String();
  return resource->GetResourceRequest().HttpHeaderField(header.Utf8().data());
}

bool Internals::isValidContentSelect(Element* insertion_point,
                                     ExceptionState& exception_state) {
  DCHECK(insertion_point);
  if (!insertion_point->IsV0InsertionPoint()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The element is not an insertion point.");
    return false;
  }

  return IsHTMLContentElement(*insertion_point) &&
         ToHTMLContentElement(*insertion_point).IsSelectValid();
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

unsigned short Internals::compareTreeScopePosition(
    const Node* node1,
    const Node* node2,
    ExceptionState& exception_state) const {
  DCHECK(node1 && node2);
  const TreeScope* tree_scope1 =
      IsA<Document>(node1)
          ? static_cast<const TreeScope*>(To<Document>(node1))
          : node1->IsShadowRoot()
                ? static_cast<const TreeScope*>(ToShadowRoot(node1))
                : nullptr;
  const TreeScope* tree_scope2 =
      IsA<Document>(node2)
          ? static_cast<const TreeScope*>(To<Document>(node2))
          : node2->IsShadowRoot()
                ? static_cast<const TreeScope*>(ToShadowRoot(node2))
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

  GetFrame()->View()->UpdateAllLifecyclePhases();
  GetFrame()->GetDocument()->Timeline().PauseAnimationsForTesting(pause_time);
}

bool Internals::isCompositedAnimation(Animation* animation) {
  return animation->HasActiveAnimationsOnCompositor();
}

void Internals::disableCompositedAnimation(Animation* animation) {
  animation->DisableCompositedAnimationForTesting();
}

void Internals::disableCSSAdditiveAnimations() {
  RuntimeEnabledFeatures::SetCSSAdditiveAnimationsEnabled(false);
}

void Internals::advanceImageAnimation(Element* image,
                                      ExceptionState& exception_state) {
  DCHECK(image);

  ImageResourceContent* resource = nullptr;
  if (auto* html_image = ToHTMLImageElementOrNull(*image)) {
    resource = html_image->CachedImage();
  } else if (auto* svg_image = ToSVGImageElementOrNull(*image)) {
    resource = svg_image->CachedImage();
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The element provided is not a image element.");
    return;
  }

  if (!resource || !resource->HasImage()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The image resource is not available.");
    return;
  }

  Image* image_data = resource->GetImage();
  image_data->AdvanceAnimationForTesting();
}

bool Internals::hasShadowInsertionPoint(const Node* root,
                                        ExceptionState& exception_state) const {
  DCHECK(root);
  if (!root->IsShadowRoot()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node argument is not a shadow root.");
    return false;
  }
  return ToShadowRoot(root)->V0().ContainsShadowElements();
}

bool Internals::hasContentElement(const Node* root,
                                  ExceptionState& exception_state) const {
  DCHECK(root);
  if (!root->IsShadowRoot()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node argument is not a shadow root.");
    return false;
  }
  return ToShadowRoot(root)->V0().ContainsContentElements();
}

uint32_t Internals::countElementShadow(const Node* root,
                                       ExceptionState& exception_state) const {
  DCHECK(root);
  if (!root->IsShadowRoot()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node argument is not a shadow root.");
    return 0;
  }
  return ToShadowRoot(root)->ChildShadowRootCount();
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
  element->GetDocument().View()->UpdateAllLifecyclePhases();

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
  return CSSComputedStyleDeclaration::Create(node, allow_visited_style);
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

void Internals::setBrowserControlsShownRatio(float ratio) {
  document_->GetPage()->GetChromeClient().SetBrowserControlsShownRatio(ratio);
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
  if (!root->IsShadowRoot()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node provided is not a shadow root.");
    return String();
  }

  switch (ToShadowRoot(root)->GetType()) {
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
  return IsHTMLFormControlElement(element) &&
         ToHTMLFormControlElement(element)->IsValidationMessageVisible();
}

void Internals::selectColorInColorChooser(Element* element,
                                          const String& color_value) {
  DCHECK(element);
  Color color;
  if (!color.SetFromString(color_value))
    return;
  if (auto* input = ToHTMLInputElementOrNull(*element))
    input->SelectColorInColorChooser(color);
}

void Internals::endColorChooser(Element* element) {
  DCHECK(element);
  if (auto* input = ToHTMLInputElementOrNull(*element))
    input->EndColorChooser();
}

bool Internals::hasAutofocusRequest(Document* document) {
  if (!document)
    document = document_;
  return document->AutofocusElement();
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
    LocalDOMWindow* popup =
        ToLocalDOMWindow(page->GetChromeClient().PagePopupWindowForTesting());
    if (popup) {
      // We need to make the popup same origin so layout tests can access it.
      popup->document()->UpdateSecurityOrigin(
          document_->GetMutableSecurityOrigin());
    }
    return popup;
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

  document_->UpdateStyleAndLayoutIgnorePendingStylesheets();
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

  element->GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object)
    return DOMRectReadOnly::Create(0, 0, 0, 0);
  return DOMRectReadOnly::FromIntRect(
      layout_object->AbsoluteBoundingBoxRectIgnoringTransforms());
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

  document->UpdateStyleAndLayoutIgnorePendingStylesheets();
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
      .MarkersFor(ToText(*node), marker_types.value())
      .size();
}

unsigned Internals::activeMarkerCountForNode(Node* node) {
  DCHECK(node);

  // Only TextMatch markers can be active.
  DocumentMarkerVector markers = node->GetDocument().Markers().MarkersFor(
      ToText(*node), DocumentMarker::MarkerTypes::TextMatch());

  unsigned active_marker_count = 0;
  for (const auto& marker : markers) {
    if (ToTextMatchMarker(marker)->IsActiveMatch())
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
      ToText(*node), marker_types.value());
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
  return Range::Create(node->GetDocument(), node, marker->StartOffset(), node,
                       marker->EndOffset());
}

String Internals::markerDescriptionForNode(Node* node,
                                           const String& marker_type,
                                           unsigned index,
                                           ExceptionState& exception_state) {
  DocumentMarker* marker = MarkerAt(node, marker_type, index, exception_state);
  if (!marker || !IsSpellCheckMarker(*marker))
    return String();
  return ToSpellCheckMarker(marker)->Description();
}

unsigned Internals::markerBackgroundColorForNode(
    Node* node,
    const String& marker_type,
    unsigned index,
    ExceptionState& exception_state) {
  DocumentMarker* marker = MarkerAt(node, marker_type, index, exception_state);
  if (!marker || !IsStyleableMarker(*marker))
    return 0;
  return ToStyleableMarker(marker)->BackgroundColor().Rgb();
}

unsigned Internals::markerUnderlineColorForNode(
    Node* node,
    const String& marker_type,
    unsigned index,
    ExceptionState& exception_state) {
  DocumentMarker* marker = MarkerAt(node, marker_type, index, exception_state);
  if (!marker || !IsStyleableMarker(*marker))
    return 0;
  return ToStyleableMarker(marker)->UnderlineColor().Rgb();
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

  range->OwnerDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
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

namespace {

void addStyleableMarkerHelper(
    const Range* range,
    const String& underline_color_value,
    const String& thickness_value,
    const String& background_color_value,
    ExceptionState& exception_state,
    std::function<
        void(const EphemeralRange&, Color, ImeTextSpanThickness, Color)>
        create_marker) {
  DCHECK(range);
  range->OwnerDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  base::Optional<ImeTextSpanThickness> thickness =
      ThicknessFrom(thickness_value);
  if (!thickness) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The thickness provided ('" + thickness_value + "') is invalid.");
    return;
  }

  Color underline_color;
  Color background_color;
  if (ParseColor(underline_color_value, underline_color, exception_state,
                 "Invalid underline color.") &&
      ParseColor(background_color_value, background_color, exception_state,
                 "Invalid background color.")) {
    create_marker(EphemeralRange(range), underline_color, thickness.value(),
                  background_color);
  }
}

}  // namespace

void Internals::addCompositionMarker(const Range* range,
                                     const String& underline_color_value,
                                     const String& thickness_value,
                                     const String& background_color_value,
                                     ExceptionState& exception_state) {
  DocumentMarkerController& document_marker_controller =
      range->OwnerDocument().Markers();
  addStyleableMarkerHelper(
      range, underline_color_value, thickness_value, background_color_value,
      exception_state,
      [&document_marker_controller](
          const EphemeralRange& range, Color underline_color,
          ImeTextSpanThickness thickness, Color background_color) {
        document_marker_controller.AddCompositionMarker(
            range, underline_color, thickness, background_color);
      });
}

void Internals::addActiveSuggestionMarker(const Range* range,
                                          const String& underline_color_value,
                                          const String& thickness_value,
                                          const String& background_color_value,
                                          ExceptionState& exception_state) {
  DocumentMarkerController& document_marker_controller =
      range->OwnerDocument().Markers();
  addStyleableMarkerHelper(
      range, underline_color_value, thickness_value, background_color_value,
      exception_state,
      [&document_marker_controller](
          const EphemeralRange& range, Color underline_color,
          ImeTextSpanThickness thickness, Color background_color) {
        document_marker_controller.AddActiveSuggestionMarker(
            range, underline_color, thickness, background_color);
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
  Color suggestion_highlight_color;
  if (!ParseColor(suggestion_highlight_color_value, suggestion_highlight_color,
                  exception_state, "Invalid suggestion highlight color."))
    return;

  DocumentMarkerController& document_marker_controller =
      range->OwnerDocument().Markers();
  addStyleableMarkerHelper(
      range, underline_color_value, thickness_value, background_color_value,
      exception_state,
      [&document_marker_controller, &suggestions, &suggestion_highlight_color](
          const EphemeralRange& range, Color underline_color,
          ImeTextSpanThickness thickness, Color background_color) {
        document_marker_controller.AddSuggestionMarker(
            range,
            SuggestionMarkerProperties::Builder()
                .SetType(SuggestionMarker::SuggestionType::kNotMisspelling)
                .SetSuggestions(suggestions)
                .SetHighlightColor(suggestion_highlight_color)
                .SetUnderlineColor(underline_color)
                .SetThickness(thickness)
                .SetBackgroundColor(background_color)
                .Build());
      });
}

void Internals::setTextMatchMarkersActive(Node* node,
                                          unsigned start_offset,
                                          unsigned end_offset,
                                          bool active) {
  DCHECK(node);
  node->GetDocument().Markers().SetTextMatchMarkersActive(node, start_offset,
                                                          end_offset, active);
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

  document->UpdateStyleAndLayoutIgnorePendingStylesheets();

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
  if (auto* input = ToHTMLInputElementOrNull(*element))
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
  if (auto* input = ToHTMLInputElementOrNull(*element))
    return input->SuggestedValue();

  if (auto* textarea = ToHTMLTextAreaElementOrNull(*element))
    return textarea->SuggestedValue();

  if (auto* select = ToHTMLSelectElementOrNull(*element))
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

  if (auto* input = ToHTMLInputElementOrNull(*element))
    input->SetSuggestedValue(value);

  if (auto* textarea = ToHTMLTextAreaElementOrNull(*element))
    textarea->SetSuggestedValue(value);

  if (auto* select = ToHTMLSelectElementOrNull(*element))
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

  if (auto* input = ToHTMLInputElementOrNull(*element)) {
    input->DispatchScopedEvent(*Event::CreateBubble(EventTypeNames::keydown));
    input->SetAutofillValue(value);
    input->DispatchScopedEvent(*Event::CreateBubble(EventTypeNames::keyup));
  }

  if (auto* textarea = ToHTMLTextAreaElementOrNull(*element)) {
    textarea->DispatchScopedEvent(
        *Event::CreateBubble(EventTypeNames::keydown));
    textarea->SetAutofillValue(value);
    textarea->DispatchScopedEvent(*Event::CreateBubble(EventTypeNames::keyup));
  }

  if (auto* select = ToHTMLSelectElementOrNull(*element))
    select->setValue(value, kDispatchInputAndChangeEvent);

  ToHTMLFormControlElement(element)->SetAutofillState(
      blink::WebAutofillState::kAutofilled);
}

void Internals::setEditingValue(Element* element,
                                const String& value,
                                ExceptionState& exception_state) {
  DCHECK(element);
  if (!IsHTMLInputElement(*element)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidNodeTypeError,
                                      "The element provided is not an INPUT.");
    return;
  }

  ToHTMLInputElement(*element).SetEditingValue(value);
}

void Internals::setAutofilled(Element* element,
                              bool enabled,
                              ExceptionState& exception_state) {
  DCHECK(element);
  if (!element->IsFormControlElement()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidNodeTypeError,
        "The element provided is not a form control element.");
    return;
  }
  if (enabled) {
    ToHTMLFormControlElement(element)->SetAutofillState(
        WebAutofillState::kAutofilled);
  } else {
    ToHTMLFormControlElement(element)->SetAutofillState(
        WebAutofillState::kNotFilled);
  }
}

Range* Internals::rangeFromLocationAndLength(Element* scope,
                                             int range_location,
                                             int range_length) {
  DCHECK(scope);

  // TextIterator depends on Layout information, make sure layout it up to date.
  scope->GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  return CreateRange(
      PlainTextRange(range_location, range_location + range_length)
          .CreateRange(*scope));
}

unsigned Internals::locationFromRange(Element* scope, const Range* range) {
  DCHECK(scope && range);
  // PlainTextRange depends on Layout information, make sure layout it up to
  // date.
  scope->GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  return PlainTextRange::Create(*scope, *range).Start();
}

unsigned Internals::lengthFromRange(Element* scope, const Range* range) {
  DCHECK(scope && range);
  // PlainTextRange depends on Layout information, make sure layout it up to
  // date.
  scope->GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  return PlainTextRange::Create(*scope, *range).length();
}

String Internals::rangeAsText(const Range* range) {
  DCHECK(range);
  // Clean layout is required by plain text extraction.
  range->OwnerDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

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
  document->UpdateStyleAndLayout();
  EventHandler& event_handler = document->GetFrame()->GetEventHandler();
  LayoutPoint hit_test_point(
      document->GetFrame()->View()->ConvertFromRootFrame(LayoutPoint(x, y)));
  location = HitTestLocation(
      (LayoutRect(hit_test_point, LayoutSize((int)width, (int)height))));
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

unsigned Internals::mediaKeysCount() {
  return InstanceCounters::CounterValue(InstanceCounters::kMediaKeysCounter);
}

unsigned Internals::mediaKeySessionCount() {
  return InstanceCounters::CounterValue(
      InstanceCounters::kMediaKeySessionCounter);
}

unsigned Internals::pausableObjectCount(Document* document) {
  DCHECK(document);
  return document->PausableObjectCount();
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
                           EventHandlerRegistry::kWheelEventBlocking);
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
  return EventHandlerCount(*document, EventHandlerRegistry::kPointerEvent);
}

static PaintLayer* FindLayerForGraphicsLayer(PaintLayer* search_root,
                                             GraphicsLayer* graphics_layer,
                                             IntSize* layer_offset,
                                             String* layer_type) {
  *layer_offset = IntSize();
  if (search_root->HasCompositedLayerMapping() &&
      graphics_layer ==
          search_root->GetCompositedLayerMapping()->MainGraphicsLayer()) {
    // If the |graphicsLayer| sets the scrollingContent layer as its
    // scroll parent, consider it belongs to the scrolling layer and
    // mark the layer type as "scrolling".
    if (!search_root->GetLayoutObject().HasTransformRelatedProperty() &&
        search_root->ScrollParent() &&
        search_root->Parent() == search_root->ScrollParent()) {
      *layer_type = "scrolling";
      // For hit-test rect visualization to work, the hit-test rect should
      // be relative to the scrolling layer and in this case the hit-test
      // rect is relative to the element's own GraphicsLayer. So we will have
      // to adjust the rect to be relative to the scrolling layer here.
      // Only when the element's offsetParent == scroller's offsetParent we
      // can compute the element's relative position to the scrolling content
      // in this way.
      if (search_root->GetLayoutObject().OffsetParent() ==
          search_root->Parent()->GetLayoutObject().OffsetParent()) {
        LayoutBoxModelObject& current = search_root->GetLayoutObject();
        LayoutBoxModelObject& parent = search_root->Parent()->GetLayoutObject();
        layer_offset->SetWidth((parent.OffsetLeft(parent.OffsetParent()) -
                                current.OffsetLeft(parent.OffsetParent()))
                                   .ToInt());
        layer_offset->SetHeight((parent.OffsetTop(parent.OffsetParent()) -
                                 current.OffsetTop(parent.OffsetParent()))
                                    .ToInt());
        return search_root->Parent();
      }
    }

    LayoutRect rect;
    PaintLayer::MapRectInPaintInvalidationContainerToBacking(
        search_root->GetLayoutObject(), rect);
    rect.Move(search_root->GetCompositedLayerMapping()
                  ->ContentOffsetInCompositingLayer());

    *layer_offset = IntSize(rect.X().ToInt(), rect.Y().ToInt());
    return search_root;
  }

  // If the |graphicsLayer| is a scroller's scrollingContent layer,
  // consider this is a scrolling layer.
  GraphicsLayer* layer_for_scrolling =
      search_root->GetScrollableArea()
          ? search_root->GetScrollableArea()->LayerForScrolling()
          : nullptr;
  if (graphics_layer == layer_for_scrolling) {
    *layer_type = "scrolling";
    return search_root;
  }

  if (search_root->GetCompositingState() == kPaintsIntoGroupedBacking) {
    GraphicsLayer* squashing_layer =
        search_root->GroupedMapping()->SquashingLayer();
    if (graphics_layer == squashing_layer) {
      *layer_type = "squashing";
      LayoutRect rect;
      PaintLayer::MapRectInPaintInvalidationContainerToBacking(
          search_root->GetLayoutObject(), rect);
      *layer_offset = IntSize(rect.X().ToInt(), rect.Y().ToInt());
      return search_root;
    }
  }

  GraphicsLayer* layer_for_horizontal_scrollbar =
      search_root->GetScrollableArea()
          ? search_root->GetScrollableArea()->LayerForHorizontalScrollbar()
          : nullptr;
  if (graphics_layer == layer_for_horizontal_scrollbar) {
    *layer_type = "horizontalScrollbar";
    return search_root;
  }

  GraphicsLayer* layer_for_vertical_scrollbar =
      search_root->GetScrollableArea()
          ? search_root->GetScrollableArea()->LayerForVerticalScrollbar()
          : nullptr;
  if (graphics_layer == layer_for_vertical_scrollbar) {
    *layer_type = "verticalScrollbar";
    return search_root;
  }

  GraphicsLayer* layer_for_scroll_corner =
      search_root->GetScrollableArea()
          ? search_root->GetScrollableArea()->LayerForScrollCorner()
          : nullptr;
  if (graphics_layer == layer_for_scroll_corner) {
    *layer_type = "scrollCorner";
    return search_root;
  }

  // Search right to left to increase the chances that we'll choose the top-most
  // layers in a grouped mapping for squashing.
  for (PaintLayer* child = search_root->LastChild(); child;
       child = child->PreviousSibling()) {
    PaintLayer* found_layer = FindLayerForGraphicsLayer(
        child, graphics_layer, layer_offset, layer_type);
    if (found_layer)
      return found_layer;
  }

  return nullptr;
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

static void AccumulateLayerRectList(PaintLayerCompositor* compositor,
                                    GraphicsLayer* graphics_layer,
                                    LayerRectList* rects) {
  const cc::TouchActionRegion& touch_action_region =
      graphics_layer->CcLayer()->touch_action_region();
  if (!touch_action_region.region().IsEmpty()) {
    Vector<IntRect> layer_rects;
    for (const gfx::Rect& rect : touch_action_region.region()) {
      layer_rects.push_back(IntRect(rect));
    }
    MergeRects(layer_rects);
    String layer_type;
    IntSize layer_offset;
    PaintLayer* paint_layer = FindLayerForGraphicsLayer(
        compositor->RootLayer(), graphics_layer, &layer_offset, &layer_type);
    Node* node =
        paint_layer ? paint_layer->GetLayoutObject().GetNode() : nullptr;
    for (const IntRect& layer_rect : layer_rects) {
      if (!layer_rect.IsEmpty()) {
        rects->Append(node, layer_type, layer_offset.Width(),
                      layer_offset.Height(),
                      DOMRectReadOnly::FromIntRect(layer_rect));
      }
    }
  }

  for (GraphicsLayer* child_layer : graphics_layer->Children())
    AccumulateLayerRectList(compositor, child_layer, rects);
}

LayerRectList* Internals::touchEventTargetLayerRects(
    Document* document,
    ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->View() || !document->GetPage() || document != document_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return nullptr;
  }

  if (ScrollingCoordinator* scrolling_coordinator =
          document->GetPage()->GetScrollingCoordinator()) {
    FrameView* view = document->GetPage()->MainFrame()->View();
    if (view->IsLocalFrameView()) {
      scrolling_coordinator->UpdateAfterPaint(
          static_cast<LocalFrameView*>(view));
    } else {
      NOTREACHED();
    }
  }

  if (auto* view = document->GetLayoutView()) {
    if (PaintLayerCompositor* compositor = view->Compositor()) {
      // Use the paint-root since we sometimes want to know about touch rects
      // on layers outside the document hierarchy (e.g. when we replace the
      // document with a video layer).
      if (GraphicsLayer* root_layer = compositor->PaintRootGraphicsLayer()) {
        LayerRectList* rects = LayerRectList::Create();
        AccumulateLayerRectList(compositor, root_layer, rects);
        return rects;
      }
    }
  }

  return nullptr;
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

AtomicString Internals::htmlNamespace() {
  return HTMLNames::xhtmlNamespaceURI;
}

Vector<AtomicString> Internals::htmlTags() {
  Vector<AtomicString> tags(HTMLNames::kTagsCount);
  std::unique_ptr<const HTMLQualifiedName* []> qualified_names =
      HTMLNames::GetTags();
  for (wtf_size_t i = 0; i < HTMLNames::kTagsCount; ++i)
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
  LayoutRect rect(x, y, width, height);
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

  document->UpdateStyleAndLayoutIgnorePendingStylesheets();
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

  document->UpdateStyleAndLayoutIgnorePendingStylesheets();
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

  document->UpdateStyleAndLayoutIgnorePendingStylesheets();
  return document->GetFrame()->GetSpellChecker().SelectionStartHasMarkerFor(
      DocumentMarker::kGrammar, from, length);
}

unsigned Internals::numberOfScrollableAreas(Document* document) {
  DCHECK(document);
  if (!document->GetFrame())
    return 0;

  unsigned count = 0;
  LocalFrame* frame = document->GetFrame();
  if (frame->View()->ScrollableAreas())
    count += frame->View()->ScrollableAreas()->size();

  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (child->IsLocalFrame() && ToLocalFrame(child)->View() &&
        ToLocalFrame(child)->View()->ScrollableAreas())
      count += ToLocalFrame(child)->View()->ScrollableAreas()->size();
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

String Internals::elementLayerTreeAsText(
    Element* element,
    ExceptionState& exception_state) const {
  DCHECK(element);
  LocalFrameView* frame_view = element->GetDocument().View();
  frame_view->UpdateAllLifecyclePhases();

  return elementLayerTreeAsText(element, 0, exception_state);
}

bool Internals::scrollsWithRespectTo(Element* element1,
                                     Element* element2,
                                     ExceptionState& exception_state) {
  DCHECK(element1 && element2);
  element1->GetDocument().View()->UpdateAllLifecyclePhases();

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

  PaintLayer* layer1 = ToLayoutBox(layout_object1)->Layer();
  PaintLayer* layer2 = ToLayoutBox(layout_object2)->Layer();
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

  document->View()->UpdateAllLifecyclePhases();

  return document->GetFrame()->GetLayerTreeAsTextForTesting(flags);
}

String Internals::elementLayerTreeAsText(
    Element* element,
    unsigned flags,
    ExceptionState& exception_state) const {
  DCHECK(element);
  element->GetDocument().UpdateStyleAndLayout();

  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        layout_object ? "The provided element's layoutObject is not a box."
                      : "The provided element has no layoutObject.");
    return String();
  }

  PaintLayer* layer = ToLayoutBox(layout_object)->Layer();
  if (!layer || !layer->HasCompositedLayerMapping() ||
      !layer->GetCompositedLayerMapping()->MainGraphicsLayer()) {
    // Don't raise exception in these cases which may be normally used in tests.
    return String();
  }

  return GraphicsLayerTreeAsTextForTesting(
      layer->GetCompositedLayerMapping()->MainGraphicsLayer(), flags);
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

  document->GetFrame()->View()->UpdateAllLifecyclePhases();

  return document->GetFrame()->View()->MainThreadScrollingReasonsAsText();
}

void Internals::markGestureScrollRegionDirty(
    Document* document,
    ExceptionState& exception_state) const {
  FrameView* frame_view = document->View();
  if (!frame_view || !frame_view->IsLocalFrameView())
    return;
  LocalFrameView* lfv = static_cast<LocalFrameView*>(frame_view);
  lfv->GetScrollingContext()->SetScrollGestureRegionIsDirty(true);
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

  // Update lifecycle to kPrePaintClean.  This includes the compositing update
  // and ScrollingCoordinator::UpdateAfterPaint, which computes the non-fast
  // scrollable region.
  frame->View()->UpdateAllLifecyclePhases();

  GraphicsLayer* layer = frame->View()->LayoutViewport()->LayerForScrolling();
  if (!layer)
    return DOMRectList::Create();
  const cc::Region& region = layer->CcLayer()->non_fast_scrollable_region();
  Vector<IntRect> rects;
  rects.ReserveCapacity(region.GetRegionComplexity());
  for (const gfx::Rect& rect : region)
    rects.push_back(IntRect(rect));
  return DOMRectList::Create(rects);
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
  return IconURLs(document, kFavicon);
}

Vector<String> Internals::allIconURLs(Document* document) const {
  return IconURLs(document, kFavicon | kTouchIcon | kTouchPrecomposedIcon);
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
                               int page_number,
                               ExceptionState& exception_state) const {
  if (!GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No frame is available.");
    return String();
  }

  return PrintContext::PageProperty(GetFrame(), property_name.Utf8().data(),
                                    page_number);
}

String Internals::pageSizeAndMarginsInPixels(
    int page_number,
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

bool Internals::magnifyScaleAroundAnchor(float scale_factor, float x, float y) {
  if (!GetFrame())
    return false;

  return GetFrame()->GetPage()->GetVisualViewport().MagnifyScaleAroundAnchor(
      scale_factor, FloatPoint(x, y));
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

String Internals::effectivePreload(HTMLMediaElement* media_element) {
  DCHECK(media_element);
  return media_element->EffectivePreload();
}

void Internals::mediaPlayerRemoteRouteAvailabilityChanged(
    HTMLMediaElement* media_element,
    bool available) {
  DCHECK(media_element);
  DCHECK(media_element->remote_playback_client_);
  media_element->remote_playback_client_->AvailabilityChanged(
      available ? WebRemotePlaybackAvailability::kDeviceAvailable
                : WebRemotePlaybackAvailability::kDeviceNotAvailable);
}

void Internals::mediaPlayerPlayingRemotelyChanged(
    HTMLMediaElement* media_element,
    bool remote) {
  DCHECK(media_element);
  if (remote)
    media_element->ConnectedToRemoteDevice();
  else
    media_element->DisconnectedFromRemoteDevice();
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
  return TypeConversions::Create();
}

DictionaryTest* Internals::dictionaryTest() const {
  return DictionaryTest::Create();
}

RecordTest* Internals::recordTest() const {
  return RecordTest::Create();
}

SequenceTest* Internals::sequenceTest() const {
  return SequenceTest::create();
}

UnionTypesTest* Internals::unionTypesTest() const {
  return UnionTypesTest::Create();
}

OriginTrialsTest* Internals::originTrialsTest() const {
  return OriginTrialsTest::Create();
}

CallbackFunctionTest* Internals::callbackFunctionTest() const {
  return CallbackFunctionTest::Create();
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
  frame_view->UpdateAllLifecyclePhases();
  frame_view->SetTracksPaintInvalidations(true);
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
  frame_view->UpdateAllLifecyclePhases();
  frame_view->SetTracksPaintInvalidations(false);
}

void Internals::updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(
    Node* node,
    ExceptionState& exception_state) {
  Document* document = nullptr;
  if (!node) {
    document = document_;
  } else if (IsA<Document>(node)) {
    document = To<Document>(node);
  } else if (auto* iframe = ToHTMLIFrameElementOrNull(*node)) {
    document = iframe->contentDocument();
  }

  if (!document) {
    exception_state.ThrowTypeError(
        "The node provided is neither a document nor an IFrame.");
    return;
  }
  document->UpdateStyleAndLayoutIgnorePendingStylesheets(
      Document::kRunPostLayoutTasksSynchronously);
}

void Internals::forceFullRepaint(Document* document,
                                 ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->View()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return;
  }

  auto* layout_view = document->GetLayoutView();
  if (layout_view)
    layout_view->InvalidatePaintForViewAndCompositedLayers();
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
    return DOMRectList::Create();
  }

  document->UpdateStyleAndLayout();
  document->View()->UpdateDocumentAnnotatedRegions();
  Vector<AnnotatedRegionValue> regions = document->AnnotatedRegions();

  Vector<FloatQuad> quads;
  for (const AnnotatedRegionValue& region : regions) {
    if (region.draggable == draggable)
      quads.push_back(FloatQuad(FloatRect(region.bounds)));
  }
  return DOMRectList::Create(quads);
}

static const char* CursorTypeToString(Cursor::Type cursor_type) {
  switch (cursor_type) {
    case Cursor::kPointer:
      return "Pointer";
    case Cursor::kCross:
      return "Cross";
    case Cursor::kHand:
      return "Hand";
    case Cursor::kIBeam:
      return "IBeam";
    case Cursor::kWait:
      return "Wait";
    case Cursor::kHelp:
      return "Help";
    case Cursor::kEastResize:
      return "EastResize";
    case Cursor::kNorthResize:
      return "NorthResize";
    case Cursor::kNorthEastResize:
      return "NorthEastResize";
    case Cursor::kNorthWestResize:
      return "NorthWestResize";
    case Cursor::kSouthResize:
      return "SouthResize";
    case Cursor::kSouthEastResize:
      return "SouthEastResize";
    case Cursor::kSouthWestResize:
      return "SouthWestResize";
    case Cursor::kWestResize:
      return "WestResize";
    case Cursor::kNorthSouthResize:
      return "NorthSouthResize";
    case Cursor::kEastWestResize:
      return "EastWestResize";
    case Cursor::kNorthEastSouthWestResize:
      return "NorthEastSouthWestResize";
    case Cursor::kNorthWestSouthEastResize:
      return "NorthWestSouthEastResize";
    case Cursor::kColumnResize:
      return "ColumnResize";
    case Cursor::kRowResize:
      return "RowResize";
    case Cursor::kMiddlePanning:
      return "MiddlePanning";
    case Cursor::kEastPanning:
      return "EastPanning";
    case Cursor::kNorthPanning:
      return "NorthPanning";
    case Cursor::kNorthEastPanning:
      return "NorthEastPanning";
    case Cursor::kNorthWestPanning:
      return "NorthWestPanning";
    case Cursor::kSouthPanning:
      return "SouthPanning";
    case Cursor::kSouthEastPanning:
      return "SouthEastPanning";
    case Cursor::kSouthWestPanning:
      return "SouthWestPanning";
    case Cursor::kWestPanning:
      return "WestPanning";
    case Cursor::kMove:
      return "Move";
    case Cursor::kVerticalText:
      return "VerticalText";
    case Cursor::kCell:
      return "Cell";
    case Cursor::kContextMenu:
      return "ContextMenu";
    case Cursor::kAlias:
      return "Alias";
    case Cursor::kProgress:
      return "Progress";
    case Cursor::kNoDrop:
      return "NoDrop";
    case Cursor::kCopy:
      return "Copy";
    case Cursor::kNone:
      return "None";
    case Cursor::kNotAllowed:
      return "NotAllowed";
    case Cursor::kZoomIn:
      return "ZoomIn";
    case Cursor::kZoomOut:
      return "ZoomOut";
    case Cursor::kGrab:
      return "Grab";
    case Cursor::kGrabbing:
      return "Grabbing";
    case Cursor::kCustom:
      return "Custom";
  }

  NOTREACHED();
  return "UNKNOWN";
}

String Internals::getCurrentCursorInfo() {
  if (!GetFrame())
    return String();

  Cursor cursor =
      GetFrame()->GetPage()->GetChromeClient().LastSetCursorForTesting();

  StringBuilder result;
  result.Append("type=");
  result.Append(CursorTypeToString(cursor.GetType()));
  result.Append(" hotSpot=");
  result.AppendNumber(cursor.HotSpot().X());
  result.Append(',');
  result.AppendNumber(cursor.HotSpot().Y());
  if (cursor.GetImage()) {
    IntSize size = cursor.GetImage()->Size();
    result.Append(" image=");
    result.AppendNumber(size.Width());
    result.Append('x');
    result.AppendNumber(size.Height());
  }
  if (cursor.ImageScaleFactor() != 1) {
    result.Append(" scale=");
    result.AppendNumber(cursor.ImageScaleFactor(), 8);
  }

  return result.ToString();
}

bool Internals::cursorUpdatePending() const {
  if (!GetFrame())
    return false;

  return GetFrame()->GetEventHandler().CursorUpdatePending();
}

bool Internals::fakeMouseMovePending() const {
  if (!GetFrame())
    return false;

  return GetFrame()->GetEventHandler().FakeMouseMovePending();
}

DOMArrayBuffer* Internals::serializeObject(
    scoped_refptr<SerializedScriptValue> value) const {
  base::span<const uint8_t> span = value->GetWireData();
  DOMArrayBuffer* buffer = DOMArrayBuffer::CreateUninitializedOrNull(
      SafeCast<uint32_t>(span.size()), sizeof(uint8_t));
  if (buffer)
    memcpy(buffer->Data(), span.data(), span.size());
  return buffer;
}

scoped_refptr<SerializedScriptValue> Internals::deserializeBuffer(
    DOMArrayBuffer* buffer) const {
  return SerializedScriptValue::Create(static_cast<const char*>(buffer->Data()),
                                       buffer->ByteLength());
}

DOMArrayBuffer* Internals::serializeWithInlineWasm(ScriptValue value) const {
  v8::Isolate* isolate = value.GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "Internals", "serializeWithInlineWasm");
  v8::Local<v8::Value> v8_value = value.V8Value();
  SerializedScriptValue::SerializeOptions options;
  options.wasm_policy = SerializedScriptValue::SerializeOptions::kSerialize;
  scoped_refptr<SerializedScriptValue> obj = SerializedScriptValue::Serialize(
      isolate, v8_value, options, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return serializeObject(obj);
}

ScriptValue Internals::deserializeBufferContainingWasm(
    ScriptState* state,
    DOMArrayBuffer* buffer) const {
  DummyExceptionStateForTesting exception_state;
  SerializedScriptValue::DeserializeOptions options;
  options.read_wasm_from_stream = true;
  return ScriptValue::From(state, deserializeBuffer(buffer)->Deserialize(
                                      state->GetIsolate(), options));
}

void Internals::forceReload(bool bypass_cache) {
  if (!GetFrame())
    return;

  GetFrame()->Reload(bypass_cache ? WebFrameLoadType::kReloadBypassingCache
                                  : WebFrameLoadType::kReload,
                     ClientRedirectPolicy::kNotClientRedirect);
}

StaticSelection* Internals::getSelectionInFlatTree(
    DOMWindow* window,
    ExceptionState& exception_state) {
  Frame* const frame = window->GetFrame();
  if (!frame || !frame->IsLocalFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "Must supply local window");
    return nullptr;
  }
  return StaticSelection::FromSelectionInFlatTree(ConvertToSelectionInFlatTree(
      ToLocalFrame(frame)->Selection().GetSelectionInDOMTree()));
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
  if (!element || !IsHTMLImageElement(*element)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The element should be HTMLImageElement.");
  }

  ToHTMLImageElement(*element).ForceReload();
}

String Internals::selectMenuListText(HTMLSelectElement* select) {
  DCHECK(select);
  LayoutObject* layout_object = select->GetLayoutObject();
  if (!layout_object || !layout_object->IsMenuList())
    return String();

  LayoutMenuList* menu_list = ToLayoutMenuList(layout_object);
  return menu_list->GetText();
}

bool Internals::isSelectPopupVisible(Node* node) {
  DCHECK(node);
  if (auto* select = ToHTMLSelectElementOrNull(*node))
    return select->PopupIsVisible();
  return false;
}

bool Internals::selectPopupItemStyleIsRtl(Node* node, int item_index) {
  if (!node || !IsHTMLSelectElement(*node))
    return false;

  HTMLSelectElement& select = ToHTMLSelectElement(*node);
  if (item_index < 0 ||
      static_cast<wtf_size_t>(item_index) >= select.GetListItems().size())
    return false;
  const ComputedStyle* item_style =
      select.ItemComputedStyle(*select.GetListItems()[item_index]);
  return item_style && item_style->Direction() == TextDirection::kRtl;
}

int Internals::selectPopupItemStyleFontHeight(Node* node, int item_index) {
  if (!node || !IsHTMLSelectElement(*node))
    return false;

  HTMLSelectElement& select = ToHTMLSelectElement(*node);
  if (item_index < 0 ||
      static_cast<wtf_size_t>(item_index) >= select.GetListItems().size())
    return false;
  const ComputedStyle* item_style =
      select.ItemComputedStyle(*select.GetListItems()[item_index]);

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

  document->GetFrame()->View()->UpdateAllLifecyclePhases();
}

void Internals::setZoomFactor(float factor) {
  if (!GetFrame())
    return;

  GetFrame()->SetPageZoomFactor(factor);
}

void Internals::setShouldRevealPassword(Element* element,
                                        bool reveal,
                                        ExceptionState& exception_state) {
  DCHECK(element);
  if (!IsHTMLInputElement(element)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidNodeTypeError,
                                      "The element provided is not an INPUT.");
    return;
  }

  return ToHTMLInputElement(*element).SetShouldRevealPassword(reveal);
}

namespace {

class AddOneFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    AddOneFunction* self = new AddOneFunction(script_state);
    return self->BindToV8Function();
  }

 private:
  explicit AddOneFunction(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  ScriptValue Call(ScriptValue value) override {
    v8::Local<v8::Value> v8_value = value.V8Value();
    DCHECK(v8_value->IsNumber());
    int32_t int_value =
        static_cast<int32_t>(v8_value.As<v8::Integer>()->Value());
    return ScriptValue(
        GetScriptState(),
        v8::Integer::New(GetScriptState()->GetIsolate(), int_value + 1));
  }
};

}  // namespace

ScriptPromise Internals::createResolvedPromise(ScriptState* script_state,
                                               ScriptValue value) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(value);
  return promise;
}

ScriptPromise Internals::createRejectedPromise(ScriptState* script_state,
                                               ScriptValue value) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Reject(value);
  return promise;
}

ScriptPromise Internals::addOneToPromise(ScriptState* script_state,
                                         ScriptPromise promise) {
  return promise.Then(AddOneFunction::CreateFunction(script_state));
}

ScriptPromise Internals::promiseCheck(ScriptState* script_state,
                                      long arg1,
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
                                           long arg1) {
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
                                              long,
                                              long) {
  return ScriptPromise::Cast(script_state,
                             V8String(script_state->GetIsolate(), "done"));
}

void Internals::Trace(blink::Visitor* visitor) {
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
      reverse ? kWebFocusTypeBackward : kWebFocusTypeForward);
}

bool Internals::ignoreLayoutWithPendingStylesheets(Document* document) {
  DCHECK(document);
  return document->IgnoreLayoutWithPendingStylesheets();
}

void Internals::setNetworkConnectionInfoOverride(
    bool on_line,
    const String& type,
    const String& effective_type,
    unsigned long http_rtt_msec,
    double downlink_max_mbps,
    ExceptionState& exception_state) {
  WebConnectionType webtype;
  if (type == "cellular2g") {
    webtype = kWebConnectionTypeCellular2G;
  } else if (type == "cellular3g") {
    webtype = kWebConnectionTypeCellular3G;
  } else if (type == "cellular4g") {
    webtype = kWebConnectionTypeCellular4G;
  } else if (type == "bluetooth") {
    webtype = kWebConnectionTypeBluetooth;
  } else if (type == "ethernet") {
    webtype = kWebConnectionTypeEthernet;
  } else if (type == "wifi") {
    webtype = kWebConnectionTypeWifi;
  } else if (type == "wimax") {
    webtype = kWebConnectionTypeWimax;
  } else if (type == "other") {
    webtype = kWebConnectionTypeOther;
  } else if (type == "none") {
    webtype = kWebConnectionTypeNone;
  } else if (type == "unknown") {
    webtype = kWebConnectionTypeUnknown;
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        ExceptionMessages::FailedToEnumerate("connection type", type));
    return;
  }
  WebEffectiveConnectionType web_effective_type =
      WebEffectiveConnectionType::kTypeUnknown;
  if (effective_type == "offline") {
    web_effective_type = WebEffectiveConnectionType::kTypeOffline;
  } else if (effective_type == "slow-2g") {
    web_effective_type = WebEffectiveConnectionType::kTypeSlow2G;
  } else if (effective_type == "2g") {
    web_effective_type = WebEffectiveConnectionType::kType2G;
  } else if (effective_type == "3g") {
    web_effective_type = WebEffectiveConnectionType::kType3G;
  } else if (effective_type == "4g") {
    web_effective_type = WebEffectiveConnectionType::kType4G;
  } else if (effective_type != "unknown") {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        ExceptionMessages::FailedToEnumerate("effective connection type",
                                             effective_type));
    return;
  }
  GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
      on_line, webtype, web_effective_type, http_rtt_msec, downlink_max_mbps);
  GetFrame()->Client()->SetEffectiveConnectionTypeForTesting(
      web_effective_type);
}

void Internals::setSaveDataEnabled(bool enabled) {
  GetNetworkStateNotifier().SetSaveDataEnabledOverride(enabled);
}

void Internals::clearNetworkConnectionInfoOverride() {
  GetNetworkStateNotifier().ClearOverride();
}

unsigned Internals::countHitRegions(CanvasRenderingContext* context) {
  return context->HitRegionsCount();
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
  std::deque<DOMNodeId> scroll_chain;
  for (wtf_size_t i = 0; i < elements.size(); ++i)
    scroll_chain.push_back(DOMNodeIds::IdForNode(elements[i].Get()));
  scroll_state->SetScrollChain(scroll_chain);
}

void Internals::forceBlinkGCWithoutV8GC() {
  ThreadState::Current()->ScheduleFullGC();
}

String Internals::selectedHTMLForClipboard() {
  if (!GetFrame())
    return String();

  // Selection normalization and markup generation require clean layout.
  GetFrame()->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  return GetFrame()->Selection().SelectedHTMLForClipboard();
}

String Internals::selectedTextForClipboard() {
  if (!GetFrame() || !GetFrame()->GetDocument())
    return String();

  // Clean layout is required for extracting plain text from selection.
  GetFrame()->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

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
  return UseCounter::IsCounted(*document, static_cast<WebFeature>(feature));
}

bool Internals::isCSSPropertyUseCounted(Document* document,
                                        const String& property_name) {
  return UseCounter::IsCounted(*document, property_name);
}

bool Internals::isAnimatedCSSPropertyUseCounted(Document* document,
                                                const String& property_name) {
  return UseCounter::IsCountedAnimatedCSS(*document, property_name);
}

void Internals::clearUseCounter(Document* document, uint32_t feature) {
  if (feature >= static_cast<int32_t>(WebFeature::kNumberOfFeatures))
    return;
  UseCounter::ClearCountForTesting(*document, static_cast<WebFeature>(feature));
}

Vector<String> Internals::getCSSPropertyLonghands() const {
  Vector<String> result;
  for (int id = firstCSSProperty; id <= lastCSSProperty; ++id) {
    CSSPropertyID property = static_cast<CSSPropertyID>(id);
    const CSSProperty& property_class = CSSProperty::Get(property);
    if (property_class.IsLonghand()) {
      result.push_back(property_class.GetPropertyNameString());
    }
  }
  return result;
}

Vector<String> Internals::getCSSPropertyShorthands() const {
  Vector<String> result;
  for (int id = firstCSSProperty; id <= lastCSSProperty; ++id) {
    CSSPropertyID property = static_cast<CSSPropertyID>(id);
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
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (feature >= static_cast<int32_t>(WebFeature::kNumberOfFeatures)) {
    resolver->Reject();
    return promise;
  }

  WebFeature use_counter_feature = static_cast<WebFeature>(feature);
  if (UseCounter::IsCounted(*document, use_counter_feature)) {
    resolver->Resolve();
    return promise;
  }

  DocumentLoader* loader = document->Loader();
  if (!loader) {
    resolver->Reject();
    return promise;
  }

  loader->GetUseCounter().AddObserver(new UseCounterObserverImpl(
      resolver, static_cast<WebFeature>(use_counter_feature)));
  return promise;
}

String Internals::unscopableAttribute() {
  return "unscopableAttribute";
}

String Internals::unscopableMethod() {
  return "unscopableMethod";
}

DOMRectList* Internals::focusRingRects(Element* element) {
  Vector<LayoutRect> rects;
  if (element && element->GetLayoutObject()) {
    element->GetLayoutObject()->AddOutlineRects(
        rects, LayoutPoint(), NGOutlineType::kIncludeBlockVisualOverflow);
  }
  return DOMRectList::Create(rects);
}

DOMRectList* Internals::outlineRects(Element* element) {
  Vector<LayoutRect> rects;
  if (element && element->GetLayoutObject()) {
    element->GetLayoutObject()->AddOutlineRects(
        rects, LayoutPoint(), NGOutlineType::kDontIncludeBlockVisualOverflow);
  }
  return DOMRectList::Create(rects);
}

void Internals::setCapsLockState(bool enabled) {
  KeyboardEventManager::SetCurrentCapsLockState(
      enabled ? OverrideCapsLockState::kOn : OverrideCapsLockState::kOff);
}

bool Internals::setScrollbarVisibilityInScrollableArea(Node* node,
                                                       bool visible) {
  if (ScrollableArea* scrollable_area = ScrollableAreaForNode(node)) {
    scrollable_area->SetScrollbarsHiddenIfOverlay(!visible);
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
      .MonotonicTimeToZeroBasedDocumentTime(TimeTicksFromSeconds(platform_time))
      .InSecondsF();
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

DOMRect* Internals::visualRect(Node* node) {
  if (!node || !node->GetLayoutObject())
    return DOMRect::Create();

  return DOMRect::FromFloatRect(
      FloatRect(node->GetLayoutObject()->FragmentsVisualRectBoundingBox()));
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
  MemoryCoordinator::SetIsLowEndDeviceForTesting(is_low_end_device);
}

bool Internals::isLowEndDevice() const {
  return MemoryCoordinator::IsLowEndDevice();
}

Vector<String> Internals::supportedTextEncodingLabels() const {
  return WTF::TextEncodingAliasesForTesting();
}

void Internals::simulateRasterUnderInvalidations(bool enable) {
  RasterInvalidationTracking::SimulateRasterUnderInvalidations(enable);
}

void Internals::BypassLongCompileThresholdOnce(
    ExceptionState& exception_state) {
  LocalFrame* frame = GetFrame();
  DCHECK(frame);
  PerformanceMonitor* performance_monitor = frame->GetPerformanceMonitor();
  if (!performance_monitor) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "PerformanceObserver should be observing 'longtask' while "
        "calling BypassLongCompileThresholdOnce.");
    return;
  }
  return performance_monitor->BypassLongCompileThresholdOnceForTesting();
}

unsigned Internals::LifecycleUpdateCount() const {
  return document_->View()->LifecycleUpdateCountForTesting();
}

void Internals::DisableIntersectionObserverThrottleDelay() const {
  // This gets reset by Internals::ResetToConsistentState
  IntersectionObserver::SetThrottleDelayEnabledForTesting(false);
}

void Internals::addEmbedderCustomElementName(const AtomicString& name,
                                             ExceptionState& exception_state) {
  CustomElement::AddEmbedderCustomElementNameForTesting(name, exception_state);
}

}  // namespace blink
