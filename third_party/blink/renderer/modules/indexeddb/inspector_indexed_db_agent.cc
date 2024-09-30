/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/inspector_indexed_db_agent.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_transaction_options.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/modules/buckets/storage_bucket.h"
#include "third_party/blink/renderer/modules/buckets/storage_bucket_manager.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/global_indexed_db.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor_with_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_index.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_open_db_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using blink::protocol::Array;
using blink::protocol::Maybe;
using blink::protocol::IndexedDB::DatabaseWithObjectStores;
using blink::protocol::IndexedDB::DataEntry;
using blink::protocol::IndexedDB::Key;
using blink::protocol::IndexedDB::KeyPath;
using blink::protocol::IndexedDB::KeyRange;
using blink::protocol::IndexedDB::ObjectStore;
using blink::protocol::IndexedDB::ObjectStoreIndex;

typedef blink::protocol::IndexedDB::Backend::RequestDatabaseNamesCallback
    RequestDatabaseNamesCallback;
typedef blink::protocol::IndexedDB::Backend::RequestDatabaseCallback
    RequestDatabaseCallback;
typedef blink::protocol::IndexedDB::Backend::RequestDataCallback
    RequestDataCallback;
typedef blink::protocol::IndexedDB::Backend::DeleteObjectStoreEntriesCallback
    DeleteObjectStoreEntriesCallback;
typedef blink::protocol::IndexedDB::Backend::ClearObjectStoreCallback
    ClearObjectStoreCallback;
typedef blink::protocol::IndexedDB::Backend::GetMetadataCallback
    GetMetadataCallback;
typedef blink::protocol::IndexedDB::Backend::DeleteDatabaseCallback
    DeleteDatabaseCallback;

namespace blink {
namespace {

const char kIndexedDBObjectGroup[] = "indexeddb";
const char kNoDocumentError[] = "No document for given frame found";

base::expected<LocalFrame*, protocol::Response> ResolveFrame(
    InspectedFrames* inspected_frames,
    const protocol::Maybe<String>& security_origin,
    const protocol::Maybe<String>& storage_key,
    protocol::Maybe<protocol::Storage::StorageBucket>& storage_bucket) {
  if (security_origin.has_value() + storage_key.has_value() +
          storage_bucket.has_value() !=
      1) {
    return base::unexpected(protocol::Response::InvalidParams(
        "At least and at most one of security_origin, "
        "storage_key, and storage_bucket must be specified."));
  }
  LocalFrame* frame;
  if (storage_bucket.has_value()) {
    frame =
        inspected_frames->FrameWithStorageKey(storage_bucket->getStorageKey());
  } else if (storage_key.has_value()) {
    frame = inspected_frames->FrameWithStorageKey(storage_key.value());
  } else {
    frame = inspected_frames->FrameWithSecurityOrigin(security_origin.value());
  }
  if (!frame) {
    return base::unexpected(protocol::Response::ServerError(kNoDocumentError));
  }
  return frame;
}

// Gets the IDBFactory for the given frame and optional storage bucket, and
// passes it to the callback given to Start.
class ExecutableWithIdbFactory
    : public GarbageCollected<ExecutableWithIdbFactory> {
 public:
  using IdbFactoryGetterCallback =
      base::OnceCallback<void(protocol::Response, IDBFactory*)>;

  explicit ExecutableWithIdbFactory(IdbFactoryGetterCallback callback)
      : callback_(std::move(callback)) {}
  virtual ~ExecutableWithIdbFactory() = default;

  static void Start(
      LocalFrame* frame,
      protocol::Maybe<protocol::Storage::StorageBucket> storage_bucket,
      IdbFactoryGetterCallback request_callback) {
    ExecutableWithIdbFactory* idb_factory_getter =
        MakeGarbageCollected<ExecutableWithIdbFactory>(
            std::move(request_callback));
    idb_factory_getter->SetUp(frame, std::move(storage_bucket));
  }

  void Trace(Visitor* visitor) const {}

 private:
  void SetUp(LocalFrame* frame,
             protocol::Maybe<protocol::Storage::StorageBucket> storage_bucket) {
    if (storage_bucket.has_value() && storage_bucket->hasName()) {
      GetBucketIDBFactory(frame, storage_bucket->getName(""));
    } else {
      GetDefaultIDBFactory(frame);
    }
  }
  void OnFailure(protocol::Response response) {
    std::move(callback_).Run(std::move(response), nullptr);
  }
  void OnSuccess(IDBFactory* idb_factory) {
    std::move(callback_).Run(protocol::Response::Success(), idb_factory);
  }

  void GetDefaultIDBFactory(LocalFrame* frame) {
    LocalDOMWindow* dom_window = frame->DomWindow();
    CHECK(dom_window);
    IDBFactory* idb_factory = GlobalIndexedDB::indexedDB(*dom_window);

    if (!idb_factory) {
      OnFailure(protocol::Response::ServerError(
          "No IndexedDB factory for given frame found"));
      return;
    }
    OnSuccess(idb_factory);
  }

  void GetBucketIDBFactory(LocalFrame* frame,
                           const WTF::String& storage_bucket_name) {
    LocalDOMWindow* dom_window = frame->DomWindow();
    CHECK(dom_window);

    ScriptState* script_state = ToScriptStateForMainWorld(frame);
    CHECK(script_state);

    Navigator* navigator = dom_window->navigator();
    StorageBucketManager* storage_bucket_manager =
        StorageBucketManager::storageBuckets(*navigator);
    StorageBucket* storage_bucket =
        storage_bucket_manager->GetBucketForDevtools(script_state,
                                                     storage_bucket_name);
    if (storage_bucket) {
      OnSuccess(storage_bucket->indexedDB());
    } else {
      OnFailure(protocol::Response::ServerError("Couldn't retrieve bucket"));
    }
  }
  IdbFactoryGetterCallback callback_;
};

void OnGotDatabaseNames(
    std::unique_ptr<RequestDatabaseNamesCallback> request_callback,
    Vector<mojom::blink::IDBNameAndVersionPtr> names_and_versions,
    mojom::blink::IDBErrorPtr error) {
  if (error->error_code != mojom::blink::IDBException::kNoError) {
    request_callback->sendFailure(
        protocol::Response::ServerError("Could not obtain database names."));
    return;
  }

  auto database_names = std::make_unique<protocol::Array<String>>();
  for (const auto& name_and_version : names_and_versions) {
    database_names->emplace_back(name_and_version->name);
  }
  request_callback->sendSuccess(std::move(database_names));
}

class DeleteCallback final : public NativeEventListener {
 public:
  DeleteCallback(std::unique_ptr<DeleteDatabaseCallback> request_callback,
                 const String& security_origin)
      : request_callback_(std::move(request_callback)),
        security_origin_(security_origin) {}
  ~DeleteCallback() override = default;

