// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_PUSH_MESSAGING_SERVICE_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_PUSH_MESSAGING_SERVICE_IMPL_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/version_info/channel.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_driver.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_messaging_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

class PushMessagingServiceImpl : public content::PushMessagingService,
                                 public gcm::GCMAppHandler {
 public:
  PushMessagingServiceImpl(content::BrowserContext&,
                           os_crypt_async::OSCryptAsync*);
  ~PushMessagingServiceImpl() override;

  // PushMessagingService implementations.
  void SubscribeFromDocument(const GURL& requesting_origin,
                             int64_t service_worker_registration_id,
                             int render_process_id,
                             int render_frame_id,
                             blink::mojom::PushSubscriptionOptionsPtr options,
                             bool user_gesture,
                             RegisterCallback callback) override;
  void SubscribeFromWorker(const GURL& requesting_origin,
                           int64_t service_worker_registration_id,
                           int render_process_id,
                           blink::mojom::PushSubscriptionOptionsPtr options,
                           RegisterCallback callback) override;
  void GetSubscriptionInfo(const GURL& origin,
                           int64_t service_worker_registration_id,
                           const std::string& sender_id,
                           const std::string& subscription_id,
                           SubscriptionInfoCallback callback) override;
  void Unsubscribe(blink::mojom::PushUnregistrationReason reason,
                   const GURL& requesting_origin,
                   int64_t service_worker_registration_id,
                   const std::string& sender_id,
                   UnregisterCallback callback) override;
  bool SupportNonVisibleMessages() override;
  void DidDeleteServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id) override;
  void DidDeleteServiceWorkerDatabase() override;

  // GCMAppHandler implementations.
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

 private:
  gcm::GCMDriver& GetGCMDriver();

  void RequestProxyResolvingSocketFactory(
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
          receiver);

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      const;

  version_info::Channel GetChannel() const;

  std::string GetProductCategoryForSubtypes() const;

  // Lazy initialized.
  std::unique_ptr<gcm::GCMDriver> gcm_driver_;

  // Outlive this instance.
  content::BrowserContext& parent_context_;
  raw_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be the last member variable.
  base::WeakPtrFactory<PushMessagingServiceImpl> weak_ptr_factory_{this};
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_PUSH_MESSAGING_SERVICE_IMPL_H_
