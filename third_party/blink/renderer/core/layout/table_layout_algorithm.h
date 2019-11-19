/*
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TABLE_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TABLE_LAYOUT_ALGORITHM_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutTable;

class TableLayoutAlgorithm {
  USING_FAST_MALLOC(TableLayoutAlgorithm);

 public:
  explicit TableLayoutAlgorithm(LayoutTable* table) : table_(table) {}

  virtual ~TableLayoutAlgorithm() = default;

  virtual void ComputeIntrinsicLogicalWidths(LayoutUnit& min_width,
                                             LayoutUnit& max_width) = 0;
  virtual LayoutUnit ScaledWidthFromPercentColumns() { return LayoutUnit(); }
  virtual void ApplyPreferredLogicalWidthQuirks(
      LayoutUnit& min_width,
      LayoutUnit& max_width) const = 0;
  virtual void UpdateLayout() = 0;
  virtual void WillChangeTableLayout() = 0;

 protected:
  // FIXME: Once we enable SATURATED_LAYOUT_ARITHMETHIC, this should just be
  // LayoutUnit::nearlyMax(). Until then though, using nearlyMax causes
  // overflow in some tests, so we just pick a large number.
  const static int kTableMaxWidth = 1000000;

  LayoutTable* table_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TableLayoutAlgorithm);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TABLE_LAYOUT_ALGORITHM_H_
