// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_BUFFER_H_

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/editing/finder/find_options.h"
#include "third_party/blink/renderer/core/editing/iterators/text_searcher_icu.h"
#include "third_party/blink/renderer/core/editing/position.h"

namespace blink {

class LayoutBlockFlow;
class NGOffsetMapping;
class Node;
class WebString;

// Buffer for find-in-page, collects text until it meets a block/other
// delimiters. Uses TextSearcherICU to find match in buffer.
// See doc at https://goo.gl/rnXjBu
class CORE_EXPORT FindBuffer {
  STACK_ALLOCATED();

 public:
  explicit FindBuffer(const EphemeralRangeInFlatTree& range);

  static EphemeralRangeInFlatTree FindMatchInRange(
      const EphemeralRangeInFlatTree& range,
      String search_text,
      const FindOptions);

  // Returns the closest ancestor of |start_node| (including the node itself)
  // that is block level.
  static Node& GetFirstBlockLevelAncestorInclusive(const Node& start_node);

  // See |GetVisibleTextNode|.
  static Node* ForwardVisibleTextNode(Node& start_node);
  static Node* BackwardVisibleTextNode(Node& start_node);

  // A match result, containing the starting position of the match and
  // the length of the match.
  struct BufferMatchResult {
    const unsigned start;
    const unsigned length;

    bool operator==(const BufferMatchResult& other) const {
      return start == other.start && length == other.length;
    }

    bool operator!=(const BufferMatchResult& other) const {
      return !operator==(other);
    }
  };

  // All match results for this buffer. We can iterate through the
  // BufferMatchResults one by one using the Iterator.
  class CORE_EXPORT Results {
    STACK_ALLOCATED();

   public:
    Results();

    Results(const FindBuffer& find_buffer,
            TextSearcherICU* text_searcher,
            const Vector<UChar>& buffer,
            const String& search_text,
            const blink::FindOptions options);

    class CORE_EXPORT Iterator
        : public std::iterator<std::forward_iterator_tag, BufferMatchResult> {
      STACK_ALLOCATED();

     public:
      Iterator() = default;
      Iterator(const FindBuffer& find_buffer,
               TextSearcherICU* text_searcher,
               const String& search_text);

      bool operator==(const Iterator& other) {
        return has_match_ == other.has_match_;
      }

      bool operator!=(const Iterator& other) {
        return has_match_ != other.has_match_;
      }

      const BufferMatchResult operator*() const;

      void operator++();

     private:
      const FindBuffer* find_buffer_;
      TextSearcherICU* text_searcher_;
      MatchResultICU match_;
      bool has_match_ = false;
    };

    Iterator begin() const;

    Iterator end() const;

    bool IsEmpty() const;

    BufferMatchResult front() const;

    BufferMatchResult back() const;

    unsigned CountForTesting() const;

   private:
    String search_text_;
    const FindBuffer* find_buffer_;
    TextSearcherICU* text_searcher_;
    bool empty_result_ = false;
  };

  // Finds all the match for |search_text| in |buffer_|.
  Results FindMatches(const WebString& search_text,
                      const blink::FindOptions options);

  // Gets a flat tree range corresponding to text in the [start_index,
  // end_index) of |buffer|.
  EphemeralRangeInFlatTree RangeFromBufferIndex(unsigned start_index,
                                                unsigned end_index) const;

  PositionInFlatTree PositionAfterBlock() const {
    if (!node_after_block_)
      return PositionInFlatTree();
    return PositionInFlatTree::FirstPositionInNode(*node_after_block_);
  }

  bool IsInvalidMatch(MatchResultICU match) const;

 private:
  // Collects text for one LayoutBlockFlow located within |range| to |buffer_|,
  // might be stopped without finishing one full LayoutBlockFlow  if we
  // encountered another LayoutBLockFlow, or if the end of |range| is
  // surpassed. Saves the next starting node after the block (first node in
  // another LayoutBlockFlow or after |end_position|) to |node_after_block_|.
  void CollectTextUntilBlockBoundary(const EphemeralRangeInFlatTree& range);

  // Mapping for position in buffer -> actual node where the text came from,
  // along with the offset in the NGOffsetMapping of this find_buffer.
  // This is needed because when we find a match in the buffer, we want to know
  // where it's located in the NGOffsetMapping for this FindBuffer.
  // Example: (assume there are no whitespace)
  // <div>
  //  aaa
  //  <span style="float:right;">bbb<span>ccc</span></span>
  //  ddd
  // </div>
  // We can finish FIP with three FindBuffer runs:
  // Run #1, 1 BufferNodeMapping with mapping text = "aaa\uFFFCddd",
  // The "\uFFFC" is the object replacement character created by the float.
  // For text node "aaa", oib = 0, oim = 0.
  // Content of |buffer_| = "aaa".
  // Run #2, 2 BufferNodeMappings, with mapping text = "bbbccc",
  //  1. For text node "bbb", oib = 0, oim = 0.
  //  2. For text node "ccc", oib = 3, oim = 3.
  // Content of |buffer_| = "bbbccc".
  // Run #3, 1 BufferNodeMapping with mapping text = "aaa\uFFFCddd",
  // For text node "ddd", oib = 0, oim = 4.
  // Content of |buffer_| = "ddd".
  // Since the LayoutBlockFlow for "aaa" and "ddd" is the same, they have the
  // same NGOffsetMapping, the |offset_in_mapping_| for the BufferNodeMapping in
  // run #3 is 4 (the index of first "d" character in the mapping text).
  struct BufferNodeMapping {
    const unsigned offset_in_buffer;
    const unsigned offset_in_mapping;
  };

  const BufferNodeMapping* MappingForIndex(unsigned index) const;

  PositionInFlatTree PositionAtStartOfCharacterAtIndex(unsigned index) const;

  PositionInFlatTree PositionAtEndOfCharacterAtIndex(unsigned index) const;

  // Adds text in |text_node| that are located within |range| to |buffer_|.
  void AddTextToBuffer(const Text& text_node,
                       LayoutBlockFlow& block_flow,
                       const EphemeralRangeInFlatTree& range);

  Node* node_after_block_ = nullptr;
  Vector<UChar> buffer_;
  Vector<BufferNodeMapping> buffer_node_mappings_;
  TextSearcherICU text_searcher_;

  const NGOffsetMapping* offset_mapping_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_BUFFER_H_
