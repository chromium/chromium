// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_registry_impl.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "storage/browser/blob/blob_builder_from_stream.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_transport_strategy.h"
#include "storage/browser/blob/blob_url_store_impl.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"

namespace storage {

namespace {

using MemoryStrategy = BlobMemoryController::Strategy;

}  // namespace

class BlobRegistryImpl::BlobUnderConstruction {
 public:
  // Holds onto a blink::mojom::DataElement struct and optionally a bound
  // mojo::Remote<blink::mojom::BytesProvider> or
  // mojo::Remote<blink::mojom::Blob>, if the element encapsulates a large byte
  // array or a blob.
  struct ElementEntry {
    explicit ElementEntry(blink::mojom::DataElementPtr e)
        : element(std::move(e)) {
      if (element && element->is_bytes())
        bytes_provider.Bind(std::move(element->get_bytes()->data));
      else if (element && element->is_blob())
        blob.Bind(std::move(element->get_blob()->blob));
    }

    ElementEntry(ElementEntry&& other) = default;
    ~ElementEntry() = default;

    ElementEntry& operator=(ElementEntry&& other) = default;

    blink::mojom::DataElementPtr element;
    mojo::Remote<blink::mojom::BytesProvider> bytes_provider;
    mojo::Remote<blink::mojom::Blob> blob;
  };

  BlobUnderConstruction(BlobRegistryImpl* blob_registry,
                        const std::string& uuid,
                        const std::string& content_type,
                        const std::string& content_disposition,
                        std::vector<ElementEntry> elements,
                        mojo::ReportBadMessageCallback bad_message_callback)
      : blob_registry_(blob_registry),
        uuid_(uuid),
        builder_(std::make_unique<BlobDataBuilder>(uuid)),
        elements_(std::move(elements)),
        bad_message_callback_(std::move(bad_message_callback)) {
    builder_->set_content_type(content_type);
    builder_->set_content_disposition(content_disposition);
  }

  // Call this after constructing to kick of fetching of UUIDs of blobs
  // referenced by this new blob. This (and any further methods) could end up
  // deleting |this| by removing it from the blobs_under_construction_
  // collection in the blob service.
  void StartTransportation(base::WeakPtr<BlobImpl> blob_impl);

  BlobUnderConstruction(const BlobUnderConstruction&) = delete;
  BlobUnderConstruction& operator=(const BlobUnderConstruction&) = delete;

  ~BlobUnderConstruction() = default;

  const std::string& uuid() const { return uuid_; }

 private:
  BlobStorageContext* context() const { return blob_registry_->context_.get(); }

  // Marks this blob as broken. If an optional |bad_message_reason| is provided,
  // this will also report a BadMessage on the binding over which the initial
  // Register request was received.
  // Also deletes |this| by removing it from the blobs_under_construction_ list.
  void MarkAsBroken(BlobStatus reason,
                    const std::string& bad_message_reason = "") {
    DCHECK(BlobStatusIsError(reason));
    DCHECK_EQ(bad_message_reason.empty(), !BlobStatusIsBadIPC(reason));
    // Cancelling would also try to delete |this| by removing it from
    // blobs_under_construction_, so preemptively own |this|.
    auto self = std::move(blob_registry_->blobs_under_construction_[uuid()]);
    // The blob might no longer have any references, in which case it may no
    // longer exist. If that happens just skip calling cancel.
    if (context() && context()->registry().HasEntry(uuid()))
      context()->CancelBuildingBlob(uuid(), reason);
    if (!bad_message_reason.empty())
      std::move(bad_message_callback_).Run(bad_message_reason);
    MarkAsFinishedAndDeleteSelf();
  }

  void MarkAsFinishedAndDeleteSelf() {
    blob_registry_->blobs_under_construction_.erase(uuid());
  }

