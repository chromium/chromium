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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/testing/internals.h"

#include <atomic>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/function_ref.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
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
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_unresolved_property.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
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
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/remote_playback_controller.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/keyboard_event_manager.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_conversion.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/page/touch_adjustment.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/import_map.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/scroll/mac_scrollbar_animator.h"
#include "third_party/blink/renderer/core/scroll/programmatic_scroll_animator.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/readable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
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
#include "third_party/blink/renderer/core/testing/internals_ukm_recorder.h"
#include "third_party/blink/renderer/core/testing/mock_hyphenation.h"
#include "third_party/blink/renderer/core/testing/origin_trials_test.h"
#include "third_party/blink/renderer/core/testing/record_test.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/core/testing/sequence_test.h"
#include "third_party/blink/renderer/core/testing/static_selection.h"
#include "third_party/blink/renderer/core/testing/type_conversions.h"
#include "third_party/blink/renderer/core/testing/union_types_test.h"
#include "third_party/blink/renderer/core/timezone/timezone_controller.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-blink.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "v8/include/v8.h"

namespace blink {

using ui::mojom::ImeTextSpanThickness;
using ui::mojom::ImeTextSpanUnderlineStyle;

namespace {

ScopedMockOverlayScrollbars* g_mock_overlay_scrollbars = nullptr;

void ResetMockOverlayScrollbars() {
  if (g_mock_overlay_scrollbars)
    delete g_mock_overlay_scrollbars;
  g_mock_overlay_scrollbars = nullptr;
}

class UseCounterImplObserverImpl final : public UseCounterImpl::Observer {
 public:
  UseCounterImplObserverImpl(ScriptPromiseResolver<IDLUndefined>* resolver,
                             WebFeature feature)
      : resolver_(resolver), feature_(feature) {}
  UseCounterImplObserverImpl(const UseCounterImplObserverImpl&) = delete;
  UseCounterImplObserverImpl& operator=(const UseCounterImplObserverImpl&) =
      delete;

  bool OnCountFeature(WebFeature feature) final {
    if (feature_ != feature)
      return false;
    resolver_->Resolve();
    return true;
  }

  void Trace(Visitor* visitor) const override {
    UseCounterImpl::Observer::Trace(visitor);
    visitor->Trace(resolver_);
  }

 private:
  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  WebFeature feature_;
};

class TestReadableStreamSource : public UnderlyingSourceBase {
 public:
  class Generator;

  using Reply = CrossThreadOnceFunction<void(std::unique_ptr<Generator>)>;
  using OptimizerCallback =
      CrossThreadOnceFunction<void(scoped_refptr<base::SingleThreadTaskRunner>,
                                   Reply)>;

  enum class Type {
    kWithNullOptimizer,
    kWithPerformNullOptimizer,
    kWithObservableOptimizer,
    kWithPerfectOptimizer,
  };

  class Generator final {
    USING_FAST_MALLOC(Generator);

   public:
    explicit Generator(int max_count) : max_count_(max_count) {}

    std::optional<int> Generate() {
      if (count_ >= max_count_) {
        return std::nullopt;
      }
      ++count_;
      return current_++;
    }

    void Add(int n) { current_ += n; }

   private:
    friend class Optimizer;

    int current_ = 0;
    int count_ = 0;
    const int max_count_;
  };

  class Optimizer final : public ReadableStreamTransferringOptimizer {
    USING_FAST_MALLOC(Optimizer);

   public:
    Optimizer(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
              OptimizerCallback callback,
              Type type)
        : task_runner_(std::move(task_runner)),
          callback_(std::move(callback)),
          type_(type) {}

    UnderlyingSourceBase* PerformInProcessOptimization(
        ScriptState* script_state) override;

   private:
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
    OptimizerCallback callback_;
    const Type type_;
  };

  TestReadableStreamSource(ScriptState* script_state, Type type)
      : UnderlyingSourceBase(script_state), type_(type) {}

  ScriptPromiseUntyped Start(ScriptState* script_state,
                             ExceptionState&) override {
    if (generator_) {
      return ToResolvedUndefinedPromise(script_state);
    }
    resolver_ =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    return resolver_->Promise();
  }

  ScriptPromiseUntyped Pull(ScriptState* script_state,
                            ExceptionState&) override {
    if (!generator_) {
      return ToResolvedUndefinedPromise(script_state);
    }

    const auto result = generator_->Generate();
    if (!result) {
      Controller()->Close();
      return ToResolvedUndefinedPromise(script_state);
    }
    Controller()->Enqueue(
        v8::Integer::New(script_state->GetIsolate(), *result));
    return ToResolvedUndefinedPromise(script_state);
  }

  std::unique_ptr<ReadableStreamTransferringOptimizer>
  CreateTransferringOptimizer(ScriptState* script_state) {
    switch (type_) {
      case Type::kWithNullOptimizer:
        return nullptr;
      case Type::kWithPerformNullOptimizer:
        return std::make_unique<ReadableStreamTransferringOptimizer>();
      case Type::kWithObservableOptimizer:
      case Type::kWithPerfectOptimizer:
        ExecutionContext* context = ExecutionContext::From(script_state);
        return std::make_unique<Optimizer>(
            context->GetTaskRunner(TaskType::kInternalDefault),
            CrossThreadBindOnce(&TestReadableStreamSource::Detach,
                                MakeUnwrappingCrossThreadWeakHandle(this)),
            type_);
    }
  }

  void Attach(std::unique_ptr<Generator> generator) {
    if (type_ == Type::kWithObservableOptimizer) {
      generator->Add(100);
    }
    generator_ = std::move(generator);
    if (resolver_) {
      resolver_->Resolve();
    }
  }

  void Detach(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
              Reply reply) {
    Controller()->Close();
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(std::move(reply), std::move(generator_)));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  const Type type_;
  std::unique_ptr<Generator> generator_;
  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
};

UnderlyingSourceBase*
TestReadableStreamSource::Optimizer::PerformInProcessOptimization(
    ScriptState* script_state) {
  TestReadableStreamSource* source =
      MakeGarbageCollected<TestReadableStreamSource>(script_state, type_);
  ExecutionContext* context = ExecutionContext::From(script_state);

  Reply reply = CrossThreadBindOnce(&TestReadableStreamSource::Attach,
                                    MakeUnwrappingCrossThreadHandle(source));

  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(std::move(callback_),
                          context->GetTaskRunner(TaskType::kInternalDefault),
                          std::move(reply)));
  return source;
}

class TestWritableStreamSink final : public UnderlyingSinkBase {
 public:
  class InternalSink;

  using Reply = CrossThreadOnceFunction<void(std::unique_ptr<InternalSink>)>;
  using OptimizerCallback =
      CrossThreadOnceFunction<void(scoped_refptr<base::SingleThreadTaskRunner>,
                                   Reply)>;
  enum class Type {
    kWithNullOptimizer,
    kWithPerformNullOptimizer,
    kWithObservableOptimizer,
    kWithPerfectOptimizer,
  };

  class InternalSink final {
    USING_FAST_MALLOC(InternalSink);

