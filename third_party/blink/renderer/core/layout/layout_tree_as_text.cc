/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_value_id_mappings.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/list/list_marker.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

static void PrintBorderStyle(WTF::TextStream& ts,
                             const EBorderStyle border_style) {
  ts << getValueName(PlatformEnumToCSSValueID(border_style)) << " ";
}

static String GetTagName(Node* n) {
  if (n->IsDocumentNode())
    return "";
  if (n->getNodeType() == Node::kCommentNode)
    return "COMMENT";
  if (const auto* element = DynamicTo<Element>(n)) {
    const AtomicString& pseudo = element->ShadowPseudoId();
    if (!pseudo.empty())
      return "::" + pseudo;
  }
  return n->nodeName();
}

String QuoteAndEscapeNonPrintables(const String& s) {
  StringBuilder result;
  result.Append('"');
  for (unsigned i = 0; i != s.length(); ++i) {
    UChar c = s[i];
    if (c == '\\') {
      result.Append('\\');
      result.Append('\\');
    } else if (c == '"') {
      result.Append('\\');
      result.Append('"');
    } else if (c == '\n' || c == kNoBreakSpaceCharacter) {
      result.Append(' ');
    } else {
      if (c >= 0x20 && c < 0x7F) {
        result.Append(c);
      } else {
        result.AppendFormat("\\x{%X}", c);
      }
    }
  }
  result.Append('"');
  return result.ToString();
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const Color& c) {
  return ts << c.NameForLayoutTreeAsText();
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const LayoutPoint& point) {
  return ts << gfx::PointF(point);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const gfx::Point& p) {
  return ts << "(" << p.x() << "," << p.y() << ")";
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const gfx::Size& s) {
  return ts << "width=" << s.width() << " height=" << s.height();
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const gfx::SizeF& s) {
  ts << "width=" << WTF::TextStream::FormatNumberRespectingIntegers(s.width());
  ts << " height="
     << WTF::TextStream::FormatNumberRespectingIntegers(s.height());
  return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const gfx::PointF& p) {
  ts << "(" << WTF::TextStream::FormatNumberRespectingIntegers(p.x());
  ts << "," << WTF::TextStream::FormatNumberRespectingIntegers(p.y());
  ts << ")";
  return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const gfx::RectF& r) {
  ts << "at " << r.origin();
  ts << " size " << WTF::TextStream::FormatNumberRespectingIntegers(r.width());
  ts << "x" << WTF::TextStream::FormatNumberRespectingIntegers(r.height());
  return ts;
}

