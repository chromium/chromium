// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"

#include <iomanip>
#include <sstream>

#if DCHECK_IS_ON()

namespace blink {
namespace {

template <typename PropertyTreeNode>
class PropertyTreePrinterTraits;

template <typename PropertyTreeNode>
class FrameViewPropertyTreePrinter
    : public PropertyTreePrinter<PropertyTreeNode> {
 public:
  String TreeAsString(const LocalFrameView& frame_view) {
    CollectNodes(frame_view);
    return PropertyTreePrinter<PropertyTreeNode>::NodesAsTreeString();
  }

 private:
  using Traits = PropertyTreePrinterTraits<PropertyTreeNode>;

  void CollectNodes(const LocalFrameView& frame_view) {
    Traits::AddVisualViewportProperties(
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
  }

  void CollectNodes(const LayoutObject& object) {
    for (const auto* fragment = &object.FirstFragment(); fragment;
         fragment = fragment->NextFragment()) {
      if (const auto* properties = fragment->PaintProperties())
        Traits::AddObjectPaintProperties(*properties, *this);
    }
    for (const auto* child = object.SlowFirstChild(); child;
         child = child->NextSibling()) {
      CollectNodes(*child);
    }
  }
};

template <>
class PropertyTreePrinterTraits<TransformPaintPropertyNodeOrAlias> {
 public:
  static void AddVisualViewportProperties(
      const VisualViewport& visual_viewport,
      PropertyTreePrinter<TransformPaintPropertyNodeOrAlias>& printer) {
    printer.AddNode(visual_viewport.GetDeviceEmulationTransformNode());
    printer.AddNode(visual_viewport.GetOverscrollElasticityTransformNode());
    printer.AddNode(visual_viewport.GetPageScaleNode());
    printer.AddNode(visual_viewport.GetScrollTranslationNode());
  }
  static void AddObjectPaintProperties(
      const ObjectPaintProperties& properties,
      PropertyTreePrinter<TransformPaintPropertyNodeOrAlias>& printer) {
    printer.AddNode(properties.PaintOffsetTranslation());
    printer.AddNode(properties.StickyTranslation());
    printer.AddNode(properties.Transform());
    printer.AddNode(properties.Perspective());
    printer.AddNode(properties.ReplacedContentTransform());
    printer.AddNode(properties.ScrollTranslation());
    printer.AddNode(properties.TransformIsolationNode());
  }
};

template <>
class PropertyTreePrinterTraits<ClipPaintPropertyNodeOrAlias> {
 public:
  static void AddVisualViewportProperties(
      const VisualViewport& visual_viewport,
      PropertyTreePrinter<ClipPaintPropertyNodeOrAlias>& printer) {}
  static void AddObjectPaintProperties(
      const ObjectPaintProperties& properties,
      PropertyTreePrinter<ClipPaintPropertyNodeOrAlias>& printer) {
    printer.AddNode(properties.FragmentClip());
    printer.AddNode(properties.ClipPathClip());
    printer.AddNode(properties.MaskClip());
    printer.AddNode(properties.CssClip());
    printer.AddNode(properties.CssClipFixedPosition());
    printer.AddNode(properties.OverflowControlsClip());
    printer.AddNode(properties.InnerBorderRadiusClip());
    printer.AddNode(properties.OverflowClip());
    printer.AddNode(properties.ClipIsolationNode());
  }
};

template <>
class PropertyTreePrinterTraits<EffectPaintPropertyNodeOrAlias> {
 public:
  static void AddVisualViewportProperties(
      const VisualViewport& visual_viewport,
      PropertyTreePrinter<EffectPaintPropertyNodeOrAlias>& printer) {
    printer.AddNode(visual_viewport.GetOverscrollElasticityEffectNode());
  }

  static void AddObjectPaintProperties(
      const ObjectPaintProperties& properties,
      PropertyTreePrinter<EffectPaintPropertyNodeOrAlias>& printer) {
    printer.AddNode(properties.Effect());
    printer.AddNode(properties.Filter());
    printer.AddNode(properties.VerticalScrollbarEffect());
    printer.AddNode(properties.HorizontalScrollbarEffect());
    printer.AddNode(properties.Mask());
    printer.AddNode(properties.ClipPathMask());
    printer.AddNode(properties.EffectIsolationNode());
  }
};

template <>
class PropertyTreePrinterTraits<ScrollPaintPropertyNode> {
 public:
  static void AddVisualViewportProperties(
      const VisualViewport& visual_viewport,
      PropertyTreePrinter<ScrollPaintPropertyNode>& printer) {
    printer.AddNode(visual_viewport.GetScrollNode());
  }

  static void AddObjectPaintProperties(
      const ObjectPaintProperties& properties,
      PropertyTreePrinter<ScrollPaintPropertyNode>& printer) {
    printer.AddNode(properties.Scroll());
  }
};

template <typename PropertyTreeNode>
void SetDebugName(const PropertyTreeNode* node, const String& debug_name) {
  if (node)
    const_cast<PropertyTreeNode*>(node)->SetDebugName(debug_name);
}

template <typename PropertyTreeNode>
void SetDebugName(const PropertyTreeNode* node,
                  const String& name,
                  const LayoutObject& object) {
  if (node)
    SetDebugName(node, name + " (" + object.DebugName() + ")");
}

}  // namespace

namespace paint_property_tree_printer {

void UpdateDebugNames(const VisualViewport& viewport) {
  if (auto* device_emulation_node = viewport.GetDeviceEmulationTransformNode())
    device_emulation_node->SetDebugName("Device Emulation Node");
  if (auto* overscroll_effect_node =
          viewport.GetOverscrollElasticityEffectNode()) {
    overscroll_effect_node->SetDebugName("Overscroll Elasticity Effect Node");
  }
  if (auto* overscroll_node = viewport.GetOverscrollElasticityTransformNode())
    overscroll_node->SetDebugName("Overscroll Elasticity Node");
  viewport.GetPageScaleNode()->SetDebugName("VisualViewport Scale Node");
  viewport.GetScrollTranslationNode()->SetDebugName(
      "VisualViewport Translate Node");
  viewport.GetScrollNode()->SetDebugName("VisualViewport Scroll Node");
}

void UpdateDebugNames(const LayoutObject& object,
                      ObjectPaintProperties& properties) {
  SetDebugName(properties.PaintOffsetTranslation(), "PaintOffsetTranslation",
               object);
  SetDebugName(properties.StickyTranslation(), "StickyTranslation", object);
  SetDebugName(properties.Transform(), "Transform", object);
  SetDebugName(properties.Perspective(), "Perspective", object);
  SetDebugName(properties.ReplacedContentTransform(),
               "ReplacedContentTransform", object);
  SetDebugName(properties.ScrollTranslation(), "ScrollTranslation", object);
  SetDebugName(properties.TransformIsolationNode(), "TransformIsolationNode",
               object);

  SetDebugName(properties.FragmentClip(), "FragmentClip", object);
  SetDebugName(properties.ClipPathClip(), "ClipPathClip", object);
  SetDebugName(properties.MaskClip(), "MaskClip", object);
  SetDebugName(properties.CssClip(), "CssClip", object);
  SetDebugName(properties.CssClipFixedPosition(), "CssClipFixedPosition",
               object);
  SetDebugName(properties.OverflowControlsClip(), "OverflowControlsClip",
               object);
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
  SetDebugName(properties.Mask(), "Mask", object);
  SetDebugName(properties.ClipPathMask(), "ClipPathMask", object);
  SetDebugName(properties.EffectIsolationNode(), "EffectIsolationNode", object);

  SetDebugName(properties.Scroll(), "Scroll", object);
}

}  // namespace paint_property_tree_printer

}  // namespace blink

CORE_EXPORT void showAllPropertyTrees(const blink::LocalFrameView& rootFrame) {
  showTransformPropertyTree(rootFrame);
  showClipPropertyTree(rootFrame);
  showEffectPropertyTree(rootFrame);
  showScrollPropertyTree(rootFrame);
}

void showTransformPropertyTree(const blink::LocalFrameView& rootFrame) {
  LOG(INFO) << "Transform tree:\n"
            << transformPropertyTreeAsString(rootFrame).Utf8();
}

void showClipPropertyTree(const blink::LocalFrameView& rootFrame) {
  LOG(INFO) << "Clip tree:\n" << clipPropertyTreeAsString(rootFrame).Utf8();
}

void showEffectPropertyTree(const blink::LocalFrameView& rootFrame) {
  LOG(INFO) << "Effect tree:\n" << effectPropertyTreeAsString(rootFrame).Utf8();
}

void showScrollPropertyTree(const blink::LocalFrameView& rootFrame) {
  LOG(INFO) << "Scroll tree:\n" << scrollPropertyTreeAsString(rootFrame).Utf8();
}

String transformPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::FrameViewPropertyTreePrinter<
             blink::TransformPaintPropertyNodeOrAlias>()
      .TreeAsString(rootFrame);
}

String clipPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::FrameViewPropertyTreePrinter<
             blink::ClipPaintPropertyNodeOrAlias>()
      .TreeAsString(rootFrame);
}

String effectPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::FrameViewPropertyTreePrinter<
             blink::EffectPaintPropertyNodeOrAlias>()
      .TreeAsString(rootFrame);
}

String scrollPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::FrameViewPropertyTreePrinter<blink::ScrollPaintPropertyNode>()
      .TreeAsString(rootFrame);
}

#endif  // DCHECK_IS_ON()