   public:
    InternalSink(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 CrossThreadOnceFunction<void(std::string)> success_callback,
                 CrossThreadOnceFunction<void()> error_callback)
        : task_runner_(std::move(task_runner)),
          success_callback_(std::move(success_callback)),
          error_callback_(std::move(error_callback)) {}

    void Append(const std::string& s) { result_.append(s); }
    void Close() {
      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(std::move(success_callback_), result_));
    }
    void Abort() {
      PostCrossThreadTask(*task_runner_, FROM_HERE, std::move(error_callback_));
    }

    // We don't use WTF::String because this object can be accessed from
    // multiple threads.
    std::string result_;

    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
    CrossThreadOnceFunction<void(std::string)> success_callback_;
    CrossThreadOnceFunction<void()> error_callback_;
  };

  class Optimizer final : public WritableStreamTransferringOptimizer {
    USING_FAST_MALLOC(Optimizer);

   public:
    Optimizer(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
        OptimizerCallback callback,
        scoped_refptr<base::RefCountedData<std::atomic_bool>> optimizer_flag,
        Type type)
        : task_runner_(std::move(task_runner)),
          callback_(std::move(callback)),
          optimizer_flag_(std::move(optimizer_flag)),
          type_(type) {}

    UnderlyingSinkBase* PerformInProcessOptimization(
        ScriptState* script_state) override;

   private:
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
    OptimizerCallback callback_;
    scoped_refptr<base::RefCountedData<std::atomic_bool>> optimizer_flag_;
    const Type type_;
  };

  explicit TestWritableStreamSink(ScriptState* script_state, Type type)
      : type_(type),
        optimizer_flag_(
            base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
                std::in_place,
                false)) {}

  ScriptPromise<IDLUndefined> start(ScriptState* script_state,
                                    WritableStreamDefaultController*,
                                    ExceptionState&) override {
    if (internal_sink_) {
      return ToResolvedUndefinedPromise(script_state);
    }
    start_resolver_ =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    return start_resolver_->Promise();
  }
  ScriptPromise<IDLUndefined> write(ScriptState* script_state,
                                    ScriptValue chunk,
                                    WritableStreamDefaultController*,
                                    ExceptionState&) override {
    DCHECK(internal_sink_);
    internal_sink_->Append(
        ToCoreString(script_state->GetIsolate(),
                     chunk.V8Value()
                         ->ToString(script_state->GetContext())
                         .ToLocalChecked())
            .Utf8());
    return ToResolvedUndefinedPromise(script_state);
  }
  ScriptPromise<IDLUndefined> close(ScriptState* script_state,
                                    ExceptionState&) override {
    DCHECK(internal_sink_);
    closed_ = true;
    if (!optimizer_flag_->data.load()) {
      // The normal closure case.
      internal_sink_->Close();
      return ToResolvedUndefinedPromise(script_state);
    }

    // When the optimizer is active, we need to detach `internal_sink_` and
    // pass it to the optimizer (i.e., the sink in the destination realm).
    if (detached_) {
      PostCrossThreadTask(
          *reply_task_runner_, FROM_HERE,
          CrossThreadBindOnce(std::move(reply_), std::move(internal_sink_)));
    }
    return ToResolvedUndefinedPromise(script_state);
  }
  ScriptPromise<IDLUndefined> abort(ScriptState* script_state,
                                    ScriptValue reason,
                                    ExceptionState&) override {
    return ToResolvedUndefinedPromise(script_state);
  }

  void Attach(std::unique_ptr<InternalSink> internal_sink) {
    DCHECK(!internal_sink_);

    if (type_ == Type::kWithObservableOptimizer) {
      internal_sink->Append("A");
    }

    internal_sink_ = std::move(internal_sink);
    if (start_resolver_) {
      start_resolver_->Resolve();
    }
  }

  void Detach(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
              Reply reply) {
    detached_ = true;

    // We need to wait for the close signal before actually detaching
    // `internal_sink_`.
    if (closed_) {
      PostCrossThreadTask(
          *task_runner, FROM_HERE,
          CrossThreadBindOnce(std::move(reply), std::move(internal_sink_)));
    } else {
      reply_ = std::move(reply);
      reply_task_runner_ = std::move(task_runner);
    }
  }

  std::unique_ptr<WritableStreamTransferringOptimizer>
  CreateTransferringOptimizer(ScriptState* script_state) {
    DCHECK(internal_sink_);

    if (type_ == Type::kWithNullOptimizer) {
      return nullptr;
    }

    ExecutionContext* context = ExecutionContext::From(script_state);
    return std::make_unique<Optimizer>(
        context->GetTaskRunner(TaskType::kInternalDefault),
        CrossThreadBindOnce(&TestWritableStreamSink::Detach,
                            MakeUnwrappingCrossThreadWeakHandle(this)),
        optimizer_flag_, type_);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(start_resolver_);
    UnderlyingSinkBase::Trace(visitor);
  }

  static void Resolve(ScriptPromiseResolver<IDLString>* resolver,
                      std::string result) {
    resolver->Resolve(String::FromUTF8(result));
  }
  static void Reject(ScriptPromiseResolverBase* resolver) {
    ScriptState* script_state = resolver->GetScriptState();
    ScriptState::Scope scope(script_state);
    resolver->Reject(
        V8ThrowException::CreateTypeError(script_state->GetIsolate(), "error"));
  }

 private:
  const Type type_;
  // `optimizer_flag_` is always non_null. The flag referenced is false
  // initially, and set atomically when the associated optimizer is activated.
  scoped_refptr<base::RefCountedData<std::atomic_bool>> optimizer_flag_;
  std::unique_ptr<InternalSink> internal_sink_;
  Member<ScriptPromiseResolver<IDLUndefined>> start_resolver_;
  bool closed_ = false;
  bool detached_ = false;
  Reply reply_;
  scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner_;
};

UnderlyingSinkBase*
TestWritableStreamSink::Optimizer::PerformInProcessOptimization(
    ScriptState* script_state) {
  if (type_ == Type::kWithPerformNullOptimizer) {
    return nullptr;
  }
  TestWritableStreamSink* sink =
      MakeGarbageCollected<TestWritableStreamSink>(script_state, type_);

  // Set the flag atomically, to notify that this optimizer is active.
  optimizer_flag_->data.store(true);

  ExecutionContext* context = ExecutionContext::From(script_state);
  Reply reply = CrossThreadBindOnce(&TestWritableStreamSink::Attach,
                                    MakeUnwrappingCrossThreadHandle(sink));
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(std::move(callback_),
                          context->GetTaskRunner(TaskType::kInternalDefault),
                          std::move(reply)));
  return sink;
}

void OnLCPPredicted(ScriptPromiseResolver<IDLString>* resolver,
                    const Element* lcp_element) {
  const ElementLocator locator =
      lcp_element ? element_locator::OfElement(*lcp_element) : ElementLocator();
  resolver->Resolve(element_locator::ToStringForTesting(locator));
}

}  // namespace

static std::optional<DocumentMarker::MarkerType> MarkerTypeFrom(
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
  return std::nullopt;
}

