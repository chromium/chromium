// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/task_session.h"

#include <utility>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"

namespace blink {

namespace {
bool IsConstantStreamingEnabled() {
  return base::FeatureList::IsEnabled(
      features::kContentCaptureConstantStreaming);
}

}  // namespace

TaskSession::DocumentSession::DocumentSession(const Document& document,
                                              SentNodeCountCallback& callback)
    : document_(&document), callback_(callback) {}

TaskSession::DocumentSession::~DocumentSession() {
  if (callback_.has_value()) {
    callback_.value().Run(total_sent_nodes_);
  }
}

bool TaskSession::DocumentSession::AddDetachedNode(const Node& node) {
  // Only notify the detachment of visible node which shall be in |sent_nodes|
  // or |changed_nodes|.
  // Take the node out of |sent_nodes| or |changed_nodes|, otherwise, the |node|
  // would be found invisible in next capturing and be reported as the removed
  // node again.
  if (sent_nodes_.Take(&node) || changed_nodes_.Take(&node)) {
    detached_nodes_.emplace_back(reinterpret_cast<int64_t>(&node));
    return true;
  }
  return false;
}

WebVector<int64_t> TaskSession::DocumentSession::MoveDetachedNodes() {
  return std::move(detached_nodes_);
}

ContentHolder* TaskSession::DocumentSession::GetNextUnsentNode() {
  while (!captured_content_.empty()) {
    auto node = captured_content_.begin()->key;
    const gfx::Rect rect = captured_content_.Take(node);
    if (node && node->GetLayoutObject() && !sent_nodes_.Contains(node)) {
      sent_nodes_.insert(WeakMember<const Node>(node));
      total_sent_nodes_++;
      return MakeGarbageCollected<ContentHolder>(node, rect);
    }
  }
  return nullptr;
}

ContentHolder* TaskSession::DocumentSession::GetNextChangedNode() {
  while (!changed_content_.empty()) {
    auto node = changed_content_.begin()->key;
    const gfx::Rect rect = changed_content_.Take(node);
    if (node.Get() && node->GetLayoutObject()) {
      sent_nodes_.insert(WeakMember<const Node>(node));
      total_sent_nodes_++;
      return MakeGarbageCollected<ContentHolder>(node, rect);
    }
  }
  return nullptr;
}

bool TaskSession::DocumentSession::AddChangedNode(Node& node) {
  // No need to save the node that hasn't been sent because it will be captured
  // once being on screen.
  if (sent_nodes_.Contains(&node)) {
    changed_nodes_.insert(WeakMember<const Node>(&node));
    return true;
  }
  return false;
}

void TaskSession::DocumentSession::OnContentCaptured(
    Node& node,
    const gfx::Rect& visual_rect) {
  if (changed_nodes_.Take(&node)) {
    changed_content_.Set(WeakMember<Node>(&node), visual_rect);
    if (IsConstantStreamingEnabled())
      sent_nodes_.Take(&node);
  } else {
    if (IsConstantStreamingEnabled()) {
      if (auto value = sent_nodes_.Take(&node))
        visible_sent_nodes_.insert(value);
      else
        captured_content_.Set(WeakMember<Node>(&node), visual_rect);
    } else {
      if (!sent_nodes_.Contains(&node))
        captured_content_.Set(WeakMember<Node>(&node), visual_rect);
      // else |node| has been sent and unchanged.
    }
  }
}

void TaskSession::DocumentSession::OnGroupingComplete() {
  if (!IsConstantStreamingEnabled())
    return;

  // All nodes in |sent_nodes_| aren't visible any more, remove them.
  for (auto weak_node : sent_nodes_) {
    if (auto* node = weak_node.Get())
      detached_nodes_.emplace_back(reinterpret_cast<int64_t>(node));
  }
  // |visible_sent_nodes_| are still visible and moved to |sent_nodes_|.
  sent_nodes_.swap(visible_sent_nodes_);
  visible_sent_nodes_.clear();
  // Any node in |changed_nodes_| isn't visible any more and shall be clear.
  changed_nodes_.clear();
}

void TaskSession::DocumentSession::Trace(Visitor* visitor) const {
  visitor->Trace(captured_content_);
  visitor->Trace(changed_content_);
  visitor->Trace(document_);
  visitor->Trace(sent_nodes_);
  visitor->Trace(visible_sent_nodes_);
  visitor->Trace(changed_nodes_);
}

void TaskSession::DocumentSession::Reset() {
  changed_content_.clear();
  captured_content_.clear();
  detached_nodes_.clear();
  sent_nodes_.clear();
  visible_sent_nodes_.clear();
  changed_nodes_.clear();
}

TaskSession::TaskSession() = default;

TaskSession::DocumentSession* TaskSession::GetNextUnsentDocumentSession() {
  for (auto& doc : to_document_session_.Values()) {
    if (!doc->HasUnsentData())
      continue;
    return doc.Get();
  }
  has_unsent_data_ = false;
  return nullptr;
}

void TaskSession::SetCapturedContent(
    const Vector<cc::NodeInfo>& captured_content) {
  DCHECK(!HasUnsentData());
  DCHECK(!captured_content.empty());
  GroupCapturedContentByDocument(captured_content);
  has_unsent_data_ = true;
}

void TaskSession::GroupCapturedContentByDocument(
    const Vector<cc::NodeInfo>& captured_content) {
  // In rare cases, the same node could have multiple entries in the
  // |captured_content|, but the visual_rect are almost same, we just let the
  // later replace the previous.
  for (const auto& i : captured_content) {
    if (Node* node = DOMNodeIds::NodeForId(i.node_id)) {
      EnsureDocumentSession(node->GetDocument())
          .OnContentCaptured(*node, i.visual_rect);
    }
  }
  for (auto doc_session : to_document_session_.Values()) {
    doc_session->OnGroupingComplete();
  }
}

void TaskSession::OnNodeDetached(const Node& node) {
  if (EnsureDocumentSession(node.GetDocument()).AddDetachedNode(node))
    has_unsent_data_ = true;
}

void TaskSession::OnNodeChanged(Node& node) {
  if (EnsureDocumentSession(node.GetDocument()).AddChangedNode(node))
    has_unsent_data_ = true;
}

TaskSession::DocumentSession& TaskSession::EnsureDocumentSession(
    const Document& doc) {
  DocumentSession* doc_session = GetDocumentSession(doc);
  if (!doc_session) {
    doc_session = MakeGarbageCollected<DocumentSession>(doc, callback_);
    to_document_session_.insert(&doc, doc_session);
  }
  return *doc_session;
}

TaskSession::DocumentSession* TaskSession::GetDocumentSession(
    const Document& document) const {
  auto it = to_document_session_.find(&document);
  if (it == to_document_session_.end())
    return nullptr;
  return it->value.Get();
}

void TaskSession::Trace(Visitor* visitor) const {
  visitor->Trace(to_document_session_);
}

void TaskSession::ClearDocumentSessionsForTesting() {
  to_document_session_.clear();
}

}  // namespace blink
