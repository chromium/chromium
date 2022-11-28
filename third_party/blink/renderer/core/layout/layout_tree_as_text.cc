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
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_tree_as_text.h"
#include "third_party/blink/renderer/core/page/print_context.h"
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

WTF::TextStream& operator<<(WTF::TextStream& ts, const gfx::Rect& r) {
  return ts << "at " << r.origin() << " size " << r.width() << "x"
            << r.height();
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
    if (o.IsFileUploadControl()) {
      ts << " "
         << QuoteAndEscapeNonPrintables(
                To<LayoutFileUploadControl>(o).FileTextValue());
    }

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

      BorderValue prev_border = o.StyleRef().BorderTop();
      if (!box.BorderTop()) {
        ts << " none";
      } else {
        ts << " (" << box.BorderTop() << "px ";
        PrintBorderStyle(ts, o.StyleRef().BorderTopStyle());
        ts << o.ResolveColor(GetCSSPropertyBorderTopColor()) << ")";
      }

      if (!o.StyleRef().BorderRightEquals(prev_border)) {
        prev_border = o.StyleRef().BorderRight();
        if (!box.BorderRight()) {
          ts << " none";
        } else {
          ts << " (" << box.BorderRight() << "px ";
          PrintBorderStyle(ts, o.StyleRef().BorderRightStyle());
          ts << o.ResolveColor(GetCSSPropertyBorderRightColor()) << ")";
        }
      }

      if (!o.StyleRef().BorderBottomEquals(prev_border)) {
        prev_border = box.StyleRef().BorderBottom();
        if (!box.BorderBottom()) {
          ts << " none";
        } else {
          ts << " (" << box.BorderBottom() << "px ";
          PrintBorderStyle(ts, o.StyleRef().BorderBottomStyle());
          ts << o.ResolveColor(GetCSSPropertyBorderBottomColor()) << ")";
        }
      }

      if (!o.StyleRef().BorderLeftEquals(prev_border)) {
        prev_border = o.StyleRef().BorderLeft();
        if (!box.BorderLeft()) {
          ts << " none";
        } else {
          ts << " (" << box.BorderLeft() << "px ";
          PrintBorderStyle(ts, o.StyleRef().BorderLeftStyle());
          ts << o.ResolveColor(GetCSSPropertyBorderLeftColor()) << ")";
        }
      }

      ts << "]";
    }
  }

  if (o.IsTableCell()) {
    const LayoutNGTableCellInterface& c =
        ToInterface<LayoutNGTableCellInterface>(o);
    ts << " [r=" << c.RowIndex() << " c=" << c.AbsoluteColumnIndex()
       << " rs=" << c.ResolvedRowSpan() << " cs=" << c.ColSpan() << "]";
  }

  if (o.IsListMarkerForNormalContent()) {
    String text = To<LayoutListMarker>(o).GetText();
    if (!text.empty()) {
      if (text.length() != 1) {
        text = QuoteAndEscapeNonPrintables(text);
      } else {
        switch (text[0]) {
          case kBulletCharacter:
            text = "bullet";
            break;
          case kBlackSquareCharacter:
            text = "black square";
            break;
          case kWhiteBulletCharacter:
            text = "white bullet";
            break;
          default:
            text = QuoteAndEscapeNonPrintables(text);
        }
      }
      ts << ": " << text;
    }
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
    bool needs_layout = o.SelfNeedsLayout() ||
                        o.NeedsPositionedMovementLayout() ||
                        o.PosChildNeedsLayout() || o.NormalChildNeedsLayout();
    if (needs_layout)
      ts << " (needs layout:";

    bool have_previous = false;
    if (o.SelfNeedsLayout()) {
      ts << " self";
      have_previous = true;
    }

    if (o.NeedsPositionedMovementLayout()) {
      if (have_previous)
        ts << ",";
      have_previous = true;
      ts << " positioned movement";
    }

    if (o.NormalChildNeedsLayout()) {
      if (have_previous)
        ts << ",";
      have_previous = true;
      ts << " child";
    }

    if (o.PosChildNeedsLayout()) {
      if (have_previous)
        ts << ",";
      ts << " positioned child";
    }

    if (needs_layout)
      ts << ")";
  }

  if (o.ChildLayoutBlockedByDisplayLock())
    ts << " (display-locked)";
}