  void Invoke(ExecutionContext*, Event* event) override {
    if (event->type() != event_type_names::kSuccess) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Failed to delete database."));
      return;
    }
    request_callback_->sendSuccess();
  }

 private:
  std::unique_ptr<DeleteDatabaseCallback> request_callback_;
  String security_origin_;
};

template <typename RequestCallback>
class OpenDatabaseCallback;
template <typename RequestCallback>
class UpgradeDatabaseCallback;

template <typename RequestCallback>
class ExecutableWithDatabase
    : public RefCounted<ExecutableWithDatabase<RequestCallback>> {
 public:
  virtual ~ExecutableWithDatabase() = default;
  virtual void Execute(IDBDatabase*, ScriptState*) = 0;
  virtual RequestCallback* GetRequestCallback() = 0;
  void Start(LocalFrame* frame,
             protocol::Maybe<protocol::Storage::StorageBucket> storage_bucket,
             const String& database_name) {
    if (!frame) {
      SendFailure(protocol::Response::ServerError(kNoDocumentError));
      return;
    }
    database_name_ = database_name;
    frame_ = frame;
    ExecutableWithIdbFactory::Start(
        frame_, std::move(storage_bucket),
        WTF::BindOnce(&ExecutableWithDatabase::OnGetIDBFactory,
                      WrapRefCounted(this)));
  }

 private:
  void OnGetIDBFactory(protocol::Response response, IDBFactory* idb_factory) {
    if (!response.IsSuccess()) {
      SendFailure(response);
      return;
    }

    ScriptState* script_state = ToScriptStateForMainWorld(frame_);
    if (!script_state) {
      SendFailure(protocol::Response::InternalError());
      return;
    }

    ScriptState::Scope scope(script_state);
    DoStart(idb_factory, script_state, frame_->DomWindow()->GetSecurityOrigin(),
            database_name_);
  }

  void DoStart(IDBFactory* idb_factory,
               ScriptState* script_state,
               const SecurityOrigin*,
               const String& database_name) {
    OpenDatabaseCallback<RequestCallback>* open_callback =
        OpenDatabaseCallback<RequestCallback>::Create(this, script_state);
    UpgradeDatabaseCallback<RequestCallback>* upgrade_callback =
        UpgradeDatabaseCallback<RequestCallback>::Create(this);
    DummyExceptionStateForTesting exception_state;
    IDBOpenDBRequest* idb_open_db_request =
        idb_factory->open(script_state, database_name, exception_state);
    if (exception_state.HadException()) {
      SendFailure(protocol::Response::ServerError("Could not open database."));
      return;
    }
    idb_open_db_request->addEventListener(event_type_names::kUpgradeneeded,
                                          upgrade_callback, false);
    idb_open_db_request->addEventListener(event_type_names::kSuccess,
                                          open_callback, false);
  }

  void SendFailure(protocol::Response response) {
    GetRequestCallback()->sendFailure(response);
  }

  Persistent<LocalFrame> frame_;
  String database_name_;
};

template <typename RequestCallback>
class OpenDatabaseCallback final : public NativeEventListener {
 public:
  static OpenDatabaseCallback* Create(
      ExecutableWithDatabase<RequestCallback>* executable_with_database,
      ScriptState* script_state) {
    return MakeGarbageCollected<OpenDatabaseCallback>(executable_with_database,
                                                      script_state);
  }

  OpenDatabaseCallback(
      ExecutableWithDatabase<RequestCallback>* executable_with_database,
      ScriptState* script_state)
      : executable_with_database_(executable_with_database),
        script_state_(script_state) {}
  ~OpenDatabaseCallback() override = default;

  void Invoke(ExecutionContext* context, Event* event) override {
    if (event->type() != event_type_names::kSuccess) {
      executable_with_database_->GetRequestCallback()->sendFailure(
          protocol::Response::ServerError("Unexpected event type."));
      return;
    }

    IDBOpenDBRequest* idb_open_db_request =
        static_cast<IDBOpenDBRequest*>(event->target());
    IDBAny* request_result = idb_open_db_request->ResultAsAny();
    if (request_result->GetType() != IDBAny::kIDBDatabaseType) {
      executable_with_database_->GetRequestCallback()->sendFailure(
          protocol::Response::ServerError("Unexpected result type."));
      return;
    }

    IDBDatabase* idb_database = request_result->IdbDatabase();
    executable_with_database_->Execute(idb_database, script_state_.Get());
    context->GetAgent()->event_loop()->RunEndOfMicrotaskCheckpointTasks();
    idb_database->close();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    NativeEventListener::Trace(visitor);
  }

 private:
  scoped_refptr<ExecutableWithDatabase<RequestCallback>>
      executable_with_database_;
  Member<ScriptState> script_state_;
};

template <typename RequestCallback>
class UpgradeDatabaseCallback final : public NativeEventListener {
 public:
  static UpgradeDatabaseCallback* Create(
      ExecutableWithDatabase<RequestCallback>* executable_with_database) {
    return MakeGarbageCollected<UpgradeDatabaseCallback>(
        executable_with_database);
  }

