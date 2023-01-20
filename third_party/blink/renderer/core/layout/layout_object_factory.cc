// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_object_factory.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_button.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_deprecated_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_frame_set.h"
#include "third_party/blink/renderer/core/layout/layout_grid.h"
#include "third_party/blink/renderer/core/layout/layout_inside_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_slider_track.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_col.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_text_control_multi_line.h"
#include "third_party/blink/renderer/core/layout/layout_text_control_single_line.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/custom/layout_ng_custom.h"
#include "third_party/blink/renderer/core/layout/ng/flex/layout_ng_flexible_box.h"
#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_br.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_counter.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_word_break.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_button.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_fieldset.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_frame_set.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_progress.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_as_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_text.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_text_control_inner_editor.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_text_control_multi_line.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_text_control_single_line.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_view.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_inside_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_token_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

inline Element* GetElementForLayoutObject(Node& node) {
  if (auto* element = DynamicTo<Element>(node))
    return element;
  // If |node| is a Document, the layout object is going to be anonymous.
  DCHECK(node.IsDocumentNode());
  return nullptr;
}

template <typename BaseType, typename NGType, typename LegacyType = BaseType>
inline BaseType* CreateObject(Node& node,
                              LegacyLayout legacy,
                              bool disable_ng_for_type = false) {
  Element* element = GetElementForLayoutObject(node);
  bool force_legacy = false;

  // If no reason has been found for disabling NG for this particular type,
  // check if the NG feature is enabled at all, before considering creating an
  // NG object.
  if (!disable_ng_for_type) {
    // The last thing to check is whether we should force legacy layout. This
    // happens when the NG feature is enabled for the object in question, but
    // we're dealing with something that isn't implemented in NG yet (such as
    // editing or multicol). We then need to force legacy layout for the entire
    // subtree.
    force_legacy = legacy == LegacyLayout::kForce;

    if (!force_legacy)
      return MakeGarbageCollected<NGType>(element);
  }
  BaseType* new_object = MakeGarbageCollected<LegacyType>(element);
  if (force_legacy)
    new_object->SetForceLegacyLayout();
  return new_object;
}

}  // anonymous namespace

LayoutBlockFlow* LayoutObjectFactory::CreateBlockFlow(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  if (style.Display() == EDisplay::kListItem &&
      node.GetPseudoId() != kPseudoIdBackdrop) {
    // Create a LayoutBlockFlow with a ListItemOrdinal and maybe a ::marker.
    // ::backdrop is excluded since it's not tree-abiding, and ListItemOrdinal
    // needs to traverse the tree.
    return CreateObject<LayoutBlockFlow, LayoutNGListItem, LayoutListItem>(
        node, legacy);
  }

  // Create a plain LayoutBlockFlow
  return CreateObject<LayoutBlockFlow, LayoutNGBlockFlow>(node, legacy);
}

// static
LayoutBlock* LayoutObjectFactory::CreateBlockForLineClamp(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutBlock, LayoutNGBlockFlow,
                      LayoutDeprecatedFlexibleBox>(node, legacy);
}

LayoutView* LayoutObjectFactory::CreateView(Document& document,
                                            const ComputedStyle& style) {
  if (!RuntimeEnabledFeatures::LayoutNGPrintingEnabled())
    return MakeGarbageCollected<LayoutView>(&document);
  return MakeGarbageCollected<LayoutNGView>(&document);
}

LayoutBlock* LayoutObjectFactory::CreateFlexibleBox(Node& node,
                                                    const ComputedStyle& style,
                                                    LegacyLayout legacy) {
  return CreateObject<LayoutBlock, LayoutNGFlexibleBox, LayoutFlexibleBox>(
      node, legacy);
}

LayoutBlock* LayoutObjectFactory::CreateGrid(Node& node,
                                             const ComputedStyle& style,
                                             LegacyLayout legacy) {
  return CreateObject<LayoutBlock, LayoutNGGrid, LayoutGrid>(node, legacy);
}

