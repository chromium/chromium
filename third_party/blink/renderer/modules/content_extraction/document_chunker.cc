// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/document_chunker.h"

#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// Checks for excluded tags. Text within these will be excluded from passages.
bool IsExcludedElement(const Node& node) {
  const Element* element = DynamicTo<Element>(node);
  if (!element) {
    return false;
  }
  return element->HasTagName(html_names::kNoscriptTag) ||
         element->HasTagName(html_names::kScriptTag) ||
         element->HasTagName(html_names::kStyleTag) ||
         element->HasTagName(svg_names::kDefsTag) ||
         element->HasTagName(svg_names::kStyleTag) ||
         element->HasTagName(svg_names::kScriptTag);
}

// Checks for tags that indicate a section break. Sibling nodes will not be
// greedily aggregated into a chunk across one of these tags.
bool IsSectionBreak(const Node& node) {
  const HTMLElement* html_element = DynamicTo<HTMLElement>(node);
  if (!html_element) {
    return false;
  }
  return html_element->HasTagName(html_names::kArticleTag) ||
         html_element->HasTagName(html_names::kBrTag) ||
         html_element->HasTagName(html_names::kDivTag) ||
         html_element->HasTagName(html_names::kH1Tag) ||
         html_element->HasTagName(html_names::kH2Tag) ||
         html_element->HasTagName(html_names::kH3Tag) ||
         html_element->HasTagName(html_names::kH4Tag) ||
         html_element->HasTagName(html_names::kH5Tag) ||
         html_element->HasTagName(html_names::kH6Tag) ||
         html_element->HasTagName(html_names::kHrTag) ||
         html_element->HasTagName(html_names::kFooterTag) ||
         html_element->HasTagName(html_names::kHeaderTag) ||
         html_element->HasTagName(html_names::kMainTag) ||
         html_element->HasTagName(html_names::kNavTag);
}

}  // namespace

bool ShouldContentExtractionIncludeIFrame(const HTMLIFrameElement& iframe_element) {
  if (iframe_element.IsAdRelated()) {
    return false;
  }
  LocalFrame* iframe_frame =
      DynamicTo<LocalFrame>(iframe_element.ContentFrame());
  if (!iframe_frame || iframe_frame->IsCrossOriginToParentOrOuterDocument()) {
    return false;
  }
  Document* iframe_document = iframe_frame->GetDocument();
  if (!iframe_document->body()) {
    return false;
  }
  return true;
}

DocumentChunker::DocumentChunker(size_t max_words_per_aggregate_passage,
                                 bool greedily_aggregate_sibling_nodes,
                                 uint32_t max_passages,
                                 uint32_t min_words_per_passage)
    : max_words_per_aggregate_passage_(max_words_per_aggregate_passage),
      greedily_aggregate_sibling_nodes_(greedily_aggregate_sibling_nodes),
      max_passages_(max_passages),
      min_words_per_passage_(min_words_per_passage) {}

Vector<String> DocumentChunker::Chunk(const Node& tree) {
  AggregateNode root = ProcessNode(tree, 0, 0);
  if (root.passage_list.passages.empty()) {
    root.passage_list.AddPassageForNode(root, min_words_per_passage_);
  }

  Vector<String> passages(root.passage_list.passages);
  if (max_passages_ != 0 && passages.size() > max_passages_) {
    passages.Shrink(max_passages_);
  }

  return passages;
}

