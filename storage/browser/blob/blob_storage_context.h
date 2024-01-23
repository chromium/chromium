// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_
#define STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/blob/blob_memory_controller.h"
#include "storage/browser/blob/blob_storage_constants.h"
#include "storage/browser/blob/blob_storage_registry.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace content {

class ChromeBlobStorageContext;

namespace indexed_db_backing_store_unittest {
class BlobStorageContextShim;
}  // namespace indexed_db_backing_store_unittest

}  // namespace content

namespace storage {

class BlobDataBuilder;
class BlobDataHandle;
class BlobDataSnapshot;
class ShareableBlobDataItem;

// This class handles the logistics of blob storage within the browser process.
// This class is not threadsafe, access on IO thread. In Chromium there is one
// instance per profile.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobStorageContext
    : public base::trace_event::MemoryDumpProvider,
      public mojom::BlobStorageContext {
 public:
  using TransportAllowedCallback = BlobEntry::TransportAllowedCallback;
  using BuildAbortedCallback = BlobEntry::BuildAbortedCallback;

  // Initializes the context without disk support.
  BlobStorageContext();
  // Disk support is enabled if |file_runner| isn't null.
  BlobStorageContext(const base::FilePath& profile_directory,
                     const base::FilePath& blob_storage_directory,
                     scoped_refptr<base::TaskRunner> file_runner);

  BlobStorageContext(const BlobStorageContext&) = delete;
  BlobStorageContext& operator=(const BlobStorageContext&) = delete;

  ~BlobStorageContext() override;

  // The following three methods all lookup a BlobDataHandle based on some
  // input. If no blob matching the input exists these methods return null.
  std::unique_ptr<BlobDataHandle> GetBlobDataFromUUID(const std::string& uuid);
  // If this BlobStorageContext is deleted before this method finishes, the
  // callback will still be called with null.
  void GetBlobDataFromBlobRemote(
      mojo::PendingRemote<blink::mojom::Blob> blob,
      base::OnceCallback<void(std::unique_ptr<BlobDataHandle>)> callback);

  // Always returns a handle to a blob. Use BlobStatus::GetBlobStatus() and
  // BlobStatus::RunOnConstructionComplete(callback) to determine construction
  // completion and possible errors.
  std::unique_ptr<BlobDataHandle> AddFinishedBlob(
      std::unique_ptr<BlobDataBuilder> builder);

  std::unique_ptr<BlobDataHandle> AddFinishedBlob(
      const std::string& uuid,
      const std::string& content_type,
      const std::string& content_disposition,
      std::vector<scoped_refptr<ShareableBlobDataItem>> items);

  std::unique_ptr<BlobDataHandle> AddBrokenBlob(
      const std::string& uuid,
      const std::string& content_type,
      const std::string& content_disposition,
      BlobStatus reason);

  size_t blob_count() const { return registry_.blob_count(); }

  const BlobStorageRegistry& registry() { return registry_; }

  // This builds a blob with the given |input_builder| and returns a handle to
  // the constructed Blob. Blob metadata and data should be accessed through
  // this handle.
  // If there is data present that needs further population then we will call
  // |transport_allowed_callback| when we're ready for the user data to be
  // populated with the PENDING_DATA_POPULATION status. This can happen
  // synchronously or asynchronously. Otherwise |transport_allowed_callback|
  // should be null. In the further population case, the caller must call either
  // NotifyTransportComplete or CancelBuildingBlob after
  // |transport_allowed_callback| is called to signify the data is finished
  // populating or an error occurred (respectively).
  // If the returned handle is broken, then the possible error cases are:
  // * OUT_OF_MEMORY if we don't have enough memory to store the blob,
  // * REFERENCED_BLOB_BROKEN if a referenced blob is broken or we're
  //   referencing ourself.
  std::unique_ptr<BlobDataHandle> BuildBlob(
      std::unique_ptr<BlobDataBuilder> input_builder,
      TransportAllowedCallback transport_allowed_callback);

  // Similar to BuildBlob, but this merely registers a blob that will be built
  // in the future. The caller must later call either BuildPreregisteredBlob
  // (to actually start building the blob), or CancelBuildingBlob (if an error
  // occured).
  // The returned BlobDataHandle (as well as any handles returned by
  // GetBlobDataFromUUID before BuildPreregisteredBlob is called) will always
  // have kUnknownSize for its size. A BlobDataHandle with the correct size is
  // later returned by BuildPreregisteredBlob.
  std::unique_ptr<BlobDataHandle> AddFutureBlob(
      const std::string& uuid,
      const std::string& content_type,
      const std::string& content_disposition,
      BuildAbortedCallback build_aborted_callback);

  // Same as BuildBlob, but for a blob that was previously registered by calling
  // AddFutureBlob.
  std::unique_ptr<BlobDataHandle> BuildPreregisteredBlob(
      std::unique_ptr<BlobDataBuilder> input_builder,
      TransportAllowedCallback transport_allowed_callback);

  // This breaks a blob that is currently being built by using the BuildBlob
  // method above. Any callbacks waiting on this blob, including the
  // |transport_allowed_callback| callback given to BuildBlob, will be called
  // with this status code.
  void CancelBuildingBlob(const std::string& uuid, BlobStatus code);

  // After calling BuildBlob above, the caller should call this method to
  // notify the construction system that the unpopulated data in the given blob
  // has been. populated. Caller must have all pending items populated in the
  // original builder |input_builder| given in BuildBlob or we'll check-fail.
  // If there is no pending data in the |input_builder| for the BuildBlob call,
  // then this method doesn't need to be called.
  void NotifyTransportComplete(const std::string& uuid);

  const BlobMemoryController& memory_controller() { return memory_controller_; }

  base::WeakPtr<BlobStorageContext> AsWeakPtr() {
    return ptr_factory_.GetWeakPtr();
  }

  void Bind(mojo::PendingReceiver<mojom::BlobStorageContext> receiver);

  void set_limits_for_testing(const BlobStorageLimits& limits) {
    mutable_memory_controller()->set_limits_for_testing(limits);
  }

  void DisableFilePagingForTesting() {
    mutable_memory_controller()->DisableFilePaging(base::File::FILE_OK);
  }

 protected:
  friend class content::ChromeBlobStorageContext;
  friend class BlobBuilderFromStream;
  friend class BlobDataHandle;
  friend class BlobDataHandle::BlobDataHandleShared;
  friend class BlobRegistryImplTest;
  friend class BlobStorageContextTest;
  friend class BlobURLTokenImpl;
  friend class content::indexed_db_backing_store_unittest::
      BlobStorageContextShim;

  enum class TransportQuotaType { MEMORY, FILE };

  void IncrementBlobRefCount(const std::string& uuid);
  void DecrementBlobRefCount(const std::string& uuid);

  // This will return an empty snapshot until the blob is complete.
  // TODO(dmurph): After we make the snapshot method in BlobHandle private, then
  // make this DCHECK on the blob not being complete.
  std::unique_ptr<BlobDataSnapshot> CreateSnapshot(const std::string& uuid);

  BlobStatus GetBlobStatus(const std::string& uuid) const;

  // Runs |done| when construction completes with the final status of the blob.
  void RunOnConstructionComplete(const std::string& uuid,
                                 BlobStatusCallback done_callback);

  // Runs |done| when construction begins (when the blob is no longer
  // PENDING_CONSTRUCTION) with the new status of the blob.
  void RunOnConstructionBegin(const std::string& uuid,
                              BlobStatusCallback done_callback);

  BlobStorageRegistry* mutable_registry() { return &registry_; }

  BlobMemoryController* mutable_memory_controller() {
    return &memory_controller_;
  }

 private:
  std::unique_ptr<BlobDataHandle> BuildBlobInternal(
      BlobEntry* entry,
      std::unique_ptr<BlobDataBuilder> input_builder,
      TransportAllowedCallback transport_allowed_callback);

  std::unique_ptr<BlobDataHandle> CreateHandle(const std::string& uuid,
                                               BlobEntry* entry);

  void NotifyTransportCompleteInternal(BlobEntry* entry);

  void CancelBuildingBlobInternal(BlobEntry* entry, BlobStatus reason);

  void FinishBuilding(BlobEntry* entry);

  void RequestTransport(
      BlobEntry* entry,
      std::vector<BlobMemoryController::FileCreationInfo> files);

  // The files array is empty for memory quota request responses.
  void OnEnoughSpaceForTransport(
      const std::string& uuid,
      std::vector<BlobMemoryController::FileCreationInfo> files,
      bool can_fit);

  void OnEnoughSpaceForCopies(const std::string& uuid, bool can_fit);

  void OnDependentBlobFinished(const std::string& owning_blob_uuid,
                               BlobStatus reason);

  void ClearAndFreeMemory(BlobEntry* entry);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // mojom::BlobStorageContext implementation.
  void RegisterFromDataItem(mojo::PendingReceiver<blink::mojom::Blob> blob,
                            const std::string& uuid,
                            mojom::BlobDataItemPtr item) override;
  void RegisterFromMemory(mojo::PendingReceiver<::blink::mojom::Blob> blob,
                          const std::string& uuid,
                          mojo_base::BigBuffer data) override;
  void WriteBlobToFile(mojo::PendingRemote<::blink::mojom::Blob> blob,
                       const base::FilePath& path,
                       bool flush_on_write,
                       std::optional<base::Time> last_modified,
                       WriteBlobToFileCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::BlobStorageContext> cloned) override;

  base::FilePath profile_directory_;
  BlobStorageRegistry registry_;
  BlobMemoryController memory_controller_;
  mojo::ReceiverSet<mojom::BlobStorageContext> receivers_;
  base::WeakPtrFactory<BlobStorageContext> ptr_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_