LayoutBlock* LayoutObjectFactory::CreateMath(Node& node,
                                             const ComputedStyle& style,
                                             LegacyLayout legacy) {
  DCHECK(IsA<MathMLElement>(node) || node.IsDocumentNode() /* is_anonymous */);
  bool disable_ng_for_type = !RuntimeEnabledFeatures::MathMLCoreEnabled();
  if (IsA<MathMLTokenElement>(node)) {
    return CreateObject<LayoutBlockFlow, LayoutNGMathMLBlockFlow,
                        LayoutBlockFlow>(node, legacy, disable_ng_for_type);
  }
  return CreateObject<LayoutBlock, LayoutNGMathMLBlock, LayoutBlockFlow>(
      node, legacy, disable_ng_for_type);
}

LayoutBlock* LayoutObjectFactory::CreateCustom(Node& node,
                                               const ComputedStyle& style,
                                               LegacyLayout legacy) {
  DCHECK(node.IsElementNode());
  bool disable_ng_for_type = !RuntimeEnabledFeatures::CSSLayoutAPIEnabled();
  return CreateObject<LayoutBlock, LayoutNGCustom, LayoutBlockFlow>(
      node, legacy, disable_ng_for_type);
}

LayoutObject* LayoutObjectFactory::CreateListMarker(Node& node,
                                                    const ComputedStyle& style,
                                                    LegacyLayout legacy) {
  const Node* parent = node.parentNode();
  const ComputedStyle* parent_style = parent->GetComputedStyle();

  if (legacy == LegacyLayout::kForce) {
    // A table inside an inline element with specified columns may end up
    // marking a list-item ancestor with a size container-type for forced legacy
    // without re-attaching it during interleaved style recalc. Enforce
    // legacy/ng consistency between list-item and marker.
    DCHECK(!RuntimeEnabledFeatures::LayoutNGPrintingEnabled());
    DCHECK(parent->GetLayoutObject());
    if (parent->GetLayoutObject()->IsLayoutNGObject())
      legacy = LegacyLayout::kAuto;
  }
  bool is_inside =
      parent_style->ListStylePosition() == EListStylePosition::kInside ||
      (IsA<HTMLLIElement>(parent) && !parent_style->IsInsideListElement());
  if (style.ContentBehavesAsNormal()) {
    if (is_inside) {
      return CreateObject<LayoutObject, LayoutNGInsideListMarker,
                          LayoutListMarker>(node, legacy);
    }
    return CreateObject<LayoutObject, LayoutNGOutsideListMarker,
                        LayoutListMarker>(node, legacy);
  }
  if (is_inside) {
    return CreateObject<LayoutObject, LayoutNGInsideListMarker,
                        LayoutInsideListMarker>(node, legacy);
  }
  return CreateObject<LayoutObject, LayoutNGOutsideListMarker,
                      LayoutOutsideListMarker>(node, legacy);
}

LayoutBlock* LayoutObjectFactory::CreateTable(Node& node,
                                              const ComputedStyle& style,
                                              LegacyLayout legacy) {
  return CreateObject<LayoutBlock, LayoutNGTable, LayoutTable>(node, legacy);
}

LayoutTableCaption* LayoutObjectFactory::CreateTableCaption(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutTableCaption, LayoutNGTableCaption>(node, legacy);
}

LayoutBlockFlow* LayoutObjectFactory::CreateTableCell(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutBlockFlow, LayoutNGTableCell, LayoutTableCell>(
      node, legacy);
}

LayoutBox* LayoutObjectFactory::CreateTableColumn(Node& node,
                                                  const ComputedStyle& style,
                                                  LegacyLayout legacy) {
  return CreateObject<LayoutBox, LayoutNGTableColumn, LayoutTableCol>(node,
                                                                      legacy);
}

LayoutBox* LayoutObjectFactory::CreateTableRow(Node& node,
                                               const ComputedStyle& style,
                                               LegacyLayout legacy) {
  return CreateObject<LayoutBox, LayoutNGTableRow, LayoutTableRow>(node,
                                                                   legacy);
}

LayoutBox* LayoutObjectFactory::CreateTableSection(Node& node,
                                                   const ComputedStyle& style,
                                                   LegacyLayout legacy) {
  return CreateObject<LayoutBox, LayoutNGTableSection, LayoutTableSection>(
      node, legacy);
}

LayoutObject* LayoutObjectFactory::CreateButton(Node& node,
                                                const ComputedStyle& style,
                                                LegacyLayout legacy) {
  return CreateObject<LayoutBlock, LayoutNGButton, LayoutButton>(node, legacy);
}