void LayoutTreeAsText::WriteLayoutObject(WTF::TextStream& ts,
                                         const LayoutObject& o,
                                         LayoutAsTextBehavior behavior) {
  ts << o.DecoratedName();

  if (behavior & kLayoutAsTextShowAddresses)
    ts << " " << static_cast<const void*>(&o);

  if (o.Style() && o.StyleRef().ZIndex())
    ts << " zI: " << o.StyleRef().ZIndex();

  if (o.GetNode()) {
    String tag_name = GetTagName(o.GetNode());
    if (!tag_name.empty())
      ts << " {" << tag_name << "}";
  }

  PhysicalRect rect = o.DebugRect();
  ts << " " << rect;

  if (!(o.IsText() && !o.IsBR())) {
    if (o.Parent()) {
      Color color = o.ResolveColor(GetCSSPropertyColor());
      if (o.Parent()->ResolveColor(GetCSSPropertyColor()) != color)
        ts << " [color=" << color << "]";

      // Do not dump invalid or transparent backgrounds, since that is the
      // default.
      Color background_color = o.ResolveColor(GetCSSPropertyBackgroundColor());
      if (o.Parent()->ResolveColor(GetCSSPropertyBackgroundColor()) !=
              background_color &&
          background_color.Rgb())
        ts << " [bgcolor=" << background_color << "]";

      Color text_fill_color =
          o.ResolveColor(GetCSSPropertyWebkitTextFillColor());
      if (o.Parent()->ResolveColor(GetCSSPropertyWebkitTextFillColor()) !=
              text_fill_color &&
          text_fill_color != color && text_fill_color.Rgb())
        ts << " [textFillColor=" << text_fill_color << "]";

      Color text_stroke_color =
          o.ResolveColor(GetCSSPropertyWebkitTextStrokeColor());
      if (o.Parent()->ResolveColor(GetCSSPropertyWebkitTextStrokeColor()) !=
              text_stroke_color &&
          text_stroke_color != color && text_stroke_color.Rgb())
        ts << " [textStrokeColor=" << text_stroke_color << "]";

      if (o.Parent()->StyleRef().TextStrokeWidth() !=
              o.StyleRef().TextStrokeWidth() &&
          o.StyleRef().TextStrokeWidth() > 0)
        ts << " [textStrokeWidth=" << o.StyleRef().TextStrokeWidth() << "]";
    }

    if (!o.IsBoxModelObject())
      return;

    const auto& box = To<LayoutBoxModelObject>(o);
    if (box.BorderTop() || box.BorderRight() || box.BorderBottom() ||
        box.BorderLeft()) {
      ts << " [border:";

      if (!box.BorderTop()) {
        ts << " none";
      } else {
        ts << " (" << box.BorderTop() << "px ";
        PrintBorderStyle(ts, o.StyleRef().BorderTopStyle());
        ts << o.ResolveColor(GetCSSPropertyBorderTopColor()) << ")";
      }

      if (!box.BorderRight()) {
        ts << " none";
      } else {
        ts << " (" << box.BorderRight() << "px ";
        PrintBorderStyle(ts, o.StyleRef().BorderRightStyle());
        ts << o.ResolveColor(GetCSSPropertyBorderRightColor()) << ")";
      }

      if (!box.BorderBottom()) {
        ts << " none";
      } else {
        ts << " (" << box.BorderBottom() << "px ";
        PrintBorderStyle(ts, o.StyleRef().BorderBottomStyle());
        ts << o.ResolveColor(GetCSSPropertyBorderBottomColor()) << ")";
      }

      if (!box.BorderLeft()) {
        ts << " none";
      } else {
        ts << " (" << box.BorderLeft() << "px ";
        PrintBorderStyle(ts, o.StyleRef().BorderLeftStyle());
        ts << o.ResolveColor(GetCSSPropertyBorderLeftColor()) << ")";
      }

      ts << "]";
    }
  }

  if (o.IsTableCell()) {
    const auto& c = To<LayoutTableCell>(o);
    ts << " [r=" << c.RowIndex() << " c=" << c.AbsoluteColumnIndex()
       << " rs=" << c.ResolvedRowSpan() << " cs=" << c.ColSpan() << "]";
  }

  if (behavior & kLayoutAsTextShowIDAndClass) {
    if (auto* element = DynamicTo<Element>(o.GetNode())) {
      if (element->HasID())
        ts << " id=\"" + element->GetIdAttribute() + "\"";

      if (element->HasClass()) {
        ts << " class=\"";
        for (wtf_size_t i = 0; i < element->ClassNames().size(); ++i) {
          if (i > 0)
            ts << " ";
          ts << element->ClassNames()[i];
        }
        ts << "\"";
      }
    }
  }

  if (behavior & kLayoutAsTextShowLayoutState) {
    bool needs_layout = o.NeedsLayout();
    if (needs_layout)
      ts << " (needs layout:";

    bool have_previous = false;
    if (o.SelfNeedsFullLayout()) {
      ts << " self";
      have_previous = true;
    }

    if (o.ChildNeedsFullLayout()) {
      if (have_previous)
        ts << ",";
      have_previous = true;
      ts << " child";
    }

    if (o.NeedsSimplifiedLayout()) {
      if (have_previous) {
        ts << ",";
      }
      have_previous = true;
      ts << " simplified";
    }

    if (needs_layout)
      ts << ")";
  }

  if (o.ChildLayoutBlockedByDisplayLock())
    ts << " (display-locked)";
}