  UpgradeDatabaseCallback(
      ExecutableWithDatabase<RequestCallback>* executable_with_database)
      : executable_with_database_(executable_with_database) {}
  ~UpgradeDatabaseCallback() override = default;

  void Invoke(ExecutionContext* context, Event* event) override {
    if (event->type() != event_type_names::kUpgradeneeded) {
      executable_with_database_->GetRequestCallback()->sendFailure(
          protocol::Response::ServerError("Unexpected event type."));
      return;
    }

    // If an "upgradeneeded" event comes through then the database that
    // had previously been enumerated was deleted. We don't want to
    // implicitly re-create it here, so abort the transaction.
    IDBOpenDBRequest* idb_open_db_request =
        static_cast<IDBOpenDBRequest*>(event->target());
    NonThrowableExceptionState exception_state;
    idb_open_db_request->transaction()->abort(exception_state);
    executable_with_database_->GetRequestCallback()->sendFailure(
        protocol::Response::ServerError("Aborted upgrade."));
  }

 private:
  scoped_refptr<ExecutableWithDatabase<RequestCallback>>
      executable_with_database_;
};

IDBTransaction* TransactionForDatabase(
    ScriptState* script_state,
    IDBDatabase* idb_database,
    const String& object_store_name,
    const String& mode = indexed_db_names::kReadonly) {
  DummyExceptionStateForTesting exception_state;
  V8UnionStringOrStringSequence* scope =
      MakeGarbageCollected<V8UnionStringOrStringSequence>(object_store_name);
  IDBTransactionOptions* options =
      MakeGarbageCollected<IDBTransactionOptions>();
  options->setDurability("relaxed");
  auto v8_mode = V8IDBTransactionMode::Create(mode);
  if (!v8_mode) {
    return nullptr;
  }
  IDBTransaction* idb_transaction = idb_database->transaction(
      script_state, scope, v8_mode.value(), options, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return idb_transaction;
}

IDBObjectStore* ObjectStoreForTransaction(IDBTransaction* idb_transaction,
                                          const String& object_store_name) {
  DummyExceptionStateForTesting exception_state;
  IDBObjectStore* idb_object_store =
      idb_transaction->objectStore(object_store_name, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return idb_object_store;
}

IDBIndex* IndexForObjectStore(IDBObjectStore* idb_object_store,
                              const String& index_name) {
  DummyExceptionStateForTesting exception_state;
  IDBIndex* idb_index = idb_object_store->index(index_name, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return idb_index;
}

std::unique_ptr<KeyPath> KeyPathFromIDBKeyPath(const IDBKeyPath& idb_key_path) {
  std::unique_ptr<KeyPath> key_path;
  switch (idb_key_path.GetType()) {
    case mojom::IDBKeyPathType::Null:
      key_path = KeyPath::create().setType(KeyPath::TypeEnum::Null).build();
      break;
    case mojom::IDBKeyPathType::String:
      key_path = KeyPath::create()
                     .setType(KeyPath::TypeEnum::String)
                     .setString(idb_key_path.GetString())
                     .build();
      break;
    case mojom::IDBKeyPathType::Array: {
      key_path = KeyPath::create().setType(KeyPath::TypeEnum::Array).build();
      const Vector<String>& array = idb_key_path.Array();
      key_path->setArray(std::make_unique<protocol::Array<String>>(
          array.begin(), array.end()));
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return key_path;
}

class DatabaseLoader final
    : public ExecutableWithDatabase<RequestDatabaseCallback> {
 public:
  static scoped_refptr<DatabaseLoader> Create(
      std::unique_ptr<RequestDatabaseCallback> request_callback) {
    return base::AdoptRef(new DatabaseLoader(std::move(request_callback)));
  }

  ~DatabaseLoader() override = default;

  void Execute(IDBDatabase* idb_database, ScriptState*) override {
    const IDBDatabaseMetadata database_metadata = idb_database->Metadata();

    auto object_stores =
        std::make_unique<protocol::Array<protocol::IndexedDB::ObjectStore>>();

    for (const auto& store_map_entry : database_metadata.object_stores) {
      const IDBObjectStoreMetadata& object_store_metadata =
          *store_map_entry.value;

      auto indexes = std::make_unique<
          protocol::Array<protocol::IndexedDB::ObjectStoreIndex>>();

      for (const auto& metadata_map_entry : object_store_metadata.indexes) {
        const IDBIndexMetadata& index_metadata = *metadata_map_entry.value;

        std::unique_ptr<ObjectStoreIndex> object_store_index =
            ObjectStoreIndex::create()
                .setName(index_metadata.name)
                .setKeyPath(KeyPathFromIDBKeyPath(index_metadata.key_path))
                .setUnique(index_metadata.unique)
                .setMultiEntry(index_metadata.multi_entry)
                .build();
        indexes->emplace_back(std::move(object_store_index));
      }

      std::unique_ptr<ObjectStore> object_store =
          ObjectStore::create()
              .setName(object_store_metadata.name)
              .setKeyPath(KeyPathFromIDBKeyPath(object_store_metadata.key_path))
              .setAutoIncrement(object_store_metadata.auto_increment)
              .setIndexes(std::move(indexes))
              .build();
      object_stores->emplace_back(std::move(object_store));
    }
    std::unique_ptr<DatabaseWithObjectStores> result =
        DatabaseWithObjectStores::create()
            .setName(idb_database->name())
            .setVersion(idb_database->version())
            .setObjectStores(std::move(object_stores))
            .build();

    request_callback_->sendSuccess(std::move(result));
  }

  RequestDatabaseCallback* GetRequestCallback() override {
    return request_callback_.get();
  }

 private:
  explicit DatabaseLoader(
      std::unique_ptr<RequestDatabaseCallback> request_callback)
      : request_callback_(std::move(request_callback)) {}
  std::unique_ptr<RequestDatabaseCallback> request_callback_;
};

static std::unique_ptr<IDBKey> IdbKeyFromInspectorObject(
    protocol::IndexedDB::Key* key) {
  std::unique_ptr<IDBKey> idb_key;

  if (!key) {
    return nullptr;
  }
  String type = key->getType();

  DEFINE_STATIC_LOCAL(String, number_type, ("number"));
  DEFINE_STATIC_LOCAL(String, string_type, ("string"));
  DEFINE_STATIC_LOCAL(String, date_type, ("date"));
  DEFINE_STATIC_LOCAL(String, array_type, ("array"));

  if (type == number_type) {
    if (!key->hasNumber()) {
      return nullptr;
    }
    idb_key = IDBKey::CreateNumber(key->getNumber(0));
  } else if (type == string_type) {
    if (!key->hasString()) {
      return nullptr;
    }
    idb_key = IDBKey::CreateString(key->getString(String()));
  } else if (type == date_type) {
    if (!key->hasDate()) {
      return nullptr;
    }
    idb_key = IDBKey::CreateDate(key->getDate(0));
  } else if (type == array_type) {
    IDBKey::KeyArray key_array;
    auto* array = key->getArray(nullptr);
    if (array) {
      for (const std::unique_ptr<protocol::IndexedDB::Key>& elem : *array) {
        key_array.emplace_back(IdbKeyFromInspectorObject(elem.get()));
      }
    }
    idb_key = IDBKey::CreateArray(std::move(key_array));
  } else {
    return nullptr;
  }

  return idb_key;
}

static IDBKeyRange* IdbKeyRangeFromKeyRange(
    protocol::IndexedDB::KeyRange* key_range) {
  std::unique_ptr<IDBKey> idb_lower =
      IdbKeyFromInspectorObject(key_range->getLower(nullptr));
  if (key_range->hasLower() && !idb_lower) {
    return nullptr;
  }

  std::unique_ptr<IDBKey> idb_upper =
      IdbKeyFromInspectorObject(key_range->getUpper(nullptr));
  if (key_range->hasUpper() && !idb_upper) {
    return nullptr;
  }

  IDBKeyRange::LowerBoundType lower_bound_type =
      key_range->getLowerOpen() ? IDBKeyRange::kLowerBoundOpen
                                : IDBKeyRange::kLowerBoundClosed;
  IDBKeyRange::UpperBoundType upper_bound_type =
      key_range->getUpperOpen() ? IDBKeyRange::kUpperBoundOpen
                                : IDBKeyRange::kUpperBoundClosed;
  return IDBKeyRange::Create(std::move(idb_lower), std::move(idb_upper),
                             lower_bound_type, upper_bound_type);
}

class DataLoader;

class OpenCursorCallback final : public NativeEventListener {
 public:
  static OpenCursorCallback* Create(
      v8_inspector::V8InspectorSession* v8_session,
      ScriptState* script_state,
      std::unique_ptr<RequestDataCallback> request_callback,
      int skip_count,
      unsigned page_size) {
    return MakeGarbageCollected<OpenCursorCallback>(v8_session, script_state,
                                                    std::move(request_callback),
                                                    skip_count, page_size);
  }

  OpenCursorCallback(v8_inspector::V8InspectorSession* v8_session,
                     ScriptState* script_state,
                     std::unique_ptr<RequestDataCallback> request_callback,
                     int skip_count,
                     unsigned page_size)
      : v8_session_(v8_session),
        script_state_(script_state),
        request_callback_(std::move(request_callback)),
        skip_count_(skip_count),
        page_size_(page_size) {
    result_ = std::make_unique<protocol::Array<DataEntry>>();
  }
  ~OpenCursorCallback() override = default;

  void Invoke(ExecutionContext*, Event* event) override {
    if (event->type() != event_type_names::kSuccess) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Unexpected event type."));
      return;
    }

    IDBRequest* idb_request = static_cast<IDBRequest*>(event->target());
    IDBAny* request_result = idb_request->ResultAsAny();
    if (request_result->GetType() == IDBAny::kIDBValueType) {
      end(false);
      return;
    }
    if (request_result->GetType() != IDBAny::kIDBCursorWithValueType) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Unexpected result type."));
      return;
    }

    IDBCursorWithValue* idb_cursor = request_result->IdbCursorWithValue();

    if (skip_count_) {
      DummyExceptionStateForTesting exception_state;
      idb_cursor->advance(skip_count_, exception_state);
      if (exception_state.HadException()) {
        request_callback_->sendFailure(
            protocol::Response::ServerError("Could not advance cursor."));
      }
      skip_count_ = 0;
      return;
    }

    if (result_->size() == page_size_) {
      end(true);
      return;
    }

    // Continue cursor before making injected script calls, otherwise
    // transaction might be finished.
    DummyExceptionStateForTesting exception_state;
    idb_cursor->Continue(nullptr, nullptr, IDBRequest::AsyncTraceState(),
                         exception_state);
    if (exception_state.HadException()) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Could not continue cursor."));
      return;
    }

    if (!script_state_->ContextIsValid()) {
      return;
    }
    ScriptState::Scope scope(script_state_);
    v8::Local<v8::Context> context = script_state_->GetContext();
    v8_inspector::StringView object_group =
        ToV8InspectorStringView(kIndexedDBObjectGroup);
    std::unique_ptr<DataEntry> data_entry =
        DataEntry::create()
            .setKey(v8_session_->wrapObject(
                context, idb_cursor->key(script_state_).V8Value(), object_group,
                true /* generatePreview */))
            .setPrimaryKey(v8_session_->wrapObject(
                context, idb_cursor->primaryKey(script_state_).V8Value(),
                object_group, true /* generatePreview */))
            .setValue(v8_session_->wrapObject(
                context, idb_cursor->value(script_state_).V8Value(),
                object_group, true /* generatePreview */))
            .build();
    result_->emplace_back(std::move(data_entry));
  }

  void end(bool has_more) {
    request_callback_->sendSuccess(std::move(result_), has_more);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    NativeEventListener::Trace(visitor);
  }

 private:
  raw_ptr<v8_inspector::V8InspectorSession> v8_session_;
  Member<ScriptState> script_state_;
  std::unique_ptr<RequestDataCallback> request_callback_;
  int skip_count_;
  unsigned page_size_;
  std::unique_ptr<Array<DataEntry>> result_;
};

class DataLoader final : public ExecutableWithDatabase<RequestDataCallback> {
 public:
  static scoped_refptr<DataLoader> Create(
      v8_inspector::V8InspectorSession* v8_session,
      std::unique_ptr<RequestDataCallback> request_callback,
      const String& object_store_name,
      const String& index_name,
      IDBKeyRange* idb_key_range,
      int skip_count,
      unsigned page_size) {
    return base::AdoptRef(new DataLoader(
        v8_session, std::move(request_callback), object_store_name, index_name,
        idb_key_range, skip_count, page_size));
  }

  ~DataLoader() override = default;

  void Execute(IDBDatabase* idb_database, ScriptState* script_state) override {
    IDBTransaction* idb_transaction =
        TransactionForDatabase(script_state, idb_database, object_store_name_);
    if (!idb_transaction) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Could not get transaction"));
      return;
    }
    IDBObjectStore* idb_object_store =
        ObjectStoreForTransaction(idb_transaction, object_store_name_);
    if (!idb_object_store) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Could not get object store"));
      return;
    }

    IDBRequest* idb_request;
    if (!index_name_.empty()) {
      IDBIndex* idb_index = IndexForObjectStore(idb_object_store, index_name_);
      if (!idb_index) {
        request_callback_->sendFailure(
            protocol::Response::ServerError("Could not get index"));
        return;
      }

      idb_request = idb_index->openCursor(script_state, idb_key_range_.Get(),
                                          mojom::IDBCursorDirection::Next);
    } else {
      idb_request = idb_object_store->openCursor(
          script_state, idb_key_range_.Get(), mojom::IDBCursorDirection::Next);
    }
    OpenCursorCallback* open_cursor_callback = OpenCursorCallback::Create(
        v8_session_, script_state, std::move(request_callback_), skip_count_,
        page_size_);
    idb_request->addEventListener(event_type_names::kSuccess,
                                  open_cursor_callback, false);
  }

  RequestDataCallback* GetRequestCallback() override {
    return request_callback_.get();
  }
  DataLoader(v8_inspector::V8InspectorSession* v8_session,
             std::unique_ptr<RequestDataCallback> request_callback,
             const String& object_store_name,
             const String& index_name,
             IDBKeyRange* idb_key_range,
             int skip_count,
             unsigned page_size)
      : v8_session_(v8_session),
        request_callback_(std::move(request_callback)),
        object_store_name_(object_store_name),
        index_name_(index_name),
        idb_key_range_(idb_key_range),
        skip_count_(skip_count),
        page_size_(page_size) {}

  raw_ptr<v8_inspector::V8InspectorSession> v8_session_;
  std::unique_ptr<RequestDataCallback> request_callback_;
  String object_store_name_;
  String index_name_;
  Persistent<IDBKeyRange> idb_key_range_;
  int skip_count_;
  unsigned page_size_;
};

}  // namespace