static void WriteInlineBox(WTF::TextStream& ts,
                           const InlineBox& box,
                           int indent) {
  WriteIndent(ts, indent);
  ts << "+ ";
  ts << box.BoxName() << " {" << box.GetLineLayoutItem().DebugName() << "}"
     << " pos=(" << box.X() << "," << box.Y() << ")"
     << " size=(" << box.Width() << "," << box.Height() << ")"
     << " baseline=" << box.BaselinePosition(kAlphabeticBaseline) << "/"
     << box.BaselinePosition(kCentralBaseline);
}

static void WriteInlineTextBox(WTF::TextStream& ts,
                               const InlineTextBox& text_box,
                               int indent) {
  WriteInlineBox(ts, text_box, indent);
  String value = text_box.GetText();
  value.Replace('\\', "\\\\");
  value.Replace('\n', "\\n");
  value.Replace('"', "\\\"");
  ts << " range=(" << text_box.Start() << ","
     << (text_box.Start() + text_box.Len()) << ")"
     << " \"" << value << "\"";
}

static void WriteInlineFlowBox(WTF::TextStream& ts,
                               const InlineFlowBox& root_box,
                               int indent) {
  WriteInlineBox(ts, root_box, indent);
  ts << "\n";
  for (const InlineBox* box = root_box.FirstChild(); box;
       box = box->NextOnLine()) {
    if (box->IsInlineFlowBox()) {
      WriteInlineFlowBox(ts, static_cast<const InlineFlowBox&>(*box),
                         indent + 1);
      continue;
    }
    if (box->IsInlineTextBox())
      WriteInlineTextBox(ts, static_cast<const InlineTextBox&>(*box),
                         indent + 1);
    else
      WriteInlineBox(ts, *box, indent + 1);
    ts << "\n";
  }
}

void LayoutTreeAsText::WriteLineBoxTree(WTF::TextStream& ts,
                                        const LayoutBlockFlow& o,
                                        int indent) {
  for (const InlineFlowBox* root_box : o.LineBoxes()) {
    WriteInlineFlowBox(ts, *root_box, indent);
  }
}

static void WriteTextRun(WTF::TextStream& ts,
                         const LayoutText& o,
                         const InlineTextBox& run) {
  // FIXME: For now use an "enclosingIntRect" model for x, y and logicalWidth,
  // although this makes it harder to detect any changes caused by the
  // conversion to floating point. :(
  int x = run.X().ToInt();
  int y = run.Y().ToInt();
  int logical_width = (run.X() + run.LogicalWidth()).Ceil() - x;

  // FIXME: Table cell adjustment is temporary until results can be updated.
  if (o.ContainingBlock()->IsTableCellLegacy())
    y -= To<LayoutTableCell>(o.ContainingBlock())->IntrinsicPaddingBefore();

  ts << "text run at (" << x << "," << y << ") width " << logical_width;
  if (!run.IsLeftToRightDirection() || run.DirOverride()) {
    ts << (!run.IsLeftToRightDirection() ? " RTL" : " LTR");
    if (run.DirOverride())
      ts << " override";
  }
  ts << ": "
     << QuoteAndEscapeNonPrintables(
            String(o.GetText()).Substring(run.Start(), run.Len()));
  if (run.HasHyphen()) {
    ts << " + hyphen string "
       << QuoteAndEscapeNonPrintables(o.StyleRef().HyphenString());
  }
  ts << "\n";
}

static void WriteTextFragment(WTF::TextStream& ts,
                              const LayoutObject* layout_object,
                              PhysicalRect rect,
                              const ComputedStyle& style,
                              StringView text,
                              LayoutUnit inline_size) {
  // TODO(layout-dev): Dump physical coordinates when removing the legacy inline
  // layout code.
  PhysicalOffset offset_to_container_box = rect.offset;
  if (UNLIKELY(style.IsFlippedBlocksWritingMode())) {
    if (layout_object) {
      const LayoutBlock* containing_block = layout_object->ContainingBlock();
      LayoutRect layout_rect = containing_block->FlipForWritingMode(rect);
      offset_to_container_box.left = layout_rect.X();
    }
  }

  // See WriteTextRun() for why we convert to int.
  int x = offset_to_container_box.left.ToInt();
  int y = offset_to_container_box.top.ToInt();
  int logical_width = (offset_to_container_box.left + inline_size).Ceil() - x;
  ts << "text run at (" << x << "," << y << ") width " << logical_width;
  ts << ": " << QuoteAndEscapeNonPrintables(text.ToString());
  ts << "\n";
}

