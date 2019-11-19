# Chrome Sync's Model API

Chrome Sync operates on discrete, explicitly defined model types (bookmarks,
preferences, tabs, etc). These model types are individually responsible for
implementing their own local storage and responding to remote changes. This
guide is for developers interested in syncing data for their model type to the
cloud using Chrome Sync. It describes the newest version of the API, known as
Unified Sync and Storage (USS). There is also the deprecated [SyncableService
API] (aka Directory), which as of mid-2019 is still used by several legacy model
types, but "wrapped into" USS (see [SyncableServiceBasedBridge]).

[SyncableService API]: https://www.chromium.org/developers/design-documents/sync/syncable-service-api
[SyncableServiceBasedBridge]: https://cs.chromium.org/chromium/src/components/sync/model_impl/syncable_service_based_bridge.h

[TOC]

## Overview

To correctly sync data, USS requires that sync metadata be stored alongside your
model data in a way such that they are written together atomically. **This is
very important!** Sync must be able to update the metadata for any local data
changes as part of the same write to disk. If you attempt to write data to disk
and only notify sync afterwards, a crash in between the two writes can result in
changes being dropped and never synced to the server, or data being duplicated
due to being committed more than once.

[`ModelTypeSyncBridge`][Bridge] is the interface the model code must implement.
The bridge is usually owned by a [`KeyedService`][KeyedService].
The correct place for the bridge generally lies as close to where your model
data is stored as possible, as the bridge needs to be able to inject metadata
updates into any local data changes that occur.

The bridge owns a [`ModelTypeChangeProcessor`][MTCP] object, which it uses to
communicate local changes to sync using the `Put` and `Delete` methods.
The processor will communicate remote changes from sync to the bridge using the
`MergeSyncData` and `ApplySyncChanges` methods, respectively for the initial
merge of remote and local data, and for incremental changes coming from sync.
[`MetadataChangeList`][MCL] is the way sync communicates metadata changes to the
storage mechanism. Note that it is typically implemented on a per-storage basis,
not a per-type basis.

[Bridge]: https://cs.chromium.org/chromium/src/components/sync/model/model_type_sync_bridge.h
[KeyedService]: https://cs.chromium.org/chromium/src/components/keyed_service/core/keyed_service.h
[MTCP]: https://cs.chromium.org/chromium/src/components/sync/model/model_type_change_processor.h
[MCL]: https://cs.chromium.org/chromium/src/components/sync/model/metadata_change_list.h

## Data

### Specifics

Model types will define a proto that contains the necessary fields of the
corresponding native type (e.g. [`TypedUrlSpecifics`][TypedUrlSpecifics]
contains a URL and a list of visit timestamps) and include it as a field in the
generic [`EntitySpecifics`][EntitySpecifics] proto. This is the form that all
communications with sync will use. This proto form of the model data is referred
to as the specifics.

[TypedUrlSpecifics]: https://cs.chromium.org/chromium/src/components/sync/protocol/typed_url_specifics.proto
[EntitySpecifics]: https://cs.chromium.org/search/?q="message+EntitySpecifics"+file:sync.proto

### Identifiers

There are two primary identifiers for entities: **storage key** and **client
tag**. The bridge will need to take an [`EntityData`][EntityData] object (which
contains the specifics) and be able generate both of these from it. For
non-legacy types without significant performance concerns, these will generally
be the same.

The storage key is meant to be the primary key in the local model/database.
It’s what’s used to refer to entities most of the time and, as its name implies,
the bridge needs to be able to look up local data and metadata entries in the
store using it. Because it is a local identifier, it can change as part of
database migrations, etc. This may be desirable for efficiency reasons.

The client tag is used to generate the **client tag hash**, which will identify
entities **across clients**. This means that its implementation can **never
change** once entities have begun to sync, without risking massive duplication
of entities. This means it must be generated using only immutable data in the
specifics. If your type does not have any immutable fields to use, you will need
to add one (e.g. a GUID, though be wary as they have the potential to conflict).
While the hash gets written to disk as part of the metadata, the tag itself is
never persisted locally.