// static
InspectorIndexedDBAgent::InspectorIndexedDBAgent(
    InspectedFrames* inspected_frames,
    v8_inspector::V8InspectorSession* v8_session)
    : inspected_frames_(inspected_frames),
      v8_session_(v8_session),
      enabled_(&agent_state_, /*default_value=*/false) {}

InspectorIndexedDBAgent::~InspectorIndexedDBAgent() = default;

void InspectorIndexedDBAgent::Restore() {
  if (enabled_.Get()) {
    enable();
  }
}

void InspectorIndexedDBAgent::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  if (frame == inspected_frames_->Root()) {
    v8_session_->releaseObjectGroup(
        ToV8InspectorStringView(kIndexedDBObjectGroup));
  }
}

protocol::Response InspectorIndexedDBAgent::enable() {
  enabled_.Set(true);
  return protocol::Response::Success();
}

protocol::Response InspectorIndexedDBAgent::disable() {
  enabled_.Clear();
  v8_session_->releaseObjectGroup(
      ToV8InspectorStringView(kIndexedDBObjectGroup));
  return protocol::Response::Success();
}

void InspectorIndexedDBAgent::requestDatabaseNames(
    protocol::Maybe<String> security_origin,
    protocol::Maybe<String> storage_key,
    protocol::Maybe<protocol::Storage::StorageBucket> storage_bucket,
    std::unique_ptr<RequestDatabaseNamesCallback> request_callback) {
  base::expected<LocalFrame*, protocol::Response> frame_or_response =
      ResolveFrame(inspected_frames_.Get(), security_origin, storage_key,
                   storage_bucket);
  if (!frame_or_response.has_value()) {
    request_callback->sendFailure(frame_or_response.error());
    return;
  }
  LocalFrame* frame = frame_or_response.value();
  ExecutableWithIdbFactory::Start(
      frame, std::move(storage_bucket),
      WTF::BindOnce(
          [](std::unique_ptr<RequestDatabaseNamesCallback> request_callback,
             LocalFrame* frame, protocol::Response response,
             IDBFactory* idb_factory) {
            if (!response.IsSuccess()) {
              request_callback->sendFailure(response);
              return;
            }
            ScriptState* script_state = ToScriptStateForMainWorld(frame);
            if (!script_state) {
              request_callback->sendFailure(
                  protocol::Response::InternalError());
              return;
            }
            idb_factory->GetDatabaseInfoForDevTools(WTF::BindOnce(
                &OnGotDatabaseNames, std::move(request_callback)));
          },
          std::move(request_callback), WrapPersistent(frame)));
}