LayoutObject* LayoutObjectFactory::CreateCounter(
    PseudoElement& pseduo,
    const CounterContentData& counter,
    LegacyLayout legacy) {
  bool force_legacy = legacy == LegacyLayout::kForce;
  if (!force_legacy) {
    return MakeGarbageCollected<LayoutNGCounter>(pseduo, counter);
  }
  auto* const new_object = MakeGarbageCollected<LayoutCounter>(pseduo, counter);
  if (force_legacy)
    new_object->SetForceLegacyLayout();
  return new_object;
}

LayoutBlock* LayoutObjectFactory::CreateFieldset(Node& node,
                                                 const ComputedStyle& style,
                                                 LegacyLayout legacy) {
  return CreateObject<LayoutBlock, LayoutNGFieldset, LayoutFieldset>(node,
                                                                     legacy);
}

LayoutBlockFlow* LayoutObjectFactory::CreateFileUploadControl(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutBlockFlow, LayoutNGBlockFlow,
                      LayoutFileUploadControl>(node, legacy);
}

LayoutBox* LayoutObjectFactory::CreateFrameSet(HTMLFrameSetElement& element,
                                               const ComputedStyle& style,
                                               LegacyLayout legacy) {
  return CreateObject<LayoutBox, LayoutNGFrameSet, LayoutFrameSet>(element,
                                                                   legacy);
}

LayoutObject* LayoutObjectFactory::CreateSliderTrack(Node& node,
                                                     const ComputedStyle& style,
                                                     LegacyLayout legacy) {
  return CreateObject<LayoutBlock, LayoutNGBlockFlow, LayoutSliderTrack>(
      node, legacy);
}

LayoutObject* LayoutObjectFactory::CreateTextControlInnerEditor(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutBlockFlow, LayoutNGTextControlInnerEditor,
                      LayoutTextControlInnerEditor>(node, legacy);
}

LayoutObject* LayoutObjectFactory::CreateTextControlMultiLine(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutBlockFlow, LayoutNGTextControlMultiLine,
                      LayoutTextControlMultiLine>(node, legacy);
}

LayoutObject* LayoutObjectFactory::CreateTextControlSingleLine(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutBlockFlow, LayoutNGTextControlSingleLine,
                      LayoutTextControlSingleLine>(node, legacy);
}

LayoutText* LayoutObjectFactory::CreateText(Node* node,
                                            scoped_refptr<StringImpl> str,
                                            LegacyLayout legacy) {
  bool force_legacy = legacy == LegacyLayout::kForce;
  if (!force_legacy) {
    return MakeGarbageCollected<LayoutNGText>(node, str);
  }
  LayoutText* layout_text = MakeGarbageCollected<LayoutText>(node, str);
  if (force_legacy)
    layout_text->SetForceLegacyLayout();
  return layout_text;
}

LayoutText* LayoutObjectFactory::CreateTextCombine(
    Node* node,
    scoped_refptr<StringImpl> str,
    LegacyLayout legacy) {
  bool force_legacy = legacy == LegacyLayout::kForce;
  if (!force_legacy) {
    return MakeGarbageCollected<LayoutNGText>(node, str);
  }
  LayoutText* const layout_text =
      MakeGarbageCollected<LayoutTextCombine>(node, str);
  if (force_legacy)
    layout_text->SetForceLegacyLayout();
  return layout_text;
}

LayoutTextFragment* LayoutObjectFactory::CreateTextFragment(
    Node* node,
    StringImpl* str,
    int start_offset,
    int length,
    LegacyLayout legacy) {
  bool force_legacy = legacy == LegacyLayout::kForce;
  if (!force_legacy) {
    return MakeGarbageCollected<LayoutNGTextFragment>(node, str, start_offset,
                                                      length);
  }
  LayoutTextFragment* layout_text_fragment =
      MakeGarbageCollected<LayoutTextFragment>(node, str, start_offset, length);
  if (force_legacy)
    layout_text_fragment->SetForceLegacyLayout();
  return layout_text_fragment;
}

LayoutProgress* LayoutObjectFactory::CreateProgress(Node* node,
                                                    const ComputedStyle& style,
                                                    LegacyLayout legacy) {
  return CreateObject<LayoutProgress, LayoutNGProgress>(*node, legacy);
}

