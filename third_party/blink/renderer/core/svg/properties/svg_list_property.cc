/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/svg/properties/svg_list_property.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

void SVGListPropertyBase::Clear() {
  // Detach all list items as they are no longer part of this list.
  for (auto& value : values_) {
    DCHECK_EQ(value->OwnerList(), this);
    value->SetOwnerList(nullptr);
  }
  values_.clear();
}

void SVGListPropertyBase::Insert(uint32_t index,
                                 SVGListablePropertyBase* new_item) {
  values_.insert(index, new_item);
  new_item->SetOwnerList(this);
}

void SVGListPropertyBase::Remove(uint32_t index) {
  DCHECK_EQ(values_[index]->OwnerList(), this);
  values_[index]->SetOwnerList(nullptr);
  values_.EraseAt(index);
}

void SVGListPropertyBase::Append(SVGListablePropertyBase* new_item) {
  values_.push_back(new_item);
  new_item->SetOwnerList(this);
}

void SVGListPropertyBase::Replace(uint32_t index,
                                  SVGListablePropertyBase* new_item) {
  DCHECK_EQ(values_[index]->OwnerList(), this);
  values_[index]->SetOwnerList(nullptr);
  values_[index] = new_item;
  new_item->SetOwnerList(this);
}

String SVGListPropertyBase::ValueAsString() const {
  if (values_.empty())
    return String();

  StringBuilder builder;

  auto it = values_.begin();
  auto it_end = values_.end();
  while (it != it_end) {
    builder.Append((*it)->ValueAsString());
    ++it;
    if (it != it_end)
      builder.Append(' ');
  }
  return builder.ToString();
}

}  // namespace blink
