// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_push_messaging_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "content/public/browser/permission_type.h"
#include "content/shell/browser/web_test/web_test_browser_context.h"
#include "content/shell/browser/web_test/web_test_content_browser_client.h"
#include "content/shell/browser/web_test/web_test_permission_manager.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

// NIST P-256 public key made available to web tests. Must be an uncompressed
// point in accordance with SEC1 2.3.3.
const uint8_t kTestP256Key[] = {
    0x04, 0x55, 0x52, 0x6A, 0xA5, 0x6E, 0x8E, 0xAA, 0x47, 0x97, 0x36,
    0x10, 0xC1, 0x66, 0x3C, 0x1E, 0x65, 0xBF, 0xA1, 0x7B, 0xEE, 0x48,
    0xC9, 0xC6, 0xBB, 0xBF, 0x02, 0x18, 0x53, 0x72, 0x1D, 0x0C, 0x7B,
    0xA9, 0xE3, 0x11, 0xB7, 0x03, 0x52, 0x21, 0xD3, 0x71, 0x90, 0x13,
    0xA8, 0xC1, 0xCF, 0xED, 0x20, 0xF7, 0x1F, 0xD1, 0x7F, 0xF2, 0x76,
    0xB6, 0x01, 0x20, 0xD8, 0x35, 0xA5, 0xD9, 0x3C, 0x43, 0xFD};

static_assert(sizeof(kTestP256Key) == 65,
              "The fake public key must be a valid P-256 uncompressed point.");

// 92-bit (12 byte) authentication key associated with a subscription.
const uint8_t kAuthentication[] = {0xA5, 0xD9, 0x3C, 0x43, 0x0C, 0x00,
                                   0xA9, 0xE3, 0x1E, 0x65, 0xBF, 0xA1};

static_assert(sizeof(kAuthentication) == 12,
              "The fake authentication key must be at least 12 bytes in size.");

}  // anonymous namespace

WebTestPushMessagingService::WebTestPushMessagingService()
    : subscribed_service_worker_registration_(
          blink::mojom::kInvalidServiceWorkerRegistrationId) {}

WebTestPushMessagingService::~WebTestPushMessagingService() {}

void WebTestPushMessagingService::SubscribeFromDocument(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    int renderer_id,
    int render_frame_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    bool user_gesture,
    RegisterCallback callback) {
  SubscribeFromWorker(requesting_origin, service_worker_registration_id,
                      std::move(options), std::move(callback));
}

void WebTestPushMessagingService::SubscribeFromWorker(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    RegisterCallback callback) {
  blink::mojom::PermissionStatus permission_status =
      WebTestContentBrowserClient::Get()
          ->browser_context()
          ->GetPermissionControllerDelegate()
          ->GetPermissionStatus(PermissionType::NOTIFICATIONS,
                                requesting_origin, requesting_origin);

  // The `userVisibleOnly` option is still required when subscribing.
  if (!options->user_visible_only)
    permission_status = blink::mojom::PermissionStatus::DENIED;

  if (permission_status == blink::mojom::PermissionStatus::GRANTED) {
    std::vector<uint8_t> p256dh(kTestP256Key,
                                kTestP256Key + base::size(kTestP256Key));
    std::vector<uint8_t> auth(kAuthentication,
                              kAuthentication + base::size(kAuthentication));

    const std::string subscription_id = "layoutTestRegistrationId";
    const GURL endpoint = CreateEndpoint(subscription_id);

    subscribed_service_worker_registration_ = service_worker_registration_id;
    std::move(callback).Run(
        subscription_id, endpoint, p256dh, auth,
        blink::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE);
  } else {
    std::move(callback).Run(
        "registration_id", GURL::EmptyGURL() /* endpoint */,
        std::vector<uint8_t>() /* p256dh */, std::vector<uint8_t>() /* auth */,
        blink::mojom::PushRegistrationStatus::PERMISSION_DENIED);
  }
}

void WebTestPushMessagingService::GetSubscriptionInfo(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    const std::string& subscription_id,
    const SubscriptionInfoCallback& callback) {
  std::vector<uint8_t> p256dh(kTestP256Key,
                              kTestP256Key + base::size(kTestP256Key));
  std::vector<uint8_t> auth(kAuthentication,
                            kAuthentication + base::size(kAuthentication));

  const GURL endpoint = CreateEndpoint(subscription_id);
  callback.Run(true /* is_valid */, endpoint, p256dh, auth);
}

bool WebTestPushMessagingService::SupportNonVisibleMessages() {
  return false;
}

void WebTestPushMessagingService::Unsubscribe(
    blink::mojom::PushUnregistrationReason reason,
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    UnregisterCallback callback) {
  ClearPushSubscriptionId(
      WebTestContentBrowserClient::Get()->browser_context(), requesting_origin,
      service_worker_registration_id,
      base::BindOnce(
          std::move(callback),
          service_worker_registration_id ==
                  subscribed_service_worker_registration_
              ? blink::mojom::PushUnregistrationStatus::SUCCESS_UNREGISTERED
              : blink::mojom::PushUnregistrationStatus::
                    SUCCESS_WAS_NOT_REGISTERED));
  if (service_worker_registration_id ==
      subscribed_service_worker_registration_) {
    subscribed_service_worker_registration_ =
        blink::mojom::kInvalidServiceWorkerRegistrationId;
  }
}

void WebTestPushMessagingService::DidDeleteServiceWorkerRegistration(
    const GURL& origin,
    int64_t service_worker_registration_id) {
  if (service_worker_registration_id ==
      subscribed_service_worker_registration_) {
    subscribed_service_worker_registration_ =
        blink::mojom::kInvalidServiceWorkerRegistrationId;
  }
}

void WebTestPushMessagingService::DidDeleteServiceWorkerDatabase() {
  subscribed_service_worker_registration_ =
      blink::mojom::kInvalidServiceWorkerRegistrationId;
}

GURL WebTestPushMessagingService::CreateEndpoint(
    const std::string& subscription_id) const {
  return GURL("https://example.com/StandardizedEndpoint/" + subscription_id);
}

}  // namespace content