void InspectorIndexedDBAgent::requestDatabase(
    protocol::Maybe<String> security_origin,
    protocol::Maybe<String> storage_key,
    protocol::Maybe<protocol::Storage::StorageBucket> storage_bucket,
    const String& database_name,
    std::unique_ptr<RequestDatabaseCallback> request_callback) {
  base::expected<LocalFrame*, protocol::Response> frame_or_response =
      ResolveFrame(inspected_frames_.Get(), security_origin, storage_key,
                   storage_bucket);
  if (!frame_or_response.has_value()) {
    request_callback->sendFailure(frame_or_response.error());
    return;
  }
  scoped_refptr<DatabaseLoader> database_loader =
      DatabaseLoader::Create(std::move(request_callback));
  database_loader->Start(frame_or_response.value(), std::move(storage_bucket),
                         database_name);
}

void InspectorIndexedDBAgent::requestData(
    protocol::Maybe<String> security_origin,
    protocol::Maybe<String> storage_key,
    protocol::Maybe<protocol::Storage::StorageBucket> storage_bucket,
    const String& database_name,
    const String& object_store_name,
    const String& index_name,
    int skip_count,
    int page_size,
    Maybe<protocol::IndexedDB::KeyRange> key_range,
    std::unique_ptr<RequestDataCallback> request_callback) {
  IDBKeyRange* idb_key_range = key_range.has_value()
                                   ? IdbKeyRangeFromKeyRange(&key_range.value())
                                   : nullptr;
  if (key_range.has_value() && !idb_key_range) {
    request_callback->sendFailure(
        protocol::Response::ServerError("Can not parse key range."));
    return;
  }
  base::expected<LocalFrame*, protocol::Response> frame_or_response =
      ResolveFrame(inspected_frames_.Get(), security_origin, storage_key,
                   storage_bucket);
  if (!frame_or_response.has_value()) {
    request_callback->sendFailure(frame_or_response.error());
    return;
  }
  scoped_refptr<DataLoader> data_loader = DataLoader::Create(
      v8_session_, std::move(request_callback), object_store_name, index_name,
      idb_key_range, skip_count, page_size);

  data_loader->Start(frame_or_response.value(), std::move(storage_bucket),
                     database_name);
}

