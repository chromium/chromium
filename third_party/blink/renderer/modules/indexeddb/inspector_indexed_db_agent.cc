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

#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
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
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction_options.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using blink::protocol::Array;
using blink::protocol::IndexedDB::DatabaseWithObjectStores;
using blink::protocol::IndexedDB::DataEntry;
using blink::protocol::IndexedDB::Key;
using blink::protocol::IndexedDB::KeyPath;
using blink::protocol::IndexedDB::KeyRange;
using blink::protocol::IndexedDB::ObjectStore;
using blink::protocol::IndexedDB::ObjectStoreIndex;
using blink::protocol::Maybe;
using blink::protocol::Response;

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

Response AssertIDBFactory(Document* document, IDBFactory*& result) {
  LocalDOMWindow* dom_window = document->domWindow();
  if (!dom_window)
    return Response::Error("No IndexedDB factory for given frame found");
  IDBFactory* idb_factory = GlobalIndexedDB::indexedDB(*dom_window);

  if (!idb_factory)
    return Response::Error("No IndexedDB factory for given frame found");
  result = idb_factory;
  return Response::OK();
}

class GetDatabaseNamesCallback final : public NativeEventListener {
 public:
  GetDatabaseNamesCallback(
      std::unique_ptr<RequestDatabaseNamesCallback> request_callback,
      const String& security_origin)
      : request_callback_(std::move(request_callback)),
        security_origin_(security_origin) {}
  ~GetDatabaseNamesCallback() override = default;

  void Invoke(ExecutionContext*, Event* event) override {
    if (event->type() != event_type_names::kSuccess) {
      request_callback_->sendFailure(Response::Error("Unexpected event type."));
      return;
    }

    IDBRequest* idb_request = static_cast<IDBRequest*>(event->target());
    IDBAny* request_result = idb_request->ResultAsAny();
    if (request_result->GetType() != IDBAny::kDOMStringListType) {
      request_callback_->sendFailure(
          Response::Error("Unexpected result type."));
      return;
    }

    DOMStringList* database_names_list = request_result->DomStringList();
    auto database_names = std::make_unique<protocol::Array<String>>();
    for (uint32_t i = 0; i < database_names_list->length(); ++i)
      database_names->emplace_back(database_names_list->item(i));
    request_callback_->sendSuccess(std::move(database_names));
  }

 private:
  std::unique_ptr<RequestDatabaseNamesCallback> request_callback_;
  String security_origin_;
};

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
          Response::Error("Failed to delete database."));
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
  void Start(LocalFrame* frame, const String& database_name) {
    Document* document = frame ? frame->GetDocument() : nullptr;
    if (!document) {
      SendFailure(Response::Error(kNoDocumentError));
      return;
    }
    IDBFactory* idb_factory = nullptr;
    Response response = AssertIDBFactory(document, idb_factory);
    if (!response.isSuccess()) {
      SendFailure(response);
      return;
    }

    ScriptState* script_state = ToScriptStateForMainWorld(frame);
    if (!script_state) {
      SendFailure(Response::InternalError());
      return;
    }

    ScriptState::Scope scope(script_state);
    DoStart(idb_factory, script_state, document->GetSecurityOrigin(),
            database_name);
  }

 private:
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
      SendFailure(Response::Error("Could not open database."));
      return;
    }
    idb_open_db_request->addEventListener(event_type_names::kUpgradeneeded,
                                          upgrade_callback, false);
    idb_open_db_request->addEventListener(event_type_names::kSuccess,
                                          open_callback, false);
  }

  void SendFailure(Response response) {
    GetRequestCallback()->sendFailure(response);
  }
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
          Response::Error("Unexpected event type."));
      return;
    }

    IDBOpenDBRequest* idb_open_db_request =
        static_cast<IDBOpenDBRequest*>(event->target());
    IDBAny* request_result = idb_open_db_request->ResultAsAny();
    if (request_result->GetType() != IDBAny::kIDBDatabaseType) {
      executable_with_database_->GetRequestCallback()->sendFailure(
          Response::Error("Unexpected result type."));
      return;
    }

    IDBDatabase* idb_database = request_result->IdbDatabase();
    executable_with_database_->Execute(idb_database, script_state_);
    V8PerIsolateData::From(script_state_->GetIsolate())->RunEndOfScopeTasks();
    idb_database->close();
  }

  void Trace(blink::Visitor* visitor) override {
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
          Response::Error("Unexpected event type."));
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
        Response::Error("Aborted upgrade."));
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
  StringOrStringSequence scope;
  scope.SetString(object_store_name);
  IDBTransactionOptions options;
  options.setDurability("relaxed");
  IDBTransaction* idb_transaction = idb_database->transaction(
      script_state, scope, mode, &options, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return idb_transaction;
}

