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
class LayoutBlock;
class LayoutBlockFlow;
enum class LegacyLayout;
class LayoutProgress;
class LayoutTableCaption;
class LayoutTableCell;
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
  static LayoutBlock* CreateFlexibleBox(Node&,
                                        const ComputedStyle&,
                                        LegacyLayout);
  static LayoutBlockFlow* CreateListItem(Node&,
                                         const ComputedStyle&,
                                         LegacyLayout);
  static LayoutTableCaption* CreateTableCaption(Node&,
                                                const ComputedStyle&,
                                                LegacyLayout);
  static LayoutTableCell* CreateTableCell(Node&,
                                          const ComputedStyle&,
                                          LegacyLayout);
  static LayoutBlock* CreateFieldset(Node&, const ComputedStyle&, LegacyLayout);
  static LayoutText* CreateText(Node*, scoped_refptr<StringImpl>, LegacyLayout);
  static LayoutTextFragment* CreateTextFragment(Node*,
                                                StringImpl*,
                                                int start_offset,
                                                int length,
                                                LegacyLayout);
  static LayoutProgress* CreateProgress(Node* node,
                                        const ComputedStyle& style,
                                        LegacyLayout legacy);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_FACTORY_H_
