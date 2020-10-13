// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/task_session.h"

#include <utility>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"

namespace blink {

TaskSession::DocumentSession::DocumentSession(
    const Document& document,
    HeapHashSet<WeakMember<const Node>>& sent_nodes,
    SentNodeCountCallback& callback)
    : document_(&document), sent_nodes_(&sent_nodes), callback_(callback) {}

TaskSession::DocumentSession::~DocumentSession() {
  if (callback_.has_value())
    callback_.value().Run(total_sent_nodes_);
}

void TaskSession::DocumentSession::AddCapturedNode(Node& node,
                                                   const gfx::Rect& rect) {
  // Replace the previous rect if any.
  captured_content_.Set(WeakMember<Node>(&node), rect);
}

void TaskSession::DocumentSession::AddDetachedNode(int64_t id) {
  detached_nodes_.emplace_back(id);
}

void TaskSession::DocumentSession::AddChangedNode(Node& node,
                                                  const gfx::Rect& rect) {
  // Replace the previous rect if any.
  changed_content_.Set(WeakMember<Node>(&node), rect);
}

WebVector<int64_t> TaskSession::DocumentSession::MoveDetachedNodes() {
  return std::move(detached_nodes_);
}

ContentHolder* TaskSession::DocumentSession::GetNextUnsentNode() {
  while (!captured_content_.IsEmpty()) {
    auto node = captured_content_.begin()->key;
    const gfx::Rect rect = captured_content_.Take(node);
    if (node && node->GetLayoutObject() && !sent_nodes_->Contains(node)) {
      sent_nodes_->insert(WeakMember<const Node>(node));
      total_sent_nodes_++;
      return MakeGarbageCollected<ContentHolder>(node, rect);
    }
  }
  return nullptr;
}

ContentHolder* TaskSession::DocumentSession::GetNextChangedNode() {
  while (!changed_content_.IsEmpty()) {
    auto node = changed_content_.begin()->key;
    const gfx::Rect rect = changed_content_.Take(node);
    if (node.Get() && node->GetLayoutObject()) {
      total_sent_nodes_++;
      return MakeGarbageCollected<ContentHolder>(node, rect);
    }
  }
  return nullptr;
}

void TaskSession::DocumentSession::Trace(Visitor* visitor) const {
  visitor->Trace(captured_content_);
  visitor->Trace(document_);
  visitor->Trace(changed_content_);
}

void TaskSession::DocumentSession::Reset() {
  changed_content_.clear();
  captured_content_.clear();
  detached_nodes_.Clear();
}

TaskSession::TaskSession() = default;

TaskSession::DocumentSession* TaskSession::GetNextUnsentDocumentSession() {
  for (auto& doc : to_document_session_.Values()) {
    if (!doc->HasUnsentData())
      continue;
    return doc;
  }
  has_unsent_data_ = false;
  return nullptr;
}

void TaskSession::SetCapturedContent(
    const Vector<cc::NodeInfo>& captured_content) {
  DCHECK(!HasUnsentData());
  DCHECK(!captured_content.IsEmpty());
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
      if (changed_nodes_.Take(node)) {
        // The changed node might not be sent.
        if (sent_nodes_.Contains(node)) {
          EnsureDocumentSession(node->GetDocument())
              .AddChangedNode(*node, i.visual_rect);
        } else {
          EnsureDocumentSession(node->GetDocument())
              .AddCapturedNode(*node, i.visual_rect);
        }
        continue;
      }
      if (!sent_nodes_.Contains(node)) {
        EnsureDocumentSession(node->GetDocument())
            .AddCapturedNode(*node, i.visual_rect);
      }
    }
  }
}

void TaskSession::OnNodeDetached(const Node& node) {
  if (sent_nodes_.Contains(&node)) {
    EnsureDocumentSession(node.GetDocument())
        .AddDetachedNode(reinterpret_cast<int64_t>(&node));
    has_unsent_data_ = true;
  }
}

void TaskSession::OnNodeChanged(Node& node) {
  changed_nodes_.insert(WeakMember<Node>(&node));
}

TaskSession::DocumentSession& TaskSession::EnsureDocumentSession(
    const Document& doc) {
  DocumentSession* doc_session = GetDocumentSession(doc);
  if (!doc_session) {
    doc_session =
        MakeGarbageCollected<DocumentSession>(doc, sent_nodes_, callback_);
    to_document_session_.insert(&doc, doc_session);
  }
  return *doc_session;
}

TaskSession::DocumentSession* TaskSession::GetDocumentSession(
    const Document& document) const {
  auto it = to_document_session_.find(&document);
  if (it == to_document_session_.end())
    return nullptr;
  return it->value;
}

void TaskSession::Trace(Visitor* visitor) const {
  visitor->Trace(sent_nodes_);
  visitor->Trace(changed_nodes_);
  visitor->Trace(to_document_session_);
}

void TaskSession::ClearDocumentSessionsForTesting() {
  to_document_session_.clear();
}

}  // namespace blink