static std::optional<DocumentMarker::MarkerTypes> MarkerTypesFrom(
    const String& marker_type) {
  if (marker_type.empty() || EqualIgnoringASCIICase(marker_type, "all"))
    return DocumentMarker::MarkerTypes::All();
  std::optional<DocumentMarker::MarkerType> type = MarkerTypeFrom(marker_type);
  if (!type)
    return std::nullopt;
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

  KeyboardEventManager::SetCurrentCapsLockState(
      OverrideCapsLockState::kDefault);

  IntersectionObserver::SetThrottleDelayEnabledForTesting(true);
  ResetMockOverlayScrollbars();

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

GCObservation* Internals::observeGC(ScriptValue script_value,
                                    ExceptionState& exception_state) {
  v8::Local<v8::Value> observed_value = script_value.V8Value();
  DCHECK(!observed_value.IsEmpty());
  if (observed_value->IsNull() || observed_value->IsUndefined()) {
    exception_state.ThrowTypeError("value to observe is null or undefined");
    return nullptr;
  }

  return MakeGarbageCollected<GCObservation>(script_value.GetIsolate(),
                                             observed_value);
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

unsigned Internals::styleForElementCount(
    ExceptionState& exception_state) const {
  if (!document_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No context document is available.");
    return 0;
  }

  return document_->GetStyleEngine().StyleForElementCount();
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

unsigned Internals::layoutCountForTesting(
    ExceptionState& exception_state) const {
  LocalFrame* context_frame = GetFrame();
  if (!context_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No context frame is available.");
    return 0;
  }

  return context_frame->View()->LayoutCountForTesting();
}

bool Internals::nodeNeedsStyleRecalc(Node* node,
                                     ExceptionState& exception_state) const {
  if (!node) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidNodeTypeError,
                                      "Not a node");
    return false;
  }

  return node->NeedsStyleRecalc();
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
  return isPreloadedBy(url, document_.Get());
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
      MemoryCache::Get()->ResourceForURL(full_url, cache_identifier);
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
      MemoryCache::Get()->ResourceForURL(full_url, cache_identifier);
  return resource && resource->GetStatus() == ResourceStatus::kCached;
}

ScriptPromise<IDLLong> Internals::getInitialResourcePriority(
    ScriptState* script_state,
    const String& url,
    Document* document,
    bool new_load_only) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLLong>>(script_state);
  auto promise = resolver->Promise();
  KURL resource_url = url_test_helpers::ToKURL(url.Utf8());

  auto callback = WTF::BindOnce(&Internals::ResolveResourcePriority,
                                WrapPersistent(this), WrapPersistent(resolver));
  document->Fetcher()->AddPriorityObserverForTesting(
      resource_url, std::move(callback), new_load_only);

  return promise;
}

ScriptPromise<IDLLong> Internals::getInitialResourcePriorityOfNewLoad(
    ScriptState* script_state,
    const String& url,
    Document* document) {
  return getInitialResourcePriority(script_state, url, document, true);
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
      IsA<Document>(node1) ? static_cast<const TreeScope*>(To<Document>(node1))
      : IsA<ShadowRoot>(node1)
          ? static_cast<const TreeScope*>(To<ShadowRoot>(node1))
          : nullptr;
  const TreeScope* tree_scope2 =
      IsA<Document>(node2) ? static_cast<const TreeScope*>(To<Document>(node2))
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
  GetFrame()->GetDocument()->Timeline().PauseAnimationsForTesting(
      ANIMATION_TIME_DELTA_FROM_SECONDS(pause_time));
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

namespace {

bool CheckForFlatTreeExceptions(Node* node, ExceptionState& exception_state) {
  if (node && !node->IsShadowRoot())
    return false;
  exception_state.ThrowDOMException(
      DOMExceptionCode::kInvalidAccessError,
      "The node argument doesn't participate in the flat tree.");
  return true;
}

}  // namespace

Node* Internals::nextSiblingInFlatTree(Node* node,
                                       ExceptionState& exception_state) {
  if (CheckForFlatTreeExceptions(node, exception_state))
    return nullptr;
  return FlatTreeTraversal::NextSibling(*node);
}

Node* Internals::firstChildInFlatTree(Node* node,
                                      ExceptionState& exception_state) {
  if (CheckForFlatTreeExceptions(node, exception_state))
    return nullptr;
  return FlatTreeTraversal::FirstChild(*node);
}

Node* Internals::lastChildInFlatTree(Node* node,
                                     ExceptionState& exception_state) {
  if (CheckForFlatTreeExceptions(node, exception_state))
    return nullptr;
  return FlatTreeTraversal::LastChild(*node);
}

Node* Internals::nextInFlatTree(Node* node, ExceptionState& exception_state) {
  if (CheckForFlatTreeExceptions(node, exception_state))
    return nullptr;
  return FlatTreeTraversal::Next(*node);
}

Node* Internals::previousInFlatTree(Node* node,
                                    ExceptionState& exception_state) {
  if (CheckForFlatTreeExceptions(node, exception_state))
    return nullptr;
  return FlatTreeTraversal::Previous(*node);
}

String Internals::elementLayoutTreeAsText(Element* element,
                                          ExceptionState& exception_state) {
  DCHECK(element);
  element->GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  String representation = ExternalRepresentation(element);
  if (representation.empty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The element provided has no external representation.");
    return String();
  }

  return representation;
}

CSSStyleDeclaration* Internals::computedStyleIncludingVisitedInfo(
    Element* element) const {
  DCHECK(element);
  bool allow_visited_style = true;
  return MakeGarbageCollected<CSSComputedStyleDeclaration>(element,
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
  if (auto* input = DynamicTo<HTMLInputElement>(*host)) {
    input->EnsureShadowSubtree();
  }
  return host->GetShadowRoot();
}

String Internals::ShadowRootMode(const Node* root,
                                 ExceptionState& exception_state) const {
  DCHECK(root);
  auto* shadow_root = DynamicTo<ShadowRoot>(root);
  if (!shadow_root) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The node provided is not a shadow root.");
    return String();
  }

  switch (shadow_root->GetMode()) {
    case ShadowRootMode::kUserAgent:
      return String("UserAgentShadowRoot");
    case ShadowRootMode::kOpen:
      return String("OpenShadowRoot");
    case ShadowRootMode::kClosed:
      return String("ClosedShadowRoot");
    default:
      NOTREACHED_IN_MIGRATION();
      return String("Unknown");
  }
}

const AtomicString& Internals::shadowPseudoId(Element* element) {
  DCHECK(element);
  return element->ShadowPseudoId();
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
  return DOMRectReadOnly::FromRect(
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
  return DOMRectReadOnly::FromRect(layout_object->AbsoluteBoundingBoxRect());
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

  std::optional<DocumentMarker::MarkerType> type = MarkerTypeFrom(marker_type);
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

void Internals::removeMarker(Document* document,
                             const Range* range,
                             const String& marker_type,
                             ExceptionState& exception_state) {
  if (!document) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "No context document is available.");
    return;
  }

  std::optional<DocumentMarker::MarkerType> type = MarkerTypeFrom(marker_type);
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
  if (type == DocumentMarker::kSpelling) {
    document->Markers().RemoveMarkersInRange(
        EphemeralRange(range), DocumentMarker::MarkerTypes::Spelling());
  } else {
    document->Markers().RemoveMarkersInRange(
        EphemeralRange(range), DocumentMarker::MarkerTypes::Grammar());
  }
}

unsigned Internals::markerCountForNode(Text* text,
                                       const String& marker_type,
                                       ExceptionState& exception_state) {
  DCHECK(text);
  std::optional<DocumentMarker::MarkerTypes> marker_types =
      MarkerTypesFrom(marker_type);
  if (!marker_types) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The marker type provided ('" + marker_type + "') is invalid.");
    return 0;
  }

  return text->GetDocument()
      .Markers()
      .MarkersFor(*text, marker_types.value())
      .size();
}

