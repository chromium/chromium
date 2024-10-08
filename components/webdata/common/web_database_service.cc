// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_database_service.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/webdata/common/web_data_request_manager.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database_backend.h"

namespace features {

// If enabled, then an Encryptor will be requested that is not always backwards
// compatible with OSCrypt Sync. On some platforms, this might mean a key is
// used that is stored more securely, such as using App-Bound encryption on
// Windows.
// If this feature is enabled, any data stored by `WebDatabaseService` is not
// guaranteed to be retrievable if OSCrypt Async is not used.
BASE_FEATURE(kUseNewEncryptionKeyForWebData,
             "UseNewEncryptionKeyForWebData",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

// Receives messages from the backend on the DB sequence, posts them to
// WebDatabaseService on the UI sequence.
class WebDatabaseService::BackendDelegate
    : public WebDatabaseBackend::Delegate {
 public:
  BackendDelegate(const base::WeakPtr<WebDatabaseService>& web_database_service)
      : web_database_service_(web_database_service),
        callback_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  void DBLoaded(sql::InitStatus status,
                const std::string& diagnostics) override {
    callback_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WebDatabaseService::OnDatabaseLoadDone,
                                  web_database_service_, status, diagnostics));
  }
 private:
  const base::WeakPtr<WebDatabaseService> web_database_service_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
};

WebDatabaseService::WebDatabaseService(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : base::RefCountedDeleteOnSequence<WebDatabaseService>(ui_task_runner),
      path_(path),
      db_task_runner_(std::move(db_task_runner)),
      pending_task_queue_(
          base::MakeRefCounted<base::DeferredSequencedTaskRunner>(
              db_task_runner_)) {
  DCHECK(ui_task_runner->RunsTasksInCurrentSequence());
  DCHECK(db_task_runner_);
}

WebDatabaseService::~WebDatabaseService() = default;

void WebDatabaseService::AddTable(std::unique_ptr<WebDatabaseTable> table) {
  CHECK(!pending_task_queue_->Started())
      << "Cannot call AddTable after LoadDatabase.";
  if (!web_db_backend_) {
    web_db_backend_ = base::MakeRefCounted<WebDatabaseBackend>(
        path_,
        std::make_unique<BackendDelegate>(weak_ptr_factory_.GetWeakPtr()),
        db_task_runner_);
  }
  web_db_backend_->AddTable(std::move(table));
}

void WebDatabaseService::CompleteLoadDatabase(
    os_crypt_async::Encryptor encryptor,
    bool success) {
  DCHECK(web_db_backend_);
  // All AddTable calls must have happened by the time LoadDatabase is called.
  web_db_backend_->MaybeInitEncryptorOnUiSequence(std::move(encryptor));
  // This ensures that the InitDatabase task gets executed on the DB task runner
  // before any database tasks.
  db_task_runner_->PostTask(
      FROM_HERE, BindOnce(&WebDatabaseBackend::InitDatabase, web_db_backend_));
  pending_task_queue_->Start();
}

void WebDatabaseService::LoadDatabase(os_crypt_async::OSCryptAsync* os_crypt) {
  const auto option =
      base::FeatureList::IsEnabled(features::kUseNewEncryptionKeyForWebData)
          ? os_crypt_async::Encryptor::Option::kNone
          : os_crypt_async::Encryptor::Option::kEncryptSyncCompat;
  // TODO(crbug.com/40267945): Place kEncryptSyncCompat behind base::Feature and
  // then remove it.
  subscription_ = os_crypt->GetInstance(
      base::BindOnce(&WebDatabaseService::CompleteLoadDatabase, this), option);
}

void WebDatabaseService::ShutdownDatabase() {
  error_callbacks_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!web_db_backend_)
    return;
  db_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(&WebDatabaseBackend::ShutdownDatabase, web_db_backend_));
}

WebDatabase* WebDatabaseService::GetDatabaseOnDB() const {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return web_db_backend_ ? web_db_backend_->database() : nullptr;
}

scoped_refptr<WebDatabaseBackend> WebDatabaseService::GetBackend() const {
  return web_db_backend_;
}

scoped_refptr<base::SequencedTaskRunner> WebDatabaseService::GetDbSequence() {
  return pending_task_queue_;
}

void WebDatabaseService::ScheduleDBTask(const base::Location& from_here,
                                        WriteTask task) {
  DCHECK(web_db_backend_);
  std::unique_ptr<WebDataRequest> request =
      web_db_backend_->request_manager()->NewRequest(nullptr);
  pending_task_queue_->PostTask(
      from_here,
      BindOnce(&WebDatabaseBackend::DBWriteTaskWrapper, web_db_backend_,
               std::move(task), std::move(request)));
}

WebDataServiceBase::Handle WebDatabaseService::ScheduleDBTaskWithResult(
    const base::Location& from_here,
    ReadTask task,
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  DCHECK(web_db_backend_);
  std::unique_ptr<WebDataRequest> request =
      web_db_backend_->request_manager()->NewRequest(consumer);
  WebDataServiceBase::Handle handle = request->GetHandle();
  pending_task_queue_->PostTask(
      from_here,
      BindOnce(&WebDatabaseBackend::DBReadTaskWrapper, web_db_backend_,
               std::move(task), std::move(request)));
  return handle;
}

void WebDatabaseService::CancelRequest(WebDataServiceBase::Handle h) {
  if (web_db_backend_)
    web_db_backend_->request_manager()->CancelRequest(h);
}

void WebDatabaseService::RegisterDBErrorCallback(DBLoadErrorCallback callback) {
  error_callbacks_.push_back(std::move(callback));
}

bool WebDatabaseService::UsesInMemoryDatabaseForTest() const {
  // This mimics what WebDatabase::Init() does internally, as it would require
  // significant test-only boilerplate to actually fetch the authoritative
  // boolean from the very underlying `sql::Database::in_memory_`.
  return path_.value() == WebDatabase::kInMemoryPath;
}

void WebDatabaseService::OnDatabaseLoadDone(sql::InitStatus status,
                                            const std::string& diagnostics) {
  // The INIT_OK_WITH_DATA_LOSS status is an initialization success but with
  // suspected data loss, so we also run the error callbacks.
  if (status == sql::INIT_OK)
    return;

  // Notify that the database load failed.
  while (!error_callbacks_.empty()) {
    // The profile error callback is a message box that runs in a nested run
    // loop. While it's being displayed, other OnDatabaseLoadDone() will run
    // (posted from WebDatabaseBackend::Delegate::DBLoaded()). We need to make
    // sure that after the callback running the message box returns, it checks
    // |error_callbacks_| before it accesses it.
    DBLoadErrorCallback error_callback = std::move(error_callbacks_.back());
    error_callbacks_.pop_back();
    if (!error_callback.is_null())
      std::move(error_callback).Run(status, diagnostics);
  }
}
