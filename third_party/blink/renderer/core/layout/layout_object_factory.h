// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ComputedStyle;
class HTMLElement;
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
class Node;

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
                                              LegacyLayout legacy);
  static LayoutBlock* CreateFlexibleBox(Node&,
                                        LegacyLayout);
  static LayoutBlock* CreateGrid(Node&, LegacyLayout);
  static LayoutBlock* CreateMath(Node&, LegacyLayout);
  static LayoutObject* CreateListMarker(Node&,
                                        const ComputedStyle&,
                                        LegacyLayout);
  static LayoutBlock* CreateTable(Node&, LegacyLayout);
  static LayoutTableCaption* CreateTableCaption(Node&,
                                                LegacyLayout);
  static LayoutBlockFlow* CreateTableCell(Node&,
                                          LegacyLayout);
  static LayoutBox* CreateTableColumn(Node&,
                                      LegacyLayout);

  static LayoutBox* CreateTableRow(Node&, LegacyLayout);
  static LayoutBox* CreateTableSection(Node&,
                                       LegacyLayout);

  static LayoutObject* CreateButton(Node& node,
                                    LegacyLayout legacy);
  static LayoutBlock* CreateFieldset(Node&, LegacyLayout);
  static LayoutBlockFlow* CreateFileUploadControl(Node& node,
                                                  LegacyLayout legacy);
  static LayoutObject* CreateSliderTrack(Node& node,
                                         LegacyLayout legacy);
  static LayoutObject* CreateTextControlInnerEditor(Node& node,
                                                    LegacyLayout legacy);
  static LayoutObject* CreateTextControlMultiLine(Node& node,
                                                  LegacyLayout legacy);
  static LayoutObject* CreateTextControlSingleLine(Node& node,
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
                                        LegacyLayout legacy);
  static LayoutRubyAsBlock* CreateRubyAsBlock(Node* node,
                                              LegacyLayout legacy);
  static LayoutObject* CreateRubyText(Node* node,
                                      LegacyLayout legacy);

  static LayoutObject* CreateSVGText(Node& node,
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
