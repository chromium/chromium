// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ComputedStyle;
class CounterContentData;
class Document;
class HTMLElement;
class HTMLFrameSetElement;
class LayoutBlock;
class LayoutBlockFlow;
class LayoutObject;
class LayoutBox;
enum class LegacyLayout;
class LayoutProgress;
class LayoutRubyAsBlock;
class LayoutTableCaption;
class LayoutText;
class LayoutTextFragment;
class LayoutView;
class Node;
class PseudoElement;

// Helper class for creation of certain LayoutObject-derived objects that may
// need to be of different types, depending on whether or not LayoutNG is to be
// used in the given context.
class LayoutObjectFactory {
  STATIC_ONLY(LayoutObjectFactory);

 public:
  // The following methods will create and return some LayoutObject-derived
  // object. If |node| is of type Element, it will be associated with the new
  // LayoutObject. Otherwise it will be assumed to be a Document node, in which
  // case the LayoutObject created will be anonymous. The |style| reference
  // passed will only be used to determine which object type to create.
  static LayoutBlockFlow* CreateBlockFlow(Node&,
                                          const ComputedStyle&,
                                          LegacyLayout);
  static LayoutBlock* CreateBlockForLineClamp(Node& node,
                                              const ComputedStyle& style,
                                              LegacyLayout legacy);
  static LayoutView* CreateView(Document&, const ComputedStyle&);
  static LayoutBlock* CreateFlexibleBox(Node&,
                                        const ComputedStyle&,
                                        LegacyLayout);
  static LayoutBlock* CreateGrid(Node&, const ComputedStyle&, LegacyLayout);
  static LayoutBlock* CreateMath(Node&, const ComputedStyle&, LegacyLayout);
  static LayoutBlock* CreateCustom(Node&, const ComputedStyle&, LegacyLayout);
  static LayoutObject* CreateListMarker(Node&,
                                        const ComputedStyle&,
                                        LegacyLayout);
  static LayoutBlock* CreateTable(Node&, const ComputedStyle&, LegacyLayout);
  static LayoutTableCaption* CreateTableCaption(Node&,
                                                const ComputedStyle&,
                                                LegacyLayout);
  static LayoutBlockFlow* CreateTableCell(Node&,
                                          const ComputedStyle&,
                                          LegacyLayout);
  static LayoutBox* CreateTableColumn(Node&,
                                      const ComputedStyle&,
                                      LegacyLayout);

  static LayoutBox* CreateTableRow(Node&, const ComputedStyle&, LegacyLayout);
  static LayoutBox* CreateTableSection(Node&,
                                       const ComputedStyle&,
                                       LegacyLayout);

  static LayoutObject* CreateButton(Node& node,
                                    const ComputedStyle& style,
                                    LegacyLayout legacy);
  static LayoutObject* CreateCounter(PseudoElement& pseduo,
                                     const CounterContentData& counter,
                                     LegacyLayout legacy);
  static LayoutBlock* CreateFieldset(Node&, const ComputedStyle&, LegacyLayout);
  static LayoutBlockFlow* CreateFileUploadControl(Node& node,
                                                  const ComputedStyle& style,
                                                  LegacyLayout legacy);
  static LayoutBox* CreateFrameSet(HTMLFrameSetElement& element,
                                   const ComputedStyle& style,
                                   LegacyLayout legacy);
  static LayoutObject* CreateSliderTrack(Node& node,
                                         const ComputedStyle& style,
                                         LegacyLayout legacy);
  static LayoutObject* CreateTextControlInnerEditor(Node& node,
                                                    const ComputedStyle& style,
                                                    LegacyLayout legacy);
  static LayoutObject* CreateTextControlMultiLine(Node& node,
                                                  const ComputedStyle& style,
                                                  LegacyLayout legacy);
  static LayoutObject* CreateTextControlSingleLine(Node& node,
                                                   const ComputedStyle& style,
                                                   LegacyLayout legacy);

  static LayoutText* CreateText(Node*, scoped_refptr<StringImpl>, LegacyLayout);
  static LayoutText* CreateTextCombine(Node*,
                                       scoped_refptr<StringImpl>,
                                       LegacyLayout);
  static LayoutTextFragment* CreateTextFragment(Node*,
                                                StringImpl*,
                                                int start_offset,
                                                int length,
                                                LegacyLayout);
  static LayoutProgress* CreateProgress(Node* node,
                                        const ComputedStyle& style,
                                        LegacyLayout legacy);
  static LayoutRubyAsBlock* CreateRubyAsBlock(Node* node,
                                              const ComputedStyle& style,
                                              LegacyLayout legacy);
  static LayoutObject* CreateRubyText(Node* node,
                                      const ComputedStyle& style,
                                      LegacyLayout legacy);

  static LayoutObject* CreateSVGForeignObject(Node& node,
                                              const ComputedStyle& style,
                                              LegacyLayout legacy);
  static LayoutObject* CreateSVGText(Node& node,
                                     const ComputedStyle& style,
                                     LegacyLayout legacy);

  static LayoutObject* CreateBR(Node*, LegacyLayout);
  static LayoutObject* CreateWordBreak(HTMLElement*, LegacyLayout);

  // Anonymous creation methods

  // |child_forces_legacy| true if creating parents boxes for legacy child.
  // Table must match child's type.
  static LayoutBox* CreateAnonymousTableWithParent(
      const LayoutObject& parent,
      bool child_forces_legacy = false);

  static LayoutBox* CreateAnonymousTableSectionWithParent(
      const LayoutObject& parent);

  static LayoutBox* CreateAnonymousTableRowWithParent(
      const LayoutObject& parent);

  static LayoutBlockFlow* CreateAnonymousTableCellWithParent(
      const LayoutObject& parent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_FACTORY_H_