unsigned Internals::activeMarkerCountForNode(Text* text) {
  DCHECK(text);

  // Only TextMatch markers can be active.
  DocumentMarkerVector markers = text->GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextMatch());

  unsigned active_marker_count = 0;
  for (const auto& marker : markers) {
    if (To<TextMatchMarker>(marker.Get())->IsActiveMatch())
      active_marker_count++;
  }

  return active_marker_count;
}

DocumentMarker* Internals::MarkerAt(Text* text,
                                    const String& marker_type,
                                    unsigned index,
                                    ExceptionState& exception_state) {
  DCHECK(text);
  std::optional<DocumentMarker::MarkerTypes> marker_types =
      MarkerTypesFrom(marker_type);
  if (!marker_types) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The marker type provided ('" + marker_type + "') is invalid.");
    return nullptr;
  }

  DocumentMarkerVector markers =
      text->GetDocument().Markers().MarkersFor(*text, marker_types.value());
  if (markers.size() <= index)
    return nullptr;
  return markers[index].Get();
}

Range* Internals::markerRangeForNode(Text* text,
                                     const String& marker_type,
                                     unsigned index,
                                     ExceptionState& exception_state) {
  DCHECK(text);
  DocumentMarker* marker = MarkerAt(text, marker_type, index, exception_state);
  if (!marker)
    return nullptr;
  return MakeGarbageCollected<Range>(text->GetDocument(), text,
                                     marker->StartOffset(), text,
                                     marker->EndOffset());
}

String Internals::markerDescriptionForNode(Text* text,
                                           const String& marker_type,
                                           unsigned index,
                                           ExceptionState& exception_state) {
  DocumentMarker* marker = MarkerAt(text, marker_type, index, exception_state);
  if (!marker || !IsSpellCheckMarker(*marker))
    return String();
  return To<SpellCheckMarker>(marker)->Description();
}

unsigned Internals::markerBackgroundColorForNode(
    Text* text,
    const String& marker_type,
    unsigned index,
    ExceptionState& exception_state) {
  DocumentMarker* marker = MarkerAt(text, marker_type, index, exception_state);
  auto* style_marker = DynamicTo<StyleableMarker>(marker);
  if (!style_marker)
    return 0;
  return style_marker->BackgroundColor().Rgb();
}

unsigned Internals::markerUnderlineColorForNode(
    Text* text,
    const String& marker_type,
    unsigned index,
    ExceptionState& exception_state) {
  DocumentMarker* marker = MarkerAt(text, marker_type, index, exception_state);
  auto* style_marker = DynamicTo<StyleableMarker>(marker);
  if (!style_marker)
    return 0;
  return style_marker->UnderlineColor().Rgb();
}

static std::optional<TextMatchMarker::MatchStatus> MatchStatusFrom(
    const String& match_status) {
  if (EqualIgnoringASCIICase(match_status, "kActive"))
    return TextMatchMarker::MatchStatus::kActive;
  if (EqualIgnoringASCIICase(match_status, "kInactive"))
    return TextMatchMarker::MatchStatus::kInactive;
  return std::nullopt;
}

