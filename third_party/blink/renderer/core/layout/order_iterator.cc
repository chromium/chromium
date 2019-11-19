/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/order_iterator.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

OrderIterator::OrderIterator(const LayoutBox* container_box)
    : container_box_(container_box) {}

LayoutBox* OrderIterator::First() {
  Reset();
  return Next();
}

LayoutBox* OrderIterator::Next() {
  do {
    if (!current_child_) {
      if (order_values_iterator_ == order_values_.end())
        return nullptr;

      if (!is_reset_) {
        ++order_values_iterator_;
        if (order_values_iterator_ == order_values_.end())
          return nullptr;
      } else {
        is_reset_ = false;
      }

      current_child_ = container_box_->FirstChildBox();
    } else {
      current_child_ = current_child_->NextSiblingBox();
    }
  } while (!current_child_ ||
           ResolvedOrder(*current_child_) != *order_values_iterator_);

  return current_child_;
}

void OrderIterator::Reset() {
  current_child_ = nullptr;
  order_values_iterator_ = order_values_.begin();
  is_reset_ = true;
}

int OrderIterator::ResolvedOrder(const LayoutBox& child) const {
  if (container_box_->StyleRef().IsDeprecatedWebkitBox())
    return child.StyleRef().BoxOrdinalGroup();
  return child.StyleRef().Order();
}

OrderIteratorPopulator::~OrderIteratorPopulator() {
  iterator_.Reset();
}

void OrderIteratorPopulator::CollectChild(const LayoutBox* child) {
  iterator_.order_values_.insert(iterator_.ResolvedOrder(*child));
}

}  // namespace blink
