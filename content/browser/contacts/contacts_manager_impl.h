// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTACTS_CONTACTS_MANAGER_IMPL_H_
#define CONTENT_BROWSER_CONTACTS_CONTACTS_MANAGER_IMPL_H_

#include "content/browser/contacts/contacts_provider.h"
#include "content/public/browser/document_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/contacts/contacts_manager.mojom.h"

namespace content {

class RenderFrameHostImpl;

class ContactsManagerImpl
    : public DocumentService<blink::mojom::ContactsManager> {
 public:
  static void Create(
      RenderFrameHostImpl* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ContactsManager> receiver) {
    CHECK(render_frame_host);
    // The object is bound to the lifetime of `render_frame_host`'s logical
    // document by virtue of being a `DocumentService` implementation.
    new ContactsManagerImpl(*render_frame_host, std::move(receiver));
  }

  ContactsManagerImpl(const ContactsManagerImpl&) = delete;
  ContactsManagerImpl& operator=(const ContactsManagerImpl&) = delete;

  ~ContactsManagerImpl() override;

  void Select(bool multiple,
              bool include_names,
              bool include_emails,
              bool include_tel,
              bool include_addresses,
              bool include_icons,
              SelectCallback mojom_callback) override;

 private:
  explicit ContactsManagerImpl(
      RenderFrameHostImpl& render_frame_host,
      mojo::PendingReceiver<blink::mojom::ContactsManager> receiver);

  std::unique_ptr<ContactsProvider> contacts_provider_;

  // The source id to use when reporting back UKM statistics.
  ukm::SourceId source_id_ = ukm::kInvalidSourceId;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTACTS_CONTACTS_MANAGER_IMPL_H_
