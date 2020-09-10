// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_TASK_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_TASK_SESSION_H_

#include <utility>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/node_id.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/core/content_capture/content_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Document;
class SentNodes;

// This class wraps the captured content and the detached nodes that need to be
// sent out by the ContentCaptureTask, it has a Document to DocumentSession
// mapping, and all data is grouped by document. There are two sources of data:
//
// One is the captured content which is set by the ContentCaptureTask through
// SetCapturedContent() only if the task session is empty, i.e all data must be
// sent before capturing the on-screen content, the captured content is then
// grouped into DocumentSession.
//
// Another is the detached nodes which are set by the ContentCaptureManager,
// they are saved to the DocumentSession directly.
//
// ContentCaptureTask gets the data per document by using
// GetUnsentDocumentSession() and GetNextUnsentNode(), and must send
// all data out before capturing on-screen content again.
class TaskSession final : public GarbageCollected<TaskSession> {
 public:
  // This class manages the captured content and the detached nodes per
  // document, the data is moved to the ContentCaptureTask while required. This
  // class has an instance per document, will be released while the associated
  // document is GC-ed, see TaskSession::to_document_session_.
  class DocumentSession final : public GarbageCollected<DocumentSession> {
   public:
    // The callback for total_sent_nodes_ metrics.
    using SentNodeCountCallback = base::RepeatingCallback<void(size_t)>;

    DocumentSession(const Document& document,
                    SentNodes& sent_nodes,
                    SentNodeCountCallback& call_back);
    ~DocumentSession();
    void AddCapturedNode(Node& node, const gfx::Rect& rect);
    void AddDetachedNode(int64_t id);
    void AddChangedNode(Node& node, const gfx::Rect& rect);
    bool HasUnsentData() const {
      return HasUnsentCapturedContent() || HasUnsentChangedContent() ||
             HasUnsentDetachedNodes();
    }
    bool HasUnsentCapturedContent() const {
      return !captured_content_.IsEmpty();
    }
    bool HasUnsentChangedContent() const { return !changed_content_.IsEmpty(); }
    bool HasUnsentDetachedNodes() const { return !detached_nodes_.empty(); }
    WebVector<int64_t> MoveDetachedNodes();
    const Document* GetDocument() const { return document_; }
    bool FirstDataHasSent() const { return first_data_has_sent_; }
    void SetFirstDataHasSent() { first_data_has_sent_ = true; }

    // Removes the unsent node from |captured_content_|, and returns it.
    ContentHolder* GetNextUnsentNode();

    ContentHolder* GetNextChangedNode();

    // Resets the |captured_content_| and the |detached_nodes_|, shall only be
    // used if those data doesn't need to be sent, e.g. there is no
    // WebContentCaptureClient for this document.
    void Reset();

    void Trace(Visitor*) const;

   private:
    // The captured content that belongs to this document.
    HeapHashMap<WeakMember<Node>, gfx::Rect> captured_content_;
    // The list of content id of node that has been detached from the
    // LayoutTree.
    WebVector<int64_t> detached_nodes_;
    WeakMember<const Document> document_;
    Member<SentNodes> sent_nodes_;
    // The list of changed nodes that needs to be sent.
    HeapHashMap<WeakMember<Node>, gfx::Rect> changed_content_;

    bool first_data_has_sent_ = false;
    // This is for the metrics to record the total node that has been sent.
    size_t total_sent_nodes_ = 0;
    // Histogram could be disabed in low time resolution OS, see
    // base::TimeTicks::IsHighResolution and ContentCaptureTask.
    base::Optional<SentNodeCountCallback> callback_;
  };

  TaskSession(SentNodes& sent_nodes);

  // Returns the DocumentSession that hasn't been sent.
  DocumentSession* GetNextUnsentDocumentSession();

  // This can only be invoked when all data has been sent (i.e. HasUnsentData()
  // returns False).
  void SetCapturedContent(const Vector<cc::NodeInfo>& captured_content);

  void OnNodeDetached(const Node& node);

  void OnNodeChanged(Node& node);

  bool HasUnsentData() const { return has_unsent_data_; }

  void SetSentNodeCountCallback(
      DocumentSession::SentNodeCountCallback call_back) {
    callback_ = std::move(call_back);
  }

  void Trace(Visitor*) const;

  void ClearDocumentSessionsForTesting();

 private:
  void GroupCapturedContentByDocument(
      const Vector<cc::NodeInfo>& captured_content);
  DocumentSession& EnsureDocumentSession(const Document& doc);
  DocumentSession* GetDocumentSession(const Document& document) const;

  Member<SentNodes> sent_nodes_;

  // The list of node whose value has changed.
  HeapHashSet<WeakMember<Node>> changed_nodes_;

  // This owns the DocumentSession which is released along with Document.
  HeapHashMap<WeakMember<const Document>, Member<DocumentSession>>
      to_document_session_;

  // Because the captured content and the detached node are in the
  // DocumentSession, this is used to avoid to iterate all document sessions
  // to find out if there is any of them.
  bool has_unsent_data_ = false;
  DocumentSession::SentNodeCountCallback callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_TASK_SESSION_H_