IDBObjectStore* ObjectStoreForTransaction(IDBTransaction* idb_transaction,
                                          const String& object_store_name) {
  DummyExceptionStateForTesting exception_state;
  IDBObjectStore* idb_object_store =
      idb_transaction->objectStore(object_store_name, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return idb_object_store;
}

IDBIndex* IndexForObjectStore(IDBObjectStore* idb_object_store,
                              const String& index_name) {
  DummyExceptionStateForTesting exception_state;
  IDBIndex* idb_index = idb_object_store->index(index_name, exception_state);
  if (exception_state.HadException())
    return nullptr;
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
      NOTREACHED();
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
  DatabaseLoader(std::unique_ptr<RequestDatabaseCallback> request_callback)
      : request_callback_(std::move(request_callback)) {}
  std::unique_ptr<RequestDatabaseCallback> request_callback_;
};

static std::unique_ptr<IDBKey> IdbKeyFromInspectorObject(
    protocol::IndexedDB::Key* key) {
  std::unique_ptr<IDBKey> idb_key;

  if (!key)
    return nullptr;
  String type = key->getType();

  DEFINE_STATIC_LOCAL(String, number_type, ("number"));
  DEFINE_STATIC_LOCAL(String, string_type, ("string"));
  DEFINE_STATIC_LOCAL(String, date_type, ("date"));
  DEFINE_STATIC_LOCAL(String, array_type, ("array"));

  if (type == number_type) {
    if (!key->hasNumber())
      return nullptr;
    idb_key = IDBKey::CreateNumber(key->getNumber(0));
  } else if (type == string_type) {
    if (!key->hasString())
      return nullptr;
    idb_key = IDBKey::CreateString(key->getString(String()));
  } else if (type == date_type) {
    if (!key->hasDate())
      return nullptr;
    idb_key = IDBKey::CreateDate(key->getDate(0));
  } else if (type == array_type) {
    IDBKey::KeyArray key_array;
    auto* array = key->getArray(nullptr);
    if (array) {
      for (const std::unique_ptr<protocol::IndexedDB::Key>& key : *array)
        key_array.emplace_back(IdbKeyFromInspectorObject(key.get()));
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
  if (key_range->hasLower() && !idb_lower)
    return nullptr;

  std::unique_ptr<IDBKey> idb_upper =
      IdbKeyFromInspectorObject(key_range->getUpper(nullptr));
  if (key_range->hasUpper() && !idb_upper)
    return nullptr;

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
      request_callback_->sendFailure(Response::Error("Unexpected event type."));
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
          Response::Error("Unexpected result type."));
      return;
    }

    IDBCursorWithValue* idb_cursor = request_result->IdbCursorWithValue();

    if (skip_count_) {
      DummyExceptionStateForTesting exception_state;
      idb_cursor->advance(skip_count_, exception_state);
      if (exception_state.HadException()) {
        request_callback_->sendFailure(
            Response::Error("Could not advance cursor."));
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
          Response::Error("Could not continue cursor."));
      return;
    }

    Document* document = To<Document>(ExecutionContext::From(script_state_));
    if (!document)
      return;
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

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(script_state_);
    NativeEventListener::Trace(visitor);
  }

 private:
  v8_inspector::V8InspectorSession* v8_session_;
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
          Response::Error("Could not get transaction"));
      return;
    }
    IDBObjectStore* idb_object_store =
        ObjectStoreForTransaction(idb_transaction, object_store_name_);
    if (!idb_object_store) {
      request_callback_->sendFailure(
          Response::Error("Could not get object store"));
      return;
    }

    IDBRequest* idb_request;
    if (!index_name_.IsEmpty()) {
      IDBIndex* idb_index = IndexForObjectStore(idb_object_store, index_name_);
      if (!idb_index) {
        request_callback_->sendFailure(Response::Error("Could not get index"));
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

  v8_inspector::V8InspectorSession* v8_session_;
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
  if (enabled_.Get())
    enable();
}

void InspectorIndexedDBAgent::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  if (frame == inspected_frames_->Root()) {
    v8_session_->releaseObjectGroup(
        ToV8InspectorStringView(kIndexedDBObjectGroup));
  }
}

