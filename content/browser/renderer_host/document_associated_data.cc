// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/document_associated_data.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/containers/queue.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "content/browser/navigation_or_document_handle.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/page_factory.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/document_service_internal.h"
#include "content/public/browser/render_frame_host.h"
#include "net/cookies/cookie_setting_override.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

namespace {
auto& GetDocumentTokenMap() {
  static base::NoDestructor<
      absl::flat_hash_map<blink::DocumentToken, RenderFrameHostImpl*>>
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
    : token_(token),
      network_restrictions_id_(std::nullopt),
      weak_factory_(&document) {
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

DocumentAssociatedData::~DocumentAssociatedData() {
  TRACE_EVENT0("navigation", "DocumentAssociatedData::~DocumentAssociatedData");
  base::ScopedUmaHistogramTimer histogram_timer(
      "Navigation.DocumentAssociatedDataDestructor");
  decltype(services_) services;
  std::swap(services_, services);
  for (auto& service : services) {
    service->WillBeDestroyed(
        DocumentServiceDestructionReason::kEndOfDocumentLifetime);
    service->ResetAndDeleteThisInternal({});
  }

  // Explicitly clear all user data here, so that the other fields of
  // DocumentAssociatedData are still valid while user data is being destroyed.
  ClearAllUserData();

  // Explicitly clear all PageUserData here before destruction of |owned_page_|
  // (A std::unique_ptr's stored pointer value is (intentionally) undefined
  // during destruction (e.g. it could be nullptr)), so that |owned_page_| and
  // the other fields of DocumentAssociatedData are still valid and accessible
  // from RenderFrameHost interface while its page user data is being destroyed.
  if (owned_page_) {
    owned_page_->ClearAllUserData();
  }

  // Remove any network restrictions for this document from the network service
  if (network_restrictions_id_.has_value()) {
    StoragePartitionImpl* storage_partition =
        GetWeakPtr()->GetStoragePartition();
    storage_partition->ClearNoncesInNetworkContextAfterDelay({
        network_restrictions_id_.value(),
    });
  }

  // Last in case any DocumentService / DocumentUserData service destructors try
  // to look up RenderFrameHosts by DocumentToken.
  CHECK_EQ(1u, GetDocumentTokenMap().erase(token_));
}

void DocumentAssociatedData::set_navigation_or_document_handle(
    scoped_refptr<NavigationOrDocumentHandle> handle) {
  navigation_or_document_handle_ = std::move(handle);
}

void DocumentAssociatedData::AddService(
    internal::DocumentServiceBase* service,
    base::PassKey<internal::DocumentServiceBase>) {
  services_.push_back(service);
}

void DocumentAssociatedData::RemoveService(
    internal::DocumentServiceBase* service,
    base::PassKey<internal::DocumentServiceBase>) {
  std::erase(services_, service);
}

void DocumentAssociatedData::AddPostPrerenderingActivationStep(
    base::OnceClosure callback) {
  CHECK_EQ(GetSafeRef()->GetLifecycleState(),
           RenderFrameHost::LifecycleState::kPrerendering);
  post_prerendering_activation_callbacks_.push(std::move(callback));
}

void DocumentAssociatedData::RunPostPrerenderingActivationSteps() {
  CHECK_NE(GetSafeRef()->GetLifecycleState(),
           RenderFrameHost::LifecycleState::kPrerendering);
  while (!post_prerendering_activation_callbacks_.empty()) {
    std::move(post_prerendering_activation_callbacks_.front()).Run();
    post_prerendering_activation_callbacks_.pop();
  }
}

void DocumentAssociatedData::PutCookieSettingOverride(
    net::CookieSettingOverride cookie_setting_override) {
  cookie_setting_overrides_.Put(cookie_setting_override);
}

void DocumentAssociatedData::RemoveCookieSettingOverride(
    net::CookieSettingOverride cookie_setting_override) {
  cookie_setting_overrides_.Remove(cookie_setting_override);
}

}  // namespace content
