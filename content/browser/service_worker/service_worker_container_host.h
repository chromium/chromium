// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_

#include <map>
#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace service_worker_object_host_unittest {
class ServiceWorkerObjectHostTest;
}

class ServiceWorkerClient;
class ServiceWorkerContainerHost;
class ServiceWorkerContextCore;
class ServiceWorkerHost;
class ServiceWorkerObjectHost;
class ServiceWorkerRegistration;
class ServiceWorkerRegistrationObjectHost;
class ServiceWorkerVersion;

// Manager classes that manages *Host objects associated with a
// `ServiceWorkerContainerHost`. These objects are owned by, corresponds 1:1 to,
// and have the same lifetime as the `ServiceWorkerContainerHost` object and
// thus the `container_host_` pointers are always valid.
class CONTENT_EXPORT ServiceWorkerRegistrationObjectManager final {
 public:
  explicit ServiceWorkerRegistrationObjectManager(
      ServiceWorkerContainerHost* container_host);
  ~ServiceWorkerRegistrationObjectManager();

  // Returns an object info representing |registration|. The object info holds a
  // Mojo connection to the ServiceWorkerRegistrationObjectHost for the
  // |registration| to ensure the host stays alive while the object info is
  // alive. A new ServiceWorkerRegistrationObjectHost instance is created if one
  // can not be found in |registration_object_hosts_|.
  //
  // NOTE: The registration object info should be sent over Mojo in the same
  // task with calling this method. Otherwise, some Mojo calls to
  // blink::mojom::ServiceWorkerRegistrationObject or
  // blink::mojom::ServiceWorkerObject may happen before establishing the
  // connections, and they'll end up with crashes.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr CreateInfo(
      scoped_refptr<ServiceWorkerRegistration> registration);

  // Removes the ServiceWorkerRegistrationObjectHost corresponding to
  // |registration_id|.
  void RemoveHost(int64_t registration_id);

 private:
  friend class service_worker_object_host_unittest::ServiceWorkerObjectHostTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, Unregister);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, RegisterDuplicateScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerUpdateJobTest,
                           RegisterWithDifferentUpdateViaCache);
  FRIEND_TEST_ALL_PREFIXES(BackgroundSyncManagerTest,
                           RegisterWithoutLiveSWRegistration);

  // Contains all ServiceWorkerRegistrationObjectHost instances corresponding to
  // the service worker registration JavaScript objects for the hosted execution
  // context (service worker global scope or service worker client) in the
  // renderer process.
  std::map<int64_t /* registration_id */,
           std::unique_ptr<ServiceWorkerRegistrationObjectHost>>
      registration_object_hosts_;

  // Always non-null and valid.
  const raw_ref<ServiceWorkerContainerHost> container_host_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class CONTENT_EXPORT ServiceWorkerObjectManager final {
 public:
  explicit ServiceWorkerObjectManager(
      ServiceWorkerContainerHost* container_host);
  ~ServiceWorkerObjectManager();

  // For service worker execution contexts.
  // Returns an object info representing |self.serviceWorker|. The object
  // info holds a Mojo connection to the ServiceWorkerObjectHost for the
  // |serviceWorker| to ensure the host stays alive while the object info is
  // alive. See documentation.
  blink::mojom::ServiceWorkerObjectInfoPtr CreateInfoToSend(
      scoped_refptr<ServiceWorkerVersion> version);

  // Returns a ServiceWorkerObjectHost instance for |version| for this
  // container host. A new instance is created if one does not already exist.
  // ServiceWorkerObjectHost will have an ownership of the |version|.
  base::WeakPtr<ServiceWorkerObjectHost> GetOrCreateHost(
      scoped_refptr<ServiceWorkerVersion> version);

  // Removes the ServiceWorkerObjectHost corresponding to |version_id|.
  void RemoveHost(int64_t version_id);

 private:
  friend class service_worker_object_host_unittest::ServiceWorkerObjectHostTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, Unregister);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, RegisterDuplicateScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerUpdateJobTest,
                           RegisterWithDifferentUpdateViaCache);

  // Contains all ServiceWorkerObjectHost instances corresponding to
  // the service worker JavaScript objects for the hosted execution
  // context (service worker global scope or service worker client) in the
  // renderer process.
  std::map<int64_t /* version_id */, std::unique_ptr<ServiceWorkerObjectHost>>
      service_worker_object_hosts_;

  // Always non-null and valid.
  const raw_ref<ServiceWorkerContainerHost> container_host_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// `ServiceWorkerContainerHost` is the host of a ServiceWorkerContainer in the