void Internals::addTextMatchMarker(const Range* range,
                                   const String& match_status,
                                   ExceptionState& exception_state) {
  DCHECK(range);
  if (!range->OwnerDocument().View())
    return;

  std::optional<TextMatchMarker::MatchStatus> match_status_enum =
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

static std::optional<ImeTextSpanThickness> ThicknessFrom(
    const String& thickness) {
  if (EqualIgnoringASCIICase(thickness, "none"))
    return ImeTextSpanThickness::kNone;
  if (EqualIgnoringASCIICase(thickness, "thin"))
    return ImeTextSpanThickness::kThin;
  if (EqualIgnoringASCIICase(thickness, "thick"))
    return ImeTextSpanThickness::kThick;
  return std::nullopt;
}

static std::optional<ImeTextSpanUnderlineStyle> UnderlineStyleFrom(
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
  return std::nullopt;
}

namespace {

void AddStyleableMarkerHelper(const Range* range,
                              const String& underline_color_value,
                              const String& thickness_value,
                              const String& underline_style_value,
                              const String& text_color_value,
                              const String& background_color_value,
                              ExceptionState& exception_state,
                              base::FunctionRef<void(const EphemeralRange&,
                                                     Color,
                                                     ImeTextSpanThickness,
                                                     ImeTextSpanUnderlineStyle,
                                                     Color,
                                                     Color)> create_marker) {
  DCHECK(range);
  range->OwnerDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  std::optional<ImeTextSpanThickness> thickness =
      ThicknessFrom(thickness_value);
  if (!thickness) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The thickness provided ('" + thickness_value + "') is invalid.");
    return;
  }

  std::optional<ImeTextSpanUnderlineStyle> underline_style =
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
  AddStyleableMarkerHelper(
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
  AddStyleableMarkerHelper(
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
  AddStyleableMarkerHelper(
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
  gfx::Size initial_viewport_size(available_width, available_height);
  document->GetPage()->DeprecatedLocalMainFrame()->View()->SetFrameRect(
      gfx::Rect(gfx::Point(), initial_viewport_size));

  ViewportDescription description = page->GetViewportDescription();
  PageScaleConstraints constraints =
      description.Resolve(gfx::SizeF(initial_viewport_size), Length());

  constraints.FitToContentsWidth(constraints.layout_size.width(),
                                 available_width);
  constraints.ResolveAutoInitialScale();

  StringBuilder builder;

  builder.Append("viewport size ");
  builder.Append(String::Number(constraints.layout_size.width()));
  builder.Append('x');
  builder.Append(String::Number(constraints.layout_size.height()));

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

  if (auto* select = DynamicTo<HTMLSelectElement>(*element)) {
    // A Null string resets the suggested value.
    select->SetSuggestedValue(value.empty() ? String() : value);
  }

  To<HTMLFormControlElement>(element)->SetAutofillState(
      value.empty() ? WebAutofillState::kNotFilled
                    : WebAutofillState::kPreviewed);
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

  if (auto* select = DynamicTo<HTMLSelectElement>(*element)) {
    select->SetAutofillValue(
        value.empty() ? String()  // Null string resets the autofill state.
                      : value,
        value.empty() ? WebAutofillState::kNotFilled
                      : WebAutofillState::kAutofilled);
  }
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

// TODO(mustaq): The next 5 functions are very similar, can we combine them?

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
  gfx::Point adjusted_point;

  EventHandler& event_handler = document->GetFrame()->GetEventHandler();
  bool found_node = event_handler.BestNodeForHitTestResult(
      TouchAdjustmentCandidateType::kClickable, location, result,
      adjusted_point, target_node);
  if (found_node)
    return DOMPoint::Create(adjusted_point.x(), adjusted_point.y());

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
  gfx::Point adjusted_point;
  document->GetFrame()->GetEventHandler().BestNodeForHitTestResult(
      TouchAdjustmentCandidateType::kClickable, location, result,
      adjusted_point, target_node);
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
  gfx::Point adjusted_point;

  EventHandler& event_handler = document->GetFrame()->GetEventHandler();
  bool found_node = event_handler.BestNodeForHitTestResult(
      TouchAdjustmentCandidateType::kContextMenu, location, result,
      adjusted_point, target_node);
  if (found_node)
    return DOMPoint::Create(adjusted_point.x(), adjusted_point.y());

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
  gfx::Point adjusted_point;
  document->GetFrame()->GetEventHandler().BestNodeForHitTestResult(
      TouchAdjustmentCandidateType::kContextMenu, location, result,
      adjusted_point, target_node);
  return target_node;
}

Node* Internals::touchNodeAdjustedToBestStylusWritableNode(
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
  gfx::Point adjusted_point;
  document->GetFrame()->GetEventHandler().BestNodeForHitTestResult(
      TouchAdjustmentCandidateType::kStylusWritable, location, result,
      adjusted_point, target_node);
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

int Internals::spellCheckedTextLength(Document* document,
                                      ExceptionState& exception_state) {
  SpellCheckRequester* requester = GetSpellCheckRequester(document);

  if (!requester) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No spell check requestor can be obtained for the provided document.");
    return -1;
  }

  return requester->SpellCheckedTextLength();
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
  if (!document || !document->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No frame can be obtained from the provided document.");
    return String();
  }

  return document->GetFrame()
      ->GetSpellChecker()
      .GetIdleSpellCheckController()
      .GetStateAsString();
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

bool Internals::hasLastEditCommand(Document* document,
                                   ExceptionState& exception_state) {
  if (!document || !document->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "No frame can be obtained from the provided document.");
    return false;
  }

  return document->GetFrame()->GetEditor().LastEditCommand();
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
static void MergeRects(Vector<gfx::Rect>& rects) {
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
        if (rects[i].y() == rects[j].y() &&
            rects[i].height() == rects[j].height()) {
          if (rects[i].x() + rects[i].width() == rects[j].x()) {
            rects[i].set_width(rects[i].width() + rects[j].width());
            rects[j] = gfx::Rect();
            updated = true;
          } else if (rects[i].x() == rects[j].x() + rects[j].width()) {
            rects[i].set_x(rects[j].x());
            rects[i].set_width(rects[i].width() + rects[j].width());
            rects[j] = gfx::Rect();
            updated = true;
          }
        } else if (rects[i].x() == rects[j].x() &&
                   rects[i].width() == rects[j].width()) {
          if (rects[i].y() + rects[i].height() == rects[j].y()) {
            rects[i].set_height(rects[i].height() + rects[j].height());
            rects[j] = gfx::Rect();
            updated = true;
          } else if (rects[i].y() == rects[j].y() + rects[j].height()) {
            rects[i].set_y(rects[j].y());
            rects[i].set_height(rects[i].height() + rects[j].height());
            rects[j] = gfx::Rect();
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
  if (!document->View()->RootCcLayer()) {
    return hit_test_rects;
  }
  for (const auto& layer : document->View()->RootCcLayer()->children()) {
    const cc::TouchActionRegion& touch_action_region =
        layer->touch_action_region();
    if (!touch_action_region.GetAllRegions().IsEmpty()) {
      const auto& offset = layer->offset_to_transform_parent();
      gfx::Rect layer_rect(
          gfx::ToRoundedPoint(gfx::PointAtOffsetFromOrigin(offset)),
          layer->bounds());

      Vector<gfx::Rect> layer_hit_test_rects;
      for (auto hit_test_rect : touch_action_region.GetAllRegions())
        layer_hit_test_rects.push_back(hit_test_rect);
      MergeRects(layer_hit_test_rects);

      for (const gfx::Rect& hit_test_rect : layer_hit_test_rects) {
        if (!hit_test_rect.IsEmpty()) {
          hit_test_rects->Append(DOMRectReadOnly::FromRect(layer_rect),
                                 DOMRectReadOnly::FromRect(hit_test_rect));
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
      mojom::InspectorIssueCode::kCookieIssue,
      mojom::blink::InspectorIssueDetails::New());
  document->GetFrame()->AddInspectorIssue(
      AuditsIssue(ConvertInspectorIssueToProtocolFormat(
          InspectorIssue::Create(std::move(info)))));
}

AtomicString Internals::htmlNamespace() {
  return html_names::xhtmlNamespaceURI;
}

Vector<AtomicString> Internals::htmlTags() {
  Vector<AtomicString> tags(html_names::kTagsCount);
  std::unique_ptr<const HTMLQualifiedName*[]> qualified_names =
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
  std::unique_ptr<const SVGQualifiedName*[]> qualified_names =
      svg_names::GetTags();
  for (wtf_size_t i = 0; i < svg_names::kTagsCount; ++i)
    tags[i] = qualified_names[i]->LocalName();
  return tags;
}

StaticNodeList* Internals::nodesFromRect(
    ScriptState* script_state,
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
  } else if (!gfx::Rect(gfx::Point(), frame->View()->Size())
                  .Intersects(ToEnclosingRect(rect))) {
    return nullptr;
  }
  if (allow_child_frame_content)
    hit_type |= HitTestRequest::kAllowChildFrameContent;

  HitTestRequest request(hit_type);
  HitTestLocation location(rect);
  HitTestResult result(request, location);
  frame->ContentLayoutObject()->HitTest(location, result);
  HeapVector<Member<Node>> matches(result.ListBasedTestResult());

  // Ensure WindowProxy instances for child frames. crbug.com/1407555.
  for (auto& node : matches) {
    if (node->IsDocumentNode() && node.Get() != document) {
      node->GetDocument().GetFrame()->GetWindowProxy(script_state->World());
    }
  }

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
  LayoutLocale::SetHyphenationForTesting(locale, MockHyphenation::Create());
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
  if (frame->View()->UserScrollableAreas()) {
    for (const auto& scrollable_area :
         frame->View()->UserScrollableAreas()->Values()) {
      if (scrollable_area->ScrollsOverflow())
        count++;
    }
  }

  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (child_local_frame && child_local_frame->View() &&
        child_local_frame->View()->UserScrollableAreas()) {
      for (const auto& scrollable_area :
           child_local_frame->View()->UserScrollableAreas()->Values()) {
        if (scrollable_area->ScrollsOverflow())
          count++;
      }
    }
  }

  return count;
}

String Internals::layerTreeAsText(Document* document,
                                  ExceptionState& exception_state) const {
  return layerTreeAsText(document, 0, exception_state);
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

void Internals::evictAllResources() const {
  MemoryCache::Get()->EvictResources();
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

  return PrintContext::PageNumberForElement(
      element, gfx::SizeF(page_width, page_height));
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
                                     gfx::SizeF(page_width, page_height));
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

float Internals::layoutZoomFactor(ExceptionState& exception_state) {
  if (!document_->GetPage()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The document's page cannot be retrieved.");
    return 0;
  }
  // Layout zoom without Device Scale Factor.
  return document_->GetPage()->GetChromeClient().UserZoomFactor(
      document_->GetFrame());
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
  video_element->SetPersistentState(persistent);
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
#if DCHECK_IS_ON()
  WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
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
#if DCHECK_IS_ON()
  WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy(
      scheme, static_cast<SchemeRegistry::PolicyAreas>(policy_areas_enum));
}

void Internals::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
    const String& scheme) {
#if DCHECK_IS_ON()
  WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
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

InternalsUkmRecorder* Internals::initializeUKMRecorder() {
  return MakeGarbageCollected<InternalsUkmRecorder>(document_);
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

void Internals::disableReferencedFilePathsVerification() const {
  if (!GetFrame())
    return;
  GetFrame()
      ->GetDocument()
      ->GetFormController()
      .SetDropReferencedFilePathsForTesting();
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
  } else if (auto* node_document = DynamicTo<Document>(node)) {
    document = node_document;
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
  return DraggableRegions(document, true, exception_state);
}

DOMRectList* Internals::nonDraggableRegions(Document* document,
                                            ExceptionState& exception_state) {
  return DraggableRegions(document, false, exception_state);
}

void Internals::SetSupportsDraggableRegions(bool supports_draggable_regions) {
  document_->GetPage()
      ->GetChromeClient()
      .GetWebView()
      ->SetSupportsDraggableRegions(supports_draggable_regions);
}

DOMRectList* Internals::DraggableRegions(Document* document,
                                         bool draggable,
                                         ExceptionState& exception_state) {
  DCHECK(document);
  if (!document->View()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return MakeGarbageCollected<DOMRectList>();
  }

  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  document->View()->UpdateDocumentDraggableRegions();
  Vector<DraggableRegionValue> regions = document->DraggableRegions();

  Vector<gfx::QuadF> quads;
  for (const DraggableRegionValue& region : regions) {
    if (region.draggable == draggable)
      quads.push_back(gfx::QuadF(gfx::RectF(region.bounds)));
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
    case ui::mojom::blink::CursorType::kNorthSouthNoResize:
      return "NorthSouthNoResize";
    case ui::mojom::blink::CursorType::kEastWestNoResize:
      return "EastWestNoResize";
    case ui::mojom::blink::CursorType::kNorthEastSouthWestNoResize:
      return "NorthEastSouthWestNoResize";
    case ui::mojom::blink::CursorType::kNorthWestSouthEastNoResize:
      return "NorthWestSouthEastNoResize";
  }

  NOTREACHED_IN_MIGRATION();
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

    if (cursor.image_scale_factor() != 1.0f) {
      result.Append(" scale=");
      result.AppendNumber(cursor.image_scale_factor(), 8);
    }
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
      base::checked_cast<uint32_t>(span.size()), sizeof(uint8_t));
  if (buffer)
    memcpy(buffer->Data(), span.data(), span.size());
  return buffer;
}

ScriptValue Internals::deserializeBuffer(v8::Isolate* isolate,
                                         DOMArrayBuffer* buffer) const {
  scoped_refptr<SerializedScriptValue> serialized_value =
      SerializedScriptValue::Create(base::make_span(
          static_cast<const uint8_t*>(buffer->Data()), buffer->ByteLength()));
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
  GetFrame()->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Position position =
      GetFrame()->Selection().ComputeVisibleSelectionInDOMTree().Anchor();
  return position.IsNull() ? nullptr : position.ComputeContainerNode();
}

unsigned Internals::visibleSelectionAnchorOffset() {
  if (!GetFrame())
    return 0;
  GetFrame()->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Position position =
      GetFrame()->Selection().ComputeVisibleSelectionInDOMTree().Anchor();
  return position.IsNull() ? 0 : position.ComputeOffsetInContainerNode();
}

Node* Internals::visibleSelectionFocusNode() {
  if (!GetFrame())
    return nullptr;
  GetFrame()->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Position position =
      GetFrame()->Selection().ComputeVisibleSelectionInDOMTree().Focus();
  return position.IsNull() ? nullptr : position.ComputeContainerNode();
}

unsigned Internals::visibleSelectionFocusOffset() {
  if (!GetFrame())
    return 0;
  GetFrame()->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Position position =
      GetFrame()->Selection().ComputeVisibleSelectionInDOMTree().Focus();
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
  return DOMRect::FromRectF(
      gfx::RectF(GetFrame()->Selection().AbsoluteUnclippedBounds()));
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

void Internals::setForcedColorsAndDarkPreferredColorScheme(Document* document) {
  DCHECK(document);
  color_scheme_helper_.emplace(*document);
  color_scheme_helper_->SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  color_scheme_helper_->SetInForcedColors(*document, /*in_forced_colors=*/true);
  color_scheme_helper_->SetEmulatedForcedColors(*document,
                                                /*is_dark_theme=*/false);
}

void Internals::setDarkPreferredColorScheme(Document* document) {
  DCHECK(document);
  Settings* settings = document->GetSettings();
  settings->SetPreferredColorScheme(mojom::blink::PreferredColorScheme::kDark);
}

void Internals::setDarkPreferredRootScrollbarColorScheme(Document* document) {
  DCHECK(document);
  color_scheme_helper_.emplace(*document);
  color_scheme_helper_->SetPreferredRootScrollbarColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
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

class AddOneFunction : public ScriptFunction::Callable {
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

ScriptPromise<IDLAny> Internals::createResolvedPromise(
    ScriptState* script_state,
    ScriptValue value) {
  return ToResolvedPromise<IDLAny>(script_state, value);
}

ScriptPromise<IDLAny> Internals::createRejectedPromise(
    ScriptState* script_state,
    ScriptValue value) {
  return ScriptPromise<IDLAny>::Reject(script_state, value);
}

ScriptPromise<IDLAny> Internals::addOneToPromise(ScriptState* script_state,
                                                 ScriptPromiseUntyped promise) {
  return promise.Then(MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<AddOneFunction>()));
}

ScriptPromise<IDLAny> Internals::promiseCheck(ScriptState* script_state,
                                              int32_t arg1,
                                              bool arg2,
                                              const ScriptValue& arg3,
                                              const String& arg4,
                                              const Vector<String>& arg5,
                                              ExceptionState& exception_state) {
  if (arg2) {
    return ToResolvedPromise<IDLAny>(
        script_state, V8String(script_state->GetIsolate(), "done"));
  }
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Thrown from the native implementation.");
  return EmptyPromise();
}

ScriptPromise<IDLAny> Internals::promiseCheckWithoutExceptionState(
    ScriptState* script_state,
    const ScriptValue& arg1,
    const String& arg2,
    const Vector<String>& arg3) {
  return ToResolvedPromise<IDLAny>(
      script_state, V8String(script_state->GetIsolate(), "done"));
}

ScriptPromise<IDLAny> Internals::promiseCheckRange(ScriptState* script_state,
                                                   int32_t arg1) {
  return ToResolvedPromise<IDLAny>(
      script_state, V8String(script_state->GetIsolate(), "done"));
}

ScriptPromise<IDLAny> Internals::promiseCheckOverload(ScriptState* script_state,
                                                      Location*) {
  return ToResolvedPromise<IDLAny>(
      script_state, V8String(script_state->GetIsolate(), "done"));
}

ScriptPromise<IDLAny> Internals::promiseCheckOverload(ScriptState* script_state,
                                                      Document*) {
  return ToResolvedPromise<IDLAny>(
      script_state, V8String(script_state->GetIsolate(), "done"));
}

ScriptPromise<IDLAny> Internals::promiseCheckOverload(ScriptState* script_state,
                                                      Location*,
                                                      int32_t,
                                                      int32_t) {
  return ToResolvedPromise<IDLAny>(
      script_state, V8String(script_state->GetIsolate(), "done"));
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

void Internals::forceLoseCanvasContext(HTMLCanvasElement* canvas,
                                       const String& context_type) {
  CanvasContextCreationAttributesCore attr;
  CanvasRenderingContext* context =
      canvas->GetCanvasRenderingContext(context_type, attr);
  if (!context)
    return;
  context->LoseContext(CanvasRenderingContext::kSyntheticLostContext);
}

void Internals::forceLoseCanvasContext(OffscreenCanvas* offscreencanvas,
                                       const String& context_type) {
  CanvasContextCreationAttributesCore attr;
  CanvasRenderingContext* context = offscreencanvas->GetCanvasRenderingContext(
      document_->GetExecutionContext(),
      CanvasRenderingContext::RenderingAPIFromId(context_type), attr);
  if (!context)
    return;
  context->LoseContext(CanvasRenderingContext::kSyntheticLostContext);
}

void Internals::disableCanvasAcceleration(HTMLCanvasElement* canvas) {
  canvas->DisableAcceleration();
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

void Internals::setVisualViewportOffset(int css_x, int css_y) {
  if (!GetFrame())
    return;
  float zoom = GetFrame()->LayoutZoomFactor();
  gfx::PointF offset(css_x * zoom, css_y * zoom);
  GetFrame()->GetPage()->GetVisualViewport().SetLocation(offset);
}

bool Internals::isUseCounted(Document* document, uint32_t feature) {
  if (feature > static_cast<int32_t>(WebFeature::kMaxValue)) {
    return false;
  }
  return document->IsUseCounted(static_cast<WebFeature>(feature));
}

bool Internals::isWebDXFeatureUseCounted(Document* document, uint32_t feature) {
  if (feature > static_cast<int32_t>(WebDXFeature::kMaxValue)) {
    return false;
  }
  return document->IsWebDXFeatureCounted(static_cast<WebDXFeature>(feature));
}

bool Internals::isCSSPropertyUseCounted(Document* document,
                                        const String& property_name) {
  return document->IsPropertyCounted(
      UnresolvedCSSPropertyID(document->GetExecutionContext(), property_name));
}

bool Internals::isAnimatedCSSPropertyUseCounted(Document* document,
                                                const String& property_name) {
  return document->IsAnimatedPropertyCounted(
      UnresolvedCSSPropertyID(document->GetExecutionContext(), property_name));
}

void Internals::clearUseCounter(Document* document, uint32_t feature) {
  if (feature > static_cast<int32_t>(WebFeature::kMaxValue)) {
    return;
  }
  document->ClearUseCounterForTesting(static_cast<WebFeature>(feature));
}

Vector<String> Internals::getCSSPropertyLonghands() const {
  Vector<String> result;
  for (CSSPropertyID property : CSSPropertyIDList()) {
    const CSSProperty& property_class = CSSProperty::Get(property);
    if (property_class.IsWebExposed(document_->GetExecutionContext()) &&
        property_class.IsLonghand()) {
      result.push_back(property_class.GetPropertyNameString());
    }
  }
  return result;
}

Vector<String> Internals::getCSSPropertyShorthands() const {
  Vector<String> result;
  for (CSSPropertyID property : CSSPropertyIDList()) {
    const CSSProperty& property_class = CSSProperty::Get(property);
    if (property_class.IsWebExposed(document_->GetExecutionContext()) &&
        property_class.IsShorthand()) {
      result.push_back(property_class.GetPropertyNameString());
    }
  }
  return result;
}

Vector<String> Internals::getCSSPropertyAliases() const {
  Vector<String> result;
  for (CSSPropertyID alias : kCSSPropertyAliasList) {
    DCHECK(IsPropertyAlias(alias));
    const CSSUnresolvedProperty& property_class = *GetPropertyInternal(alias);
    if (property_class.IsWebExposed(document_->GetExecutionContext())) {
      result.push_back(property_class.GetPropertyNameString());
    }
  }
  return result;
}

ScriptPromise<IDLUndefined> Internals::observeUseCounter(
    ScriptState* script_state,
    Document* document,
    uint32_t feature) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  if (feature > static_cast<int32_t>(WebFeature::kMaxValue)) {
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

  loader->GetUseCounter().AddObserver(
      MakeGarbageCollected<UseCounterImplObserverImpl>(
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

    if (MacScrollbarAnimator* scrollbar_animator =
            scrollable_area->GetMacScrollbarAnimator()) {
      scrollbar_animator->SetScrollbarsVisibleForTesting(visible);
    }

    return scrollable_area->GetPageScrollbarTheme().UsesOverlayScrollbars();
  }
  return false;
}

double Internals::monotonicTimeToZeroBasedDocumentTime(
    double platform_time,
    ExceptionState& exception_state) {
  return document_->Loader()
      ->GetTiming()
      .MonotonicTimeToZeroBasedDocumentTime(base::TimeTicks() +
                                            base::Seconds(platform_time))
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

void Internals::addEmbedderCustomElementName(const AtomicString& name,
                                             ExceptionState& exception_state) {
  CustomElement::AddEmbedderCustomElementNameForTesting(name, exception_state);
}

String Internals::getParsedImportMap(Document* document,
                                     ExceptionState& exception_state) {
  Modulator* modulator =
      Modulator::From(ToScriptStateForMainWorld(document->GetFrame()));

  if (!modulator) {
    exception_state.ThrowTypeError("No modulator");
    return String();
  }

  const ImportMap* import_map = modulator->GetImportMapForTest();
  if (!import_map)
    return "{}";

  return import_map->ToStringForTesting();
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

void Internals::ResolveResourcePriority(
    ScriptPromiseResolver<IDLLong>* resolver,
    int resource_load_priority) {
  resolver->Resolve(resource_load_priority);
}

String Internals::getAgentId(DOMWindow* window) {
  if (!window->IsLocalDOMWindow())
    return String();

  // Create a unique id from the process id and the address of the agent.
  const base::ProcessId process_id = base::GetCurrentProcId();
  uintptr_t agent_address =
      reinterpret_cast<uintptr_t>(To<LocalDOMWindow>(window)->GetAgent());

  // This serializes a pointer as a decimal number, which is a bit ugly, but
  // it works. Is there any utility to dump a number in a hexadecimal form?
  // I couldn't find one in WTF.
  return String::Number(process_id) + ":" + String::Number(agent_address);
}

void Internals::useMockOverlayScrollbars() {
  // Note: it's important to reset `g_mock_overlay_scrollbars` before the
  // assignment, since if `g_mock_overlay_scrollbars` is non-null, its
  // destructor will end up running after the constructor for the new
  // ScopedMockOverlayScrollbars runs, meaning the global state the new pointer
  // stores will in fact be the state from the previous pointer, which may not
  // be what was intended. E.g. if a test calls this function twice, then
  // whatever the original global state was in Blink's ScrollbarThemeSettings
  // will be lost, and the state after the second call may be wrong.
  ResetMockOverlayScrollbars();
  g_mock_overlay_scrollbars = new ScopedMockOverlayScrollbars(true);
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

void Internals::setIsAdFrame(Document* target_doc,
                             ExceptionState& exception_state) {
  LocalFrame* frame = target_doc->GetFrame();

  if (frame->IsMainFrame() && !frame->IsInFencedFrameTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Frame must be an iframe or a fenced frame.");
    return;
  }

  blink::FrameAdEvidence ad_evidence(/*parent_is_ad=*/frame->Parent() &&
                                     frame->Parent()->IsAdFrame());
  ad_evidence.set_created_by_ad_script(
      mojom::FrameCreationStackEvidence::kCreatedByAdScript);
  ad_evidence.set_is_complete();
  frame->SetAdEvidence(ad_evidence);
}

ReadableStream* Internals::createReadableStream(
    ScriptState* script_state,
    int32_t queue_size,
    const String& optimizer,
    ExceptionState& exception_state) {
  TestReadableStreamSource::Type type;
  if (optimizer.empty()) {
    type = TestReadableStreamSource::Type::kWithNullOptimizer;
  } else if (optimizer == "perform-null") {
    type = TestReadableStreamSource::Type::kWithPerformNullOptimizer;
  } else if (optimizer == "observable") {
    type = TestReadableStreamSource::Type::kWithObservableOptimizer;
  } else if (optimizer == "perfect") {
    type = TestReadableStreamSource::Type::kWithPerformNullOptimizer;
  } else {
    exception_state.ThrowRangeError(
        "The \"optimizer\" parameter is not correctly set.");
    return nullptr;
  }
  auto* source =
      MakeGarbageCollected<TestReadableStreamSource>(script_state, type);
  source->Attach(std::make_unique<TestReadableStreamSource::Generator>(10));
  return ReadableStream::CreateWithCountQueueingStrategy(
      script_state, source, queue_size, AllowPerChunkTransferring(false),
      source->CreateTransferringOptimizer(script_state));
}

ScriptValue Internals::createWritableStreamAndSink(
    ScriptState* script_state,
    int32_t queue_size,
    const String& optimizer,
    ExceptionState& exception_state) {
  TestWritableStreamSink::Type type;
  if (optimizer.empty()) {
    type = TestWritableStreamSink::Type::kWithNullOptimizer;
  } else if (optimizer == "perform-null") {
    type = TestWritableStreamSink::Type::kWithPerformNullOptimizer;
  } else if (optimizer == "observable") {
    type = TestWritableStreamSink::Type::kWithObservableOptimizer;
  } else if (optimizer == "perfect") {
    type = TestWritableStreamSink::Type::kWithPerfectOptimizer;
  } else {
    exception_state.ThrowRangeError(
        "The \"optimizer\" parameter is not correctly set.");
    return ScriptValue();
  }

  ExecutionContext* context = ExecutionContext::From(script_state);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto internal_sink = std::make_unique<TestWritableStreamSink::InternalSink>(
      context->GetTaskRunner(TaskType::kInternalDefault),
      CrossThreadBindOnce(&TestWritableStreamSink::Resolve,
                          MakeUnwrappingCrossThreadHandle(resolver)),
      CrossThreadBindOnce(&TestWritableStreamSink::Reject,
                          MakeUnwrappingCrossThreadHandle(resolver)));
  auto* sink = MakeGarbageCollected<TestWritableStreamSink>(script_state, type);

  sink->Attach(std::move(internal_sink));
  auto* stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, sink, queue_size,
      sink->CreateTransferringOptimizer(script_state));

  v8::Local<v8::Object> object = v8::Object::New(script_state->GetIsolate());
  object
      ->Set(script_state->GetContext(),
            V8String(script_state->GetIsolate(), "stream"),
            ToV8Traits<WritableStream>::ToV8(script_state, stream))
      .Check();
  object
      ->Set(script_state->GetContext(),
            V8String(script_state->GetIsolate(), "sink"),
            ToV8Traits<IDLPromise<IDLString>>::ToV8(script_state,
                                                    resolver->Promise()))
      .Check();
  return ScriptValue(script_state->GetIsolate(), object);
}

void Internals::setAllowPerChunkTransferring(ReadableStream* stream) {
  if (!stream) {
    return;
  }
  stream->SetAllowPerChunkTransferringForTesting(
      AllowPerChunkTransferring(true));
}

void Internals::setBackForwardCacheRestorationBufferSize(unsigned int maxSize) {
  WindowPerformance& perf =
      *DOMWindowPerformance::performance(*document_->domWindow());
  perf.setBackForwardCacheRestorationBufferSizeForTest(maxSize);
}

void Internals::setEventTimingBufferSize(unsigned int maxSize) {
  WindowPerformance& perf =
      *DOMWindowPerformance::performance(*document_->domWindow());
  perf.setEventTimingBufferSizeForTest(maxSize);
}

void Internals::stopResponsivenessMetricsUkmSampling() {
  WindowPerformance& perf =
      *DOMWindowPerformance::performance(*document_->domWindow());
  perf.GetResponsivenessMetrics().StopUkmSamplingForTesting();
}

Vector<String> Internals::getCreatorScripts(HTMLImageElement* img) {
  DCHECK(img);
  return Vector<String>(img->creator_scripts());
}

ScriptPromise<IDLString> Internals::LCPPrediction(ScriptState* script_state,
                                                  Document* document) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();

  LCPCriticalPathPredictor* lcpp = document->GetFrame()->GetLCPP();
  CHECK(lcpp);
  lcpp->AddLCPPredictedCallback(
      WTF::BindOnce(&OnLCPPredicted, WrapPersistent(resolver)));
  return promise;
}

void ExemptUrlFromNetworkRevocationComplete(
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  resolver->Resolve();
}

ScriptPromise<IDLUndefined> Internals::exemptUrlFromNetworkRevocation(
    ScriptState* script_state,
    const String& url) {
  if (!blink::features::IsFencedFramesEnabled()) {
    return EmptyPromise();
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kFencedFramesLocalUnpartitionedDataAccess)) {
    return EmptyPromise();
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kExemptUrlFromNetworkRevocationForTesting)) {
    return EmptyPromise();
  }
  if (!GetFrame()) {
    return EmptyPromise();
  }
  LocalFrame* frame = GetFrame();
  DCHECK(frame->GetDocument());
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  frame->GetLocalFrameHostRemote().ExemptUrlFromNetworkRevocationForTesting(
      url_test_helpers::ToKURL(url.Utf8()),
      WTF::BindOnce(&ExemptUrlFromNetworkRevocationComplete,
                    WrapPersistent(resolver)));
  return promise;
}

}  // namespace blink
