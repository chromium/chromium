// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_PUSH_MESSAGING_SERVICE_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_PUSH_MESSAGING_SERVICE_IMPL_H_

#include <memory>
#include <optional>

#include "base/sequence_checker.h"
#include "base/version_info/channel.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/push_messaging/app_identifier.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_messaging_service.h"

class GURL;

namespace content {
class BrowserContext;
}  // namespace content

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

namespace network {
class NetworkConnectionTracker;
}  // namespace network

// The PushMessagingService implementation dedicated for WebEngine since the
// //chrome/browser/push_messaging implementation uses Profile which is not a
// concept in //fuchsia_web.
// Most of the implementations are copied from
// //chrome/browser/push_messaging/push_messaging_service_impl but modified to
// fit the limits of WebEngine.
//
// The principle of this implementation is to ensure the WebAPI compatibility
// between a full functional Chrome and WebEngine. WebApp should function the
// same except for the designated specific behaviors in this implementation,
// e.g. no notification support; no per-profile messaging; no user controllable
// permission.
//
// TODO(crbug.com/424479300): Move more shared logic from
// //chrome/browser/push_messaging/ to //contents/browser/push_messaging/ and
// //components/push_messaging/ to reduce the duplications.
class PushMessagingServiceImpl final : public content::PushMessagingService,
                                       public gcm::GCMAppHandler {
 public:
  PushMessagingServiceImpl(content::BrowserContext&,
                           os_crypt_async::OSCryptAsync&,
                           network::NetworkConnectionTracker&);
  ~PushMessagingServiceImpl() override;

  // PushMessagingService implementations.
  void SubscribeFromDocument(const GURL& origin,
                             int64_t service_worker_registration_id,
                             int render_process_id,
                             int render_frame_id,
                             blink::mojom::PushSubscriptionOptionsPtr options,
                             bool user_gesture,
                             RegisterCallback callback) override;
  void SubscribeFromWorker(const GURL& origin,
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
                   const GURL& origin,
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
  bool CanHandle(const std::string& app_id) const override;

 private:
  gcm::GCMDriver& GetGCMDriver();
  instance_id::InstanceIDDriver& GetInstanceIDDriver();

  void RequestProxyResolvingSocketFactoryOnUIThread(
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
          receiver);

  static void RequestProxyResolvingSocketFactory(
      base::WeakPtr<PushMessagingServiceImpl> self,
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
          receiver);

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      const;

  version_info::Channel GetChannel() const;

  std::string GetProductCategoryForSubtypes() const;

  // Shared implementation for both SubscribeFromDocument and
  // SubscribeFromWorker.
  void DoSubscribe(const GURL& origin,
                   int64_t service_worker_registration_id,
                   blink::mojom::PushSubscriptionOptionsPtr options,
                   RegisterCallback callback);

  void DidSubscribe(const push_messaging::AppIdentifier& app_identifier,
                    const std::string& sender_id,
                    RegisterCallback callback,
                    const std::string& subscription_id,
                    instance_id::InstanceID::Result result);

  // An O(N) search on all the subscriptions to find the match of
  // |origin| and |service_worker_registration_id|.
  std::optional<push_messaging::AppIdentifier> FindByServiceWorker(
      const GURL& origin,
      int64_t service_worker_registration_id) const;

  void DidSubscribeWithEncryptionInfo(
      const push_messaging::AppIdentifier& app_identifier,
      RegisterCallback callback,
      const std::string& subscription_id,
      const GURL& endpoint,
      std::string p256dh,
      std::string auth_secret);

  void Unsubscribe(const push_messaging::AppIdentifier& app_id,
                   blink::mojom::PushUnregistrationReason reason,
                   UnregisterCallback callback);

  void DidClearPushSubscriptionId(blink::mojom::PushUnregistrationReason reason,
                                  const push_messaging::AppIdentifier& app_id,
                                  UnregisterCallback callback);

  void DidDeleteID(const std::string& app_id, instance_id::InstanceID::Result);
  // RemoveInstanceID must be run asynchronously, since it calls
  // InstanceIDDriver::RemoveInstanceID which deletes the InstanceID itself.
  // Calling that immediately would cause a use-after-free in our caller.
  void RemoveInstanceID(const std::string& app_id);

  void DidUnsubscribe(gcm::GCMClient::Result);

  void DidValidateSubscription(const std::string& app_id,
                               const std::string& sender_id,
                               const GURL& endpoint,
                               const std::optional<base::Time>& expiration_time,
                               SubscriptionInfoCallback callback,
                               bool is_valid);

  void DidGetEncryptionInfo(const GURL& endpoint,
                            const std::optional<base::Time>& expiration_time,
                            SubscriptionInfoCallback callback,
                            std::string p256dh,
                            std::string auth_secret) const;

  void DidDeliverMessage(const push_messaging::AppIdentifier& app_id,
                         blink::mojom::PushEventStatus status);

  // Class variables

  // Lazy initialized.
  std::unique_ptr<gcm::GCMDriver> gcm_driver_;

  // Lazy initialized.
  std::unique_ptr<instance_id::InstanceIDDriver> instance_id_driver_;

  // Outlive this instance.
  content::BrowserContext& parent_context_;
  os_crypt_async::OSCryptAsync& os_crypt_async_;
  network::NetworkConnectionTracker& network_connection_tracker_;

  // TODO(http://crbug.com/424479300): Implement the persistent storage of the
  // |app_ids_|.
  //
  // map<app_id, AppIdentifier>, the AppIdentifier.app_id must match the map
  // key.
  std::unordered_map<std::string, push_messaging::AppIdentifier> app_ids_;

  // Number of on-the-fly subscriptions, i.e. DoSubscribe was called, but not
  // DidSubscribe.
  int pending_subscriptions_{0};

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be the last member variable.
  base::WeakPtrFactory<PushMessagingServiceImpl> weak_ptr_factory_{this};
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_PUSH_MESSAGING_SERVICE_IMPL_H_
