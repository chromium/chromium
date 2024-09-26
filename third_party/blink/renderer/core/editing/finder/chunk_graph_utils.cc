// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/chunk_graph_utils.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

constexpr LChar kAnyLevel[] = "*";

const Node* FindOuterMostRubyContainerInBlockContaienr(const Node& node,
                                                       const Node& block) {
  const Node* ruby_container = nullptr;
  for (const auto& ancestor : FlatTreeTraversal::AncestorsOf(node)) {
    if (const auto* style = ancestor.GetComputedStyle()) {
      if (style->Display() == EDisplay::kRuby ||
          style->Display() == EDisplay::kBlockRuby) {
        ruby_container = &ancestor;
      }
      if (&ancestor == &block) {
        break;
      }
    }
  }
  return ruby_container;
}

class ChunkGraphBuilder {
  STACK_ALLOCATED();

 public:
  const Node* Build(const Node& first_visible_text_node,
                    const Node* end_node,
                    const Node& block_ancestor,
                    const Node* just_after_block) {
    bool did_see_range_start_node = false;
    bool did_see_range_end_node = false;
    const Node* node = &first_visible_text_node;
    if (const Node* ruby_container = FindOuterMostRubyContainerInBlockContaienr(
            first_visible_text_node, block_ancestor)) {
      // If the range starts inside a <ruby>, we need to start analyzing the
      // <ruby>. We don't record Text nodes until first_visible_text_node.
      node = ruby_container;
    }

    while (node && node != just_after_block) {
      if (FindBuffer::ShouldIgnoreContents(*node)) {
        if (end_node && (end_node == node ||
                         FlatTreeTraversal::IsDescendantOf(*end_node, *node))) {
          did_see_range_end_node = true;
        }
        if (std::optional<UChar> ch = FindBuffer::CharConstantForNode(*node)) {
          if (did_see_range_start_node && !did_see_range_end_node) {
            chunk_text_list_.push_back(TextOrChar(nullptr, *ch));
          }
        }
        node = FlatTreeTraversal::NextSkippingChildren(*node);
        continue;
      }
      if (IsA<Element>(*node)) {
        EDisplay display = EDisplay::kNone;
        if (const auto* style = node->GetComputedStyle()) {
          display = style->Display();
        }
        if (display == EDisplay::kNone) {
          if (end_node &&
              (end_node == node ||
               FlatTreeTraversal::IsDescendantOf(*end_node, *node))) {
            did_see_range_end_node = true;
          }
          node = FlatTreeTraversal::NextSkippingChildren(*node);
          continue;
        }
        if (FlatTreeTraversal::FirstChild(*node)) {
          if (display == EDisplay::kRubyText) {
            HandleAnnotationStart(*node);
          } else if (display == EDisplay::kRuby ||
                     display == EDisplay::kBlockRuby) {
            HandleRubyContainerStart();
          }
          node = FlatTreeTraversal::FirstChild(*node);
          continue;
        }
      } else if (const auto* text = DynamicTo<Text>(*node)) {
        if (!did_see_range_start_node && first_visible_text_node == text) {
          did_see_range_start_node = true;
        }
        if (did_see_range_start_node && !did_see_range_end_node &&
            text->GetComputedStyle() &&
            text->GetComputedStyle()->UsedVisibility() ==
                EVisibility::kVisible) {
          chunk_text_list_.push_back(TextOrChar(text, 0));
        }
      }

      if (node == end_node) {
        did_see_range_end_node = true;
        if (ruby_depth_ == 0) {
          node = FlatTreeTraversal::Next(*node);
          break;
        }
        // If the range ends inside a <ruby>, we need to continue analyzing the
        // <ruby>. We don't record Text nodes after end_node.
      }

      while (!FlatTreeTraversal::NextSibling(*node) &&
             node != &block_ancestor) {
        node = FlatTreeTraversal::ParentElement(*node);
        EDisplay display = EDisplay::kNone;
        if (const auto* style = node->GetComputedStyle()) {
          display = style->Display();
        }
        if (display == EDisplay::kRubyText) {
          if (HandleAnnotationEnd(*node, did_see_range_end_node)) {
            break;
          }
        } else if (display == EDisplay::kRuby ||
                   display == EDisplay::kBlockRuby) {
          if (HandleRubyContainerEnd(did_see_range_end_node)) {
            break;
          }
        }
      }
      if (node == &block_ancestor) {
        node = FlatTreeTraversal::NextSibling(*node);
        break;
      }
      node = FlatTreeTraversal::NextSibling(*node);
    }
    return node;
  }

 private:
  void HandleAnnotationStart(const Node& node) {}

  // Returns true if we should exit the loop.
  bool HandleAnnotationEnd(const Node& node, bool did_see_range_end_node) {
    return false;
  }

  void HandleRubyContainerStart() {}

  // Returns true if we should exit the loop.
  bool HandleRubyContainerEnd(bool did_see_range_end_node) { return false; }

  wtf_size_t ruby_depth_ = 0;
  HeapVector<TextOrChar> chunk_text_list_;
};

}  // namespace

void TextOrChar::Trace(Visitor* visitor) const {
  visitor->Trace(text);
}

CorpusChunk::CorpusChunk() : level_(String(kAnyLevel)) {}

CorpusChunk::CorpusChunk(const HeapVector<TextOrChar>& text_list,
                         const String& level)
    : level_(level) {
  text_list_ = text_list;
}

void CorpusChunk::Trace(Visitor* visitor) const {
  visitor->Trace(text_list_);
  visitor->Trace(next_list_);
}

void CorpusChunk::Link(CorpusChunk* next_chunk) {
  next_list_.push_back(next_chunk);
}

}  // namespace blink
