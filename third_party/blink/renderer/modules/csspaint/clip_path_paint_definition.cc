// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/clip_path_paint_definition.h"

#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet_proxy_client.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_id_generator.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// This class includes information that is required by the compositor thread
// when painting clip path.
class ClipPathPaintWorkletInput : public PaintWorkletInput {
 public:
  ClipPathPaintWorkletInput(const FloatRect& container_rect,
                            int worklet_id,
                            Path path)
      : PaintWorkletInput(container_rect.Size(), worklet_id), path_(path) {}

  ~ClipPathPaintWorkletInput() override = default;
  Path ClipPath() const { return path_; }

  PaintWorkletInputType GetType() const override {
    return PaintWorkletInputType::kClipPath;
  }

 private:
  Path path_;
};

class ClipPathPaintWorkletProxyClient : public NativePaintWorkletProxyClient {
 public:
  static ClipPathPaintWorkletProxyClient* Create(int worklet_id) {
    return MakeGarbageCollected<ClipPathPaintWorkletProxyClient>(worklet_id);
  }

  explicit ClipPathPaintWorkletProxyClient(int worklet_id)
      : NativePaintWorkletProxyClient(worklet_id) {}
  ~ClipPathPaintWorkletProxyClient() override = default;
  ClipPathPaintWorkletProxyClient(const ClipPathPaintWorkletProxyClient&) =
      delete;
  ClipPathPaintWorkletProxyClient& operator=(
      const ClipPathPaintWorkletProxyClient&) = delete;

  // PaintWorkletPainter implementation.
  sk_sp<PaintRecord> Paint(
      const CompositorPaintWorkletInput* compositor_input,
      const CompositorPaintWorkletJob::AnimatedPropertyValues&
          animated_property_values) override {
    const ClipPathPaintWorkletInput* input =
        To<ClipPathPaintWorkletInput>(compositor_input);
    FloatSize container_size = input->ContainerSize();
    PaintRenderingContext2DSettings* context_settings =
        PaintRenderingContext2DSettings::Create();
    auto* rendering_context = MakeGarbageCollected<PaintRenderingContext2D>(
        RoundedIntSize(container_size), context_settings, 1, 1);

    PaintFlags flags;
    flags.setAntiAlias(true);
    rendering_context->GetPaintCanvas()->drawPath(input->ClipPath().GetSkPath(),
                                                  flags);
    return rendering_context->GetRecord();
  }
};
}  // namespace

template <>
struct DowncastTraits<ClipPathPaintWorkletInput> {
  static bool AllowFrom(const cc::PaintWorkletInput& worklet_input) {
    auto* input = DynamicTo<PaintWorkletInput>(worklet_input);
    return input && AllowFrom(*input);
  }

  static bool AllowFrom(const PaintWorkletInput& worklet_input) {
    return worklet_input.GetType() ==
           PaintWorkletInput::PaintWorkletInputType::kClipPath;
  }
};

// static
ClipPathPaintDefinition* ClipPathPaintDefinition::Create(
    LocalFrame& local_root) {
  return MakeGarbageCollected<ClipPathPaintDefinition>(PassKey(), local_root);
}

ClipPathPaintDefinition::ClipPathPaintDefinition(PassKey pass_key,
                                                 LocalFrame& local_root)
    : worklet_id_(PaintWorkletIdGenerator::NextId()) {
  DCHECK(local_root.IsLocalRoot());
  paint_dispatcher_ =
      WebLocalFrameImpl::FromFrame(local_root)
          ->FrameWidgetImpl()
          ->EnsureCompositorPaintDispatcher(&compositor_host_queue_);
  DCHECK(IsMainThread());
  ExecutionContext* context = local_root.DomWindow();
  FrameOrWorkerScheduler* scheduler =
      context ? context->GetScheduler() : nullptr;
  // TODO(crbug.com/1143407): We don't need this thread if we can make the
  // compositor thread support GC.
  ThreadCreationParams params(ThreadType::kAnimationAndPaintWorkletThread);
  worker_backing_thread_ = std::make_unique<WorkerBackingThread>(
      params.SetFrameOrWorkerScheduler(scheduler));
  auto startup_data = WorkerBackingThreadStartupData::CreateDefault();
  PostCrossThreadTask(
      *worker_backing_thread_->BackingThread().GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&WorkerBackingThread::InitializeOnBackingThread,
                          CrossThreadUnretained(worker_backing_thread_.get()),
                          startup_data));
  // This is called only once per document.
  ClipPathPaintWorkletProxyClient* client =
      ClipPathPaintWorkletProxyClient::Create(worklet_id_);
  RegisterProxyClient(client);
}

void ClipPathPaintDefinition::RegisterProxyClient(
    NativePaintWorkletProxyClient* client) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      worker_backing_thread_->BackingThread().GetTaskRunner();
  // At this moment, we are in the paint phase which is before commit, we queue
  // a task to the compositor thread to register the |paint_dispatcher_|. When
  // compositor schedules the actual paint job (PaintWorkletPainter::Paint),
  // which is after commit, the |paint_dispatcher_| should have been registerted
  // and ready to use.
  PostCrossThreadTask(
      *compositor_host_queue_, FROM_HERE,
      CrossThreadBindOnce(
          &PaintWorkletPaintDispatcher::RegisterPaintWorkletPainter,
          paint_dispatcher_, WrapCrossThreadPersistent(client), task_runner));
}

void ClipPathPaintDefinition::UnregisterProxyClient() {
  PostCrossThreadTask(
      *compositor_host_queue_, FROM_HERE,
      CrossThreadBindOnce(
          &PaintWorkletPaintDispatcher::UnregisterPaintWorkletPainter,
          paint_dispatcher_, worklet_id_));
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(*worker_backing_thread_->BackingThread().GetTaskRunner(),
                      FROM_HERE,
                      CrossThreadBindOnce(
                          [](WorkerBackingThread* worker_backing_thread,
                             base::WaitableEvent* waitable_event) {
                            worker_backing_thread->ShutdownOnBackingThread();
                            waitable_event->Signal();
                          },
                          CrossThreadUnretained(worker_backing_thread_.get()),
                          CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();
  worker_backing_thread_.reset();
}

scoped_refptr<Image> ClipPathPaintDefinition::Paint(
    float zoom,
    const FloatRect& reference_box,
    const Node& node) {
  DCHECK(node.IsElementNode());
  const ClipPathOperation& clip_path =
      *node.GetLayoutObject()->StyleRef().ClipPath();

  DCHECK_EQ(clip_path.GetType(), ClipPathOperation::SHAPE);
  auto& shape = To<ShapeClipPathOperation>(clip_path);
  Path path = shape.GetPath(reference_box, zoom);

  scoped_refptr<ClipPathPaintWorkletInput> input =
      base::MakeRefCounted<ClipPathPaintWorkletInput>(reference_box,
                                                      worklet_id_, path);
  return PaintWorkletDeferredImage::Create(std::move(input),
                                           reference_box.Size());
}

void ClipPathPaintDefinition::Trace(Visitor* visitor) const {
  NativePaintDefinition::Trace(visitor);
}

}  // namespace blink
