// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/clip_path_paint_worklet.h"

#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet_proxy_client.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

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
ClipPathPaintWorklet* ClipPathPaintWorklet::Create(LocalFrame& local_root) {
  return MakeGarbageCollected<ClipPathPaintWorklet>(PassKey(), local_root);
}

ClipPathPaintWorklet::ClipPathPaintWorklet(PassKey pass_key,
                                           LocalFrame& local_root)
    : NativePaintWorklet(local_root) {
  // This is called only once per document.
  ClipPathPaintWorkletProxyClient* client =
      ClipPathPaintWorkletProxyClient::Create(worklet_id_);
  RegisterProxyClient(client);
}

ClipPathPaintWorklet::~ClipPathPaintWorklet() = default;

scoped_refptr<Image> ClipPathPaintWorklet::Paint(float zoom,
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

}  // namespace blink
