// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"

#include <iomanip>
#include <sstream>

#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"

#if DCHECK_IS_ON()

namespace blink {
namespace {

class NodeCollector {
 public:
  virtual ~NodeCollector() = default;

  virtual void AddVisualViewportProperties(const VisualViewport&,
                                           PropertyTreePrinter&) const {}
  virtual void AddOtherProperties(const LocalFrameView&,
                                  PropertyTreePrinter&) const {}
  virtual void AddObjectPaintProperties(const ObjectPaintProperties&,
                                        PropertyTreePrinter&) const {}
};

class FrameViewPropertyTreePrinter : public PropertyTreePrinter {
 public:
  explicit FrameViewPropertyTreePrinter(const NodeCollector& collector)
      : collector_(collector) {}

  String TreeAsString(const LocalFrameView& frame_view) {
    CollectNodes(frame_view);
    return PropertyTreePrinter::NodesAsTreeString();
  }

 private:
  void CollectNodes(const LocalFrameView& frame_view) {
    collector_.AddVisualViewportProperties(
        frame_view.GetPage()->GetVisualViewport(), *this);
    if (LayoutView* layout_view = frame_view.GetLayoutView())
      CollectNodes(*layout_view);
    for (Frame* child = frame_view.GetFrame().Tree().FirstChild(); child;
         child = child->Tree().NextSibling()) {
      auto* child_local_frame = DynamicTo<LocalFrame>(child);
      if (!child_local_frame)
        continue;
      if (LocalFrameView* child_view = child_local_frame->View())
        CollectNodes(*child_view);
    }
    collector_.AddOtherProperties(frame_view, *this);
  }

  void CollectNodes(const LayoutObject& object) {
    for (const FragmentData& fragment : FragmentDataIterator(object)) {
      if (const auto* properties = fragment.PaintProperties()) {
        collector_.AddObjectPaintProperties(*properties, *this);
      }
    }
    for (const auto* child = object.SlowFirstChild(); child;
         child = child->NextSibling()) {
      CollectNodes(*child);
    }
  }

  const NodeCollector& collector_;
};

class TransformNodeCollector : public NodeCollector {
 public:
  void AddVisualViewportProperties(
      const VisualViewport& visual_viewport,
      PropertyTreePrinter& printer) const override {
    printer.AddNode(visual_viewport.GetDeviceEmulationTransformNode());
    printer.AddNode(visual_viewport.GetOverscrollElasticityTransformNode());
    printer.AddNode(visual_viewport.GetPageScaleNode());
    printer.AddNode(visual_viewport.GetScrollTranslationNode());
  }
  void AddObjectPaintProperties(const ObjectPaintProperties& properties,
                                PropertyTreePrinter& printer) const override {
    properties.AddTransformNodesToPrinter(printer);
  }
};

class ClipNodeCollector : public NodeCollector {
 public:
  void AddObjectPaintProperties(const ObjectPaintProperties& properties,
                                PropertyTreePrinter& printer) const override {
    properties.AddClipNodesToPrinter(printer);
  }
};

class EffectNodeCollector : public NodeCollector {
 public:
  void AddObjectPaintProperties(const ObjectPaintProperties& properties,
                                PropertyTreePrinter& printer) const override {
    properties.AddEffectNodesToPrinter(printer);
  }

  void AddOtherProperties(const LocalFrameView& frame_view,
                          PropertyTreePrinter& printer) const override {
    printer.AddNode(&frame_view.GetFrame().Selection().CaretEffectNode());
  }
};

class ScrollNodeCollector : public NodeCollector {
 public:
  void AddVisualViewportProperties(
      const VisualViewport& visual_viewport,
      PropertyTreePrinter& printer) const override {
    printer.AddNode(visual_viewport.GetScrollNode());
  }

