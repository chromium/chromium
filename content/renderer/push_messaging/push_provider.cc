// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/push_messaging/push_provider.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/push_messaging_status.mojom.h"
#include "content/public/common/push_subscription_options.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_subscription.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_subscription_options.h"
#include "third_party/blink/public/platform/web_string.h"

namespace content {
namespace {

const char* PushRegistrationStatusToString(
    mojom::PushRegistrationStatus status) {
  switch (status) {
    case mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE:
    case mojom::PushRegistrationStatus::
        SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE:
      return "Registration successful - from push service";

    case mojom::PushRegistrationStatus::NO_SERVICE_WORKER:
      return "Registration failed - no Service Worker";

    case mojom::PushRegistrationStatus::SERVICE_NOT_AVAILABLE:
      return "Registration failed - push service not available";

    case mojom::PushRegistrationStatus::LIMIT_REACHED:
      return "Registration failed - registration limit has been reached";

    case mojom::PushRegistrationStatus::PERMISSION_DENIED:
      return "Registration failed - permission denied";

    case mojom::PushRegistrationStatus::SERVICE_ERROR:
      return "Registration failed - push service error";

    case mojom::PushRegistrationStatus::NO_SENDER_ID:
      return "Registration failed - missing applicationServerKey, and "
             "gcm_sender_id not found in manifest";

    case mojom::PushRegistrationStatus::STORAGE_ERROR:
      return "Registration failed - storage error";

    case mojom::PushRegistrationStatus::SUCCESS_FROM_CACHE:
      return "Registration successful - from cache";

    case mojom::PushRegistrationStatus::NETWORK_ERROR:
      return "Registration failed - could not connect to push server";

    case mojom::PushRegistrationStatus::INCOGNITO_PERMISSION_DENIED:
      // We split this out for UMA, but it must be indistinguishable to JS.
      return PushRegistrationStatusToString(
          mojom::PushRegistrationStatus::PERMISSION_DENIED);

    case mojom::PushRegistrationStatus::PUBLIC_KEY_UNAVAILABLE:
      return "Registration failed - could not retrieve the public key";

    case mojom::PushRegistrationStatus::MANIFEST_EMPTY_OR_MISSING:
      return "Registration failed - missing applicationServerKey, and manifest "
             "empty or missing";

    case mojom::PushRegistrationStatus::SENDER_ID_MISMATCH:
      return "Registration failed - A subscription with a different "
             "applicationServerKey (or gcm_sender_id) already exists; to "
             "change the applicationServerKey, unsubscribe then resubscribe.";

    case mojom::PushRegistrationStatus::STORAGE_CORRUPT:
      return "Registration failed - storage corrupt";

    case mojom::PushRegistrationStatus::RENDERER_SHUTDOWN:
      return "Registration failed - renderer shutdown";
  }
  NOTREACHED();
  return "";
}

}  // namespace

blink::WebPushError PushRegistrationStatusToWebPushError(
    mojom::PushRegistrationStatus status) {
  blink::WebPushError::ErrorType error_type =
      blink::WebPushError::kErrorTypeAbort;
  switch (status) {
    case mojom::PushRegistrationStatus::PERMISSION_DENIED:
      error_type = blink::WebPushError::kErrorTypeNotAllowed;
      break;
    case mojom::PushRegistrationStatus::SENDER_ID_MISMATCH:
      error_type = blink::WebPushError::kErrorTypeInvalidState;
      break;
    case mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE:
    case mojom::PushRegistrationStatus::
        SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE:
    case mojom::PushRegistrationStatus::NO_SERVICE_WORKER:
    case mojom::PushRegistrationStatus::SERVICE_NOT_AVAILABLE:
    case mojom::PushRegistrationStatus::LIMIT_REACHED:
    case mojom::PushRegistrationStatus::SERVICE_ERROR:
    case mojom::PushRegistrationStatus::NO_SENDER_ID:
    case mojom::PushRegistrationStatus::STORAGE_ERROR:
    case mojom::PushRegistrationStatus::SUCCESS_FROM_CACHE:
    case mojom::PushRegistrationStatus::NETWORK_ERROR:
    case mojom::PushRegistrationStatus::INCOGNITO_PERMISSION_DENIED:
    case mojom::PushRegistrationStatus::PUBLIC_KEY_UNAVAILABLE:
    case mojom::PushRegistrationStatus::MANIFEST_EMPTY_OR_MISSING:
    case mojom::PushRegistrationStatus::STORAGE_CORRUPT:
    case mojom::PushRegistrationStatus::RENDERER_SHUTDOWN:
      error_type = blink::WebPushError::kErrorTypeAbort;
      break;
  }
  return blink::WebPushError(
      error_type,
      blink::WebString::FromUTF8(PushRegistrationStatusToString(status)));
}

static base::LazyInstance<base::ThreadLocalPointer<PushProvider>>::Leaky
    g_push_provider_tls = LAZY_INSTANCE_INITIALIZER;

PushProvider::PushProvider(const scoped_refptr<base::SingleThreadTaskRunner>&
                               main_thread_task_runner) {
  DCHECK(main_thread_task_runner);

  auto request = mojo::MakeRequest(&push_messaging_manager_);
  if (!main_thread_task_runner->BelongsToCurrentThread()) {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&PushProvider::GetInterface, std::move(request)));
  } else {
    GetInterface(std::move(request));
  }
  g_push_provider_tls.Pointer()->Set(this);
}