class GetMetadata;

class GetMetadataListener final : public NativeEventListener {
 public:
  GetMetadataListener(scoped_refptr<GetMetadata> owner, int64_t* result)
      : owner_(owner), result_(result) {}
  ~GetMetadataListener() override = default;

  void Invoke(ExecutionContext*, Event* event) override {
    if (event->type() != event_type_names::kSuccess) {
      NotifySubtaskDone(owner_, "Failed to get meta data of object store.");
      return;
    }

    IDBRequest* idb_request = static_cast<IDBRequest*>(event->target());
    IDBAny* request_result = idb_request->ResultAsAny();
    if (request_result->GetType() != IDBAny::kIntegerType) {
      NotifySubtaskDone(owner_, "Unexpected result type.");
      return;
    }
    *result_ = request_result->Integer();
    NotifySubtaskDone(owner_, String());
  }

 private:
  void NotifySubtaskDone(scoped_refptr<GetMetadata> owner,
                         const String& error) const;
  scoped_refptr<GetMetadata> owner_;
  raw_ptr<int64_t> result_;
};

class GetMetadata final : public ExecutableWithDatabase<GetMetadataCallback> {
 public:
  static scoped_refptr<GetMetadata> Create(
      const String& object_store_name,
      std::unique_ptr<GetMetadataCallback> request_callback) {
    return AdoptRef(
        new GetMetadata(object_store_name, std::move(request_callback)));
  }

  void NotifySubtaskDone(const String& error) {
    if (!error.IsNull()) {
      request_callback_->sendFailure(
          protocol::Response::ServerError(error.Utf8()));
      return;
    }
    if (--subtask_pending_ == 0) {
      request_callback_->sendSuccess(entries_count_,
                                     key_generator_current_number_);
    }
  }

 private:
  GetMetadata(const String& object_store_name,
              std::unique_ptr<GetMetadataCallback> request_callback)
      : object_store_name_(object_store_name),
        request_callback_(std::move(request_callback)),
        subtask_pending_(2),
        entries_count_(-1),
        key_generator_current_number_(-1) {}

  void Execute(IDBDatabase* idb_database, ScriptState* script_state) override {
    IDBTransaction* idb_transaction =
        TransactionForDatabase(script_state, idb_database, object_store_name_,
                               indexed_db_names::kReadonly);
    if (!idb_transaction) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Could not get transaction"));
      return;
    }
    IDBObjectStore* idb_object_store =
        ObjectStoreForTransaction(idb_transaction, object_store_name_);
    if (!idb_object_store) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Could not get object store"));
      return;
    }

    // subtask 1. get entries count
    ScriptState::Scope scope(script_state);
    DummyExceptionStateForTesting exception_state;
    IDBRequest* idb_request_get_entries_count = idb_object_store->count(
        script_state, ScriptValue::CreateNull(script_state->GetIsolate()),
        exception_state);
    DCHECK(!exception_state.HadException());
    if (exception_state.HadException()) {
      ExceptionCode ec = exception_state.Code();
      request_callback_->sendFailure(protocol::Response::ServerError(
          String::Format("Could not count entries in object store '%s': %d",
                         object_store_name_.Latin1().c_str(), ec)
              .Utf8()));
      return;
    }
    GetMetadataListener* listener_get_entries_count =
        MakeGarbageCollected<GetMetadataListener>(this, &entries_count_);
    idb_request_get_entries_count->addEventListener(
        event_type_names::kSuccess, listener_get_entries_count, false);
    idb_request_get_entries_count->addEventListener(
        event_type_names::kError, listener_get_entries_count, false);

    // subtask 2. get key generator current number
    IDBRequest* idb_request_get_key_generator =
        idb_object_store->getKeyGeneratorCurrentNumber(script_state);
    GetMetadataListener* listener_get_key_generator =
        MakeGarbageCollected<GetMetadataListener>(
            this, &key_generator_current_number_);
    idb_request_get_key_generator->addEventListener(
        event_type_names::kSuccess, listener_get_key_generator, false);
    idb_request_get_key_generator->addEventListener(
        event_type_names::kError, listener_get_key_generator, false);
  }

  GetMetadataCallback* GetRequestCallback() override {
    return request_callback_.get();
  }

 private:
  const String object_store_name_;
  std::unique_ptr<GetMetadataCallback> request_callback_;
  uint8_t subtask_pending_;
  int64_t entries_count_;
  int64_t key_generator_current_number_;
};