  void AddObjectPaintProperties(const ObjectPaintProperties& properties,
                                PropertyTreePrinter& printer) const override {
    properties.AddScrollNodesToPrinter(printer);
  }
};

void SetDebugName(const PaintPropertyNode* node, const String& debug_name) {
  if (node) {
    const_cast<PaintPropertyNode*>(node)->SetDebugName(debug_name);
  }
}

void SetDebugName(const PaintPropertyNode* node,
                  const String& name,
                  const LayoutObject& object) {
  if (node) {
    SetDebugName(node, name + " (" + object.DebugName() + ")");
  }
}

}  // namespace

namespace paint_property_tree_printer {

void UpdateDebugNames(const VisualViewport& viewport) {
  if (auto* device_emulation_node = viewport.GetDeviceEmulationTransformNode())
    SetDebugName(device_emulation_node, "Device Emulation Node");
  if (auto* overscroll_node = viewport.GetOverscrollElasticityTransformNode())
    SetDebugName(overscroll_node, "Overscroll Elasticity Node");
  SetDebugName(viewport.GetPageScaleNode(), "VisualViewport Scale Node");
  SetDebugName(viewport.GetScrollTranslationNode(),
               "VisualViewport Translate Node");
  SetDebugName(viewport.GetScrollNode(), "VisualViewport Scroll Node");
}

void UpdateDebugNames(const LayoutObject& object,
                      ObjectPaintProperties& properties) {
  SetDebugName(properties.PaintOffsetTranslation(), "PaintOffsetTranslation",
               object);
  SetDebugName(properties.StickyTranslation(), "StickyTranslation", object);
  SetDebugName(properties.AnchorPositionScrollTranslation(),
               "AnchorPositionScrollTranslation", object);
  SetDebugName(properties.Translate(), "Translate", object);
  SetDebugName(properties.Rotate(), "Rotate", object);
  SetDebugName(properties.Scale(), "Scale", object);
  SetDebugName(properties.Offset(), "Offset", object);
  SetDebugName(properties.Transform(), "Transform", object);
  SetDebugName(properties.Perspective(), "Perspective", object);
  SetDebugName(properties.ReplacedContentTransform(),
               "ReplacedContentTransform", object);
  SetDebugName(properties.ScrollTranslation(), "ScrollTranslation", object);
  SetDebugName(properties.TransformIsolationNode(), "TransformIsolationNode",
               object);

  SetDebugName(properties.ClipPathClip(), "ClipPathClip", object);
  SetDebugName(properties.MaskClip(), "MaskClip", object);
  SetDebugName(properties.CssClip(), "CssClip", object);
  SetDebugName(properties.CssClipFixedPosition(), "CssClipFixedPosition",
               object);
  SetDebugName(properties.PixelMovingFilterClipExpander(),
               "PixelMovingFilterClip", object);
  SetDebugName(properties.OverflowControlsClip(), "OverflowControlsClip",
               object);
  SetDebugName(properties.BackgroundClip(), "BackgroundClip", object);
  SetDebugName(properties.InnerBorderRadiusClip(), "InnerBorderRadiusClip",
               object);
  SetDebugName(properties.OverflowClip(), "OverflowClip", object);
  SetDebugName(properties.ClipIsolationNode(), "ClipIsolationNode", object);

  SetDebugName(properties.Effect(), "Effect", object);
  SetDebugName(properties.Filter(), "Filter", object);
  SetDebugName(properties.VerticalScrollbarEffect(), "VerticalScrollbarEffect",
               object);
  SetDebugName(properties.HorizontalScrollbarEffect(),
               "HorizontalScrollbarEffect", object);
  SetDebugName(properties.ScrollCornerEffect(), "ScrollCornerEffect", object);
  SetDebugName(properties.Mask(), "Mask", object);
  SetDebugName(properties.ClipPathMask(), "ClipPathMask", object);
  SetDebugName(properties.ElementCaptureEffect(), "ElementCaptureEffect",
               object);
  SetDebugName(properties.EffectIsolationNode(), "EffectIsolationNode", object);

  SetDebugName(properties.Scroll(), "Scroll", object);
}

}  // namespace paint_property_tree_printer

}  // namespace blink

CORE_EXPORT void ShowAllPropertyTrees(const blink::LocalFrameView& rootFrame) {
  ShowTransformPropertyTree(rootFrame);
  ShowClipPropertyTree(rootFrame);
  ShowEffectPropertyTree(rootFrame);
  ShowScrollPropertyTree(rootFrame);
}

void ShowTransformPropertyTree(const blink::LocalFrameView& rootFrame) {
  LOG(INFO) << "Transform tree:\n"
            << TransformPropertyTreeAsString(rootFrame).Utf8();
}

void ShowClipPropertyTree(const blink::LocalFrameView& rootFrame) {
  LOG(INFO) << "Clip tree:\n" << ClipPropertyTreeAsString(rootFrame).Utf8();
}

void ShowEffectPropertyTree(const blink::LocalFrameView& rootFrame) {
  LOG(INFO) << "Effect tree:\n" << EffectPropertyTreeAsString(rootFrame).Utf8();
}

void ShowScrollPropertyTree(const blink::LocalFrameView& rootFrame) {
  LOG(INFO) << "Scroll tree:\n" << ScrollPropertyTreeAsString(rootFrame).Utf8();
}

String TransformPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::FrameViewPropertyTreePrinter(blink::TransformNodeCollector())
      .TreeAsString(rootFrame);
}

String ClipPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::FrameViewPropertyTreePrinter(blink::ClipNodeCollector())
      .TreeAsString(rootFrame);
}

String EffectPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::FrameViewPropertyTreePrinter(blink::EffectNodeCollector())
      .TreeAsString(rootFrame);
}

String ScrollPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::FrameViewPropertyTreePrinter(blink::ScrollNodeCollector())
      .TreeAsString(rootFrame);
}

#endif  // DCHECK_IS_ON()