static void WriteTextFragment(WTF::TextStream& ts,
                              const NGInlineCursor& cursor) {
  DCHECK(cursor.CurrentItem());
  const NGFragmentItem& item = *cursor.CurrentItem();
  DCHECK(item.Type() == NGFragmentItem::kText ||
         item.Type() == NGFragmentItem::kGeneratedText);
  const LayoutUnit inline_size =
      item.IsHorizontal() ? item.Size().width : item.Size().height;
  WriteTextFragment(ts, item.GetLayoutObject(), item.RectInContainerFragment(),
                    item.Style(), item.Text(cursor.Items()), inline_size);
}

static void WritePaintProperties(WTF::TextStream& ts,
                                 const LayoutObject& o,
                                 int indent) {
  bool has_fragments = o.FirstFragment().NextFragment();
  if (has_fragments) {
    WriteIndent(ts, indent);
    ts << "fragments:\n";
  }
  int fragment_index = 0;
  for (const auto* fragment = &o.FirstFragment(); fragment;
       fragment = fragment->NextFragment(), ++fragment_index) {
    WriteIndent(ts, indent);
    if (has_fragments)
      ts << " " << fragment_index << ":";
    ts << " paint_offset=(" << fragment->PaintOffset().ToString() << ")";
    if (fragment->HasLocalBorderBoxProperties()) {
      // To know where they point into the paint property tree, you can dump
      // the tree using ShowAllPropertyTrees(frame_view).
      ts << " state=(" << fragment->LocalBorderBoxProperties().ToString()
         << ")";
    }
    if (o.HasLayer()) {
      ts << " cull_rect=(" << fragment->GetCullRect().ToString()
         << ") contents_cull_rect=("
         << fragment->GetContentsCullRect().ToString() << ")";
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
  if (o.IsSVGText()) {
    WriteSVGText(ts, To<LayoutSVGText>(o), indent);
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

  auto* layout_block_flow = DynamicTo<LayoutBlockFlow>(o);
  if ((behavior & kLayoutAsTextShowLineTrees) && layout_block_flow) {
    LayoutTreeAsText::WriteLineBoxTree(ts, *layout_block_flow, indent + 1);
  }

  if (o.IsText() && !o.IsBR()) {
    const auto& text = To<LayoutText>(o);
    if (const LayoutBlockFlow* block_flow = text.FragmentItemsContainer()) {
      NGInlineCursor cursor(*block_flow);
      cursor.MoveTo(text);
      for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
        WriteIndent(ts, indent + 1);
        WriteTextFragment(ts, cursor);
      }
    } else {
      for (InlineTextBox* box : text.TextBoxes()) {
        WriteIndent(ts, indent + 1);
        WriteTextRun(ts, text, *box);
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
            LayoutTreeAsText::WriteLayers(ts, layer, layer, indent + 1,
                                          behavior);
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
                  const PhysicalRect& layer_bounds,
                  const PhysicalRect& background_clip_rect,
                  const PhysicalRect& clip_rect,
                  LayerPaintPhase paint_phase = kLayerPaintPhaseAll,
                  int indent = 0,
                  LayoutAsTextBehavior behavior = kLayoutAsTextBehaviorNormal,
                  const PaintLayer* marked_layer = nullptr) {
  gfx::Rect adjusted_layout_bounds = ToPixelSnappedRect(layer_bounds);
  gfx::Rect adjusted_background_clip_rect =
      ToPixelSnappedRect(background_clip_rect);
  gfx::Rect adjusted_clip_rect = ToPixelSnappedRect(clip_rect);

  if (marked_layer)
    ts << (marked_layer == &layer ? "*" : " ");

  WriteIndent(ts, indent);

  if (layer.GetLayoutObject().StyleRef().Visibility() == EVisibility::kHidden)
    ts << "hidden ";

  ts << "layer ";

  if (behavior & kLayoutAsTextShowAddresses)
    ts << static_cast<const void*>(&layer) << " ";

  ts << adjusted_layout_bounds;

  if (!adjusted_layout_bounds.IsEmpty()) {
    if (!adjusted_background_clip_rect.Contains(adjusted_layout_bounds))
      ts << " backgroundClip " << adjusted_background_clip_rect;
    if (!adjusted_clip_rect.Contains(adjusted_layout_bounds))
      ts << " clip " << adjusted_clip_rect;
  }
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
    if (layer.GetLayoutBox() &&
        layer.GetLayoutBox()->PixelSnappedClientWidth() !=
            layer.GetLayoutBox()->PixelSnappedScrollWidth())
      ts << " scrollWidth " << layer.GetLayoutBox()->PixelSnappedScrollWidth();
    if (layer.GetLayoutBox() &&
        layer.GetLayoutBox()->PixelSnappedClientHeight() !=
            layer.GetLayoutBox()->PixelSnappedScrollHeight())
      ts << " scrollHeight "
         << layer.GetLayoutBox()->PixelSnappedScrollHeight();
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
                                   const PaintLayer* root_layer_arg,
                                   PaintLayer* layer,
                                   int indent,
                                   LayoutAsTextBehavior behavior,
                                   const PaintLayer* marked_layer) {
  // Calculate the clip rects we should use.
  const PaintLayer* root_layer = layer->Transform() ? layer : root_layer_arg;
  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;
  if (layer->GetLayoutObject().FirstFragment().HasLocalBorderBoxProperties()) {
    layer->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
        .CalculateRects(
            ClipRectsContext(root_layer,
                             &root_layer->GetLayoutObject().FirstFragment()),
            &layer->GetLayoutObject().FirstFragment(), layer_offset,
            background_rect, foreground_rect);
  } else {
    layer->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
        .CalculateRects(ClipRectsContext(root_layer, nullptr), nullptr,
                        layer_offset, background_rect, foreground_rect);
  }

  bool should_dump = true;
  auto* embedded = DynamicTo<LayoutEmbeddedContent>(layer->GetLayoutObject());
  if (embedded && embedded->IsThrottledFrameView())
    should_dump = false;

#if DCHECK_IS_ON()
  if (layer->NeedsPositionUpdate()) {
    WriteIndent(ts, indent);
    ts << " NEEDS POSITION UPDATE\n";
  }
#endif

  bool should_dump_children =
      !layer->GetLayoutObject().ChildLayoutBlockedByDisplayLock();

  const auto& neg_list = ChildLayers(layer, kNegativeZOrderChildren);
  PhysicalRect layer_bounds(layer_offset, layer->Size());
  bool paints_background_separately = !neg_list.empty();
  if (should_dump && paints_background_separately) {
    Write(ts, *layer, layer_bounds, background_rect.Rect(),
          foreground_rect.Rect(), kLayerPaintPhaseBackground, indent, behavior,
          marked_layer);
  }

  if (should_dump_children && !neg_list.empty()) {
    int curr_indent = indent;
    if (behavior & kLayoutAsTextShowLayerNesting) {
      WriteIndent(ts, indent);
      ts << " negative z-order list(" << neg_list.size() << ")\n";
      ++curr_indent;
    }
    for (auto& child_layer : neg_list) {
      WriteLayers(ts, root_layer, child_layer, curr_indent, behavior,
                  marked_layer);
    }
  }

  if (should_dump) {
    Write(ts, *layer, layer_bounds, background_rect.Rect(),
          foreground_rect.Rect(),
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
      WriteLayers(ts, root_layer, child_layer, curr_indent, behavior,
                  marked_layer);
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
      WriteLayers(ts, root_layer, child_layer, curr_indent, behavior,
                  marked_layer);
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
  LayoutTreeAsText::WriteLayers(ts, layer, layer, 0, behavior, marked_layer);
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

  PrintContext print_context(frame, /*use_printing_layout=*/true);
  bool is_text_printing_mode = !!(behavior & kLayoutAsTextPrintingMode);
  if (is_text_printing_mode) {
    print_context.BeginPrintMode(layout_box->ClientWidth(),
                                 layout_box->ClientHeight());

    // The lifecycle needs to be run again after changing printing mode,
    // to account for any style updates due to media query change.
    if (!(behavior & kLayoutAsTextDontUpdateLayout))
      frame->View()->UpdateLifecyclePhasesForPrinting();
  }

  String representation =
      ExternalRepresentation(layout_box, behavior, marked_layer);
  if (is_text_printing_mode)
    print_context.EndPrintMode();
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
      String str(To<LayoutText>(child)->GetText());
      stream << str;
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
  if (marker && marker->IsListMarkerForNormalContent())
    return To<LayoutListMarker>(marker)->GetText();
  return String();
}

}  // namespace blink