PushProvider::~PushProvider() {
  g_push_provider_tls.Pointer()->Set(nullptr);
}

PushProvider* PushProvider::ThreadSpecificInstance(
    const scoped_refptr<base::SingleThreadTaskRunner>&
        main_thread_task_runner) {
  if (g_push_provider_tls.Pointer()->Get())
    return g_push_provider_tls.Pointer()->Get();

  PushProvider* provider = new PushProvider(main_thread_task_runner);
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(provider);
  return provider;
}

// static
void PushProvider::GetInterface(mojom::PushMessagingRequest request) {
  if (ChildThreadImpl::current()) {
    ChildThreadImpl::current()->GetConnector()->BindInterface(
        mojom::kBrowserServiceName, std::move(request));
  }
}

void PushProvider::WillStopCurrentWorkerThread() {
  delete this;
}

void PushProvider::Subscribe(
    int64_t service_worker_registration_id,
    const blink::WebPushSubscriptionOptions& options,
    bool user_gesture,
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) {
  DCHECK(callbacks);

  PushSubscriptionOptions content_options;
  content_options.user_visible_only = options.user_visible_only;

  // Just treat the server key as a string of bytes and pass it to the push
  // service.
  content_options.sender_info = options.application_server_key.Latin1();

  push_messaging_manager_->Subscribe(
      ChildProcessHost::kInvalidUniqueID, service_worker_registration_id,
      content_options, user_gesture,
      // Safe to use base::Unretained because |push_messaging_manager_ |is owned
      // by |this|.
      base::BindOnce(&PushProvider::DidSubscribe, base::Unretained(this),
                     std::move(callbacks)));
}

void PushProvider::DidSubscribe(
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
    mojom::PushRegistrationStatus status,
    const base::Optional<GURL>& endpoint,
    const base::Optional<PushSubscriptionOptions>& options,
    const base::Optional<std::vector<uint8_t>>& p256dh,
    const base::Optional<std::vector<uint8_t>>& auth) {
  DCHECK(callbacks);

  if (status == mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE ||
      status == mojom::PushRegistrationStatus::
                    SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE ||
      status == mojom::PushRegistrationStatus::SUCCESS_FROM_CACHE) {
    DCHECK(endpoint);
    DCHECK(options);
    DCHECK(p256dh);
    DCHECK(auth);

    callbacks->OnSuccess(std::make_unique<blink::WebPushSubscription>(
        endpoint.value(), options.value().user_visible_only,
        blink::WebString::FromLatin1(options.value().sender_info),
        p256dh.value(), auth.value()));
  } else {
    callbacks->OnError(PushRegistrationStatusToWebPushError(status));
  }
}

void PushProvider::Unsubscribe(
    int64_t service_worker_registration_id,
    std::unique_ptr<blink::WebPushUnsubscribeCallbacks> callbacks) {
  DCHECK(callbacks);

  push_messaging_manager_->Unsubscribe(
      service_worker_registration_id,
      // Safe to use base::Unretained because |push_messaging_manager_ |is owned
      // by |this|.
      base::BindOnce(&PushProvider::DidUnsubscribe, base::Unretained(this),
                     std::move(callbacks)));
}

void PushProvider::DidUnsubscribe(
    std::unique_ptr<blink::WebPushUnsubscribeCallbacks> callbacks,
    blink::WebPushError::ErrorType error_type,
    bool did_unsubscribe,
    const base::Optional<std::string>& error_message) {
  DCHECK(callbacks);

  // ErrorTypeNone indicates success.
  if (error_type == blink::WebPushError::kErrorTypeNone) {
    callbacks->OnSuccess(did_unsubscribe);
  } else {
    callbacks->OnError(blink::WebPushError(
        error_type, blink::WebString::FromUTF8(error_message->c_str())));
  }
}

void PushProvider::GetSubscription(
    int64_t service_worker_registration_id,
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) {
  DCHECK(callbacks);

  push_messaging_manager_->GetSubscription(
      service_worker_registration_id,
      // Safe to use base::Unretained because |push_messaging_manager_ |is owned
      // by |this|.
      base::BindOnce(&PushProvider::DidGetSubscription, base::Unretained(this),
                     std::move(callbacks)));
}

void PushProvider::DidGetSubscription(
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
    mojom::PushGetRegistrationStatus status,
    const base::Optional<GURL>& endpoint,
    const base::Optional<PushSubscriptionOptions>& options,
    const base::Optional<std::vector<uint8_t>>& p256dh,
    const base::Optional<std::vector<uint8_t>>& auth) {
  DCHECK(callbacks);

  if (status == mojom::PushGetRegistrationStatus::SUCCESS) {
    DCHECK(endpoint);
    DCHECK(options);
    DCHECK(p256dh);
    DCHECK(auth);

    callbacks->OnSuccess(std::make_unique<blink::WebPushSubscription>(
        endpoint.value(), options.value().user_visible_only,
        blink::WebString::FromLatin1(options.value().sender_info),
        p256dh.value(), auth.value()));
  } else {
    // We are only expecting an error if we can't find a registration.
    callbacks->OnSuccess(nullptr);
  }
}

}  // namespace content
