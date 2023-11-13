// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/document_associated_data.h"

#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/no_destructor.h"
#include "content/browser/navigation_or_document_handle.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/page_factory.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/document_service_internal.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

namespace {
auto& GetDocumentTokenMap() {
  static base::NoDestructor<std::unordered_map<
      blink::DocumentToken, RenderFrameHostImpl*, blink::DocumentToken::Hasher>>
      map;
  return *map;
}
}  // namespace

RenderFrameHostImpl* DocumentAssociatedData::GetDocumentFromToken(
    base::PassKey<RenderFrameHostImpl>,
    const blink::DocumentToken& token) {
  return base::FindPtrOrNull(GetDocumentTokenMap(), token);
}

DocumentAssociatedData::DocumentAssociatedData(
    RenderFrameHostImpl& document,
    const blink::DocumentToken& token)
    : token_(token), weak_factory_(&document) {
  auto [_, inserted] = GetDocumentTokenMap().insert({token_, &document});
  CHECK(inserted);

  // Only create page object for the main document as the PageImpl is 1:1 with
  // main document.
  if (!document.GetParent()) {
    PageDelegate* page_delegate = document.frame_tree()->page_delegate();
    DCHECK(page_delegate);
    owned_page_ = PageFactory::Create(document, *page_delegate);
  }
}

void DocumentAssociatedData::RemoveAllServices() {
  while (!services_.empty()) {
    // DocumentServiceBase unregisters itself at destruction time.
    services_.back()->WillBeDestroyed(
        DocumentServiceDestructionReason::kEndOfDocumentLifetime);
    services_.back()->ResetAndDeleteThis();
  }
}

DocumentAssociatedData::~DocumentAssociatedData() {
  RemoveAllServices();

  // Explicitly clear all user data here, so that the other fields of
  // DocumentAssociatedData are still valid while user data is being destroyed.
  ClearAllUserData();

  // Last in case any DocumentService / DocumentUserData service destructors try
  // to look up RenderFrameHosts by DocumentToken.
  CHECK_EQ(1u, GetDocumentTokenMap().erase(token_));
}

void DocumentAssociatedData::set_navigation_or_document_handle(
    scoped_refptr<NavigationOrDocumentHandle> handle) {
  navigation_or_document_handle_ = std::move(handle);
}

}  // namespace content
