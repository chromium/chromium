// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/contacts/contacts_manager_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/contacts_picker_properties.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/contacts/contacts_provider_android.h"
#endif

namespace content {

namespace {

std::unique_ptr<ContactsProvider> CreateProvider(
    RenderFrameHostImpl& render_frame_host) {
  if (render_frame_host.GetParentOrOuterDocument())
    return nullptr;  // This API is only supported on the main frame.
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<ContactsProviderAndroid>(&render_frame_host);
#else
  return nullptr;
#endif
}

void OnContactsSelected(
    blink::mojom::ContactsManager::SelectCallback callback,
    ukm::SourceId source_id,
    std::optional<std::vector<blink::mojom::ContactInfoPtr>> contacts,
    int percentage_shared,
    ContactsPickerProperties properties_requested) {
  if (contacts != std::nullopt) {
    int select_count = contacts.value().size();
    ukm::builders::ContactsPicker_ShareStatistics(source_id)
        .SetSelectCount(ukm::GetExponentialBucketMinForCounts1000(select_count))
        .SetSelectPercentage(percentage_shared)
        .SetPropertiesRequested(properties_requested)
        .Record(ukm::UkmRecorder::Get());
  }
  std::move(callback).Run(std::move(contacts));
}

}  // namespace

ContactsManagerImpl::ContactsManagerImpl(
    RenderFrameHostImpl& render_frame_host,
    mojo::PendingReceiver<blink::mojom::ContactsManager> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      contacts_provider_(CreateProvider(render_frame_host)) {
  CHECK(!render_frame_host.IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));
  source_id_ = render_frame_host.GetPageUkmSourceId();
}

ContactsManagerImpl::~ContactsManagerImpl() = default;

void ContactsManagerImpl::Select(bool multiple,
                                 bool include_names,
                                 bool include_emails,
                                 bool include_tel,
                                 bool include_addresses,
                                 bool include_icons,
                                 SelectCallback mojom_callback) {
  if (contacts_provider_) {
    contacts_provider_->Select(
        multiple, include_names, include_emails, include_tel, include_addresses,
        include_icons,
        base::BindOnce(&OnContactsSelected, std::move(mojom_callback),
                       source_id_));
  } else {
    std::move(mojom_callback).Run(std::nullopt);
  }
}

}  // namespace content