// renderer process: https://w3c.github.io/ServiceWorker/#serviceworkercontainer
// - `ServiceWorkerContainerHostForClient` for a window, dedicated worker, or
//   shared worker.
// - `ServiceWorkerContainerHostForServiceWorker` for a service worker execution
// context.
//
// Most of its functionality helps implement the web-exposed
// ServiceWorkerContainer interface (navigator.serviceWorker). The long-term
// goal is for it to be the host of ServiceWorkerContainer in the renderer,
// although currently only windows support ServiceWorkerContainers (see
// https://crbug.com/371690).
//
// ServiceWorkerContainerHost is also responsible for handling service worker
// related things in the execution context where the container lives. For
// example, the container host manages service worker (registration) JavaScript
// object hosts, delivers messages to/from the service worker, and dispatches
// events on the container.
class CONTENT_EXPORT ServiceWorkerContainerHost
    : public blink::mojom::ServiceWorkerContainerHost {
 public:
  ServiceWorkerContainerHost(const ServiceWorkerContainerHost& other) = delete;
  ServiceWorkerContainerHost& operator=(
      const ServiceWorkerContainerHost& other) = delete;
  ServiceWorkerContainerHost(ServiceWorkerContainerHost&& other) = delete;
  ServiceWorkerContainerHost& operator=(ServiceWorkerContainerHost&& other) =
      delete;

  ~ServiceWorkerContainerHost() override;

  // Implements blink::mojom::ServiceWorkerContainerHost.
  void CloneContainerHost(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerContainerHost> receiver)
      override;

  virtual const base::WeakPtr<ServiceWorkerContextCore>& context() const = 0;
  virtual base::WeakPtr<ServiceWorkerContainerHost> AsWeakPtr() = 0;

  // The URL of this context.
  virtual const GURL& url() const = 0;

  // Calls ContentBrowserClient::AllowServiceWorker(). Returns true if content
  // settings allows service workers to run at |scope|. If this container is for
  // a window client, the check involves the topmost frame url as well as
  // |scope|, and may display tab-level UI.
  // If non-empty, |script_url| is the script the service worker will run.
  virtual bool AllowServiceWorker(const GURL& scope,
                                  const GURL& script_url) = 0;

  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)>;
  virtual void DispatchExtendableMessageEvent(
      scoped_refptr<ServiceWorkerVersion> version,
      ::blink::TransferableMessage message,
      StatusCallback callback) = 0;
  virtual void Update(
      scoped_refptr<ServiceWorkerRegistration> registration,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      blink::mojom::ServiceWorkerRegistrationObjectHost::UpdateCallback
          callback) = 0;

  ServiceWorkerRegistrationObjectManager& registration_object_manager() {
    return registration_object_manager_;
  }
  ServiceWorkerObjectManager& version_object_manager() {
    return version_object_manager_;
  }

 protected:
  ServiceWorkerContainerHost();

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Container host receivers other than the original |receiver_|. These include
  // receivers used from (dedicated or shared) worker threads, or from
  // ServiceWorkerSubresourceLoaderFactory.
  mojo::ReceiverSet<blink::mojom::ServiceWorkerContainerHost>
      additional_receivers_;

  ServiceWorkerRegistrationObjectManager registration_object_manager_{this};
  ServiceWorkerObjectManager version_object_manager_{this};
};

