// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_installer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
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
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// Self deleting installer installs a web payment app and deletes itself.
class SelfDeleteInstaller
    : public ServiceWorkerContextObserver,
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

  SelfDeleteInstaller(const SelfDeleteInstaller& other) = delete;
  SelfDeleteInstaller& operator=(const SelfDeleteInstaller& other) = delete;

  void Init(WebContents* web_contents, bool use_cache) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    AddRef();  // Balanced by Release() in FinishInstallation.

    // TODO: Switch to being a WebContentsObserver and listen for events to
    // terminate installation early?
    web_contents_ = web_contents->GetWeakPtr();

    use_cache_ = use_cache;

    BrowserContext* browser_context = web_contents->GetBrowserContext();
    service_worker_context_ =
        base::WrapRefCounted(static_cast<ServiceWorkerContextWrapper*>(
            browser_context->GetDefaultStoragePartition()
                ->GetServiceWorkerContext()));
    service_worker_context_->FindReadyRegistrationForScope(
        scope_,
        blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_)),
        base::BindOnce(&SelfDeleteInstaller::OnFindReadyRegistrationForScope,
                       this));
  }

  void OnFindReadyRegistrationForScope(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (AbortInstallIfWebContentsOrBrowserContextIsGone())
      return;

    if (status == blink::ServiceWorkerStatusCode::kOk && registration) {
      DCHECK_EQ(scope_, registration->scope());
      // The service worker is already registered and activated (i.e., ready).
      // Use the existing service worker.
      registration_id_ = registration->id();
      SetPaymentAppIntoDatabase();
      return;
    }

    // If no service workers found, then register a new one.
    service_worker_context_->AddObserver(this);

    blink::mojom::ServiceWorkerRegistrationOptions option;
    option.scope = scope_;
    if (!use_cache_) {
      option.update_via_cache =
          blink::mojom::ServiceWorkerUpdateViaCache::kNone;
    }
    // TODO(crbug.com/40177656): Because this function can be called in a 3p
    // context we will need to generate a full StorageKey (origin + top-level
    // site) once StorageKey is expanded with the top-level site.
    service_worker_context_->RegisterServiceWorker(
        sw_url_,
        blink::StorageKey::CreateFirstParty(url::Origin::Create(option.scope)),
        option,
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

  void OnRegisterServiceWorkerResult(blink::ServiceWorkerStatusCode status) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (AbortInstallIfWebContentsOrBrowserContextIsGone())
      return;

    if (status != blink::ServiceWorkerStatusCode::kOk) {
      LOG(ERROR) << "Failed to install the web payment app " << sw_url_.spec();
      FinishInstallation(false);
    }
  }

 private:
  friend class base::RefCountedThreadSafe<SelfDeleteInstaller>;

  ~SelfDeleteInstaller() override = default;

  // If web contents or browser context are gone, then aborts payment and
  // returns true. Should be called on UI thread.
  bool AbortInstallIfWebContentsOrBrowserContextIsGone() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (!web_contents_ || !web_contents_->GetBrowserContext()) {
      FinishInstallation(false);
      return true;
    }

    return false;
  }

  void SetPaymentAppIntoDatabase() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(web_contents_);
    DCHECK(web_contents_->GetBrowserContext());

    StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
        web_contents_->GetBrowserContext()->GetDefaultStoragePartition());
    scoped_refptr<PaymentAppContextImpl> payment_app_context =
        partition->GetPaymentAppContext();

    payment_app_context->payment_app_database()
        ->SetPaymentAppInfoForRegisteredServiceWorker(
            registration_id_, scope_.spec(), app_name_, app_icon_, method_,
            supported_delegations_,
            base::BindOnce(&SelfDeleteInstaller::OnSetPaymentAppInfo, this));
  }

  void OnSetPaymentAppInfo(payments::mojom::PaymentHandlerStatus status) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    FinishInstallation(status == payments::mojom::PaymentHandlerStatus::SUCCESS
                           ? true
                           : false);
  }

  void FinishInstallation(bool success) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Do nothing if this function has been called.
    if (callback_.is_null())
      return;

    base::UmaHistogramBoolean("PaymentRequest.PaymentHandlerInstallSuccess",
                              success);

    if (success) {
      std::move(callback_).Run(registration_id_);
    } else {
      std::move(callback_).Run(-1);
    }

    service_worker_context_->RemoveObserver(this);

    Release();  // Balanced by AddRef() in the constructor.
  }

  base::WeakPtr<WebContents> web_contents_;
  std::string app_name_;
  std::string app_icon_;
  GURL sw_url_;
  GURL scope_;
  std::string method_;
  SupportedDelegations supported_delegations_;
  PaymentAppInstaller::InstallPaymentAppCallback callback_;
  bool use_cache_ = false;

  int64_t registration_id_ = -1;  // Take -1 as an invalid registration Id.
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
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
