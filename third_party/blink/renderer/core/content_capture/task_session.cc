// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/task_session.h"

#include <utility>

#include "third_party/blink/renderer/core/content_capture/sent_nodes.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"

namespace blink {

TaskSession::DocumentSession::DocumentSession(const Document& document,
                                              SentNodes& sent_nodes,
                                              SentNodeCountCallback& callback)
    : document_(&document), sent_nodes_(&sent_nodes), callback_(callback) {}

TaskSession::DocumentSession::~DocumentSession() {
  if (callback_.has_value())
    callback_.value().Run(total_sent_nodes_);
}

void TaskSession::DocumentSession::AddCapturedNode(Node& node) {
  captured_content_.insert(WeakMember<Node>(&node));
}

void TaskSession::DocumentSession::AddDetachedNode(int64_t id) {
  detached_nodes_.emplace_back(id);
}

void TaskSession::DocumentSession::AddChangedNode(Node& node) {
  changed_content_.insert(WeakMember<Node>(&node));
}

WebVector<int64_t> TaskSession::DocumentSession::MoveDetachedNodes() {
  return std::move(detached_nodes_);
}

Node* TaskSession::DocumentSession::GetNextUnsentNode() {
  while (!captured_content_.IsEmpty()) {
    Node* node = captured_content_.TakeAny().Get();
    if (node && node->GetLayoutObject() && !sent_nodes_->HasSent(*node)) {
      sent_nodes_->OnSent(*node);
      total_sent_nodes_++;
      return node;
    }
  }
  return nullptr;
}

Node* TaskSession::DocumentSession::GetNextChangedNode() {
  while (!changed_content_.IsEmpty()) {
    Node* node = changed_content_.TakeAny().Get();
    if (node && node->GetLayoutObject()) {
      total_sent_nodes_++;
      return node;
    }
  }
  return nullptr;
}

void TaskSession::DocumentSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(captured_content_);
  visitor->Trace(sent_nodes_);
  visitor->Trace(document_);
  visitor->Trace(changed_content_);
}

void TaskSession::DocumentSession::Reset() {
  changed_content_.clear();
  captured_content_.clear();
  detached_nodes_.Clear();
}

TaskSession::TaskSession(SentNodes& sent_nodes) : sent_nodes_(sent_nodes) {}

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
    const Vector<cc::NodeId>& captured_content) {
  DCHECK(!HasUnsentData());
  DCHECK(!captured_content.IsEmpty());
  GroupCapturedContentByDocument(captured_content);
  has_unsent_data_ = true;
}

void TaskSession::GroupCapturedContentByDocument(
    const Vector<cc::NodeId>& captured_content) {
  for (const cc::NodeId& node_id : captured_content) {
    if (Node* node = DOMNodeIds::NodeForId(node_id)) {
      if (changed_nodes_.Take(node)) {
        // The changed node might not be sent.
        if (sent_nodes_->HasSent(*node)) {
          EnsureDocumentSession(node->GetDocument()).AddChangedNode(*node);
        } else {
          EnsureDocumentSession(node->GetDocument()).AddCapturedNode(*node);
        }
        continue;
      }
      if (!sent_nodes_->HasSent(*node)) {
        EnsureDocumentSession(node->GetDocument()).AddCapturedNode(*node);
      }
    }
  }
}

void TaskSession::OnNodeDetached(const Node& node) {
  if (sent_nodes_->HasSent(node)) {
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
        MakeGarbageCollected<DocumentSession>(doc, *sent_nodes_, callback_);
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

void TaskSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(sent_nodes_);
  visitor->Trace(changed_nodes_);
  visitor->Trace(to_document_session_);
}

void TaskSession::ClearDocumentSessionsForTesting() {
  to_document_session_.clear();
}

}  // namespace blink
