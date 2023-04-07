/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webdatabase/inspector_database_agent.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/database_client.h"
#include "third_party/blink/renderer/modules/webdatabase/database_tracker.h"
#include "third_party/blink/renderer/modules/webdatabase/inspector_database_resource.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_error.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_result_set.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_result_set_row_list.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sql_value.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

typedef blink::protocol::Database::Backend::ExecuteSQLCallback
    ExecuteSQLCallback;

namespace blink {
using protocol::Maybe;

namespace {

class ExecuteSQLCallbackWrapper : public RefCounted<ExecuteSQLCallbackWrapper> {
 public:
  static scoped_refptr<ExecuteSQLCallbackWrapper> Create(
      std::unique_ptr<ExecuteSQLCallback> callback) {
    return base::AdoptRef(new ExecuteSQLCallbackWrapper(std::move(callback)));
  }
  ~ExecuteSQLCallbackWrapper() = default;
  ExecuteSQLCallback* Get() { return callback_.get(); }

  void ReportTransactionFailed(SQLError* error) {
    auto error_object = protocol::Database::Error::create()
                            .setMessage(error->message())
                            .setCode(error->code())
                            .build();
    callback_->sendSuccess(Maybe<protocol::Array<String>>(),
                           Maybe<protocol::Array<protocol::Value>>(),
                           std::move(error_object));
  }

 private:
  explicit ExecuteSQLCallbackWrapper(
      std::unique_ptr<ExecuteSQLCallback> callback)
      : callback_(std::move(callback)) {}
  std::unique_ptr<ExecuteSQLCallback> callback_;
};

class StatementCallback final : public SQLStatement::OnSuccessCallback {
 public:
  explicit StatementCallback(
      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback)
      : request_callback_(std::move(request_callback)) {}
  ~StatementCallback() override = default;

  bool OnSuccess(SQLTransaction*, SQLResultSet* result_set) override {
    SQLResultSetRowList* row_list = result_set->rows();

    const Vector<String>& columns = row_list->ColumnNames();
    auto column_names = std::make_unique<protocol::Array<String>>(
        columns.begin(), columns.end());

    auto values = std::make_unique<protocol::Array<protocol::Value>>();
    const Vector<SQLValue>& data = row_list->Values();
    for (wtf_size_t i = 0; i < data.size(); ++i) {
      const SQLValue& value = row_list->Values()[i];
      switch (value.GetType()) {
        case SQLValue::kStringValue:
          values->emplace_back(
              protocol::StringValue::create(value.GetString()));
          break;
        case SQLValue::kNumberValue:
          values->emplace_back(
              protocol::FundamentalValue::create(value.Number()));
          break;
        case SQLValue::kNullValue:
          values->emplace_back(protocol::Value::null());
          break;
      }
    }
    request_callback_->Get()->sendSuccess(std::move(column_names),
                                          std::move(values),
                                          Maybe<protocol::Database::Error>());
    return true;
  }

 private:
  scoped_refptr<ExecuteSQLCallbackWrapper> request_callback_;
};

class StatementErrorCallback final : public SQLStatement::OnErrorCallback {
 public:
  explicit StatementErrorCallback(
      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback)
      : request_callback_(std::move(request_callback)) {}
  ~StatementErrorCallback() override = default;

  bool OnError(SQLTransaction*, SQLError* error) override {
    request_callback_->ReportTransactionFailed(error);
    return true;
  }

 private:
  scoped_refptr<ExecuteSQLCallbackWrapper> request_callback_;
};

class TransactionCallback final : public SQLTransaction::OnProcessCallback {
 public:
  explicit TransactionCallback(
      const String& sql_statement,
      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback)
      : sql_statement_(sql_statement),
        request_callback_(std::move(request_callback)) {}
  ~TransactionCallback() override = default;

  bool OnProcess(SQLTransaction* transaction) override {
    Vector<SQLValue> sql_values;
    transaction->ExecuteSQL(
        sql_statement_, sql_values,
        MakeGarbageCollected<StatementCallback>(request_callback_),
        MakeGarbageCollected<StatementErrorCallback>(request_callback_),
        IGNORE_EXCEPTION_FOR_TESTING);
    return true;
  }

 private:
  String sql_statement_;
  scoped_refptr<ExecuteSQLCallbackWrapper> request_callback_;
};

class TransactionErrorCallback final : public SQLTransaction::OnErrorCallback {
 public:
  static TransactionErrorCallback* Create(
      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback) {
    return MakeGarbageCollected<TransactionErrorCallback>(
        std::move(request_callback));
  }

  explicit TransactionErrorCallback(
      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback)
      : request_callback_(std::move(request_callback)) {}
  ~TransactionErrorCallback() override = default;

