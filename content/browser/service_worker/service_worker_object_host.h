// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_OBJECT_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_OBJECT_HOST_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "url/origin.h"

namespace content {

class ServiceWorkerContainerHost;
class ServiceWorkerContextCore;

namespace service_worker_object_host_unittest {
class ServiceWorkerObjectHostTest;
}  // namespace service_worker_object_host_unittest

// Roughly corresponds to one blink::ServiceWorker object in the renderer
// process.
//
// The blink::ServiceWorker object in the renderer process maintains a
// reference to |this| by owning a Mojo remote to
// blink::mojom::ServiceWorkerObjectHost. When all Mojo connections bound with
// |receivers_| are disconnected, |this| will be deleted. See also comments on
// |receivers_|.
//
// Has references to the corresponding ServiceWorkerVersion in order to ensure
// that the version is alive while this handle is around.
class CONTENT_EXPORT ServiceWorkerObjectHost
    : public blink::mojom::ServiceWorkerObjectHost,
      public ServiceWorkerVersion::Observer {
 public:
  ServiceWorkerObjectHost(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerContainerHost> container_host,
      scoped_refptr<ServiceWorkerVersion> version);

  ServiceWorkerObjectHost(const ServiceWorkerObjectHost&) = delete;
  ServiceWorkerObjectHost& operator=(const ServiceWorkerObjectHost&) = delete;

  ~ServiceWorkerObjectHost() override;

  // ServiceWorkerVersion::Observer overrides.
  void OnVersionStateChanged(ServiceWorkerVersion* version) override;

  // Returns an info for the ServiceWorker object. The info contains a Mojo
  // ptr to |this| which ensures |this| stays alive while the info is alive.
  // Furthermore, it contains a Mojo request for the ServiceWorkerObject
  // interface in the renderer. |this| will make calls to the
  // ServiceWorkerObject to update its state.
  //
  // WARNING: The returned info must be sent immediately over Mojo, because
  // |this| will start making calls on an associated interface ptr to
  // ServiceWorkerObject, which crashes unless the request inside the info has
  // been sent. If the info cannot be sent immediately, use
  // CreateIncompleteObjectInfo() instead.
  blink::mojom::ServiceWorkerObjectInfoPtr CreateCompleteObjectInfoToSend();

  base::WeakPtr<ServiceWorkerObjectHost> AsWeakPtr();

 private:
  friend class service_worker_object_host_unittest::ServiceWorkerObjectHostTest;

  // Implements blink::mojom::ServiceWorkerObjectHost.
  void PostMessageToServiceWorker(
      ::blink::TransferableMessage message) override;
  void TerminateForTesting(TerminateForTestingCallback callback) override;

  // TODO(leonhsl): Remove |callback| parameter because it's just for unit tests
  // and production code does not use it. We need to figure out another way to
  // observe the dispatch result in unit tests.
  void DispatchExtendableMessageEvent(
      ::blink::TransferableMessage message,
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback);

  void OnConnectionError();

  base::WeakPtr<ServiceWorkerContextCore> context_;
  // |container_host_| is valid throughout lifetime of |this| because it owns
  // |this|.
  //
  // However, there exists an exception, because of an ownership cycle
  // between 1,2,3,4,5:
  // 1. ServiceWorkerContainerHost owns as member (2)
  // 2. ServiceWorkerObjectManager owns via unique_ptr (3)
  // 3. ServiceWorkerObjectHost owns via scoped_ptr(4)
  // 4. ServiceWorkerVersion owns via unique_ptr (5)
  // 5. ServiceWorkerHost owns via unique_ptr (1)
  //
  // The cycle is broken in `ServiceWorkerObjectManager::RemoveHost`, by
  // transferring ownership of |this| to the stack, while deleting
  // |container_host_|.
  //
  // As a result, |container_host_| is always valid, except during the
  // destructor.
  const base::WeakPtr<ServiceWorkerContainerHost> container_host_;
  // The origin of the |container_host_|. Note that this is const because once a
  // JavaScript ServiceWorker object is created for an execution context, we
  // don't expect that context to change origins and still hold on to the
  // object.
  const url::Origin container_origin_;
  scoped_refptr<ServiceWorkerVersion> version_;
  // Typically both |receivers_| and |remote_objects_| contain only one Mojo
  // connection, corresponding to the blink::ServiceWorker in the renderer which
  // corresponds to the ServiceWorker JavaScript object. However, multiple Mojo
  // connections may exist while propagating multiple service worker object
  // infos to the renderer process, but only the first one that arrived there
  // will be used to create the new blink::ServiceWorker instance and be bound
  // to it.
  mojo::AssociatedReceiverSet<blink::mojom::ServiceWorkerObjectHost> receivers_;
  mojo::AssociatedRemoteSet<blink::mojom::ServiceWorkerObject> remote_objects_;

  base::WeakPtrFactory<ServiceWorkerObjectHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_OBJECT_HOST_H_