Response InspectorIndexedDBAgent::enable() {
  enabled_.Set(true);
  return Response::OK();
}

Response InspectorIndexedDBAgent::disable() {
  enabled_.Clear();
  v8_session_->releaseObjectGroup(
      ToV8InspectorStringView(kIndexedDBObjectGroup));
  return Response::OK();
}

void InspectorIndexedDBAgent::requestDatabaseNames(
    const String& security_origin,
    std::unique_ptr<RequestDatabaseNamesCallback> request_callback) {
  LocalFrame* frame =
      inspected_frames_->FrameWithSecurityOrigin(security_origin);
  Document* document = frame ? frame->GetDocument() : nullptr;
  if (!document) {
    request_callback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idb_factory = nullptr;
  Response response = AssertIDBFactory(document, idb_factory);
  if (!response.isSuccess()) {
    request_callback->sendFailure(response);
    return;
  }

  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state) {
    request_callback->sendFailure(Response::InternalError());
    return;
  }
  ScriptState::Scope scope(script_state);
  DummyExceptionStateForTesting exception_state;
  IDBRequest* idb_request =
      idb_factory->GetDatabaseNames(script_state, exception_state);
  if (exception_state.HadException()) {
    request_callback->sendFailure(
        Response::Error("Could not obtain database names."));
    return;
  }
  idb_request->addEventListener(
      event_type_names::kSuccess,
      MakeGarbageCollected<GetDatabaseNamesCallback>(
          std::move(request_callback),
          document->GetSecurityOrigin()->ToRawString()),
      false);
}

void InspectorIndexedDBAgent::requestDatabase(
    const String& security_origin,
    const String& database_name,
    std::unique_ptr<RequestDatabaseCallback> request_callback) {
  scoped_refptr<DatabaseLoader> database_loader =
      DatabaseLoader::Create(std::move(request_callback));
  database_loader->Start(
      inspected_frames_->FrameWithSecurityOrigin(security_origin),
      database_name);
}

void InspectorIndexedDBAgent::requestData(
    const String& security_origin,
    const String& database_name,
    const String& object_store_name,
    const String& index_name,
    int skip_count,
    int page_size,
    Maybe<protocol::IndexedDB::KeyRange> key_range,
    std::unique_ptr<RequestDataCallback> request_callback) {
  IDBKeyRange* idb_key_range =
      key_range.isJust() ? IdbKeyRangeFromKeyRange(key_range.fromJust())
                         : nullptr;
  if (key_range.isJust() && !idb_key_range) {
    request_callback->sendFailure(Response::Error("Can not parse key range."));
    return;
  }

  scoped_refptr<DataLoader> data_loader = DataLoader::Create(
      v8_session_, std::move(request_callback), object_store_name, index_name,
      idb_key_range, skip_count, page_size);

  data_loader->Start(
      inspected_frames_->FrameWithSecurityOrigin(security_origin),
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
  int64_t* result_;
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
      request_callback_->sendFailure(Response::Error(error));
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
          Response::Error("Could not get transaction"));
      return;
    }
    IDBObjectStore* idb_object_store =
        ObjectStoreForTransaction(idb_transaction, object_store_name_);
    if (!idb_object_store) {
      request_callback_->sendFailure(
          Response::Error("Could not get object store"));
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
      request_callback_->sendFailure(Response::Error(
          String::Format("Could not count entries in object store '%s': %d",
                         object_store_name_.Utf8().c_str(), ec)));
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
    const String& security_origin,
    const String& database_name,
    const String& object_store_name,
    std::unique_ptr<GetMetadataCallback> request_callback) {
  scoped_refptr<GetMetadata> get_metadata =
      GetMetadata::Create(object_store_name, std::move(request_callback));
  get_metadata->Start(
      inspected_frames_->FrameWithSecurityOrigin(security_origin),
      database_name);
}

class DeleteObjectStoreEntriesListener final : public NativeEventListener {
 public:
  DeleteObjectStoreEntriesListener(
      std::unique_ptr<DeleteObjectStoreEntriesCallback> request_callback)
      : request_callback_(std::move(request_callback)) {}
  ~DeleteObjectStoreEntriesListener() override = default;

