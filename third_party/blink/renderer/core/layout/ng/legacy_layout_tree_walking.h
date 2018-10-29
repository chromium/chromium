// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LegacyLayoutTreeWalking_h
#define LegacyLayoutTreeWalking_h

namespace blink {

class LayoutBlock;
class LayoutObject;

// Return the layout object that should be the first child NGLayoutInputNode of
// |parent|. Normally this will just be the first layout object child, but there
// are certain layout objects that should be skipped for NG.
LayoutObject* GetLayoutObjectForFirstChildNode(LayoutBlock*);

// Return the layout object that should be the parent NGLayoutInputNode of
// |object|. Normally this will just be the parent layout object, but there
// are certain layout objects that should be skipped for NG.
LayoutObject* GetLayoutObjectForParentNode(LayoutObject*);

// Return the layout object that should be the sibling NGLayoutInputNode of
// |object|. Normally this will just be the next sibling layout object, but
// there are certain layout objects that should be skipped for NG.
LayoutObject* GetLayoutObjectForNextSiblingNode(LayoutObject*);

// Return true if the NGLayoutInputNode children of the NGLayoutInputNode
// established by |block| will be inline; see LayoutObject::ChildrenInline().
bool AreNGBlockFlowChildrenInline(const LayoutBlock*);

// Return true if the layout object is a LayoutNG object that is managed by the
// LayoutNG engine (i.e. its containing block is a LayoutNG object as well).
bool IsManagedByLayoutNG(const LayoutObject&);

// Return true if the block is of NG type, or if it's a block invisible to
// LayoutNG and it has an NG containg block. False if it's hosted by the legacy
// layout engine.
bool IsLayoutNGContainingBlock(const LayoutBlock*);

}  // namespace blink

#endif  // LegacyLayoutTreeWalking_h