static void WriteTextFragment(WTF::TextStream& ts,
                              PhysicalRect rect,
                              StringView text,
                              LayoutUnit inline_size) {
  // See WriteTextRun() for why we convert to int.
  int x = rect.offset.left.ToInt();
  int y = rect.offset.top.ToInt();
  int logical_width = (rect.offset.left + inline_size).Ceil() - x;
  ts << "text run at (" << x << "," << y << ") width " << logical_width;
  ts << ": " << QuoteAndEscapeNonPrintables(text.ToString());
  ts << "\n";
}

static void WriteTextFragment(WTF::TextStream& ts, const InlineCursor& cursor) {
  DCHECK(cursor.CurrentItem());
  const FragmentItem& item = *cursor.CurrentItem();
  DCHECK(item.Type() == FragmentItem::kText ||
         item.Type() == FragmentItem::kGeneratedText);
  const LayoutUnit inline_size =
      item.IsHorizontal() ? item.Size().width : item.Size().height;
  WriteTextFragment(ts, item.RectInContainerFragment(),
                    item.Text(cursor.Items()), inline_size);
}

static void WritePaintProperties(WTF::TextStream& ts,
                                 const LayoutObject& o,
                                 int indent) {
  bool has_fragments = o.IsFragmented();
  if (has_fragments) {
    WriteIndent(ts, indent);
    ts << "fragments:\n";
  }
  int fragment_index = 0;
  for (const FragmentData& fragment : FragmentDataIterator(o)) {
    WriteIndent(ts, indent);
    if (has_fragments)
      ts << " " << fragment_index++ << ":";
    ts << " paint_offset=(" << fragment.PaintOffset().ToString() << ")";
    if (fragment.HasLocalBorderBoxProperties()) {
      // To know where they point into the paint property tree, you can dump
      // the tree using ShowAllPropertyTrees(frame_view).
      ts << " state=(" << fragment.LocalBorderBoxProperties().ToString() << ")";
    }
    if (o.HasLayer()) {
      ts << " cull_rect=(" << fragment.GetCullRect().ToString()
         << ") contents_cull_rect=("
         << fragment.GetContentsCullRect().ToString() << ")";
    }
    ts << "\n";
  }
}

void Write(WTF::TextStream& ts,
           const LayoutObject& o,
           int indent,
           LayoutAsTextBehavior behavior) {
  if (o.IsSVGShape()) {
    Write(ts, To<LayoutSVGShape>(o), indent);
    return;
  }
  if (o.IsSVGResourceContainer()) {
    WriteSVGResourceContainer(ts, o, indent);
    return;
  }
  if (o.IsSVGContainer()) {
    WriteSVGContainer(ts, o, indent);
    return;
  }
  if (o.IsSVGRoot()) {
    Write(ts, To<LayoutSVGRoot>(o), indent);
    return;
  }
  if (o.IsSVGInline()) {
    WriteSVGInline(ts, To<LayoutSVGInline>(o), indent);
    return;
  }
  if (o.IsSVGInlineText()) {
    WriteSVGInlineText(ts, To<LayoutSVGInlineText>(o), indent);
    return;
  }
  if (o.IsSVGImage()) {
    WriteSVGImage(ts, To<LayoutSVGImage>(o), indent);
    return;
  }

  WriteIndent(ts, indent);

  LayoutTreeAsText::WriteLayoutObject(ts, o, behavior);
  ts << "\n";

  if (behavior & kLayoutAsTextShowPaintProperties) {
    WritePaintProperties(ts, o, indent + 1);
  }

  if (o.IsText() && !o.IsBR()) {
    const auto& text = To<LayoutText>(o);
    if (const LayoutBlockFlow* block_flow = text.FragmentItemsContainer()) {
      InlineCursor cursor(*block_flow);
      cursor.MoveTo(text);
      for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
        WriteIndent(ts, indent + 1);
        WriteTextFragment(ts, cursor);
      }
    }
  }

  if (!o.ChildLayoutBlockedByDisplayLock()) {
    for (LayoutObject* child = o.SlowFirstChild(); child;
         child = child->NextSibling()) {
      if (child->HasLayer())
        continue;
      Write(ts, *child, indent + 1, behavior);
    }

    if (o.IsLayoutEmbeddedContent()) {
      FrameView* frame_view = To<LayoutEmbeddedContent>(o).ChildFrameView();
      if (auto* local_frame_view = DynamicTo<LocalFrameView>(frame_view)) {
        if (auto* layout_view = local_frame_view->GetLayoutView()) {
          layout_view->GetDocument().UpdateStyleAndLayout(
              DocumentUpdateReason::kTest);
          if (auto* layer = layout_view->Layer()) {
            LayoutTreeAsText::WriteLayers(ts, layer, indent + 1, behavior);
          }
        }
      }
    }
  }
}

