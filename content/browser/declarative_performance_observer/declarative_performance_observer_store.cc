// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/declarative_performance_observer/declarative_performance_observer_store.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kReportsTableName[] = "declarative_performance_observer_reports";
constexpr char kReportsIndexName[] = "idx_reports_origin";

constexpr char kHistogramPrefix[] = "Storage.DeclarativePerformanceObserver.";

constexpr base::FilePath::CharType kDatabaseFilename[] =
    FILE_PATH_LITERAL("declarative_performance_observer.db");

// Time-to-live (TTL) for early failure reports stored in the database.
constexpr base::TimeDelta kReportsTimeToLive = base::Days(7);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DeclarativePerformanceObserverStoreReportResult)
enum class StoreReportResult {
  kSuccess = 0,
  kFailedDbInit = 1,
  kFailedJsonWrite = 2,
  kFailedSqlRun = 3,
  kReportTooLarge = 4,
  kMaxValue = kReportTooLarge,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml)

void RecordStoreReportResult(StoreReportResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat({kHistogramPrefix, "StoreReportResult"}), result);
}

}  // namespace

class DeclarativePerformanceObserverStore::Backend
    : public base::RefCountedDeleteOnSequence<
          DeclarativePerformanceObserverStore::Backend> {
 public:
  Backend(scoped_refptr<base::SequencedTaskRunner> db_task_runner,
          const base::FilePath& db_path)
      : base::RefCountedDeleteOnSequence<
            DeclarativePerformanceObserverStore::Backend>(db_task_runner),
        db_path_(db_path) {
    DETACH_FROM_SEQUENCE(db_sequence_checker_);
  }

  void LoadPoliciesOnDbSequence(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::OnceCallback<void(std::vector<url::Origin>)> on_loaded_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (!InitOnDbSequence()) {
      ui_task_runner->PostTask(FROM_HERE,
                               base::BindOnce(std::move(on_loaded_callback),
                                              std::vector<url::Origin>()));
      return;
    }

    std::vector<url::Origin> loaded;
    sql::Statement statement(
        db_->GetUniqueStatement("SELECT origin, capture_early_failures FROM "
                                "declarative_performance_observer_policies"));
    while (statement.Step()) {
      if (statement.ColumnBool(1)) {
        loaded.emplace_back(
            url::Origin::Create(GURL(statement.ColumnString(0))));
      }
    }

    ui_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_loaded_callback), std::move(loaded)));
  }

  void SetEarlyFailurePolicyOnDbSequence(url::Origin origin, bool enabled) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (!InitOnDbSequence()) {
      return;
    }

    if (enabled) {
      sql::Statement statement(db_->GetCachedStatement(
          SQL_FROM_HERE,
          "INSERT OR REPLACE INTO declarative_performance_observer_policies "
          "(origin, capture_early_failures) VALUES (?, 1)"));
      statement.BindString(0, origin.Serialize());
      statement.Run();
    } else {
      sql::Statement statement(db_->GetCachedStatement(
          SQL_FROM_HERE,
          "DELETE FROM declarative_performance_observer_policies WHERE origin "
          "= ?"));
      statement.BindString(0, origin.Serialize());
      statement.Run();
    }
  }

  void StoreEarlyFailureReportOnDbSequence(url::Origin origin,
                                           base::DictValue report) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (!InitOnDbSequence()) {
      RecordStoreReportResult(StoreReportResult::kFailedDbInit);
      return;
    }

    std::string payload;
    bool write_success = base::JSONWriter::Write(report, &payload);
    if (!write_success) {
      RecordStoreReportResult(StoreReportResult::kFailedJsonWrite);
      return;
    }

    if (payload.size() > quota_limit_bytes_) {
      RecordStoreReportResult(StoreReportResult::kReportTooLarge);
      return;
    }

    EnforceDiskQuotaOnDbSequence(payload.size());

    sql::Statement statement(db_->GetUniqueStatement(
        "INSERT INTO declarative_performance_observer_reports "
        "(origin, payload, created_at) VALUES (?, ?, ?)"));
    statement.BindString(0, origin.Serialize());
    statement.BindString(1, payload);
    statement.BindInt64(
        2, base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

    if (!statement.Run()) {
      RecordStoreReportResult(StoreReportResult::kFailedSqlRun);
      return;
    }

    RecordStoreReportResult(StoreReportResult::kSuccess);
  }

  void TakeEarlyFailureReportsOnDbSequence(
      url::Origin origin,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::OnceCallback<void(base::ListValue)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    base::ListValue reports;
    if (!InitOnDbSequence()) {
      ui_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::move(reports)));
      return;
    }

    sql::Transaction transaction(db_.get());
    if (!transaction.Begin()) {
      ui_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::move(reports)));
      return;
    }

    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT payload FROM declarative_performance_observer_reports WHERE "
        "origin = ? ORDER BY id ASC"));
    statement.BindString(0, origin.Serialize());
    while (statement.Step()) {
      std::optional<base::Value> value = base::JSONReader::Read(
          statement.ColumnStringView(0), base::JSON_PARSE_RFC);
      if (value && value->is_dict()) {
        reports.Append(std::move(*value));
      }
    }

    sql::Statement delete_statement(db_->GetUniqueStatement(
        "DELETE FROM declarative_performance_observer_reports WHERE origin = "
        "?"));
    delete_statement.BindString(0, origin.Serialize());

    if (delete_statement.Run() && transaction.Commit()) {
      ui_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::move(reports)));
    } else {
      ui_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::ListValue()));
    }
  }

  void SetQuotaLimitForTestingOnDbSequence(  // IN-TEST
      size_t quota_limit_bytes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    quota_limit_bytes_ = quota_limit_bytes;
  }

  void ClearDataForOriginOnDbSequence(const url::Origin& origin) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (!InitOnDbSequence()) {
      return;
    }

    sql::Transaction transaction(db_.get());
    if (!transaction.Begin()) {
      return;
    }

    sql::Statement delete_policies(db_->GetUniqueStatement(
        "DELETE FROM declarative_performance_observer_policies WHERE origin "
        "= ?"));
    delete_policies.BindString(0, origin.Serialize());
    delete_policies.Run();

    sql::Statement delete_reports(db_->GetUniqueStatement(
        "DELETE FROM declarative_performance_observer_reports WHERE origin "
        "= ?"));
    delete_reports.BindString(0, origin.Serialize());
    delete_reports.Run();

    transaction.Commit();
  }

  void ClearDataWithFilterOnDbSequence(OriginMatcherFunction filter) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (!InitOnDbSequence()) {
      return;
    }

    std::vector<url::Origin> origins;

    // 1. Query all distinct origins from both tables:
    {
      sql::Statement statement(
          db_->GetUniqueStatement("SELECT DISTINCT origin FROM "
                                  "declarative_performance_observer_policies"));
      while (statement.Step()) {
        origins.push_back(url::Origin::Create(GURL(statement.ColumnString(0))));
      }
    }

    {
      sql::Statement statement(
          db_->GetUniqueStatement("SELECT DISTINCT origin FROM "
                                  "declarative_performance_observer_reports"));
      while (statement.Step()) {
        origins.push_back(url::Origin::Create(GURL(statement.ColumnString(0))));
      }
    }

    // Deduplicate origins:
    std::sort(origins.begin(), origins.end());
    origins.erase(std::unique(origins.begin(), origins.end()), origins.end());

    // 2. Perform transaction-backed filtered deletion:
    sql::Transaction transaction(db_.get());
    if (!transaction.Begin()) {
      return;
    }

    sql::Statement delete_policies(db_->GetUniqueStatement(
        "DELETE FROM declarative_performance_observer_policies WHERE origin "
        "= ?"));
    sql::Statement delete_reports(db_->GetUniqueStatement(
        "DELETE FROM declarative_performance_observer_reports WHERE origin "
        "= ?"));

    for (const auto& origin : origins) {
      if (filter.Run(origin)) {
        delete_policies.Reset(true);
        delete_policies.BindString(0, origin.Serialize());
        delete_policies.Run();

        delete_reports.Reset(true);
        delete_reports.BindString(0, origin.Serialize());
        delete_reports.Run();
      }
    }

    transaction.Commit();
  }

  void ClearAllDataOnDbSequence() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (db_ && db_->is_open()) {
      if (!db_path_.empty()) {
        std::ignore = db_->RazeAndPoison();
      }
      db_->Close();
    }
    db_.reset();
  }

  void CloseOnDbSequence() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (db_ && db_->is_open()) {
      db_->Close();
    }
    db_.reset();
  }

  void CheckSchemaOnDbSequence(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::OnceCallback<void(bool, bool)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (!InitOnDbSequence()) {
      ui_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), false, false));
      return;
    }
    bool table_ok = db_->DoesTableExist(kReportsTableName);
    bool index_ok = db_->DoesIndexExist(kReportsIndexName);
    ui_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), table_ok, index_ok));
  }

 private:
  friend class base::RefCountedDeleteOnSequence<
      DeclarativePerformanceObserverStore::Backend>;
  friend class base::DeleteHelper<DeclarativePerformanceObserverStore::Backend>;

  ~Backend() { DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_); }

  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (sql::IsErrorCatastrophic(extended_error)) {
      if (db_ && db_->is_open() && !db_path_.empty()) {
        std::ignore = db_->RazeAndPoison();
      }
    }
  }

  bool InitOnDbSequence() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (db_ && db_->is_open()) {
      return true;
    }

    db_ = std::make_unique<sql::Database>(
        sql::DatabaseOptions().set_page_size(4096).set_cache_size(128),
        sql::Database::Tag("DeclarativePerformanceObserver"));
    db_->set_error_callback(base::BindRepeating(&Backend::DatabaseErrorCallback,
                                                base::Unretained(this)));

    if (db_path_.empty()) {
      if (!db_->OpenInMemory()) {
        return false;
      }
    } else {
      if (!base::CreateDirectory(db_path_.DirName())) {
        return false;
      }
      if (!db_->Open(db_path_)) {
        return false;
      }
    }

    sql::MetaTable meta_table;
    static constexpr int kVersionNumber = 1;
    static constexpr int kCompatibleVersionNumber = 1;
    if (!meta_table.Init(db_.get(), kVersionNumber, kCompatibleVersionNumber)) {
      return false;
    }

    static constexpr char kCreatePoliciesTable[] =
        "CREATE TABLE IF NOT EXISTS declarative_performance_observer_policies ("
        "origin TEXT PRIMARY KEY NOT NULL, "
        "capture_early_failures BOOLEAN NOT NULL)";
    if (!db_->Execute(kCreatePoliciesTable)) {
      return false;
    }

    static constexpr char kCreateReportsTable[] =
        "CREATE TABLE IF NOT EXISTS declarative_performance_observer_reports ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "origin TEXT NOT NULL, "
        "payload TEXT NOT NULL, "
        "created_at INTEGER NOT NULL)";
    if (!db_->Execute(kCreateReportsTable)) {
      return false;
    }

    static constexpr char kCreateReportsIndex[] =
        "CREATE INDEX IF NOT EXISTS idx_reports_origin ON "
        "declarative_performance_observer_reports(origin)";
    if (!db_->Execute(kCreateReportsIndex)) {
      return false;
    }

    // Clean up expired reports (TTL = 7 days).
    static constexpr char kCleanExpiredReports[] =
        "DELETE FROM declarative_performance_observer_reports WHERE "
        "created_at < ?";
    sql::Statement clean_statement(
        db_->GetUniqueStatement(kCleanExpiredReports));
    int64_t threshold_us = (base::Time::Now() - kReportsTimeToLive)
                               .ToDeltaSinceWindowsEpoch()
                               .InMicroseconds();
    clean_statement.BindInt64(0, threshold_us);
    clean_statement.Run();

    // Log storage stats:
    if (!db_path_.empty()) {
      std::optional<int64_t> file_size = base::GetFileSize(db_path_);
      if (file_size.has_value()) {
        base::UmaHistogramMemoryKB(
            base::StrCat({kHistogramPrefix, "DatabaseSize"}),
            *file_size / 1024);
      }
    }

    static constexpr char kCountPoliciesSql[] =
        "SELECT COUNT(*) FROM declarative_performance_observer_policies";
    sql::Statement origin_count_stmt(
        db_->GetUniqueStatement(kCountPoliciesSql));
    if (origin_count_stmt.Step()) {
      base::UmaHistogramCounts1000(
          base::StrCat({kHistogramPrefix, "StoredOriginCount"}),
          origin_count_stmt.ColumnInt(0));
    }

    static constexpr char kCountReportsSql[] =
        "SELECT COUNT(*) FROM declarative_performance_observer_reports";
    sql::Statement report_count_stmt(db_->GetUniqueStatement(kCountReportsSql));
    if (report_count_stmt.Step()) {
      base::UmaHistogramCounts10000(
          base::StrCat({kHistogramPrefix, "StoredReportCount"}),
          report_count_stmt.ColumnInt(0));
    }

    return true;
  }

  // Enforces the storage quota limit. If the total size of stored reports plus
  // `new_entry_bytes` exceeds the quota, older reports are deleted (FIFO)
  // in a single batch until the size fits within the limit.
  void EnforceDiskQuotaOnDbSequence(size_t new_entry_bytes) {
    sql::Statement count(
        db_->GetUniqueStatement("SELECT COALESCE(SUM(length(payload)), 0) FROM "
                                "declarative_performance_observer_reports"));
    size_t estimated_bytes = 0;
    if (count.Step()) {
      estimated_bytes = static_cast<size_t>(count.ColumnInt64(0));
    }

    if (estimated_bytes + new_entry_bytes <= quota_limit_bytes_) {
      return;
    }

    size_t target_evict_bytes =
        (estimated_bytes + new_entry_bytes) - quota_limit_bytes_;

    // Since Chromium's SQLite omits window functions (SQLITE_OMIT_WINDOWFUNC),
    // we cannot use SUM(...) OVER (...). Instead, we perform a simple O(N) scan
    // of IDs and payload lengths, and accumulate the running total in C++
    // to find the eviction boundary.
    sql::Statement select_reports(db_->GetUniqueStatement(
        "SELECT id, length(payload) FROM "
        "declarative_performance_observer_reports ORDER BY id ASC"));

    int64_t max_evicted_id = -1;
    size_t running_total = 0;
    int evicted_count = 0;
    while (select_reports.Step()) {
      int64_t id = select_reports.ColumnInt64(0);
      size_t size = static_cast<size_t>(select_reports.ColumnInt(1));
      running_total += size;
      evicted_count++;
      if (running_total >= target_evict_bytes) {
        max_evicted_id = id;
        break;
      }
    }

    if (max_evicted_id >= 0) {
      sql::Statement delete_batch(db_->GetUniqueStatement(
          "DELETE FROM declarative_performance_observer_reports WHERE id <= "
          "?"));
      delete_batch.BindInt64(0, max_evicted_id);
      if (delete_batch.Run()) {
        base::UmaHistogramCounts100(
            base::StrCat({kHistogramPrefix, "EvictionCount"}), evicted_count);
      }
    }
  }

  base::FilePath db_path_;
  std::unique_ptr<sql::Database> db_;
  // The physical storage quota limit for this store (per storage partition).
  // Default is 640KB, aligning with the fetchLater per-document quota.
  size_t quota_limit_bytes_ = 640 * 1024;
  SEQUENCE_CHECKER(db_sequence_checker_);
};

