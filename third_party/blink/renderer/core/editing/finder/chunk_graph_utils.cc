// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/chunk_graph_utils.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

constexpr LChar kAnyLevel[] = "*";
constexpr LChar kBaseLevel[] = "0";
constexpr UChar kLevelDelimiter = kComma;

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

bool IsParentRubyContainer(const Node& node) {
  const Node* parent = &node;
  while ((parent = FlatTreeTraversal::ParentElement(*parent))) {
    const auto* style = parent->GetComputedStyle();
    if (!style) {
      break;
    }
    EDisplay display = style->Display();
    if (display == EDisplay::kContents) {
      continue;
    }
    return display == EDisplay::kRuby || display == EDisplay::kBlockRuby;
  }
  return false;
}

String CreateLevel(
    const Vector<std::pair<wtf_size_t, wtf_size_t>>& depth_context) {
  StringBuilder builder;
  String delimiter;
  for (const auto [max, current] : depth_context) {
    builder.Append(delimiter);
    delimiter = String(&kLevelDelimiter, 1u);
    builder.AppendNumber(max - current + 1);
  }
  return builder.ToString();
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
    // Used for checking if we reached a new block.
    const Node* last_added_text_node = nullptr;
    // This is a std::optional because nullptr is a valid value.
    std::optional<const Node*> next_start;

    parent_chunk_ = MakeGarbageCollected<CorpusChunk>();
    corpus_chunk_list_.push_back(parent_chunk_);

    while (node && node != just_after_block) {
      if (FindBuffer::ShouldIgnoreContents(*node)) {
        const Node* next = FlatTreeTraversal::NextSkippingChildren(*node);
        if (end_node && (end_node == node ||
                         FlatTreeTraversal::IsDescendantOf(*end_node, *node))) {
          did_see_range_end_node = true;
          if (!next_start) {
            next_start = next;
          }
        }
        if (std::optional<UChar> ch = FindBuffer::CharConstantForNode(*node)) {
          if (did_see_range_start_node && !did_see_range_end_node) {
            chunk_text_list_.push_back(TextOrChar(nullptr, *ch));
          }
        }
        node = next;
        continue;
      }
      const auto* style = node->GetComputedStyle();
      if (!style) {
        const Node* next = FlatTreeTraversal::NextSkippingChildren(*node);
        if (end_node && (end_node == node ||
                         FlatTreeTraversal::IsDescendantOf(*end_node, *node))) {
          did_see_range_end_node = true;
          if (!next_start) {
            next_start = next;
          }
        }
        node = next;
        continue;
      }

      EDisplay display = style->Display();
      if (IsA<Element>(*node)) {
        if (const Node* child = FlatTreeTraversal::FirstChild(*node)) {
          if (display == EDisplay::kContents) {
            node = child;
            continue;
          } else if (display == EDisplay::kRubyText) {
            HandleAnnotationStart(*node);
            node = child;
            continue;
          } else if (display == EDisplay::kRuby ||
                     display == EDisplay::kBlockRuby) {
            HandleRubyContainerStart();
            node = child;
            continue;
          }
        }
      }
      if (style->UsedVisibility() == EVisibility::kVisible &&
          node->GetLayoutObject()) {
        // `node` is in its own sub-block separate from our starting position.
        if (last_added_text_node && !FindBuffer::IsInSameUninterruptedBlock(
                                        *last_added_text_node, *node)) {
          did_see_range_end_node = true;
          if (depth_context_.empty() && ruby_depth_ == 0) {
            break;
          }
          if (!next_start) {
            next_start = node;
          }
        }

        if (IsA<Element>(*node)) {
          if (const Node* child = FlatTreeTraversal::FirstChild(*node)) {
            node = child;
            continue;
          }
        }

        if (const auto* text = DynamicTo<Text>(*node)) {
          if (!did_see_range_start_node && first_visible_text_node == text) {
            did_see_range_start_node = true;
          }
          if (did_see_range_start_node && !did_see_range_end_node) {
            chunk_text_list_.push_back(TextOrChar(text, 0));
            last_added_text_node = node;
          }
        }
      }

      if (node == end_node) {
        did_see_range_end_node = true;
        if (!next_start) {
          next_start = FlatTreeTraversal::Next(*node);
        }
        if (depth_context_.empty() && ruby_depth_ == 0) {
          break;
        }
        // If the range ends inside a <ruby>, we need to continue analyzing the
        // <ruby>. We don't record Text nodes after end_node.
      }

      while (!FlatTreeTraversal::NextSibling(*node) &&
             node != &block_ancestor) {
        node = FlatTreeTraversal::ParentElement(*node);
        display = EDisplay::kNone;
        if ((style = node->GetComputedStyle())) {
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
    if (chunk_text_list_.size() > 0) {
      parent_chunk_->Link(PushChunk(String(kAnyLevel)));
    }
    return next_start.value_or(node);
  }

  const HeapVector<Member<CorpusChunk>>& ChunkList() const {
    return corpus_chunk_list_;
  }
  Vector<String> TakeLevelList() { return std::move(level_list_); }

 private:
  CorpusChunk* PushChunk(const String& level) {
    auto* new_chunk =
        MakeGarbageCollected<CorpusChunk>(chunk_text_list_, level);
    corpus_chunk_list_.push_back(new_chunk);
    chunk_text_list_.resize(0);
    return new_chunk;
  }

  CorpusChunk* PushBaseChunk() {
    if (depth_context_.size() > 0) {
      return PushChunk(CreateLevel(depth_context_));
    } else if (ruby_depth_ == 0) {
      return PushChunk(String(kAnyLevel));
    } else {
      return PushChunk(String(kBaseLevel));
    }
  }

  void PushBaseChunkAndLink() {
    auto* new_base_chunk = PushBaseChunk();
    parent_chunk_->Link(new_base_chunk);
    parent_chunk_ = new_base_chunk;
  }

  void HandleAnnotationStart(const Node& node) {
    CorpusChunk* new_base_chunk = PushBaseChunk();
    parent_chunk_->Link(new_base_chunk);
    if (IsParentRubyContainer(node)) {
      parent_chunk_ = parent_chunk_stack_.back();
      parent_chunk_stack_.pop_back();
    } else {
      parent_chunk_ = new_base_chunk;
      OpenRubyContainer();
      new_base_chunk = PushBaseChunk();
      parent_chunk_->Link(new_base_chunk);
    }
    base_last_chunk_stack_.push_back(new_base_chunk);
    depth_context_.push_back(std::make_pair(max_ruby_depth_, ruby_depth_));
    max_ruby_depth_ = 0;
    ruby_depth_ = 0;
    String level = CreateLevel(depth_context_);
    if (!level_list_.Contains(level)) {
      level_list_.push_back(level);
    }
  }

  // Returns true if we should exit the loop.
  bool HandleAnnotationEnd(const Node& node, bool did_see_range_end_node) {
    auto* rt_last_chunk = PushChunk(CreateLevel(depth_context_));
    parent_chunk_->Link(rt_last_chunk);

    CorpusChunk* base_last_chunk = base_last_chunk_stack_.back();
    base_last_chunk_stack_.pop_back();
    auto* void_chunk = MakeGarbageCollected<CorpusChunk>();
    corpus_chunk_list_.push_back(void_chunk);
    base_last_chunk->Link(void_chunk);
    rt_last_chunk->Link(void_chunk);
    parent_chunk_ = void_chunk;
    parent_chunk_stack_.push_back(parent_chunk_);

    auto pair = depth_context_.back();
    depth_context_.pop_back();
    max_ruby_depth_ = pair.first;
    ruby_depth_ = pair.second;
    if (ruby_depth_ == 1) {
      max_ruby_depth_ = 1;
    }
    return !IsParentRubyContainer(node) &&
           CloseRubyContainer(did_see_range_end_node);
  }

  void HandleRubyContainerStart() {
    if (chunk_text_list_.size() > 0) {
      PushBaseChunkAndLink();
    }
    // Save to use it on the start of the corresponding ruby-text.
    parent_chunk_stack_.push_back(parent_chunk_);

    OpenRubyContainer();
  }

  // Returns true if we should exit the loop.
  bool HandleRubyContainerEnd(bool did_see_range_end_node) {
    if (chunk_text_list_.size() > 0) {
      PushBaseChunkAndLink();
    }
    return CloseRubyContainer(did_see_range_end_node);
  }

  void OpenRubyContainer() {
    if (ruby_depth_ == 0) {
      max_ruby_depth_ = 1;
    }
    ++ruby_depth_;
    max_ruby_depth_ = std::max(ruby_depth_, max_ruby_depth_);
  }

  // Returns true if we should exit the loop.
  bool CloseRubyContainer(bool did_see_range_end_node) {
    parent_chunk_stack_.pop_back();
    if (--ruby_depth_ == 0) {
      max_ruby_depth_ = 0;
      if (depth_context_.empty() && did_see_range_end_node) {
        return true;
      }
    }
    return false;
  }

  // `corpus_chunk_list_` and `level_list_` are the deliverables of this class.
  HeapVector<Member<CorpusChunk>> corpus_chunk_list_;
  Vector<String> level_list_;

  // Fields required for intermediate data.
  CorpusChunk* parent_chunk_ = nullptr;
  wtf_size_t ruby_depth_ = 0;
  wtf_size_t max_ruby_depth_ = 0;
  Vector<std::pair<wtf_size_t, wtf_size_t>> depth_context_;
  HeapVector<Member<CorpusChunk>> parent_chunk_stack_;
  HeapVector<Member<CorpusChunk>> base_last_chunk_stack_;
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

const CorpusChunk* CorpusChunk::FindNext(const String& level) const {
  if (next_list_.empty()) {
    return nullptr;
  }
  const CorpusChunk* annotation_next = nullptr;
  for (const auto& chunk : next_list_) {
    if (chunk->level_ == kAnyLevel) {
      return chunk;
    } else if (level.empty() && chunk->level_ == kBaseLevel) {
      return chunk;
    } else if (chunk->level_ == level) {
      annotation_next = chunk;
    }
  }
  if (annotation_next) {
    return annotation_next;
  }
  // A single "base" link should be assumed as "any".  The "base" doesn't have
  // the requested level of an annotation.
  if (next_list_.size() == 1 && next_list_[0]->level_ == kBaseLevel) {
    return next_list_[0];
  }
  wtf_size_t delimiter_index = level.ReverseFind(kLevelDelimiter);
  if (delimiter_index == kNotFound) {
    // No link for `level`. It means the graph is incorrect.
    return nullptr;
  }
  return FindNext(level.Substring(0, delimiter_index));
}

std::tuple<HeapVector<Member<CorpusChunk>>, Vector<String>, const Node*>
BuildChunkGraph(const Node& first_visible_text_node,
                const Node* end_node,
                const Node& block_ancestor,
                const Node* just_after_block) {
  ChunkGraphBuilder builder;
  const Node* next_node = builder.Build(first_visible_text_node, end_node,
                                        block_ancestor, just_after_block);
  return {builder.ChunkList(), builder.TakeLevelList(), next_node};
}

}  // namespace blink
