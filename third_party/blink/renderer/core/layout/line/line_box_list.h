/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_BOX_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_BOX_LIST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/api/hit_test_action.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

class CullRect;
class HitTestLocation;
class HitTestResult;
class InlineFlowBox;
class InlineTextBox;
class LayoutUnit;
class LineLayoutBoxModel;
class LineLayoutItem;
struct PhysicalOffset;

template <typename InlineBoxType>
class InlineBoxList {
  DISALLOW_NEW();

 public:
  InlineBoxList() : first_(nullptr), last_(nullptr) {}

#if DCHECK_IS_ON()
  // Owners should check this on destructor. This class does not implement
  // destructor to be part of a union.
  void AssertIsEmpty();
#endif

  InlineBoxType* First() const { return first_; }
  InlineBoxType* Last() const { return last_; }

  void AppendLineBox(InlineBoxType*);

  void DeleteLineBoxes();

  void ExtractLineBox(InlineBoxType*);
  void AttachLineBox(InlineBoxType*);
  void RemoveLineBox(InlineBoxType*);

  class BaseIterator {
    STACK_ALLOCATED();

   public:
    explicit BaseIterator(InlineBoxType* first) : current_(first) {}

    InlineBoxType* operator*() const {
      DCHECK(current_);
      return current_;
    }
    InlineBoxType* operator->() const { return operator*(); }

    bool operator==(const BaseIterator& other) const {
      return current_ == other.current_;
    }
    bool operator!=(const BaseIterator& other) const {
      return !operator==(other);
    }

   protected:
    InlineBoxType* current_;
  };

  class Iterator final : public BaseIterator {
   public:
    using BaseIterator::BaseIterator;
    using BaseIterator::current_;
    void operator++() {
      DCHECK(current_);
      current_ = current_->NextForSameLayoutObject();
    }
  };

  class ReverseIterator final : public BaseIterator {
   public:
    using BaseIterator::BaseIterator;
    using BaseIterator::current_;
    void operator++() {
      DCHECK(current_);
      current_ = current_->PrevForSameLayoutObject();
    }
  };

  Iterator begin() const { return Iterator(first_); }
  Iterator end() const { return Iterator(nullptr); }

  class ReverseRange final {
    STACK_ALLOCATED();

   public:
    explicit ReverseRange(InlineBoxType* last) : last_(last) {}
    ReverseIterator begin() const { return ReverseIterator(last_); }
    ReverseIterator end() const { return ReverseIterator(nullptr); }

   private:
    InlineBoxType* last_;
  };

  ReverseRange InReverseOrder() const { return ReverseRange(last_); }

 protected:
  // For block flows, each box represents the root inline box for a line in the
  // paragraph.
  // For inline flows, each box represents a portion of that inline.
  InlineBoxType* first_;
  InlineBoxType* last_;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT InlineBoxList<InlineFlowBox>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT InlineBoxList<InlineTextBox>;

class CORE_EXPORT LineBoxList : public InlineBoxList<InlineFlowBox> {
 public:
  static const LineBoxList& Empty();

  void DeleteLineBoxTree();

  void DirtyLineBoxes();
  void DirtyLinesFromChangedChild(LineLayoutItem parent,
                                  LineLayoutItem child,
                                  bool can_dirty_ancestors);

  bool HitTest(LineLayoutBoxModel,
               HitTestResult&,
               const HitTestLocation&,
               const PhysicalOffset& accumulated_offset,
               HitTestAction) const;
  bool AnyLineIntersectsRect(LineLayoutBoxModel,
                             const CullRect&,
                             const PhysicalOffset&) const;
  bool LineIntersectsDirtyRect(LineLayoutBoxModel,
                               InlineFlowBox*,
                               const CullRect&,
                               const PhysicalOffset&) const;

 private:
  bool RangeIntersectsRect(LineLayoutBoxModel,
                           LayoutUnit logical_top,
                           LayoutUnit logical_bottom,
                           const CullRect&,
                           const PhysicalOffset&) const;
};

class CORE_EXPORT InlineTextBoxList : public InlineBoxList<InlineTextBox> {
 public:
  static const InlineTextBoxList& Empty();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_BOX_LIST_H_
