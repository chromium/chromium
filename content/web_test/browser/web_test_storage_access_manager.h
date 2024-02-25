// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_STORAGE_ACCESS_MANAGER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_STORAGE_ACCESS_MANAGER_H_

#include <stddef.h>

#include "base/containers/id_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/common/content_settings.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/storage_access/storage_access_automation.mojom.h"

namespace content {

class BrowserContext;

class WebTestStorageAccessManager
    : public blink::test::mojom::StorageAccessAutomation {
 public:
  explicit WebTestStorageAccessManager(BrowserContext* browser_context);

  WebTestStorageAccessManager(const WebTestStorageAccessManager&) = delete;
  WebTestStorageAccessManager& operator=(const WebTestStorageAccessManager&) =
      delete;

  ~WebTestStorageAccessManager() override;

  // blink::test::mojom::StorageAccessAutomation
  void SetStorageAccess(
      const std::string& origin,
      const std::string& embedding_origin,
      const bool blocked,
      blink::test::mojom::StorageAccessAutomation::SetStorageAccessCallback)
      override;

  void Bind(mojo::PendingReceiver<blink::test::mojom::StorageAccessAutomation>
                receiver);

 private:
  raw_ref<BrowserContext> browser_context_;

  mojo::ReceiverSet<blink::test::mojom::StorageAccessAutomation> receivers_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_STORAGE_ACCESS_MANAGER_H_
