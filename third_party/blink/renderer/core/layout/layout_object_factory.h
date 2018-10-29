// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_FACTORY_H_

#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class ComputedStyle;
class LayoutBlock;
class LayoutBlockFlow;
class LayoutTableCaption;
class LayoutTableCell;
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
  static LayoutBlockFlow* CreateBlockFlow(Node&, const ComputedStyle&);
  static LayoutBlock* CreateFlexibleBox(Node&, const ComputedStyle&);
  static LayoutBlockFlow* CreateListItem(Node&, const ComputedStyle&);
  static LayoutTableCaption* CreateTableCaption(Node&, const ComputedStyle&);
  static LayoutTableCell* CreateTableCell(Node&, const ComputedStyle&);
  static LayoutBlock* CreateFieldset(Node&, const ComputedStyle&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_FACTORY_H_
