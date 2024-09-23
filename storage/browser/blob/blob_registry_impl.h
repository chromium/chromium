// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_REGISTRY_IMPL_H_
#define STORAGE_BROWSER_BLOB_BLOB_REGISTRY_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"
#include "url/origin.h"

namespace base {
class FilePath;
}

namespace storage {

class BlobBuilderFromStream;
class BlobDataHandle;
class BlobStorageContext;

class COMPONENT_EXPORT(STORAGE_BROWSER) BlobRegistryImpl
    : public blink::mojom::BlobRegistry {
 public:
  // Per binding delegate, used for security checks for requests coming in on
  // specific bindings/from specific processes.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual bool CanReadFile(const base::FilePath& file) = 0;
    virtual bool CanAccessDataForOrigin(const url::Origin& origin) = 0;
  };

  explicit BlobRegistryImpl(base::WeakPtr<BlobStorageContext> context);

  BlobRegistryImpl(const BlobRegistryImpl&) = delete;
  BlobRegistryImpl& operator=(const BlobRegistryImpl&) = delete;

  ~BlobRegistryImpl() override;

  void Bind(mojo::PendingReceiver<blink::mojom::BlobRegistry> receiver,
            std::unique_ptr<Delegate> delegate);

  void Register(mojo::PendingReceiver<blink::mojom::Blob> blob,
                const std::string& uuid,
                const std::string& content_type,
                const std::string& content_disposition,
                std::vector<blink::mojom::DataElementPtr> elements,
                RegisterCallback callback) override;
  void RegisterFromStream(
      const std::string& content_type,
      const std::string& content_disposition,
      uint64_t expected_length,
      mojo::ScopedDataPipeConsumerHandle data,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      RegisterFromStreamCallback callback) override;
  void GetBlobFromUUID(mojo::PendingReceiver<blink::mojom::Blob> blob,
                       const std::string& uuid,
                       GetBlobFromUUIDCallback callback) override;

  size_t BlobsUnderConstructionForTesting() const {
    return blobs_under_construction_.size();
  }

  size_t BlobsBeingStreamedForTesting() const {
    return blobs_being_streamed_.size();
  }

  BlobStorageContext* context() { return context_.get(); }

 private:
  class BlobUnderConstruction;

  void BlobBuildAborted(const std::string& uuid);

  void StreamingBlobDone(RegisterFromStreamCallback callback,
                         BlobBuilderFromStream* builder,
                         std::unique_ptr<BlobDataHandle> result);

  base::WeakPtr<BlobStorageContext> context_;

  mojo::ReceiverSet<blink::mojom::BlobRegistry, std::unique_ptr<Delegate>>
      receivers_;

  std::map<std::string, std::unique_ptr<BlobUnderConstruction>>
      blobs_under_construction_;
  base::flat_set<std::unique_ptr<BlobBuilderFromStream>,
                 base::UniquePtrComparator>
      blobs_being_streamed_;

  base::WeakPtrFactory<BlobRegistryImpl> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_REGISTRY_IMPL_H_