// `ServiceWorkerContainerHostForClient` is owned by and corresponds 1:1 to a
// `ServiceWorkerClient`.
//
// `ServiceWorkerContainerHostForClient` is created at the same time as the
// corresponding `ServiceWorkerClient` construction.
// TODO(https://crbug.com/336154571): Create
// `ServiceWorkerContainerHostForClient` once the global scope in the renderer
// process is created and ready to receive mojo calls.
//
// The container host has a Mojo connection to the container in the renderer,
// and destruction of the container host happens upon disconnection of the Mojo
// pipe.
class CONTENT_EXPORT ServiceWorkerContainerHostForClient final
    : public ServiceWorkerContainerHost {
 public:
  // Binds `this` to the mojo pipes of `container_info`.
  // Must be called during `ServiceWorkerClient::CommitResponse()`.
  ServiceWorkerContainerHostForClient(
      base::PassKey<ServiceWorkerClient>,
      base::WeakPtr<ServiceWorkerClient> service_worker_client,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr& container_info,
      const PolicyContainerPolicies& policy_container_policies,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      ukm::SourceId ukm_source_id);

  ~ServiceWorkerContainerHostForClient() override;

  ServiceWorkerClient& service_worker_client() {
    return *service_worker_client_;
  }
  const ServiceWorkerClient& service_worker_client() const {
    return *service_worker_client_;
  }

  // For assertion only.
  bool IsContainerRemoteConnected() const;

  // Should be called only when `controller()` is non-null.
  blink::mojom::ControllerServiceWorkerInfoPtr
  CreateControllerServiceWorkerInfo();

  // Dispatches message event to the client (document, dedicated worker when
  // PlzDedicatedWorker is enabled, or shared worker).
  void PostMessageToClient(ServiceWorkerVersion& version,
                           blink::TransferableMessage message);

  // Sends information about the controller to the container of the service
  // worker clients in the renderer. If |notify_controllerchange| is true,
  // instructs the renderer to dispatch a 'controllerchange' event.
  void SendSetController(bool notify_controllerchange);

  // Called from ServiceWorkerClient.
  void CountFeature(blink::mojom::WebFeature feature);
  void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask);
  void ReturnRegistrationForReadyIfNeeded();
  void CloneControllerServiceWorker(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver);

  // Implements blink::mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                blink::mojom::FetchClientSettingsObjectPtr
                    outside_fetch_client_settings_object,
                RegisterCallback callback) override;
  void GetRegistration(const GURL& client_url,
                       GetRegistrationCallback callback) override;
  void GetRegistrations(GetRegistrationsCallback callback) override;
  void GetRegistrationForReady(
      GetRegistrationForReadyCallback callback) override;
  void EnsureControllerServiceWorker(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::mojom::ControllerServiceWorkerPurpose purpose) override;
  void HintToUpdateServiceWorker() override;
  void EnsureFileAccess(const std::vector<base::FilePath>& file_paths,
                        EnsureFileAccessCallback callback) override;
  void OnExecutionReady() override;

  // Implements ServiceWorkerContainerHost.
  const base::WeakPtr<ServiceWorkerContextCore>& context() const override;
  base::WeakPtr<ServiceWorkerContainerHost> AsWeakPtr() override;
  const GURL& url() const override;
  bool AllowServiceWorker(const GURL& scope, const GURL& script_url) override;
  void DispatchExtendableMessageEvent(
      scoped_refptr<ServiceWorkerVersion> version,
      ::blink::TransferableMessage message,
      StatusCallback callback) override;
  void Update(scoped_refptr<ServiceWorkerRegistration> registration,
              blink::mojom::FetchClientSettingsObjectPtr
                  outside_fetch_client_settings_object,
              blink::mojom::ServiceWorkerRegistrationObjectHost::UpdateCallback
                  callback) override;

  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }
  ServiceWorkerVersion* controller() const;

 private:
  // Callback for ServiceWorkerContextCore::RegisterServiceWorker().
  void RegistrationComplete(const GURL& script_url,
                            const GURL& scope,
                            RegisterCallback callback,
                            int64_t trace_id,
                            mojo::ReportBadMessageCallback bad_message_callback,
                            blink::ServiceWorkerStatusCode status,
                            const std::string& status_message,
                            int64_t registration_id);
  // Callback for ServiceWorkerRegistry::FindRegistrationForClientUrl().
  void GetRegistrationComplete(
      GetRegistrationCallback callback,
      int64_t trace_id,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  // Callback for ServiceWorkerStorage::GetRegistrationsForOrigin().
  void GetRegistrationsComplete(
      GetRegistrationsCallback callback,
      int64_t trace_id,
      blink::ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations);
  // Callback for ServiceWorkerVersion::RunAfterStartWorker()
  void StartControllerComplete(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::ServiceWorkerStatusCode status);

  bool IsValidGetRegistrationMessage(const GURL& client_url,
                                     std::string* out_error) const;
  bool IsValidGetRegistrationsMessage(std::string* out_error) const;
  bool IsValidGetRegistrationForReadyMessage(std::string* out_error) const;

  // Perform common checks that need to run before ContainerHost methods that
  // come from a child process are handled.
  // |scope| is checked if it is allowed to run a service worker.
  // If non-empty, |script_url| is the script associated with the service
  // worker.
  // Returns true if all checks have passed.
  // If anything looks wrong |callback| will run with an error
  // message prefixed by |error_prefix| and |args|, and false is returned.
  template <typename CallbackType, typename... Args>
  bool CanServeContainerHostMethods(CallbackType* callback,
                                    const GURL& scope,
                                    const GURL& script_url,
                                    const char* error_prefix,
                                    Args... args);

  // For service worker clients. Similar to EnsureControllerServiceWorker, but
  // this returns a bound Mojo ptr which is supposed to be sent to clients. The
  // controller ptr passed to the clients will be used to intercept requests
  // from them.
  // It is invalid to call this when controller_ is null.
  //
  // This method can be called in one of the following cases:
  //
  // - During navigation, right after CommitResponse().
  // - When a controller is updated by UpdateController() (e.g.
  //   by OnSkippedWaiting() or SetControllerRegistration()).
  //   In some cases the controller worker may not be started yet.
  //
  // This may return nullptr if the controller service worker does not have a
  // fetch handler, i.e. when the renderer does not need the controller ptr.
  //
  // WARNING:
  // Unlike EnsureControllerServiceWorker, this method doesn't guarantee that
  // the controller worker is running because this method can be called in some
  // situations where the worker isn't running yet. When the returned ptr is
  // stored somewhere and intended to use later, clients need to make sure
  // that the worker is eventually started to use the ptr.
  // Currently all the callsites do this, i.e. they start the worker before
  // or after calling this, but there's no mechanism to prevent future breakage.
  // TODO(crbug.com/40569659): Figure out a way to prevent misuse of this
  // method.
  // TODO(crbug.com/40569659): Make sure the connection error handler fires in
  // ControllerServiceWorkerConnector (so that it can correctly call
  // EnsureControllerServiceWorker later) if the worker gets killed before
  // events are dispatched.
  //
  // TODO(kinuko): revisit this if we start to use the ControllerServiceWorker
  // for posting messages.
  mojo::PendingRemote<blink::mojom::ControllerServiceWorker>
  GetRemoteControllerServiceWorker();

  // The corresponding service worker client that owns `this`.
  // Always valid and non-null except for initialization/destruction.
  base::WeakPtr<ServiceWorkerClient> service_worker_client_;

  // The ready() promise is only allowed to be created once.
  // |get_ready_callback_| has three states:
  // 1. |get_ready_callback_| is null when ready() has not yet been called.
  // 2. |*get_ready_callback_| is a valid OnceCallback after ready() has been
  //    called and the callback has not yet been run.
  // 3. |*get_ready_callback_| is a null OnceCallback after the callback has
  //    been run.
  std::unique_ptr<GetRegistrationForReadyCallback> get_ready_callback_;

  // |container_| is the remote renderer-side ServiceWorkerContainer that |this|
  // is hosting.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer> container_;

  // The source id of the client's ExecutionContext.
  const ukm::SourceId ukm_source_id_;

  // The policy container policies of the client.
  const PolicyContainerPolicies policy_container_policies_;

  // An endpoint connected to the COEP reporter. A clone of this connection is
  // passed to the service worker. Bound on response commit.
  mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_;

  base::WeakPtrFactory<ServiceWorkerContainerHostForClient> weak_ptr_factory_{
      this};
};

