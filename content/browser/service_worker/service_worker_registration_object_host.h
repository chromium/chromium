// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_OBJECT_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_OBJECT_HOST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

class ServiceWorkerContainerHost;
class ServiceWorkerContextCore;
class ServiceWorkerVersion;

namespace service_worker_object_host_unittest {
class ServiceWorkerObjectHostTest;
}  // namespace service_worker_object_host_unittest

// ServiceWorkerRegistrationObjectHost has a 1:1 correspondence to
// blink::ServiceWorkerRegistration in the renderer process.
// The host stays alive while the blink::ServiceWorkerRegistration is alive.
//
// Has a reference to the corresponding ServiceWorkerRegistration in order to
// ensure that the registration is alive while this object host is around.
class CONTENT_EXPORT ServiceWorkerRegistrationObjectHost
    : public blink::mojom::ServiceWorkerRegistrationObjectHost,
      public ServiceWorkerRegistration::Listener {
 public:
  ServiceWorkerRegistrationObjectHost(
      base::WeakPtr<ServiceWorkerContextCore> context,
      ServiceWorkerContainerHost* container_host,
      scoped_refptr<ServiceWorkerRegistration> registration);

  ServiceWorkerRegistrationObjectHost(
      const ServiceWorkerRegistrationObjectHost&) = delete;
  ServiceWorkerRegistrationObjectHost& operator=(
      const ServiceWorkerRegistrationObjectHost&) = delete;

  ~ServiceWorkerRegistrationObjectHost() override;

  // Establishes a new mojo connection into |receivers_|.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr CreateObjectInfo();

  ServiceWorkerRegistration* registration() { return registration_.get(); }

 private:
  friend class ServiceWorkerRegistrationObjectHostTest;
  friend class service_worker_object_host_unittest::ServiceWorkerObjectHostTest;

  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>;

  // ServiceWorkerRegistration::Listener overrides.
  void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask) override;
  void OnUpdateViaCacheChanged(
      ServiceWorkerRegistration* registration) override;
  void OnRegistrationFailed(ServiceWorkerRegistration* registration) override;
  void OnUpdateFound(ServiceWorkerRegistration* registration) override;

  // Implements blink::mojom::ServiceWorkerRegistrationObjectHost.
  void Update(blink::mojom::FetchClientSettingsObjectPtr
                  outside_fetch_client_settings_object,
              UpdateCallback callback) override;
  void Unregister(UnregisterCallback callback) override;
  void EnableNavigationPreload(
      bool enable,
      EnableNavigationPreloadCallback callback) override;
  void GetNavigationPreloadState(
      GetNavigationPreloadStateCallback callback) override;
  void SetNavigationPreloadHeader(
      const std::string& value,
      SetNavigationPreloadHeaderCallback callback) override;

  // Called back from ServiceWorkerContextCore when the unregistration is
  // complete.
  void UnregistrationComplete(UnregisterCallback callback,
                              blink::ServiceWorkerStatusCode status);
  // Called back from ServiceWorkerRegistry when setting navigation preload is
  // complete.
  void DidUpdateNavigationPreloadEnabled(
      bool enable,
      EnableNavigationPreloadCallback callback,
      blink::ServiceWorkerStatusCode status);
  // Called back from ServiceWorkerRegistry when setting navigation preload
  // header is complete.
  void DidUpdateNavigationPreloadHeader(
      const std::string& value,
      SetNavigationPreloadHeaderCallback callback,
      blink::ServiceWorkerStatusCode status);

  // Sets the corresponding version field to the given version or if the given
  // version is nullptr, clears the field.
  void SetServiceWorkerObjects(
      blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask,
      ServiceWorkerVersion* installing_version,
      ServiceWorkerVersion* waiting_version,
      ServiceWorkerVersion* active_version);

  void OnConnectionError();

  // Perform common checks that need to run before RegistrationObjectHost
  // methods that come from a child process are handled. Returns true if all
  // checks have passed. If anything looks wrong |callback| will run with an
  // error message prefixed by |error_prefix| and |args|, and false is returned.
  template <typename CallbackType, typename... Args>
  bool CanServeRegistrationObjectHostMethods(CallbackType* callback,
                                             const std::string& error_prefix,
                                             Args... args);

  // When |version_to_update| is nullptr, the returned string uses "Unknown" as
  // the script url.
  std::string ComposeUpdateErrorMessagePrefix(
      const ServiceWorkerVersion* version_to_update) const;

  // |container_host_| is valid throughout lifetime of |this| because it owns
  // |this|.
  const raw_ptr<ServiceWorkerContainerHost, DanglingUntriaged> container_host_;
  const base::WeakPtr<ServiceWorkerContextCore> context_;
  const scoped_refptr<ServiceWorkerRegistration> registration_;

  mojo::AssociatedReceiverSet<blink::mojom::ServiceWorkerRegistrationObjectHost>
      receivers_;
  // Mojo connection to the content::WebServiceWorkerRegistrationImpl in the
  // renderer, which corresponds to the ServiceWorkerRegistration JavaScript
  // object.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerRegistrationObject>
      remote_registration_;

  base::WeakPtrFactory<ServiceWorkerRegistrationObjectHost> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_OBJECT_HOST_H_
