// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/skeleton/skeleton_loader.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_builtins.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"

namespace blink {

// static
const char SkeletonLoader::kSupplementName[] = "SkeletonLoader";

SkeletonLoader::SkeletonLoader(Document& owner_document)
    : Supplement<Document>(owner_document) {}

// static
SkeletonLoader* SkeletonLoader::Get(Document& document) {
  return Supplement<Document>::From<SkeletonLoader>(document);
}

// static
SkeletonLoader& SkeletonLoader::Ensure(Document& document) {
  SkeletonLoader* loader = Get(document);
  if (!loader) {
    loader = MakeGarbageCollected<SkeletonLoader>(document);
    Supplement<Document>::ProvideTo<SkeletonLoader>(document, loader);
  }
  return *loader;
}

void SkeletonLoader::AddSkeletonPrefetchLink(KURL url) {
  use_skeleton_for_.insert(url);
}

void SkeletonLoader::NavigateTo(KURL url) {
  if (use_skeleton_for_.Contains(url)) {
    skeleton_ = MakeGarbageCollected<Skeleton>(*this);
    skeleton_->Render(url, GetDocument());
  }
}

void SkeletonLoader::CancelNavigation() {
  RemoveSkeletonTree();
  skeleton_ = nullptr;
}

void SkeletonLoader::RestoringFromBFCache() {
  RemoveSkeletonTree();
  skeleton_ = nullptr;
}

void SkeletonLoader::DocumentReady(Skeleton& skeleton) {
  if (&skeleton == skeleton_.Get()) {
    UpdateSkeletonTree();
  }
}

void SkeletonLoader::UpdateSkeletonTree() {
  Document& skeleton_document = skeleton_->GetDocument();
  const Sanitizer* sanitizer = SanitizerBuiltins::GetBaseline();
  sanitizer->SanitizeSafe(&skeleton_document);

  InsertSkeletonTree(skeleton_document);
}

void SkeletonLoader::RemoveSkeletonTree() {
  if (Element* root = GetDocument().documentElement()) {
    root->ClearSkeletonPseudo();
  }
}

void SkeletonLoader::InsertSkeletonTree(Document& skeleton_document) {
  if (Element* root = GetDocument().documentElement()) {
    PseudoElement& skeleton_pseudo = root->EnsureSkeletonPseudo();
    ShadowRoot& shadow_root = skeleton_pseudo.EnsureUserAgentShadowRoot();
    CHECK_EQ(shadow_root.firstChild(), nullptr);
    if (Element* skeleton_root = skeleton_document.documentElement()) {
      shadow_root.AppendChild(skeleton_root);
    }
  }
}

void SkeletonLoader::Trace(Visitor* visitor) const {
  visitor->Trace(skeleton_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