DocumentChunker::AggregateNode DocumentChunker::ProcessNode(
    const Node& node,
    int depth,
    uint32_t passage_count) {
  if (depth > 96 || (max_passages_ != 0 && passage_count >= max_passages_)) {
    // Limit processing of deep trees, and passages beyond the max.
    return {};
  }

  AggregateNode current_node;
  if (IsExcludedElement(node) || node.getNodeType() == Node::kCommentNode) {
    // Exclude text within these nodes.
    return current_node;
  }

  if (const HTMLIFrameElement* iframe = DynamicTo<HTMLIFrameElement>(&node)) {
    if (!ShouldContentExtractionIncludeIFrame(*iframe)) {
      return current_node;
    }
    const LocalFrame* local_frame = To<LocalFrame>(iframe->ContentFrame());
    return ProcessNode(*local_frame->GetDocument(), depth + 1, passage_count);
  }

  if (const Text* text = DynamicTo<Text>(node)) {
    String simplified_text = text->data().SimplifyWhiteSpace();
    if (!simplified_text.empty()) {
      current_node.num_words =
          WTF::VisitCharacters(simplified_text, [](auto chars) {
            return std::count(chars.begin(), chars.end(), ' ') + 1;
          });
      current_node.segments.push_back(simplified_text);
    }
    return current_node;
  }

  // Will hold the aggregate of this node and all its unchunked descendants
  // after we've recursed over all of its children.
  AggregateNode current_aggregating_node;

  // As above, but this holds the current greedy aggregate, which can be reset
  // when starting a new greedy aggregate passage (if the current greedy
  // aggregate is over max words, we hit a section break, or we hit a node
  // that is already part of another passage).
  AggregateNode current_greedy_aggregating_node;

  // Indicates whether we should attempt to aggregate the node being processed
  // in this function with its children. We only attempt to aggregate if we
  // can include all of its descendants in the aggregate.
  bool should_aggregate_current_node = true;

  // Will hold a list of descendant passages that should be added to this
  // current_node.passage_list if we do not end up aggregating the
  // current_node into a passage with its descendants.
  PassageList passage_list;

  for (const Node& child : NodeTraversal::ChildrenOf(node)) {
    AggregateNode child_node = ProcessNode(
        child, depth + 1, passage_count + passage_list.passages.size());
    if (!child_node.passage_list.passages.empty()) {
      should_aggregate_current_node = false;
      if (greedily_aggregate_sibling_nodes_) {
        passage_list.AddPassageForNode(current_greedy_aggregating_node,
                                       min_words_per_passage_);
        current_greedy_aggregating_node = AggregateNode();
      }
      passage_list.Extend(child_node.passage_list);
    } else {
      current_aggregating_node.AddNode(child_node);
      if (greedily_aggregate_sibling_nodes_) {
        if (!IsSectionBreak(child) &&
            current_greedy_aggregating_node.Fits(
                child_node, max_words_per_aggregate_passage_)) {
          current_greedy_aggregating_node.AddNode(child_node);
        } else {
          passage_list.AddPassageForNode(current_greedy_aggregating_node,
                                         min_words_per_passage_);
          current_greedy_aggregating_node = child_node;
        }
      } else {
        passage_list.AddPassageForNode(child_node, min_words_per_passage_);
      }
    }
  }

  if (greedily_aggregate_sibling_nodes_) {
    passage_list.AddPassageForNode(current_greedy_aggregating_node,
                                   min_words_per_passage_);
  }

  // If we should not or cannot aggregate this node, add passages for this
  // node and its descendant passages.
  if (!should_aggregate_current_node ||
      !current_node.Fits(current_aggregating_node,
                         max_words_per_aggregate_passage_)) {
    current_node.passage_list.AddPassageForNode(current_node,
                                                min_words_per_passage_);
    current_node.passage_list.Extend(passage_list);
    return current_node;
  }

  // Add this node to the aggregate.
  current_node.AddNode(current_aggregating_node);
  return current_node;
}

void DocumentChunker::PassageList::AddPassageForNode(
    const AggregateNode& node,
    size_t min_words_per_passage) {
  if (node.num_words < min_words_per_passage) {
    return;
  }

  String passage = node.CreatePassage();
  if (!passage.empty()) {
    passages.push_back(std::move(passage));
  }
}

void DocumentChunker::PassageList::Extend(const PassageList& passage_list) {
  passages.AppendVector(passage_list.passages);
}

bool DocumentChunker::AggregateNode::Fits(const AggregateNode& node,
                                          size_t max_words) {
  return num_words + node.num_words <= max_words;
}

void DocumentChunker::AggregateNode::AddNode(const AggregateNode& node) {
  num_words += node.num_words;
  segments.AppendVector(node.segments);
}

String DocumentChunker::AggregateNode::CreatePassage() const {
  if (segments.empty()) {
    return String();
  }
  StringBuilder builder;
  builder.Append(segments[0]);
  for (unsigned int i = 1; i < segments.size(); i++) {
    builder.Append(' ');
    builder.Append(segments[i]);
  }
  return builder.ReleaseString();
}

}  // namespace blink