enum LayerPaintPhase {
  kLayerPaintPhaseAll = 0,
  kLayerPaintPhaseBackground = -1,
  kLayerPaintPhaseForeground = 1
};

static void Write(WTF::TextStream& ts,
                  PaintLayer& layer,
                  const PhysicalOffset& layer_offset,
                  LayerPaintPhase paint_phase = kLayerPaintPhaseAll,
                  int indent = 0,
                  LayoutAsTextBehavior behavior = kLayoutAsTextBehaviorNormal,
                  const PaintLayer* marked_layer = nullptr) {
  gfx::Point adjusted_layer_offset = ToRoundedPoint(layer_offset);

  if (marked_layer)
    ts << (marked_layer == &layer ? "*" : " ");

  WriteIndent(ts, indent);

  if (layer.GetLayoutObject().StyleRef().UsedVisibility() ==
      EVisibility::kHidden) {
    ts << "hidden ";
  }

  ts << "layer ";

  if (behavior & kLayoutAsTextShowAddresses)
    ts << static_cast<const void*>(&layer) << " ";

  ts << "at " << adjusted_layer_offset;

  if (layer.Transform())
    ts << " hasTransform";
  if (layer.IsTransparent())
    ts << " transparent";

  if (layer.GetLayoutObject().IsScrollContainer()) {
    gfx::PointF scroll_position = layer.GetScrollableArea()->ScrollPosition();
    if (scroll_position.x())
      ts << " scrollX " << scroll_position.x();
    if (scroll_position.y())
      ts << " scrollY " << scroll_position.y();
    if (layer.GetLayoutBox() && layer.GetLayoutBox()->ClientWidth() !=
                                    layer.GetLayoutBox()->ScrollWidth()) {
      ts << " scrollWidth " << layer.GetLayoutBox()->ScrollWidth();
    }
    if (layer.GetLayoutBox() && layer.GetLayoutBox()->ClientHeight() !=
                                    layer.GetLayoutBox()->ScrollHeight()) {
      ts << " scrollHeight " << layer.GetLayoutBox()->ScrollHeight();
    }
  }

  if (paint_phase == kLayerPaintPhaseBackground)
    ts << " layerType: background only";
  else if (paint_phase == kLayerPaintPhaseForeground)
    ts << " layerType: foreground only";

  if (layer.GetLayoutObject().StyleRef().HasBlendMode()) {
    ts << " blendMode: "
       << BlendModeToString(layer.GetLayoutObject().StyleRef().GetBlendMode());
  }

  if (behavior & kLayoutAsTextShowPaintProperties) {
    if (layer.SelfOrDescendantNeedsRepaint())
      ts << " needsRepaint";
    if (layer.NeedsCullRectUpdate())
      ts << " needsCullRectUpdate";
    if (layer.DescendantNeedsCullRectUpdate())
      ts << " descendantNeedsCullRectUpdate";
  }

  ts << "\n";

  if (paint_phase != kLayerPaintPhaseBackground)
    Write(ts, layer.GetLayoutObject(), indent + 1, behavior);
}

static HeapVector<Member<PaintLayer>> ChildLayers(
    const PaintLayer* layer,
    PaintLayerIteration which_children) {
  HeapVector<Member<PaintLayer>> vector;
  PaintLayerPaintOrderIterator it(layer, which_children);
  while (PaintLayer* child = it.Next())
    vector.push_back(child);
  return vector;
}

