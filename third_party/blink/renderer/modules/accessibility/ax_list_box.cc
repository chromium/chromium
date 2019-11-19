/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/accessibility/ax_list_box.h"

#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_list_box_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

namespace blink {

AXListBox::AXListBox(LayoutObject* layout_object,
                     AXObjectCacheImpl& ax_object_cache)
    : AXLayoutObject(layout_object, ax_object_cache), active_index_(-1) {
  ActiveIndexChanged();
}

AXListBox::~AXListBox() = default;

ax::mojom::Role AXListBox::DetermineAccessibilityRole() {
  if ((aria_role_ = DetermineAriaRoleAttribute()) != ax::mojom::Role::kUnknown)
    return aria_role_;

  return ax::mojom::Role::kListBox;
}

AXObject* AXListBox::ActiveDescendant() {
  auto* select = DynamicTo<HTMLSelectElement>(GetNode());
  if (!select)
    return nullptr;

  int active_index = select->ActiveSelectionEndListIndex();
  if (active_index >= 0 && active_index < static_cast<int>(select->length())) {
    HTMLOptionElement* option = select->item(active_index_);
    return AXObjectCache().Get(option);
  }

  return nullptr;
}

void AXListBox::ActiveIndexChanged() {
  auto* select = DynamicTo<HTMLSelectElement>(GetNode());
  if (!select)
    return;

  int active_index = select->ActiveSelectionEndListIndex();
  if (active_index == active_index_)
    return;

  active_index_ = active_index;
  if (!select->IsFocused())
    return;

  AXObjectCache().PostNotification(this,
                                   ax::mojom::Event::kActiveDescendantChanged);
}

}  // namespace blink