[EntityData]: https://cs.chromium.org/chromium/src/components/sync/model/entity_data.h

## Storage

A crucial requirement of USS is that the model must add support for keeping
sync’s metadata in the same storage as its normal data. The metadata consists of
one [`EntityMetadata`][EntityMetadata] proto for each data entity, and one
[`ModelTypeState`][ModelTypeState] proto containing metadata pertaining to the
state of the entire type (the progress marker, for example). This typically
requires two extra tables in a database to do (one for each type of proto).

Since the processor doesn’t know anything about the store, the bridge provides
it with an implementation of the [`MetadataChangeList`][MCL] interface. The
change processor writes metadata through this interface when changes occur, and
the bridge simply has to ensure it gets passed along to the store and written
along with the data changes.

[EntityMetadata]: https://cs.chromium.org/chromium/src/components/sync/protocol/entity_metadata.proto
[ModelTypeState]: https://cs.chromium.org/chromium/src/components/sync/protocol/model_type_state.proto

### ModelTypeStore

While the model type may store its data however it chooses, many types use
[`ModelTypeStore`][Store], which was created specifically to provide a
convenient persistence solution. It’s backed by a [LevelDB] to store serialized
protos to disk. `ModelTypeStore` provides two `MetadataChangeList`
implementations for convenience; both accessed via
[`ModelTypeStore::WriteBatch`][WriteBatch]. One passes metadata changes directly
into an existing `WriteBatch` and another caches them in memory until a
`WriteBatch` exists to consume them.

The store interface abstracts away the type and will handle setting up tables
for the type’s data, so multiple `ModelTypeStore` objects for different types
can share the same LevelDB backend just by specifying the same path and task
runner. Sync already has a backend it uses for DeviceInfo that can be shared by
other types via the [`ModelTypeStoreService`][StoreService].

[Store]: https://cs.chromium.org/chromium/src/components/sync/model/model_type_store.h
[LevelDB]: https://github.com/google/leveldb/blob/master/doc/index.md
[WriteBatch]: https://cs.chromium.org/search/?q="class+WriteBatch"+file:model_type_store_base.h
[StoreService]: https://cs.chromium.org/chromium/src/components/sync/model/model_type_store_service.h

## Implementing ModelTypeSyncBridge

### Initialization

The bridge is required to load all of the metadata for its type from storage and
provide it to the processor via the [`ModelReadyToSync`][ModelReadyToSync]
method **before any local changes occur**. This can be tricky if the thread the
bridge runs on is different from the storage mechanism. No data will be synced
with the server if the processor is never informed that the model is ready.

Since the tracking of changes and updating of metadata is completely
independent, there is no need to wait for the sync engine to start before
changes can be made. This prevents the need for an expensive association step in
the initialization.

[ModelReadyToSync]: https://cs.chromium.org/search/?q=ModelReadyToSync+file:/model_type_change_processor.h

### MergeSyncData

This method is called only once, when a type is first enabled. Sync will
download all the data it has for the type from the server and provide it to the
bridge using this method. Sync filters out any tombstones for this call, so
`EntityData::is_deleted()` will never be true for the provided entities. The
bridge must then examine the sync data and the local data and merge them
together:

*   Any remote entities that don’t exist locally must be be written to local
    storage.
*   Any local entities that don’t exist remotely must be provided to sync via
    [`ModelTypeChangeProcessor::Put`][Put].
*   Any entities that appear in both sets must be merged and the model and sync
    informed accordingly. Decide which copy of the data to use (or a merged
    version or neither) and update the local store and sync as necessary to
    reflect the decision. How the decision is made can vary by model type.