void GetMetadataListener::NotifySubtaskDone(scoped_refptr<GetMetadata> owner,
                                            const String& error) const {
  owner->NotifySubtaskDone(error);
}

void InspectorIndexedDBAgent::getMetadata(
    protocol::Maybe<String> security_origin,
    protocol::Maybe<String> storage_key,
    protocol::Maybe<protocol::Storage::StorageBucket> storage_bucket,
    const String& database_name,
    const String& object_store_name,
    std::unique_ptr<GetMetadataCallback> request_callback) {
  base::expected<LocalFrame*, protocol::Response> frame_or_response =
      ResolveFrame(inspected_frames_.Get(), security_origin, storage_key,
                   storage_bucket);
  if (!frame_or_response.has_value()) {
    request_callback->sendFailure(frame_or_response.error());
    return;
  }
  scoped_refptr<GetMetadata> get_metadata =
      GetMetadata::Create(object_store_name, std::move(request_callback));
  get_metadata->Start(frame_or_response.value(), std::move(storage_bucket),
                      database_name);
}

class DeleteObjectStoreEntriesListener final : public NativeEventListener {
 public:
  explicit DeleteObjectStoreEntriesListener(
      std::unique_ptr<DeleteObjectStoreEntriesCallback> request_callback)
      : request_callback_(std::move(request_callback)) {}
  ~DeleteObjectStoreEntriesListener() override = default;

  void Invoke(ExecutionContext*, Event* event) override {
    if (event->type() != event_type_names::kSuccess) {
      request_callback_->sendFailure(protocol::Response::ServerError(
          "Failed to delete specified entries"));
      return;
    }

    request_callback_->sendSuccess();
  }

 private:
  std::unique_ptr<DeleteObjectStoreEntriesCallback> request_callback_;
};

class DeleteObjectStoreEntries final
    : public ExecutableWithDatabase<DeleteObjectStoreEntriesCallback> {
 public:
  static scoped_refptr<DeleteObjectStoreEntries> Create(
      const String& object_store_name,
      IDBKeyRange* idb_key_range,
      std::unique_ptr<DeleteObjectStoreEntriesCallback> request_callback) {
    return AdoptRef(new DeleteObjectStoreEntries(
        object_store_name, idb_key_range, std::move(request_callback)));
  }

  DeleteObjectStoreEntries(
      const String& object_store_name,
      IDBKeyRange* idb_key_range,
      std::unique_ptr<DeleteObjectStoreEntriesCallback> request_callback)
      : object_store_name_(object_store_name),
        idb_key_range_(idb_key_range),
        request_callback_(std::move(request_callback)) {}

  void Execute(IDBDatabase* idb_database, ScriptState* script_state) override {
    IDBTransaction* idb_transaction =
        TransactionForDatabase(script_state, idb_database, object_store_name_,
                               indexed_db_names::kReadwrite);
    if (!idb_transaction) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Could not get transaction"));
      return;
    }
    IDBObjectStore* idb_object_store =
        ObjectStoreForTransaction(idb_transaction, object_store_name_);
    if (!idb_object_store) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Could not get object store"));
      return;
    }

    IDBRequest* idb_request =
        idb_object_store->deleteFunction(script_state, idb_key_range_.Get());
    idb_request->addEventListener(
        event_type_names::kSuccess,
        MakeGarbageCollected<DeleteObjectStoreEntriesListener>(
            std::move(request_callback_)),
        false);
  }

  DeleteObjectStoreEntriesCallback* GetRequestCallback() override {
    return request_callback_.get();
  }

 private:
  const String object_store_name_;
  Persistent<IDBKeyRange> idb_key_range_;
  std::unique_ptr<DeleteObjectStoreEntriesCallback> request_callback_;
};

void InspectorIndexedDBAgent::deleteObjectStoreEntries(
    protocol::Maybe<String> security_origin,
    protocol::Maybe<String> storage_key,
    protocol::Maybe<protocol::Storage::StorageBucket> storage_bucket,
    const String& database_name,
    const String& object_store_name,
    std::unique_ptr<protocol::IndexedDB::KeyRange> key_range,
    std::unique_ptr<DeleteObjectStoreEntriesCallback> request_callback) {
  IDBKeyRange* idb_key_range = IdbKeyRangeFromKeyRange(key_range.get());
  if (!idb_key_range) {
    request_callback->sendFailure(
        protocol::Response::ServerError("Can not parse key range"));
    return;
  }
  base::expected<LocalFrame*, protocol::Response> frame_or_response =
      ResolveFrame(inspected_frames_.Get(), security_origin, storage_key,
                   storage_bucket);
  if (!frame_or_response.has_value()) {
    request_callback->sendFailure(frame_or_response.error());
    return;
  }
  scoped_refptr<DeleteObjectStoreEntries> delete_object_store_entries =
      DeleteObjectStoreEntries::Create(object_store_name, idb_key_range,
                                       std::move(request_callback));
  delete_object_store_entries->Start(frame_or_response.value(),
                                     std::move(storage_bucket), database_name);
}