void LayoutTreeAsText::WriteLayers(WTF::TextStream& ts,
                                   PaintLayer* layer,
                                   int indent,
                                   LayoutAsTextBehavior behavior,
                                   const PaintLayer* marked_layer) {
  const LayoutObject& layer_object = layer->GetLayoutObject();
  PhysicalOffset layer_offset =
      layer_object.LocalToAbsolutePoint(PhysicalOffset());

  bool should_dump = true;
  auto* embedded = DynamicTo<LayoutEmbeddedContent>(layer_object);
  if (embedded && embedded->IsThrottledFrameView())
    should_dump = false;

  bool should_dump_children = !layer_object.ChildLayoutBlockedByDisplayLock();

  const auto& neg_list = ChildLayers(layer, kNegativeZOrderChildren);
  bool paints_background_separately = !neg_list.empty();
  if (should_dump && paints_background_separately) {
    Write(ts, *layer, layer_offset, kLayerPaintPhaseBackground, indent,
          behavior, marked_layer);
  }

  if (should_dump_children && !neg_list.empty()) {
    int curr_indent = indent;
    if (behavior & kLayoutAsTextShowLayerNesting) {
      WriteIndent(ts, indent);
      ts << " negative z-order list(" << neg_list.size() << ")\n";
      ++curr_indent;
    }
    for (auto& child_layer : neg_list) {
      WriteLayers(ts, child_layer, curr_indent, behavior, marked_layer);
    }
  }

  if (should_dump) {
    Write(ts, *layer, layer_offset,
          paints_background_separately ? kLayerPaintPhaseForeground
                                       : kLayerPaintPhaseAll,
          indent, behavior, marked_layer);
  }

  const auto& normal_flow_list = ChildLayers(layer, kNormalFlowChildren);
  if (should_dump_children && !normal_flow_list.empty()) {
    int curr_indent = indent;
    if (behavior & kLayoutAsTextShowLayerNesting) {
      WriteIndent(ts, indent);
      ts << " normal flow list(" << normal_flow_list.size() << ")\n";
      ++curr_indent;
    }
    for (auto& child_layer : normal_flow_list) {
      WriteLayers(ts, child_layer, curr_indent, behavior, marked_layer);
    }
  }

  const auto& pos_list = ChildLayers(layer, kPositiveZOrderChildren);
  if (should_dump_children && !pos_list.empty()) {
    int curr_indent = indent;
    if (behavior & kLayoutAsTextShowLayerNesting) {
      WriteIndent(ts, indent);
      ts << " positive z-order list(" << pos_list.size() << ")\n";
      ++curr_indent;
    }
    for (auto& child_layer : pos_list) {
      WriteLayers(ts, child_layer, curr_indent, behavior, marked_layer);
    }
  }
}

static String NodePosition(Node* node) {
  StringBuilder result;

  Element* body = node->GetDocument().body();
  Node* parent;
  for (Node* n = node; n; n = parent) {
    parent = n->ParentOrShadowHostNode();
    if (n != node)
      result.Append(" of ");
    if (parent) {
      if (body && n == body) {
        // We don't care what offset body may be in the document.
        result.Append("body");
        break;
      }
      if (n->IsShadowRoot()) {
        result.Append('{');
        result.Append(GetTagName(n));
        result.Append('}');
      } else {
        result.Append("child ");
        result.AppendNumber(n->NodeIndex());
        result.Append(" {");
        result.Append(GetTagName(n));
        result.Append('}');
      }
    } else {
      result.Append("document");
    }
  }

  return result.ToString();
}

static void WriteSelection(WTF::TextStream& ts, const LayoutObject* o) {
  Document* doc = DynamicTo<Document>(o->GetNode());
  if (!doc)
    return;

  LocalFrame* frame = doc->GetFrame();
  if (!frame)
    return;

  const VisibleSelection& selection =
      frame->Selection().ComputeVisibleSelectionInDOMTree();
  if (selection.IsCaret()) {
    ts << "caret: position " << selection.Start().ComputeEditingOffset()
       << " of " << NodePosition(selection.Start().AnchorNode());
    if (selection.Affinity() == TextAffinity::kUpstream)
      ts << " (upstream affinity)";
    ts << "\n";
  } else if (selection.IsRange()) {
    ts << "selection start: position "
       << selection.Start().ComputeEditingOffset() << " of "
       << NodePosition(selection.Start().AnchorNode()) << "\n"
       << "selection end:   position " << selection.End().ComputeEditingOffset()
       << " of " << NodePosition(selection.End().AnchorNode()) << "\n";
  }
}