DeclarativePerformanceObserverStore::DeclarativePerformanceObserverStore(
    bool is_in_memory,
    const base::FilePath& profile_path,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    base::OnceClosure on_loaded_callback)
    : db_task_runner_(
          db_task_runner
              ? db_task_runner
              : base::ThreadPool::CreateSequencedTaskRunner(
                    {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                     base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      backend_(base::MakeRefCounted<Backend>(
          db_task_runner_,
          is_in_memory ? base::FilePath()
                       : profile_path.Append(kDatabaseFilename))) {
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::LoadPoliciesOnDbSequence, backend_,
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     base::BindOnce(&DeclarativePerformanceObserverStore::
                                        OnPoliciesLoadedOnUISequence,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(on_loaded_callback))));
}

DeclarativePerformanceObserverStore::~DeclarativePerformanceObserverStore() =
    default;

void DeclarativePerformanceObserverStore::OnPoliciesLoadedOnUISequence(
    base::OnceClosure on_loaded_callback,
    std::vector<url::Origin> loaded) {
  if (!clear_all_pending_) {
    // 1. Filter out loaded origins using pending filters that ran during load:
    std::erase_if(loaded, [this](const url::Origin& origin) {
      for (const auto& filter : pending_filters_) {
        if (filter.Run(origin)) {
          return true;
        }
      }
      return false;
    });

    // 2. Discard loaded origins that were modified during load:
    std::erase_if(loaded, [this](const url::Origin& origin) {
      return modified_during_load_.contains(origin);
    });
    cached_policies_.insert(loaded.begin(), loaded.end());
  }
  loaded_ = true;
  modified_during_load_.clear();
  pending_filters_.clear();
  clear_all_pending_ = false;
  std::move(on_loaded_callback).Run();
}

void DeclarativePerformanceObserverStore::SetEarlyFailurePolicy(
    const url::Origin& origin,
    bool enabled,
    base::OnceClosure callback) {
  if (origin.opaque()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }
  if (!loaded_) {
    modified_during_load_.insert(origin);
  }
  if (enabled) {
    cached_policies_.insert(origin);
  } else {
    cached_policies_.erase(origin);
  }
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Backend::SetEarlyFailurePolicyOnDbSequence, backend_,
                     origin, enabled),
      std::move(callback));
}

