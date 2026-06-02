// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_LOADER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/skeleton/skeleton.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class Document;

// One per document. Handling loading and instantiation of necessary skeletons.
class CORE_EXPORT SkeletonLoader : public GarbageCollected<SkeletonLoader>,
                                   public Skeleton::Observer,
                                   public Supplement<Document> {
 public:
  static const char kSupplementName[];

  explicit SkeletonLoader(Document& owner_document);

  virtual ~SkeletonLoader() = default;

  // Supplement support
  static SkeletonLoader* Get(Document&);
  static SkeletonLoader& Ensure(Document&);

  // Trigger prefetching of the skeleton for a given url
  void AddSkeletonPrefetchLink(KURL url);

  // A navigation to the passed-in url is happening. Render a skeleton for that
  // url as soon as possible.
  void NavigateTo(KURL url);

  void CancelNavigation();

  void RestoringFromBFCache();

  void Trace(Visitor* visitor) const final;

 private:
  Document& GetDocument() { return *GetSupplementable(); }

  // Skeleton::Observer implementation
  void DocumentReady(Skeleton&) final;

  // Sanitize and insert the skeleton document when loading finished
  void UpdateSkeletonTree();

  // Remove the ::skeleton pseudo subtree
  void RemoveSkeletonTree();

  // Insert a parsed skeleton document tree into the UA shadow of a ::skeleton
  // pseudo-element
  void InsertSkeletonTree(Document& skeleton_document);

  // The currently rendered skeleton
  Member<Skeleton> skeleton_;

  // The set of urls we should load skeletons for.
  HashSet<KURL> use_skeleton_for_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_LOADER_H_