  void Invoke(ExecutionContext*, Event* event) override {
    if (event->type() != event_type_names::kSuccess) {
      request_callback_->sendFailure(
          Response::Error("Failed to delete specified entries"));
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
          Response::Error("Could not get transaction"));
      return;
    }
    IDBObjectStore* idb_object_store =
        ObjectStoreForTransaction(idb_transaction, object_store_name_);
    if (!idb_object_store) {
      request_callback_->sendFailure(
          Response::Error("Could not get object store"));
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
    const String& security_origin,
    const String& database_name,
    const String& object_store_name,
    std::unique_ptr<protocol::IndexedDB::KeyRange> key_range,
    std::unique_ptr<DeleteObjectStoreEntriesCallback> request_callback) {
  IDBKeyRange* idb_key_range = IdbKeyRangeFromKeyRange(key_range.get());
  if (!idb_key_range) {
    request_callback->sendFailure(Response::Error("Can not parse key range"));
    return;
  }
  scoped_refptr<DeleteObjectStoreEntries> delete_object_store_entries =
      DeleteObjectStoreEntries::Create(object_store_name, idb_key_range,
                                       std::move(request_callback));
  delete_object_store_entries->Start(
      inspected_frames_->FrameWithSecurityOrigin(security_origin),
      database_name);
}

class ClearObjectStoreListener final : public NativeEventListener {
 public:
  ClearObjectStoreListener(
      std::unique_ptr<ClearObjectStoreCallback> request_callback)
      : request_callback_(std::move(request_callback)) {}
  ~ClearObjectStoreListener() override = default;

  void Invoke(ExecutionContext*, Event* event) override {
    if (event->type() != event_type_names::kComplete) {
      request_callback_->sendFailure(Response::Error("Unexpected event type."));
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
          Response::Error("Could not get transaction"));
      return;
    }
    IDBObjectStore* idb_object_store =
        ObjectStoreForTransaction(idb_transaction, object_store_name_);
    if (!idb_object_store) {
      request_callback_->sendFailure(
          Response::Error("Could not get object store"));
      return;
    }

    DummyExceptionStateForTesting exception_state;
    idb_object_store->clear(script_state, exception_state);
    DCHECK(!exception_state.HadException());
    if (exception_state.HadException()) {
      ExceptionCode ec = exception_state.Code();
      request_callback_->sendFailure(Response::Error(
          String::Format("Could not clear object store '%s': %d",
                         object_store_name_.Utf8().c_str(), ec)));
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
    const String& security_origin,
    const String& database_name,
    const String& object_store_name,
    std::unique_ptr<ClearObjectStoreCallback> request_callback) {
  scoped_refptr<ClearObjectStore> clear_object_store =
      ClearObjectStore::Create(object_store_name, std::move(request_callback));
  clear_object_store->Start(
      inspected_frames_->FrameWithSecurityOrigin(security_origin),
      database_name);
}

void InspectorIndexedDBAgent::deleteDatabase(
    const String& security_origin,
    const String& database_name,
    std::unique_ptr<DeleteDatabaseCallback> request_callback) {
  LocalFrame* frame =
      inspected_frames_->FrameWithSecurityOrigin(security_origin);
  Document* document = frame ? frame->GetDocument() : nullptr;
  if (!document) {
    request_callback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idb_factory = nullptr;
  Response response = AssertIDBFactory(document, idb_factory);
  if (!response.isSuccess()) {
    request_callback->sendFailure(response);
    return;
  }

  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state) {
    request_callback->sendFailure(Response::InternalError());
    return;
  }
  ScriptState::Scope scope(script_state);
  DummyExceptionStateForTesting exception_state;
  IDBRequest* idb_request = idb_factory->CloseConnectionsAndDeleteDatabase(
      script_state, database_name, exception_state);
  if (exception_state.HadException()) {
    request_callback->sendFailure(
        Response::Error("Could not delete database."));
    return;
  }
  idb_request->addEventListener(
      event_type_names::kSuccess,
      MakeGarbageCollected<DeleteCallback>(
          std::move(request_callback),
          document->GetSecurityOrigin()->ToRawString()),
      false);
}

void InspectorIndexedDBAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