  bool CalculateBlobMemorySize(size_t* shortcut_bytes, uint64_t* total_bytes) {
    DCHECK(shortcut_bytes);
    DCHECK(total_bytes);

    base::CheckedNumeric<uint64_t> total_size_checked = 0;
    base::CheckedNumeric<size_t> shortcut_size_checked = 0;
    for (const auto& entry : elements_) {
      const auto& e = entry.element;
      if (e->is_bytes()) {
        const auto& bytes = e->get_bytes();
        total_size_checked += bytes->length;
        if (bytes->embedded_data) {
          if (bytes->embedded_data->size() != bytes->length)
            return false;
          shortcut_size_checked += bytes->length;
        }
      } else {
        continue;
      }
      if (!total_size_checked.IsValid() || !shortcut_size_checked.IsValid())
        return false;
    }
    *shortcut_bytes = shortcut_size_checked.ValueOrDie();
    *total_bytes = total_size_checked.ValueOrDie();
    return true;
  }

  // Called when the UUID of a referenced blob is received.
  void ReceivedBlobUUID(size_t blob_index, const std::string& uuid);

  // Called by either StartFetchingBlobUUIDs or ReceivedBlobUUID when all the
  // UUIDs of referenced blobs have been resolved. Starts checking for circular
  // references. Before we can proceed with actually building the blob, all
  // referenced blobs also need to have resolved their referenced blobs (to
  // always be able to calculate the size of the newly built blob). To ensure
  // this we might have to wait for one or more possibly indirectly dependent
  // blobs to also have resolved the UUIDs of their dependencies. This waiting
  // is kicked of by this method.
  void ResolvedAllBlobUUIDs();

  void DependentBlobReady(BlobStatus status);

  // Called when all blob dependencies have been resolved, and we're sure there
  // are no circular dependencies. This finally kicks of the actually building
  // of the blob, and figures out how to transport any bytes that might need
  // transporting.
  void ResolvedAllBlobDependencies();

  // Called when memory has been reserved for this blob and transport can begin.
  // Could also be called if something caused the blob to become invalid before
  // transportation began, in which case we just give up.
  void OnReadyForTransport(
      BlobStatus status,
      std::vector<BlobMemoryController::FileCreationInfo> file_infos);

  // Called when all data has been transported, or transport has failed.
  void TransportComplete(BlobStatus result);

#if DCHECK_IS_ON()
  // Returns true if the DAG made up by this blob and any other blobs that
  // are currently being built by BlobRegistryImpl contains any cycles.
  // |path_from_root| should contain all the nodes that have been visited so
  // far on a path from whatever node we started our search from.
  bool ContainsCycles(
      std::unordered_set<BlobUnderConstruction*>* path_from_root);
#endif

  // BlobRegistryImpl we belong to.
  raw_ptr<BlobRegistryImpl> blob_registry_;

  // UUID of the blob being built.
  std::string uuid_;

  // BlobImpl representing the blob being built.
  base::WeakPtr<BlobImpl> blob_impl_;

  // BlobDataBuilder for the blob under construction. Is created in the
  // constructor, but not filled until all referenced blob UUIDs have been
  // resolved.
  std::unique_ptr<BlobDataBuilder> builder_;

  // Elements as passed in to Register.
  std::vector<ElementEntry> elements_;

  // Callback to report a BadMessage on the binding on which Register was
  // called.
  mojo::ReportBadMessageCallback bad_message_callback_;

  // Transport strategy to use when transporting data.
  std::unique_ptr<BlobTransportStrategy> transport_strategy_;

  // List of UUIDs for referenced blobs. Same size as |elements_|. All entries
  // for non-blob elements will remain empty strings.
  std::vector<std::string> referenced_blob_uuids_;

  // Number of blob UUIDs that have been resolved.
  size_t resolved_blob_uuid_count_ = 0;

  // Number of dependent blobs that have started constructing.
  size_t ready_dependent_blob_count_ = 0;