  bool OnError(SQLError* error) override {
    request_callback_->ReportTransactionFailed(error);
    return true;
  }

 private:
  scoped_refptr<ExecuteSQLCallbackWrapper> request_callback_;
};

}  // namespace

void InspectorDatabaseAgent::RegisterDatabaseOnCreation(
    blink::Database* database) {
  DidOpenDatabase(database, database->GetSecurityOrigin()->Host(),
                  database->StringIdentifier(), database->version());
}

void InspectorDatabaseAgent::DidOpenDatabase(blink::Database* database,
                                             const String& domain,
                                             const String& name,
                                             const String& version) {
  if (InspectorDatabaseResource* resource =
          FindByFileName(database->FileName())) {
    resource->SetDatabase(database);
    return;
  }

  auto* resource = MakeGarbageCollected<InspectorDatabaseResource>(
      database, domain, name, version);
  resources_.Set(resource->Id(), resource);
  // Resources are only bound while visible.
  DCHECK(enabled_.Get());
  DCHECK(GetFrontend());
  resource->Bind(GetFrontend());
}

void InspectorDatabaseAgent::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  // FIXME(dgozman): adapt this for out-of-process iframes.
  if (frame != page_->MainFrame())
    return;

  resources_.clear();
}

InspectorDatabaseAgent::InspectorDatabaseAgent(Page* page)
    : page_(page), enabled_(&agent_state_, /*default_value=*/false) {}

InspectorDatabaseAgent::~InspectorDatabaseAgent() = default;

void InspectorDatabaseAgent::InnerEnable() {
  if (DatabaseClient* client = DatabaseClient::FromPage(page_))
    client->SetInspectorAgent(this);
  DatabaseTracker::Tracker().ForEachOpenDatabaseInPage(
      page_,
      WTF::BindRepeating(&InspectorDatabaseAgent::RegisterDatabaseOnCreation,
                         WrapPersistent(this)));
}

protocol::Response InspectorDatabaseAgent::enable() {
  if (enabled_.Get())
    return protocol::Response::Success();
  enabled_.Set(true);
  InnerEnable();
  return protocol::Response::Success();
}

protocol::Response InspectorDatabaseAgent::disable() {
  if (!enabled_.Get())
    return protocol::Response::Success();
  enabled_.Set(false);
  if (DatabaseClient* client = DatabaseClient::FromPage(page_))
    client->SetInspectorAgent(nullptr);
  resources_.clear();
  return protocol::Response::Success();
}

void InspectorDatabaseAgent::Restore() {
  if (enabled_.Get())
    InnerEnable();
}

protocol::Response InspectorDatabaseAgent::getDatabaseTableNames(
    const String& database_id,
    std::unique_ptr<protocol::Array<String>>* names) {
  if (!enabled_.Get())
    return protocol::Response::ServerError("Database agent is not enabled");

  blink::Database* database = DatabaseForId(database_id);
  if (database) {
    Vector<String> table_names = database->TableNames();
    *names = std::make_unique<protocol::Array<String>>(table_names.begin(),
                                                       table_names.end());
  } else {
    *names = std::make_unique<protocol::Array<String>>();
  }
  return protocol::Response::Success();
}

void InspectorDatabaseAgent::executeSQL(
    const String& database_id,
    const String& query,
    std::unique_ptr<ExecuteSQLCallback> request_callback) {
  if (!enabled_.Get()) {
    request_callback->sendFailure(
        protocol::Response::ServerError("Database agent is not enabled"));
    return;
  }

  blink::Database* database = DatabaseForId(database_id);
  if (!database) {
    request_callback->sendFailure(
        protocol::Response::ServerError("Database not found"));
    return;
  }

  scoped_refptr<ExecuteSQLCallbackWrapper> wrapper =
      ExecuteSQLCallbackWrapper::Create(std::move(request_callback));
  auto* callback = MakeGarbageCollected<TransactionCallback>(query, wrapper);
  TransactionErrorCallback* error_callback =
      TransactionErrorCallback::Create(wrapper);
  SQLTransaction::OnSuccessCallback* success_callback = nullptr;
  database->PerformTransaction(callback, error_callback, success_callback);
}

InspectorDatabaseResource* InspectorDatabaseAgent::FindByFileName(
    const String& file_name) {
  for (auto& resource : resources_) {
    if (resource.value->GetDatabase()->FileName() == file_name)
      return resource.value.Get();
  }
  return nullptr;
}

blink::Database* InspectorDatabaseAgent::DatabaseForId(
    const String& database_id) {
  DatabaseResourcesHeapMap::iterator it = resources_.find(database_id);
  if (it == resources_.end())
    return nullptr;
  return it->value->GetDatabase();
}

void InspectorDatabaseAgent::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  visitor->Trace(resources_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
