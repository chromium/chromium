// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
namespace {

// Self deleting installer installs a web payment app and deletes itself.
class SelfDeleteInstaller
    : public WebContentsObserver,
      public ServiceWorkerContextObserver,
      public base::RefCountedThreadSafe<SelfDeleteInstaller> {
 public:
  SelfDeleteInstaller(const std::string& app_name,
                      const std::string& app_icon,
                      const GURL& sw_url,
                      const GURL& scope,
                      const std::string& method,
                      const SupportedDelegations& supported_delegations,
                      PaymentAppInstaller::InstallPaymentAppCallback callback)
      : app_name_(app_name),
        app_icon_(app_icon),
        sw_url_(sw_url),
        scope_(scope),
        method_(method),
        supported_delegations_(supported_delegations),
        callback_(std::move(callback)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  void Init(WebContents* web_contents, bool use_cache) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    AddRef();  // Balanced by Release() in FinishInstallation.

    // TODO(crbug.com/782270): Listen for web contents events to terminate
    // installation early.
    Observe(web_contents);

    content::BrowserContext* browser_context =
        web_contents->GetBrowserContext();
    service_worker_context_ =
        base::WrapRefCounted(static_cast<ServiceWorkerContextWrapper*>(
            browser_context->GetDefaultStoragePartition(browser_context)
                ->GetServiceWorkerContext()));

    service_worker_context_->AddObserver(this);

    blink::mojom::ServiceWorkerRegistrationOptions option;
    option.scope = scope_;
    if (!use_cache) {
      option.update_via_cache =
          blink::mojom::ServiceWorkerUpdateViaCache::kNone;
    }
    service_worker_context_->RegisterServiceWorker(
        sw_url_, option,
        base::BindOnce(&SelfDeleteInstaller::OnRegisterServiceWorkerResult,
                       this));
  }

  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (AbortInstallIfWebContentsOrBrowserContextIsGone())
      return;

    if (scope.EqualsIgnoringRef(scope_))
      registration_id_ = registration_id;
  }

  void OnVersionActivated(int64_t version_id, const GURL& scope) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (AbortInstallIfWebContentsOrBrowserContextIsGone())
      return;

    if (scope.EqualsIgnoringRef(scope_))
      SetPaymentAppIntoDatabase();
  }

  void OnErrorReported(int64_t version_id,
                       const GURL& scope,
                       const ErrorInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (AbortInstallIfWebContentsOrBrowserContextIsGone())
      return;

    if (scope.EqualsIgnoringRef(scope_))
      LOG(ERROR) << "The newly registered service worker has an error "
                 << info.error_message;
    FinishInstallation(false);
  }

  void OnReportConsoleMessage(int64_t version_id,
                              const GURL& scope,
                              const ConsoleMessage& message) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (AbortInstallIfWebContentsOrBrowserContextIsGone())
      return;

    if (scope.EqualsIgnoringRef(scope_) &&
        message.message_level == blink::mojom::ConsoleMessageLevel::kError)
      LOG(ERROR) << "The newly registered service worker has an error "
                 << message.message;
    FinishInstallation(false);
  }

  void OnRegisterServiceWorkerResult(bool success) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (AbortInstallIfWebContentsOrBrowserContextIsGone())
      return;

    if (!success) {
      LOG(ERROR) << "Failed to install the web payment app " << sw_url_.spec();
      FinishInstallation(false);
    }
  }

 private:
  friend class base::RefCountedThreadSafe<SelfDeleteInstaller>;

  ~SelfDeleteInstaller() override {}

  // If web contents or browser context are gone, then aborts payment and
  // returns true. Should be called on UI thread.
  bool AbortInstallIfWebContentsOrBrowserContextIsGone() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (!web_contents() || !web_contents()->GetBrowserContext()) {
      FinishInstallation(false);
      return true;
    }

    return false;
  }

  void SetPaymentAppIntoDatabase() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(web_contents());
    DCHECK(web_contents()->GetBrowserContext());

    StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetDefaultStoragePartition(
            web_contents()->GetBrowserContext()));
    scoped_refptr<PaymentAppContextImpl> payment_app_context =
        partition->GetPaymentAppContext();

    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(&SelfDeleteInstaller::SetPaymentAppInfoOnCoreThread,
                       this, payment_app_context, registration_id_,
                       scope_.spec(), app_name_, app_icon_, method_,
                       supported_delegations_));
  }

  void SetPaymentAppInfoOnCoreThread(
      scoped_refptr<PaymentAppContextImpl> payment_app_context,
      int64_t registration_id,
      const std::string& instrument_key,
      const std::string& name,
      const std::string& app_icon,
      const std::string& method,
      const SupportedDelegations& supported_delegations) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    payment_app_context->payment_app_database()
        ->SetPaymentAppInfoForRegisteredServiceWorker(
            registration_id, instrument_key, name, app_icon, method,
            supported_delegations,
            base::BindOnce(&SelfDeleteInstaller::OnSetPaymentAppInfo, this));
  }

  void OnSetPaymentAppInfo(payments::mojom::PaymentHandlerStatus status) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

    RunOrPostTaskOnThread(
        FROM_HERE, BrowserThread::UI,
        base::BindOnce(&SelfDeleteInstaller::FinishInstallation, this,
                       status == payments::mojom::PaymentHandlerStatus::SUCCESS
                           ? true
                           : false));
  }

  void FinishInstallation(bool success) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Do nothing if this function has been called.
    if (callback_.is_null())
      return;

    if (success) {
      std::move(callback_).Run(registration_id_);
    } else {
      std::move(callback_).Run(-1);
    }

    service_worker_context_->RemoveObserver(this);

    Observe(nullptr);
    Release();  // Balanced by AddRef() in the constructor.
  }

  std::string app_name_;
  std::string app_icon_;
  GURL sw_url_;
  GURL scope_;
  std::string method_;
  SupportedDelegations supported_delegations_;
  PaymentAppInstaller::InstallPaymentAppCallback callback_;

  int64_t registration_id_ = -1;  // Take -1 as an invalid registration Id.
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  DISALLOW_COPY_AND_ASSIGN(SelfDeleteInstaller);
};

}  // namespace.

// Static
void PaymentAppInstaller::Install(
    WebContents* web_contents,
    const std::string& app_name,
    const std::string& app_icon,
    const GURL& sw_url,
    const GURL& scope,
    bool use_cache,
    const std::string& method,
    const SupportedDelegations& supported_delegations,
    InstallPaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto installer = base::MakeRefCounted<SelfDeleteInstaller>(
      app_name, app_icon, sw_url, scope, method, supported_delegations,
      std::move(callback));
  installer->Init(web_contents, use_cache);
}

}  // namespace content