bool DeclarativePerformanceObserverStore::HasEarlyFailurePolicy(
    const url::Origin& origin) {
  return cached_policies_.contains(origin);
}

void DeclarativePerformanceObserverStore::StoreEarlyFailureReport(
    const url::Origin& origin,
    base::DictValue report,
    base::OnceClosure callback) {
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Backend::StoreEarlyFailureReportOnDbSequence, backend_,
                     origin, std::move(report)),
      std::move(callback));
}

void DeclarativePerformanceObserverStore::TakeEarlyFailureReports(
    const url::Origin& origin,
    base::OnceCallback<void(base::ListValue)> callback) {
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::TakeEarlyFailureReportsOnDbSequence, backend_,
                     origin, base::SequencedTaskRunner::GetCurrentDefault(),
                     std::move(callback)));
}

void DeclarativePerformanceObserverStore::ClearDataForOrigin(
    const url::Origin& origin,
    base::OnceClosure callback) {
  if (!loaded_) {
    modified_during_load_.insert(origin);
  }
  cached_policies_.erase(origin);
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Backend::ClearDataForOriginOnDbSequence, backend_,
                     origin),
      std::move(callback));
}

void DeclarativePerformanceObserverStore::ClearDataWithFilter(
    OriginMatcherFunction filter,
    base::OnceClosure callback) {
  if (!loaded_) {
    pending_filters_.push_back(filter);
  }

  // 1. Filter and remove from in-memory policy cache immediately:
  base::EraseIf(cached_policies_, [&](const url::Origin& origin) {
    if (filter.Run(origin)) {
      if (!loaded_) {
        modified_during_load_.erase(origin);
      }
      return true;
    }
    return false;
  });

  // 2. Post to DB sequence to perform the actual database deletions:
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Backend::ClearDataWithFilterOnDbSequence, backend_,
                     std::move(filter)),
      std::move(callback));
}

void DeclarativePerformanceObserverStore::ClearAllData(
    base::OnceClosure callback) {
  if (!loaded_) {
    clear_all_pending_ = true;
  }
  cached_policies_.clear();
  db_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&Backend::ClearAllDataOnDbSequence, backend_),
      std::move(callback));
}

void DeclarativePerformanceObserverStore::SetQuotaLimitForTesting(  // IN-TEST
    size_t quota_limit_bytes,
    base::OnceClosure callback) {
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Backend::SetQuotaLimitForTestingOnDbSequence, backend_,
                     quota_limit_bytes),  // IN-TEST
      std::move(callback));
}

void DeclarativePerformanceObserverStore::Close(base::OnceClosure callback) {
  db_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&Backend::CloseOnDbSequence, backend_),
      std::move(callback));
}

void DeclarativePerformanceObserverStore::CheckSchemaForTesting(  // IN-TEST
    base::OnceCallback<void(bool, bool)> callback) {
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Backend::CheckSchemaOnDbSequence, backend_,
                                base::SequencedTaskRunner::GetCurrentDefault(),
                                std::move(callback)));
}

}  // namespace content