  base::WeakPtrFactory<BlobUnderConstruction> weak_ptr_factory_{this};
};

void BlobRegistryImpl::BlobUnderConstruction::StartTransportation(
    base::WeakPtr<BlobImpl> blob_impl) {
  if (!context()) {
    MarkAsFinishedAndDeleteSelf();
    return;
  }

  blob_impl_ = std::move(blob_impl);

  size_t blob_count = 0;
  for (auto& entry : elements_) {
    const auto& element = entry.element;
    if (element->is_blob()) {
      // If connection to blob is broken, something bad happened, so mark this
      // new blob as broken, which will delete |this| and keep it from doing
      // unneeded extra work.
      entry.blob.set_disconnect_handler(base::BindOnce(
          &BlobUnderConstruction::MarkAsBroken, weak_ptr_factory_.GetWeakPtr(),
          BlobStatus::ERR_REFERENCED_BLOB_BROKEN, ""));

      entry.blob->GetInternalUUID(
          base::BindOnce(&BlobUnderConstruction::ReceivedBlobUUID,
                         weak_ptr_factory_.GetWeakPtr(), blob_count++));
    } else if (element->is_bytes()) {
      entry.bytes_provider.set_disconnect_handler(base::BindOnce(
          &BlobUnderConstruction::MarkAsBroken, weak_ptr_factory_.GetWeakPtr(),
          BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, ""));
    }
  }
  referenced_blob_uuids_.resize(blob_count);

  // TODO(mek): Do we need some kind of timeout for fetching the UUIDs?
  // Without it a blob could forever remaing pending if a renderer sends us
  // a mojo::Remote<Blob> connected to a (malicious) non-responding
  // implementation.

  // Do some basic validation of bytes to transport, and determine memory
  // transport strategy to use later.
  uint64_t transport_memory_size = 0;
  size_t shortcut_size = 0;
  if (!CalculateBlobMemorySize(&shortcut_size, &transport_memory_size)) {
    MarkAsBroken(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                 "Invalid byte element sizes in BlobRegistry::Register");
    return;
  }

  const BlobMemoryController& memory_controller =
      context()->memory_controller();
  MemoryStrategy memory_strategy =
      memory_controller.DetermineStrategy(shortcut_size, transport_memory_size);
  if (memory_strategy == MemoryStrategy::TOO_LARGE) {
    MarkAsBroken(BlobStatus::ERR_OUT_OF_MEMORY);
    return;
  }

  transport_strategy_ = BlobTransportStrategy::Create(
      memory_strategy, builder_.get(),
      base::BindOnce(&BlobUnderConstruction::TransportComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      memory_controller.limits());

  if (memory_strategy != MemoryStrategy::NONE_NEEDED) {
    // Free up memory from embedded_data, since that data is going to get
    // requested asynchronously later again anyway.
    for (auto& entry : elements_) {
      if (entry.element->is_bytes())
        entry.element->get_bytes()->embedded_data = std::nullopt;
    }
  }

  // If there were no unresolved blobs, immediately proceed to the next step.
  // Currently this will only happen if there are no blobs referenced
  // whatsoever, but hopefully in the future blob UUIDs will be cached in the
  // message pipe handle, making things much more efficient in the common case.
  if (resolved_blob_uuid_count_ == referenced_blob_uuids_.size())
    ResolvedAllBlobUUIDs();
}

void BlobRegistryImpl::BlobUnderConstruction::ReceivedBlobUUID(
    size_t blob_index,
    const std::string& uuid) {
  DCHECK(referenced_blob_uuids_[blob_index].empty());
  DCHECK_LT(resolved_blob_uuid_count_, referenced_blob_uuids_.size());

  referenced_blob_uuids_[blob_index] = uuid;
  if (++resolved_blob_uuid_count_ == referenced_blob_uuids_.size())
    ResolvedAllBlobUUIDs();
}

void BlobRegistryImpl::BlobUnderConstruction::ResolvedAllBlobUUIDs() {
  DCHECK_EQ(resolved_blob_uuid_count_, referenced_blob_uuids_.size());

#if DCHECK_IS_ON()
  std::unordered_set<BlobUnderConstruction*> visited;
  if (ContainsCycles(&visited)) {
    MarkAsBroken(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                 "Cycles in blob references in BlobRegistry::Register");
    return;
  }
#endif

  if (!context()) {
    MarkAsFinishedAndDeleteSelf();
    return;
  }

  if (referenced_blob_uuids_.size() == 0) {
    ResolvedAllBlobDependencies();
    return;
  }

  for (const std::string& blob_uuid : referenced_blob_uuids_) {
    if (blob_uuid.empty() || blob_uuid == uuid() ||
        !context()->registry().HasEntry(blob_uuid)) {
      // Will delete |this|.
      MarkAsBroken(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                   "Bad blob references in BlobRegistry::Register");
      return;
    }

    std::unique_ptr<BlobDataHandle> handle =
        context()->GetBlobDataFromUUID(blob_uuid);
    handle->RunOnConstructionBegin(
        base::BindOnce(&BlobUnderConstruction::DependentBlobReady,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void BlobRegistryImpl::BlobUnderConstruction::DependentBlobReady(
    BlobStatus status) {
  if (++ready_dependent_blob_count_ == referenced_blob_uuids_.size()) {
    // Asynchronously call ResolvedAllBlobDependencies, as otherwise |this|
    // might end up getting deleted while ResolvedAllBlobUUIDs is still
    // iterating over |referenced_blob_uuids_|.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&BlobUnderConstruction::ResolvedAllBlobDependencies,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void BlobRegistryImpl::BlobUnderConstruction::ResolvedAllBlobDependencies() {
  DCHECK_EQ(resolved_blob_uuid_count_, referenced_blob_uuids_.size());
  DCHECK_EQ(ready_dependent_blob_count_, referenced_blob_uuids_.size());

  if (!context()) {
    MarkAsFinishedAndDeleteSelf();
    return;
  }

  // The blob might no longer have any references, in which case it may no
  // longer exist. If that happens there is no need to transport data.
  if (!context()->registry().HasEntry(uuid())) {
    MarkAsFinishedAndDeleteSelf();
    return;
  }

  auto blob_uuid_it = referenced_blob_uuids_.begin();
  for (const auto& entry : elements_) {
    auto& element = entry.element;
    if (element->is_bytes()) {
      if (element->get_bytes()->length > 0) {
        transport_strategy_->AddBytesElement(element->get_bytes().get(),
                                             entry.bytes_provider);
      }
    } else if (element->is_file()) {
      const auto& f = element->get_file();
      builder_->AppendFile(f->path, f->offset, f->length,
                           f->expected_modification_time.value_or(base::Time()),
                           base::NullCallback());
    } else if (element->is_blob()) {
      CHECK(blob_uuid_it != referenced_blob_uuids_.end(),
            base::NotFatalUntil::M130);
      const std::string& blob_uuid = *blob_uuid_it++;
      builder_->AppendBlob(blob_uuid, element->get_blob()->offset,
                           element->get_blob()->length, context()->registry());
    }
  }

  // BuildPreregisterdBlob might delete `this`, so store some members in local
  // variables before calling that method.
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  auto blob_impl = std::move(blob_impl_);

  // OnReadyForTransport can be called synchronously, which can call
  // MarkAsFinishedAndDeleteSelf synchronously, so don't access any members
  // after this call.
  std::unique_ptr<BlobDataHandle> new_handle =
      context()->BuildPreregisteredBlob(
          std::move(builder_),
          base::BindOnce(&BlobUnderConstruction::OnReadyForTransport,
                         weak_ptr_factory_.GetWeakPtr()));

  // BuildPreregisteredBlob might or might not have called the callback if
  // it finished synchronously. Additionally even if the blob didn't finish
  // synchronously, the callback might end up never being called, for example
  // if no transport of bytes will be needed. To make sure `this` will get
  // cleaned up regardless of how construction completes, add a
  // OnConstructionComplete callback.
  if (weak_this) {
    new_handle->RunOnConstructionComplete(base::BindOnce(
        [](base::WeakPtr<BlobUnderConstruction> blob, BlobStatus) {
          if (blob)
            blob->MarkAsFinishedAndDeleteSelf();
        },
        std::move(weak_this)));
  }

  if (blob_impl)
    blob_impl->UpdateHandle(std::move(new_handle));
}

void BlobRegistryImpl::BlobUnderConstruction::OnReadyForTransport(
    BlobStatus status,
    std::vector<BlobMemoryController::FileCreationInfo> file_infos) {
  if (!BlobStatusIsPending(status)) {
    // Done or error.
    MarkAsFinishedAndDeleteSelf();
    return;
  }
  transport_strategy_->BeginTransport(std::move(file_infos));
}

void BlobRegistryImpl::BlobUnderConstruction::TransportComplete(
    BlobStatus result) {
  if (!context()) {
    MarkAsFinishedAndDeleteSelf();
    return;
  }

  // The calls below could delete |this|, so make sure we detect that and don't
  // try to delete |this| again afterwards.
  auto weak_this = weak_ptr_factory_.GetWeakPtr();

  // Store the bad_message_callback_, so we can invoke it if needed, after
  // notifying about the blob being finished.
  auto bad_message_callback = std::move(bad_message_callback_);

  // The blob might no longer have any references, in which case it may no
  // longer exist. If that happens just skip calling Complete.
  // TODO(mek): Stop building sooner if a blob is no longer referenced.
  if (context()->registry().HasEntry(uuid())) {
    if (result == BlobStatus::DONE)
      context()->NotifyTransportComplete(uuid());
    else
      context()->CancelBuildingBlob(uuid(), result);
  }
  if (BlobStatusIsBadIPC(result)) {
    // BlobTransportStrategy might have already reported a BadMessage on the
    // BytesProvider binding, but just to be safe, also report one on the
    // BlobRegistry binding itself.
    std::move(bad_message_callback)
        .Run("Received invalid data while transporting blob");
  }
  if (weak_this)
    MarkAsFinishedAndDeleteSelf();
}

#if DCHECK_IS_ON()
bool BlobRegistryImpl::BlobUnderConstruction::ContainsCycles(
    std::unordered_set<BlobUnderConstruction*>* path_from_root) {
  if (!path_from_root->insert(this).second)
    return true;
  for (const std::string& blob_uuid : referenced_blob_uuids_) {
    if (blob_uuid.empty())
      continue;
    auto it = blob_registry_->blobs_under_construction_.find(blob_uuid);
    if (it == blob_registry_->blobs_under_construction_.end())
      continue;
    if (it->second->ContainsCycles(path_from_root))
      return true;
  }
  path_from_root->erase(this);
  return false;
}
#endif

BlobRegistryImpl::BlobRegistryImpl(base::WeakPtr<BlobStorageContext> context)
    : context_(std::move(context)) {}

BlobRegistryImpl::~BlobRegistryImpl() {
  // BlobBuilderFromStream needs to be aborted before it can be destroyed, but
  // don't iterate directly over |blobs_being_streamed_|, as this iteration can
  // can change the underlying set.
  auto builders = std::move(blobs_being_streamed_);
  for (const auto& builder : builders)
    builder->Abort();
}

void BlobRegistryImpl::Bind(
    mojo::PendingReceiver<blink::mojom::BlobRegistry> receiver,
    std::unique_ptr<Delegate> delegate) {
  DCHECK(delegate);
  receivers_.Add(this, std::move(receiver), std::move(delegate));
}

void BlobRegistryImpl::Register(
    mojo::PendingReceiver<blink::mojom::Blob> blob,
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    std::vector<blink::mojom::DataElementPtr> elements,
    RegisterCallback callback) {
  if (!context_) {
    std::move(callback).Run();
    return;
  }

  if (uuid.empty() || context_->registry().HasEntry(uuid) ||
      base::Contains(blobs_under_construction_, uuid)) {
    receivers_.ReportBadMessage(
        "Invalid UUID passed to BlobRegistry::Register");
    return;
  }

  Delegate* delegate = receivers_.current_context().get();
  DCHECK(delegate);
  std::vector<BlobUnderConstruction::ElementEntry> element_entries;
  element_entries.reserve(elements.size());
  for (auto& element : elements) {
    BlobUnderConstruction::ElementEntry entry(std::move(element));
    if (entry.element->is_file()) {
      if (!delegate->CanReadFile(entry.element->get_file()->path)) {
        std::unique_ptr<BlobDataHandle> handle = context_->AddBrokenBlob(
            uuid, content_type, content_disposition,
            BlobStatus::ERR_REFERENCED_FILE_UNAVAILABLE);
        BlobImpl::Create(std::move(handle), std::move(blob));
        std::move(callback).Run();
        return;
      }
    }
    element_entries.push_back(std::move(entry));
  }

  blobs_under_construction_[uuid] = std::make_unique<BlobUnderConstruction>(
      this, uuid, content_type, content_disposition, std::move(element_entries),
      receivers_.GetBadMessageCallback());

  std::unique_ptr<BlobDataHandle> handle = context_->AddFutureBlob(
      uuid, content_type, content_disposition,
      base::BindOnce(&BlobRegistryImpl::BlobBuildAborted,
                     weak_ptr_factory_.GetWeakPtr(), uuid));
  auto blob_impl = BlobImpl::Create(std::move(handle), std::move(blob));

  blobs_under_construction_[uuid]->StartTransportation(std::move(blob_impl));

  std::move(callback).Run();
}

void BlobRegistryImpl::RegisterFromStream(
    const std::string& content_type,
    const std::string& content_disposition,
    uint64_t expected_length,
    mojo::ScopedDataPipeConsumerHandle data,
    mojo::PendingAssociatedRemote<blink::mojom::ProgressClient> progress_client,
    RegisterFromStreamCallback callback) {
  if (!context_) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::unique_ptr<BlobBuilderFromStream> blob_builder =
      std::make_unique<BlobBuilderFromStream>(
          context_, content_type, content_disposition,
          base::BindOnce(&BlobRegistryImpl::StreamingBlobDone,
                         base::Unretained(this), std::move(callback)));
  BlobBuilderFromStream* blob_builder_ptr = blob_builder.get();
  blobs_being_streamed_.insert(std::move(blob_builder));
  blob_builder_ptr->Start(expected_length, std::move(data),
                          std::move(progress_client));
}

void BlobRegistryImpl::GetBlobFromUUID(
    mojo::PendingReceiver<blink::mojom::Blob> blob,
    const std::string& uuid,
    GetBlobFromUUIDCallback callback) {
  if (!context_) {
    std::move(callback).Run();
    return;
  }

  if (uuid.empty()) {
    receivers_.ReportBadMessage(
        "Invalid UUID passed to BlobRegistry::GetBlobFromUUID");
    return;
  }
  if (!context_->registry().HasEntry(uuid)) {
    LOG(ERROR) << "Invalid UUID: " << uuid;
    std::move(callback).Run();
    return;
  }
  BlobImpl::Create(context_->GetBlobDataFromUUID(uuid), std::move(blob));
  std::move(callback).Run();
}

void BlobRegistryImpl::BlobBuildAborted(const std::string& uuid) {
  blobs_under_construction_.erase(uuid);
}

void BlobRegistryImpl::StreamingBlobDone(
    RegisterFromStreamCallback callback,
    BlobBuilderFromStream* builder,
    std::unique_ptr<BlobDataHandle> result) {
  blobs_being_streamed_.erase(builder);
  blink::mojom::SerializedBlobPtr blob;
  if (result) {
    DCHECK_EQ(BlobStatus::DONE, result->GetBlobStatus());
    blob = blink::mojom::SerializedBlob::New(
        result->uuid(), result->content_type(), result->size(),
        mojo::NullRemote());
    BlobImpl::Create(std::move(result),
                     blob->blob.InitWithNewPipeAndPassReceiver());
  }
  std::move(callback).Run(std::move(blob));
}

}  // namespace storage