LayoutRubyAsBlock* LayoutObjectFactory::CreateRubyAsBlock(
    Node* node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutRubyAsBlock, LayoutNGRubyAsBlock>(*node, legacy);
}

LayoutObject* LayoutObjectFactory::CreateRubyText(Node* node,
                                                  const ComputedStyle& style,
                                                  LegacyLayout legacy) {
  return CreateObject<LayoutRubyText, LayoutNGRubyText>(*node, legacy);
}

LayoutObject* LayoutObjectFactory::CreateSVGForeignObject(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutBlockFlow, LayoutNGSVGForeignObject,
                      LayoutSVGForeignObject>(node, legacy,
                                              /*disable_ng_for_type=*/false);
}

LayoutObject* LayoutObjectFactory::CreateSVGText(Node& node,
                                                 const ComputedStyle& style,
                                                 LegacyLayout legacy) {
  return CreateObject<LayoutBlockFlow, LayoutNGSVGText, LayoutSVGText>(node,
                                                                       legacy);
}

LayoutObject* LayoutObjectFactory::CreateBR(Node* node, LegacyLayout legacy) {
  return CreateObject<LayoutObject, LayoutNGBR, LayoutBR>(*node, legacy);
}

LayoutObject* LayoutObjectFactory::CreateWordBreak(HTMLElement* element,
                                                   LegacyLayout legacy) {
  return CreateObject<LayoutObject, LayoutNGWordBreak, LayoutWordBreak>(
      *element, legacy);
}

LayoutBox* LayoutObjectFactory::CreateAnonymousTableWithParent(
    const LayoutObject& parent,
    bool child_forces_legacy) {
  scoped_refptr<const ComputedStyle> new_style =
      parent.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent.StyleRef(),
          parent.IsLayoutInline() ? EDisplay::kInlineTable : EDisplay::kTable);
  LegacyLayout legacy =
      parent.ForceLegacyLayoutForChildren() || child_forces_legacy
          ? LegacyLayout::kForce
          : LegacyLayout::kAuto;

  LayoutBlock* new_table =
      CreateTable(parent.GetDocument(), *new_style, legacy);
  new_table->SetDocumentForAnonymous(&parent.GetDocument());
  new_table->SetStyle(std::move(new_style));
  return new_table;
}

LayoutBox* LayoutObjectFactory::CreateAnonymousTableSectionWithParent(
    const LayoutObject& parent) {
  scoped_refptr<const ComputedStyle> new_style =
      parent.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent.StyleRef(), EDisplay::kTableRowGroup);
  LegacyLayout legacy = parent.ForceLegacyLayoutForChildren()
                            ? LegacyLayout::kForce
                            : LegacyLayout::kAuto;

  LayoutBox* new_section =
      CreateTableSection(parent.GetDocument(), *new_style, legacy);
  new_section->SetDocumentForAnonymous(&parent.GetDocument());
  new_section->SetStyle(std::move(new_style));
  return new_section;
}

LayoutBox* LayoutObjectFactory::CreateAnonymousTableRowWithParent(
    const LayoutObject& parent) {
  scoped_refptr<const ComputedStyle> new_style =
      parent.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent.StyleRef(), EDisplay::kTableRow);
  LegacyLayout legacy = parent.ForceLegacyLayoutForChildren()
                            ? LegacyLayout::kForce
                            : LegacyLayout::kAuto;
  LayoutBox* new_row = CreateTableRow(parent.GetDocument(), *new_style, legacy);
  new_row->SetDocumentForAnonymous(&parent.GetDocument());
  new_row->SetStyle(std::move(new_style));
  return new_row;
}

LayoutBlockFlow* LayoutObjectFactory::CreateAnonymousTableCellWithParent(
    const LayoutObject& parent) {
  scoped_refptr<const ComputedStyle> new_style =
      parent.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent.StyleRef(), EDisplay::kTableCell);
  LegacyLayout legacy = parent.ForceLegacyLayoutForChildren()
                            ? LegacyLayout::kForce
                            : LegacyLayout::kAuto;
  LayoutBlockFlow* new_cell =
      CreateTableCell(parent.GetDocument(), *new_style, legacy);
  new_cell->SetDocumentForAnonymous(&parent.GetDocument());
  new_cell->SetStyle(std::move(new_style));
  return new_cell;
}

}  // namespace blink
