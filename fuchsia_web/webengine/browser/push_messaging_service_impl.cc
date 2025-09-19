// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/push_messaging_service_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/fuchsia/file_utils.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_desktop_utils.h"
#include "components/gcm_driver/gcm_driver_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

PushMessagingServiceImpl::PushMessagingServiceImpl(
    content::BrowserContext& parent_context,
    os_crypt_async::OSCryptAsync* os_crypt_async)
    : parent_context_(parent_context), os_crypt_async_(os_crypt_async) {}

PushMessagingServiceImpl::~PushMessagingServiceImpl() = default;

// PushMessagingService implementations.
void PushMessagingServiceImpl::SubscribeFromDocument(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    int render_process_id,
    int render_frame_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    bool user_gesture,
    RegisterCallback callback) {}

void PushMessagingServiceImpl::SubscribeFromWorker(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    int render_process_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    RegisterCallback callback) {}

void PushMessagingServiceImpl::GetSubscriptionInfo(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    const std::string& subscription_id,
    SubscriptionInfoCallback callback) {}

void PushMessagingServiceImpl::Unsubscribe(
    blink::mojom::PushUnregistrationReason reason,
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    UnregisterCallback callback) {}

bool PushMessagingServiceImpl::SupportNonVisibleMessages() {
  return false;
}

void PushMessagingServiceImpl::DidDeleteServiceWorkerRegistration(
    const GURL& origin,
    int64_t service_worker_registration_id) {}

void PushMessagingServiceImpl::DidDeleteServiceWorkerDatabase() {}

// GCMAppHandler implementations.
void PushMessagingServiceImpl::ShutdownHandler() {}

void PushMessagingServiceImpl::OnStoreReset() {}

void PushMessagingServiceImpl::OnMessage(const std::string& app_id,
                                         const gcm::IncomingMessage& message) {}

void PushMessagingServiceImpl::OnMessagesDeleted(const std::string& app_id) {}

void PushMessagingServiceImpl::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {}

void PushMessagingServiceImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {}

gcm::GCMDriver& PushMessagingServiceImpl::GetGCMDriver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gcm_driver_) {
    // No predefined blocking_task_runner, create one dedicated for gcm_driver.
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

    gcm_driver_ = gcm::CreateGCMDriverDesktop(
        std::make_unique<gcm::GCMClientFactory>(), /* prefs = */ nullptr,
        base::FilePath(base::kPersistedDataDirectoryPath)
            .Append(gcm_driver::kGCMStoreDirname),
        base::BindPostTask(
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::BindRepeating(
                &PushMessagingServiceImpl::RequestProxyResolvingSocketFactory,
                weak_ptr_factory_.GetWeakPtr())),
        GetSharedURLLoaderFactory(),
        /* network_connection_tracker = */ nullptr, GetChannel(),
        GetProductCategoryForSubtypes(),
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::ID::UI),
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::ID::IO),
        blocking_task_runner, os_crypt_async_);
  }
  DCHECK(gcm_driver_);
  return *gcm_driver_.get();
}

void PushMessagingServiceImpl::RequestProxyResolvingSocketFactory(
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  parent_context_.GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->CreateProxyResolvingSocketFactory(std::move(receiver));
}

scoped_refptr<network::SharedURLLoaderFactory>
PushMessagingServiceImpl::GetSharedURLLoaderFactory() const {
  return parent_context_.GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

version_info::Channel PushMessagingServiceImpl::GetChannel() const {
  // Only stable version of WebEngine is released to the users.
  return version_info::Channel::STABLE;
}

std::string PushMessagingServiceImpl::GetProductCategoryForSubtypes() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return "com.chrome.fuchsia";
#else
  return "org.chromium.fuchsia";
#endif
}