class ClearObjectStoreListener final : public NativeEventListener {
 public:
  explicit ClearObjectStoreListener(
      std::unique_ptr<ClearObjectStoreCallback> request_callback)
      : request_callback_(std::move(request_callback)) {}
  ~ClearObjectStoreListener() override = default;

  void Invoke(ExecutionContext*, Event* event) override {
    if (event->type() != event_type_names::kComplete) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Unexpected event type."));
      return;
    }

    request_callback_->sendSuccess();
  }

 private:
  std::unique_ptr<ClearObjectStoreCallback> request_callback_;
};

class ClearObjectStore final
    : public ExecutableWithDatabase<ClearObjectStoreCallback> {
 public:
  static scoped_refptr<ClearObjectStore> Create(
      const String& object_store_name,
      std::unique_ptr<ClearObjectStoreCallback> request_callback) {
    return base::AdoptRef(
        new ClearObjectStore(object_store_name, std::move(request_callback)));
  }

  ClearObjectStore(const String& object_store_name,
                   std::unique_ptr<ClearObjectStoreCallback> request_callback)
      : object_store_name_(object_store_name),
        request_callback_(std::move(request_callback)) {}

  void Execute(IDBDatabase* idb_database, ScriptState* script_state) override {
    IDBTransaction* idb_transaction =
        TransactionForDatabase(script_state, idb_database, object_store_name_,
                               indexed_db_names::kReadwrite);
    if (!idb_transaction) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Could not get transaction"));
      return;
    }
    IDBObjectStore* idb_object_store =
        ObjectStoreForTransaction(idb_transaction, object_store_name_);
    if (!idb_object_store) {
      request_callback_->sendFailure(
          protocol::Response::ServerError("Could not get object store"));
      return;
    }

    DummyExceptionStateForTesting exception_state;
    idb_object_store->clear(script_state, exception_state);
    DCHECK(!exception_state.HadException());
    if (exception_state.HadException()) {
      ExceptionCode ec = exception_state.Code();
      request_callback_->sendFailure(protocol::Response::ServerError(
          String::Format("Could not clear object store '%s': %d",
                         object_store_name_.Latin1().c_str(), ec)
              .Utf8()));
      return;
    }
    idb_transaction->addEventListener(
        event_type_names::kComplete,
        MakeGarbageCollected<ClearObjectStoreListener>(
            std::move(request_callback_)),
        false);
  }

  ClearObjectStoreCallback* GetRequestCallback() override {
    return request_callback_.get();
  }

 private:
  const String object_store_name_;
  std::unique_ptr<ClearObjectStoreCallback> request_callback_;
};

void InspectorIndexedDBAgent::clearObjectStore(
    protocol::Maybe<String> security_origin,
    protocol::Maybe<String> storage_key,
    protocol::Maybe<protocol::Storage::StorageBucket> storage_bucket,
    const String& database_name,
    const String& object_store_name,
    std::unique_ptr<ClearObjectStoreCallback> request_callback) {
  base::expected<LocalFrame*, protocol::Response> frame_or_response =
      ResolveFrame(inspected_frames_.Get(), security_origin, storage_key,
                   storage_bucket);
  if (!frame_or_response.has_value()) {
    request_callback->sendFailure(frame_or_response.error());
    return;
  }
  scoped_refptr<ClearObjectStore> clear_object_store =
      ClearObjectStore::Create(object_store_name, std::move(request_callback));
  clear_object_store->Start(frame_or_response.value(),
                            std::move(storage_bucket), database_name);
}

void InspectorIndexedDBAgent::deleteDatabase(
    protocol::Maybe<String> security_origin,
    protocol::Maybe<String> storage_key,
    protocol::Maybe<protocol::Storage::StorageBucket> storage_bucket,
    const String& database_name,
    std::unique_ptr<DeleteDatabaseCallback> request_callback) {
  base::expected<LocalFrame*, protocol::Response> frame_or_response =
      ResolveFrame(inspected_frames_.Get(), security_origin, storage_key,
                   storage_bucket);
  if (!frame_or_response.has_value()) {
    request_callback->sendFailure(frame_or_response.error());
    return;
  }
  LocalFrame* frame = frame_or_response.value();
  ExecutableWithIdbFactory::Start(
      frame, std::move(storage_bucket),
      WTF::BindOnce(
          [](std::unique_ptr<DeleteDatabaseCallback> request_callback,
             LocalFrame* frame, String database_name,
             protocol::Response response, IDBFactory* idb_factory) {
            if (!response.IsSuccess()) {
              request_callback->sendFailure(response);
              return;
            }

            ScriptState* script_state = ToScriptStateForMainWorld(frame);
            if (!script_state) {
              request_callback->sendFailure(
                  protocol::Response::InternalError());
              return;
            }
            ScriptState::Scope scope(script_state);
            DummyExceptionStateForTesting exception_state;
            IDBRequest* idb_request =
                idb_factory->CloseConnectionsAndDeleteDatabase(
                    script_state, database_name, exception_state);
            if (exception_state.HadException()) {
              request_callback->sendFailure(protocol::Response::ServerError(
                  "Could not delete database."));
              return;
            }
            idb_request->addEventListener(
                event_type_names::kSuccess,
                MakeGarbageCollected<DeleteCallback>(
                    std::move(request_callback),
                    frame->DomWindow()->GetSecurityOrigin()->ToRawString()),
                false);
          },
          std::move(request_callback), WrapPersistent(frame), database_name));
}

void InspectorIndexedDBAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