static String ExternalRepresentation(LayoutBox* layout_object,
                                     LayoutAsTextBehavior behavior,
                                     const PaintLayer* marked_layer = nullptr) {
  WTF::TextStream ts;
  if (!layout_object->HasLayer())
    return ts.Release();

  PaintLayer* layer = layout_object->Layer();
  LayoutTreeAsText::WriteLayers(ts, layer, 0, behavior, marked_layer);
  WriteSelection(ts, layout_object);
  return ts.Release();
}

String ExternalRepresentation(LocalFrame* frame,
                              LayoutAsTextBehavior behavior,
                              const PaintLayer* marked_layer) {
  if (!(behavior & kLayoutAsTextDontUpdateLayout)) {
    bool success = frame->View()->UpdateAllLifecyclePhasesExceptPaint(
        DocumentUpdateReason::kTest);
    DCHECK(success);
  };

  LayoutObject* layout_object = frame->ContentLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return String();
  auto* layout_box = To<LayoutBox>(layout_object);

  PrintContext* print_context = MakeGarbageCollected<PrintContext>(frame);
  bool is_text_printing_mode = !!(behavior & kLayoutAsTextPrintingMode);
  if (is_text_printing_mode) {
    gfx::SizeF page_size(layout_box->ClientWidth(), layout_box->ClientHeight());
    print_context->BeginPrintMode(WebPrintParams(page_size));

    // The lifecycle needs to be run again after changing printing mode,
    // to account for any style updates due to media query change.
    if (!(behavior & kLayoutAsTextDontUpdateLayout))
      frame->View()->UpdateLifecyclePhasesForPrinting();
  }

  String representation =
      ExternalRepresentation(layout_box, behavior, marked_layer);
  if (is_text_printing_mode)
    print_context->EndPrintMode();
  return representation;
}

String ExternalRepresentation(Element* element, LayoutAsTextBehavior behavior) {
  // Doesn't support printing mode.
  DCHECK(!(behavior & kLayoutAsTextPrintingMode));
  if (!(behavior & kLayoutAsTextDontUpdateLayout)) {
    element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  }

  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return String();

  return ExternalRepresentation(To<LayoutBox>(layout_object), behavior);
}

static void WriteCounterValuesFromChildren(WTF::TextStream& stream,
                                           LayoutObject* parent,
                                           bool& is_first_counter) {
  for (LayoutObject* child = parent->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsCounter()) {
      if (!is_first_counter)
        stream << " ";
      is_first_counter = false;
      stream << To<LayoutText>(child)->TransformedText();
    }
  }
}

String CounterValueForElement(Element* element) {
  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  WTF::TextStream stream;
  bool is_first_counter = true;
  // The counter LayoutObjects should be children of ::marker, ::before or
  // ::after pseudo-elements.
  if (LayoutObject* marker =
          element->PseudoElementLayoutObject(kPseudoIdMarker))
    WriteCounterValuesFromChildren(stream, marker, is_first_counter);
  if (LayoutObject* before =
          element->PseudoElementLayoutObject(kPseudoIdBefore))
    WriteCounterValuesFromChildren(stream, before, is_first_counter);
  if (LayoutObject* after = element->PseudoElementLayoutObject(kPseudoIdAfter))
    WriteCounterValuesFromChildren(stream, after, is_first_counter);
  return stream.Release();
}

String MarkerTextForListItem(Element* element) {
  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  LayoutObject* layout_object = element->GetLayoutObject();
  LayoutObject* marker = ListMarker::MarkerFromListItem(layout_object);
  if (ListMarker* list_marker = ListMarker::Get(marker))
    return list_marker->MarkerTextWithoutSuffix(*marker);
  return String();
}

}  // namespace blink
