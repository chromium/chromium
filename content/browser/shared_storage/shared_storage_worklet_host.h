// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_

#include "content/common/content_export.h"
#include "content/services/shared_storage_worklet/public/mojom/shared_storage_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"
#include "url/origin.h"

namespace content {

class SharedStorageURLLoaderFactoryProxy;
class SharedStorageWorkletDriver;
class RenderFrameHost;
class RenderFrameHostImpl;

// The worklet host is responsible for getting worklet operation requests
// (i.e. addModule and runOperation) from the renderer and running it on the
// worklet service. It will also handle the commands from the worklet service
// (i.e. storage access, console log) which could happen while running those
// worklet operations.
//
// The SharedStorageWorkletHost lives in the SharedStorageWorkletHostManager,
// and worklet host's lifetime is bounded by the lifetime of the document, as
// when the document is destroyed, it will tell the worklet host manager to
// destroy the corresponding worklet host.
class CONTENT_EXPORT SharedStorageWorkletHost
    : public shared_storage_worklet::mojom::SharedStorageWorkletServiceClient {
 public:
  enum class AddModuleState {
    kNotInitiated,
    kInitiated,
  };

  explicit SharedStorageWorkletHost(
      std::unique_ptr<SharedStorageWorkletDriver> driver,
      RenderFrameHost& rfh);
  ~SharedStorageWorkletHost() override;

  void AddModuleOnWorklet(
      const url::Origin& frame_origin,
      const GURL& script_source_url,
      blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
          callback);
  void RunOperationOnWorklet(const std::string& name,
                             const std::vector<uint8_t>& serialized_data);

  // shared_storage_worklet::mojom::SharedStorageWorkletServiceClient.
  void SharedStorageSet(const std::string& key,
                        const std::string& value,
                        bool ignore_if_present,
                        SharedStorageSetCallback callback) override;
  void SharedStorageAppend(const std::string& key,
                           const std::string& value,
                           SharedStorageAppendCallback callback) override;
  void SharedStorageDelete(const std::string& key,
                           SharedStorageDeleteCallback callback) override;
  void SharedStorageClear(SharedStorageClearCallback callback) override;
  void SharedStorageGet(const std::string& key,
                        SharedStorageGetCallback callback) override;
  void SharedStorageKeys(
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener) override;
  void SharedStorageEntries(
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener) override;
  void SharedStorageLength(SharedStorageLengthCallback callback) override;
  void ConsoleLog(const std::string& message) override;

 protected:
  // virtual for testing
  virtual void OnAddModuleOnWorkletFinished(
      blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
          callback,
      bool success,
      const std::string& error_message);

  virtual void OnRunOperationOnWorkletFinished(
      bool success,
      const std::string& error_message);

 private:
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory();

  shared_storage_worklet::mojom::SharedStorageWorkletService*
  GetAndConnectToSharedStorageWorkletService();

  AddModuleState add_module_state_ = AddModuleState::kNotInitiated;

  // Responsible for initializing the worklet service.
  std::unique_ptr<SharedStorageWorkletDriver> driver_;

  // 'render_frame_host_' is the RenderFrameHostImpl returned from the document
  // service, and since `this`'s lifetime is tied to the Document service's,
  // this reference will always be valid.
  RenderFrameHostImpl& render_frame_host_;

  // Both `shared_storage_worklet_service_`
  // and `shared_storage_worklet_service_client_` are bound in
  // GetAndConnectToSharedStorageWorkletService().
  //
  // `shared_storage_worklet_service_client_` is associated specifically with
  // the pipe that `shared_storage_worklet_service_` runs on, as we want the
  // messages initiated from the worklet (e.g. storage access, console log) to
  // be well ordered with respect to the corresponding request's callback
  // message which will be interpreted as the completion of an operation.
  mojo::Remote<shared_storage_worklet::mojom::SharedStorageWorkletService>
      shared_storage_worklet_service_;

  mojo::AssociatedReceiver<
      shared_storage_worklet::mojom::SharedStorageWorkletServiceClient>
      shared_storage_worklet_service_client_{this};

  // The proxy is used to limit the request that the worklet can make, e.g. to
  // ensure the URL is not modified by a compromised worklet; to enforce the
  // application/javascript request header; to enforce same-origin mode; etc.
  std::unique_ptr<SharedStorageURLLoaderFactoryProxy> url_loader_factory_proxy_;

  // The default url loader factory for the frame to create the loader and start
  // request. This will be initialized on demand after
  // `url_loader_factory_proxy_` successfully sanitizes the initial request.
  mojo::Remote<network::mojom::URLLoaderFactory> frame_url_loader_factory_;

  base::WeakPtrFactory<SharedStorageWorkletHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_