The [`MetadataChangeList`][MCL] passed into the function is already populated
with metadata for all the data passed in (note that neither the data nor the
metadata have been committed to storage yet at this point). It must be given to
the processor for any `Put` or `Delete` calls so the relevant metadata can be
added/updated/deleted, and then passed to the store for persisting along with
the data.

Note that if sync gets disabled and the metadata cleared, entities that
originated from other clients will exist as “local” entities the next time sync
starts and merge is called. Since tombstones are not provided for merge, this
can result in reviving the entity if it had been deleted on another client in
the meantime.

[Put]: https://cs.chromium.org/search/?q=Put+file:/model_type_change_processor.h

### ApplySyncChanges

While `MergeSyncData` provides the state of sync data using `EntityData`
objects, `ApplySyncChanges` provides changes to the state using
[`EntityChange`][EntityChange] objects. These changes must be applied to the
local state.

Here’s an example implementation of a type using `ModelTypeStore`:

```cpp
base::Optional<ModelError> DeviceInfoSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  for (const EntityChange& change : entity_changes) {
    if (change.type() == EntityChange::ACTION_DELETE) {
      batch->DeleteData(change.storage_key());
    } else {
      batch->WriteData(change.storage_key(),
                       change.data().specifics.your_type().SerializeAsString());
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(std::move(batch), base::Bind(...));
  NotifyModelOfChanges();
  return {};
}
```

A conflict can occur when an entity has a pending local commit when an update
for the same entity comes from another client. In this case, the bridge’s
[`ResolveConflict`][ResolveConflict] method will have been called prior to the
`ApplySyncChanges` call in order to determine what should happen. This method
defaults to having the remote version overwrite the local version unless the
remote version is a tombstone, in which case the local version wins.

[EntityChange]: https://cs.chromium.org/chromium/src/components/sync/model/entity_change.h
[ResolveConflict]: https://cs.chromium.org/search/?q=ResolveConflict+file:/model_type_sync_bridge.h

### Local changes

The [`ModelTypeChangeProcessor`][MTCP] must be informed of any local changes via
its `Put` and `Delete` methods. Since the processor cannot do any useful
metadata tracking until `MergeSyncData` is called, the `IsTrackingMetadata`
method is provided. It can be checked as an optimization to prevent unnecessary
processing preparing the parameters to a `Put` or `Delete` call.

Here’s an example of handling a local write using `ModelTypeStore`:

```cpp
void WriteLocalChange(std::string key, ModelData data) {
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  if (change_processor()->IsTrackingMetadata()) {
    change_processor()->Put(key, ModelToEntityData(data),
                            batch->GetMetadataChangeList());
  }
  batch->WriteData(key, specifics->SerializeAsString());
  store_->CommitWriteBatch(std::move(batch), base::Bind(...));
}
```

## Error handling

If any errors occur during store operations that could compromise the
consistency of the data and metadata, the processor’s
[`ReportError`][ReportError] method should be called. The only exception to this
is errors during `MergeSyncData` or `ApplySyncChanges`, which should just return
a [`ModelError`][ModelError].

This will inform sync of the error, which will stop all communications with the
server so bad data doesn’t get synced. Since the metadata might no longer be
valid, the bridge will asynchronously receive an `ApplyStopSyncChanges` call
with a non-null `MetadataChangeList` parameter. All the metadata will be cleared
from the store (if possible), and the type will be started again from scratch on
the next client restart.

[ReportError]: https://cs.chromium.org/search/?q=ReportError+file:/model_type_change_processor.h
[ModelError]: https://cs.chromium.org/chromium/src/components/sync/model/model_error.h

## Sync Integration Checklist

*   Define your specifics proto in [`//components/sync/protocol/`][protocol].
*   Add a field for it to [`EntitySpecifics`][EntitySpecifics].
*   Add it to the [`ModelType`][ModelType] enum and
    [`kModelTypeInfoMap`][info_map].
