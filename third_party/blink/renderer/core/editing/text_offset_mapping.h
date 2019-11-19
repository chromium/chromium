// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_TEXT_OFFSET_MAPPING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_TEXT_OFFSET_MAPPING_H_

#include <iosfwd>
#include <iterator>
#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator_behavior.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LayoutBlockFlow;

// Mapping between position and text offset in "inline contents" with using
// characters from |TextIterator|.
//
// The "inline contents" is a similar to "inline formatting context" defined
// in CSS 2.1 specification or |LayoutBlockFlow| for inline contents, except
// for:
//  - Including characters from "display:inline-block".
//  - Exclude "float" or "positioned" appeared in middle of inline contents
//  - Treats characters with CSS property "-webkit-text-security" as "x"
//    instead of a bullet (U+2022), which breaks words.
class CORE_EXPORT TextOffsetMapping final {
  STACK_ALLOCATED();

 public:
  // |InlineContents| represents "inline contents" in a range of layout tree.
  class CORE_EXPORT InlineContents final {
    STACK_ALLOCATED();

   public:
    // |first| and |last|(inclusive) represent inline layout object run, they
    // should be descendants of |block_flow|.
    InlineContents(const LayoutBlockFlow& block_flow,
                   const LayoutObject& first,
                   const LayoutObject& last);
    // |block_flow| must be non-anonymous empty block or block containing only
    // anonymous object.
    InlineContents(const LayoutBlockFlow& block_flow);
    InlineContents() = default;

    bool operator==(const InlineContents& other) const;
    bool operator!=(const InlineContents& other) const {
      return !operator==(other);
    }

    const LayoutBlockFlow* GetEmptyBlock() const;
    const LayoutObject& FirstLayoutObject() const;
    const LayoutObject& LastLayoutObject() const;
    EphemeralRangeInFlatTree GetRange() const;

    bool IsNotNull() const { return !IsNull(); }
    bool IsNull() const { return !block_flow_; }

    // Returns |InlineContents| from |block_flow_| toward last of layout tree.
    static InlineContents NextOf(const InlineContents&);

    // Returns |InlineContents| from |block_flow_| toward first of layout tree.
    static InlineContents PreviousOf(const InlineContents&);

   private:
    friend class TextOffsetMapping;

    PositionInFlatTree FirstPositionAfterBlockFlow() const;
    PositionInFlatTree LastPositionBeforeBlockFlow() const;

    const LayoutBlockFlow* block_flow_ = nullptr;
    const LayoutObject* first_ = nullptr;
    const LayoutObject* last_ = nullptr;
  };

  // |BackwardRange| class is used with range-for to traverse inline contents
  // toward start of document.
  class CORE_EXPORT BackwardRange final {
    STACK_ALLOCATED();

   public:
    class CORE_EXPORT Iterator
        : public std::iterator<std::input_iterator_tag, InlineContents> {
      STACK_ALLOCATED();

     public:
      explicit Iterator(const InlineContents& current) : current_(current) {}
      Iterator() = default;

      InlineContents operator*() const;
      void operator++();

      bool operator==(const Iterator& other) const {
        return current_ == other.current_;
      }
      bool operator!=(const Iterator& other) const {
        return !operator==(other);
      }

     private:
      InlineContents current_;
    };

    explicit BackwardRange(const InlineContents& start) : start_(start) {}

    Iterator begin() const { return Iterator(start_); }
    Iterator end() const { return Iterator(); }

   private:
    const InlineContents start_;
  };

  // |ForwardRange| class is used with range-for to traverse inline contents
  // toward end of document.
  class CORE_EXPORT ForwardRange final {
    STACK_ALLOCATED();

   public:
    class CORE_EXPORT Iterator
        : public std::iterator<std::forward_iterator_tag, InlineContents> {
      STACK_ALLOCATED();

     public:
      explicit Iterator(const InlineContents& current) : current_(current) {}
      Iterator() = default;

      InlineContents operator*() const;
      void operator++();

      bool operator==(const Iterator& other) const {
        return current_ == other.current_;
      }
      bool operator!=(const Iterator& other) const {
        return !operator==(other);
      }

     private:
      InlineContents current_;
    };

    explicit ForwardRange(const InlineContents& start) : start_(start) {}

    Iterator begin() const { return Iterator(start_); }
    Iterator end() const { return Iterator(); }

   private:
    const InlineContents start_;
  };

  // Constructor |TextOffsetMapping| for the |inline_contents|.
  explicit TextOffsetMapping(const InlineContents& inline_contents);

  ~TextOffsetMapping() = default;

  // Returns range of |LayoutBlock|.
  const EphemeralRangeInFlatTree GetRange() const { return range_; }

  // Returns characters in subtree of |LayoutBlock|, collapsed whitespaces
  // are not included.
  const String& GetText() const { return text16_; }

  // Returns offset in |text16_| of specified position.
  int ComputeTextOffset(const PositionInFlatTree&) const;

  // Returns position before |offset| in |text16_|
  PositionInFlatTree GetPositionBefore(unsigned offset) const;

  // Returns position after |offset| in |text16_|
  PositionInFlatTree GetPositionAfter(unsigned offset) const;

  // Returns a range specified by |start| and |end| offset in |text16_|.
  EphemeralRangeInFlatTree ComputeRange(unsigned start, unsigned end) const;

  // Returns an offset in |text16_| before non-whitespace character from
  // |offset|, inclusive, otherwise returns |text16_.length()|.
  // This function is used for computing trailing whitespace after word.
  unsigned FindNonWhitespaceCharacterFrom(unsigned offset) const;

  // Helper functions to construct |TextOffsetMapping|.

  // Returns a |BackwardRange| for backward iteration of |InlineContents|
  // from |InlineContens| containing |position|.
  static BackwardRange BackwardRangeOf(const PositionInFlatTree& position);

  // Returns a |ForwardRange| for forward iteration of |InlineContents|
  // from |InlineContens| containing |position|.
  static ForwardRange ForwardRangeOf(const PositionInFlatTree& position);

  // Returns |LayoutBlockFlow| satisfying |IsInlineContents()| from |position|
  // (inclusive) toward start of document, or null if no such |LayoutBlockFlow|.
  static InlineContents FindBackwardInlineContents(
      const PositionInFlatTree& position);

  // Returns |LayoutBlockFlow| satisfying |IsInlineContents()| from |position|
  // (inclusive) toward end of document, or null if no such |LayoutBlockFlow|.
  static InlineContents FindForwardInlineContents(const PositionInFlatTree&);

 private:
  TextOffsetMapping(const InlineContents&, const TextIteratorBehavior&);

  template <typename Traverser>
  static InlineContents FindInlineContentsInternal(const Node*, Traverser);

  const TextIteratorBehavior behavior_;
  const EphemeralRangeInFlatTree range_;
  const String text16_;

  DISALLOW_COPY_AND_ASSIGN(TextOffsetMapping);
};

CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                     const TextOffsetMapping::InlineContents&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_TEXT_OFFSET_MAPPING_H_