// ServiceWorkerContainerHostForServiceWorker is owned by ServiceWorkerHost,
// which in turn is owned by ServiceWorkerVersion. The container host and worker
// host are destructed when the service worker is stopped.
class CONTENT_EXPORT ServiceWorkerContainerHostForServiceWorker final
    : public ServiceWorkerContainerHost {
 public:
  ServiceWorkerContainerHostForServiceWorker(
      base::WeakPtr<ServiceWorkerContextCore> context,
      ServiceWorkerHost* service_worker_host,
      const GURL& url,
      const blink::StorageKey& storage_key);
  ~ServiceWorkerContainerHostForServiceWorker() override;

  // Implements blink::mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                blink::mojom::FetchClientSettingsObjectPtr
                    outside_fetch_client_settings_object,
                RegisterCallback callback) override;
  void GetRegistration(const GURL& client_url,
                       GetRegistrationCallback callback) override;
  void GetRegistrations(GetRegistrationsCallback callback) override;
  void GetRegistrationForReady(
      GetRegistrationForReadyCallback callback) override;
  void EnsureControllerServiceWorker(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::mojom::ControllerServiceWorkerPurpose purpose) override;
  void HintToUpdateServiceWorker() override;
  void EnsureFileAccess(const std::vector<base::FilePath>& file_paths,
                        EnsureFileAccessCallback callback) override;
  void OnExecutionReady() override;

  // Implements ServiceWorkerContainerHost.
  const base::WeakPtr<ServiceWorkerContextCore>& context() const override;
  base::WeakPtr<ServiceWorkerContainerHost> AsWeakPtr() override;
  const GURL& url() const override;
  bool AllowServiceWorker(const GURL& scope, const GURL& script_url) override;
  void DispatchExtendableMessageEvent(
      scoped_refptr<ServiceWorkerVersion> version,
      ::blink::TransferableMessage message,
      StatusCallback callback) override;
  void Update(scoped_refptr<ServiceWorkerRegistration> registration,
              blink::mojom::FetchClientSettingsObjectPtr
                  outside_fetch_client_settings_object,
              blink::mojom::ServiceWorkerRegistrationObjectHost::UpdateCallback
                  callback) override;

  ServiceWorkerHost* service_worker_host();
  const blink::StorageKey& key() const;
  const url::Origin& top_frame_origin() const;

 private:
  // The ServiceWorkerHost that owns |this|.
  const raw_ptr<ServiceWorkerHost> service_worker_host_;

  const base::WeakPtr<ServiceWorkerContextCore> context_;

  // The URL of the service worker's script.
  const GURL url_;

  const blink::StorageKey key_;

  const url::Origin top_frame_origin_;

  base::WeakPtrFactory<ServiceWorkerContainerHostForServiceWorker>
      weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_