*   Add it to the [proto value conversions][conversions] files.
*   Register a [`ModelTypeController`][ModelTypeController] for your type in
    [`ProfileSyncComponentsFactoryImpl::CreateCommonDataTypeControllers`][CreateCommonDataTypeControllers] or platform-specific equivalent in
    [`ChromeSyncClient::CreateDataTypeControllers`][CreateDataTypeControllers].
*   Add your KeyedService dependency to
    [`ProfileSyncServiceFactory`][ProfileSyncServiceFactory].
*   Add to the [start order list][kStartOrder].
*   Add an field for encrypted data to [`NigoriSpecifics`][NigoriSpecifics].
*   Add to two encrypted types translation functions in
    [`nigori_util.cc`][nigori_util].
*   If your type should have its own toggle in sync settings, add an entry to
    the [`UserSelectableType`][UserSelectableType] enum, add a
    [preference][pref_names] for tracking whether your type is enabled, and
    map your type to the pref in [`GetPrefNameForType`][GetPrefName].
*   Otherwise, if your type should be included in an existing toggle in sync
    settings, add it in [`GetUserSelectableTypeInfo`]
    [GetUserSelectableTypeInfo].
*   Add to the `SyncModelTypes` enum in [`enums.xml`][enums] and to the
    `SyncModelType` suffix in [`histograms.xml`][histograms].
*   Add to the [`SYNC_DATA_TYPE_HISTOGRAM`][DataTypeHistogram] macro.

[protocol]: https://cs.chromium.org/chromium/src/components/sync/protocol/
[ModelType]: https://cs.chromium.org/chromium/src/components/sync/base/model_type.h
[info_map]: https://cs.chromium.org/search/?q="kModelTypeInfoMap%5B%5D"+file:model_type.cc
[conversions]: https://cs.chromium.org/chromium/src/components/sync/protocol/proto_value_conversions.h
[ModelTypeController]: https://cs.chromium.org/chromium/src/components/sync/driver/model_type_controller.h
[CreateCommonDataTypeControllers]: https://cs.chromium.org/search/?q="ProfileSyncComponentsFactoryImpl::CreateCommonDataTypeControllers"
[CreateDataTypeControllers]: https://cs.chromium.org/search/?q="ChromeSyncClient::CreateDataTypeControllers"
[ProfileSyncServiceFactory]: https://cs.chromium.org/search/?q=:ProfileSyncServiceFactory%5C(%5C)
[kStartOrder]: https://cs.chromium.org/search/?q="kStartOrder[]"
[NigoriSpecifics]: https://cs.chromium.org/chromium/src/components/sync/protocol/nigori_specifics.proto
[nigori_util]: https://cs.chromium.org/chromium/src/components/sync/syncable/nigori_util.cc
[UserSelectableType]: https://cs.chromium.org/chromium/src/components/sync/base/user_selectable_type.h?type=cs&q="enum+class+UserSelectableType"
[pref_names]: https://cs.chromium.org/chromium/src/components/sync/base/pref_names.h
[GetPrefName]: https://cs.chromium.org/search/?q=GetPrefNameForType+file:sync_prefs.cc
[GetUserSelectableTypeInfo]: https://cs.chromium.org/chromium/src/components/sync/base/user_selectable_type.cc?type=cs&q="UserSelectableTypeInfo+GetUserSelectableTypeInfo"+f:components/sync/base/user_selectable_type.cc
[enums]: https://cs.chromium.org/chromium/src/tools/metrics/histograms/enums.xml
[histograms]: https://cs.chromium.org/chromium/src/tools/metrics/histograms/histograms.xml
[DataTypeHistogram]: https://cs.chromium.org/chromium/src/components/sync/base/data_type_histogram.h

## Testing

The [`TwoClientTypedUrlsSyncTest`][UssTest] suite is probably a good place to start
for integration testing. Especially note the use of a `StatusChangeChecker` to
wait for events to happen.

[UssTest]: https://cs.chromium.org/chromium/src/chrome/browser/sync/test/integration/two_client_typed_urls_sync_test.cc
