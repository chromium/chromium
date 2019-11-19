// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_internals_ui.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/text/bytes_formatting.h"
#include "url/origin.h"

using url::Origin;

namespace content {

namespace {

bool AllowWhitelistedPaths(const std::vector<base::FilePath>& allowed_paths,
                           const base::FilePath& candidate_path) {
  for (const base::FilePath& allowed_path : allowed_paths) {
    if (candidate_path == allowed_path || allowed_path.IsParent(candidate_path))
      return true;
  }
  return false;
}

}  // namespace

IndexedDBInternalsUI::IndexedDBInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->RegisterMessageCallback(
      "getAllOrigins", base::BindRepeating(&IndexedDBInternalsUI::GetAllOrigins,
                                           base::Unretained(this)));

  web_ui->RegisterMessageCallback(
      "downloadOriginData",
      base::BindRepeating(&IndexedDBInternalsUI::DownloadOriginData,
                          base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "forceClose", base::BindRepeating(&IndexedDBInternalsUI::ForceCloseOrigin,
                                        base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "forceSchemaDowngrade",
      base::BindRepeating(&IndexedDBInternalsUI::ForceSchemaDowngradeOrigin,
                          base::Unretained(this)));

  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIIndexedDBInternalsHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->UseStringsJs();
  source->AddResourcePath("indexeddb_internals.js",
                          IDR_INDEXED_DB_INTERNALS_JS);
  source->AddResourcePath("indexeddb_internals.css",
                          IDR_INDEXED_DB_INTERNALS_CSS);
  source->SetDefaultResource(IDR_INDEXED_DB_INTERNALS_HTML);

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, source);
}

IndexedDBInternalsUI::~IndexedDBInternalsUI() {}

void IndexedDBInternalsUI::AddContextFromStoragePartition(
    StoragePartition* partition) {
  scoped_refptr<IndexedDBContext> context = partition->GetIndexedDBContext();
  context->TaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IndexedDBInternalsUI::GetAllOriginsOnIndexedDBThread,
                     base::Unretained(this), context, partition->GetPath()));
}

void IndexedDBInternalsUI::GetAllOrigins(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  BrowserContext::StoragePartitionCallback cb =
      base::BindRepeating(&IndexedDBInternalsUI::AddContextFromStoragePartition,
                          base::Unretained(this));
  BrowserContext::ForEachStoragePartition(browser_context, std::move(cb));
}

void IndexedDBInternalsUI::GetAllOriginsOnIndexedDBThread(
    scoped_refptr<IndexedDBContext> context,
    const base::FilePath& context_path) {
  DCHECK(context->TaskRunner()->RunsTasksInCurrentSequence());

  IndexedDBContextImpl* context_impl =
      static_cast<IndexedDBContextImpl*>(context.get());

  std::unique_ptr<base::ListValue> info_list(
      context_impl->GetAllOriginsDetails());
  bool is_incognito = context_impl->is_incognito();

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&IndexedDBInternalsUI::OnOriginsReady,
                     base::Unretained(this), std::move(info_list),
                     is_incognito ? base::FilePath() : context_path));
}

void IndexedDBInternalsUI::OnOriginsReady(
    std::unique_ptr<base::ListValue> origins,
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_ui()->CallJavascriptFunctionUnsafe("indexeddb.onOriginsReady", *origins,
                                         base::Value(path.value()));
}

static void FindContext(const base::FilePath& partition_path,
                        StoragePartition** result_partition,
                        scoped_refptr<IndexedDBContextImpl>* result_context,
                        StoragePartition* storage_partition) {
  if (storage_partition->GetPath() == partition_path) {
    *result_partition = storage_partition;
    *result_context = static_cast<IndexedDBContextImpl*>(
        storage_partition->GetIndexedDBContext());
  }
}

bool IndexedDBInternalsUI::GetOriginData(
    const base::ListValue* args,
    base::FilePath* partition_path,
    Origin* origin,
    scoped_refptr<IndexedDBContextImpl>* context) {
  base::FilePath::StringType path_string;
  if (!args->GetString(0, &path_string))
    return false;
  *partition_path = base::FilePath(path_string);

  std::string url_string;
  if (!args->GetString(1, &url_string))
    return false;

  *origin = Origin::Create(GURL(url_string));

  return GetOriginContext(*partition_path, *origin, context);
}

bool IndexedDBInternalsUI::GetOriginContext(
    const base::FilePath& path,
    const Origin& origin,
    scoped_refptr<IndexedDBContextImpl>* context) {
  // search the origins to find the right context
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  StoragePartition* result_partition;
  BrowserContext::StoragePartitionCallback cb =
      base::BindRepeating(&FindContext, path, &result_partition, context);
  BrowserContext::ForEachStoragePartition(browser_context, std::move(cb));

  if (!result_partition || !(context->get()))
    return false;

  return true;
}

void IndexedDBInternalsUI::DownloadOriginData(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::FilePath partition_path;
  Origin origin;
  scoped_refptr<IndexedDBContextImpl> context;
  if (!GetOriginData(args, &partition_path, &origin, &context))
    return;

  DCHECK(context.get());
  context->TaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IndexedDBInternalsUI::DownloadOriginDataOnIndexedDBThread,
                     base::Unretained(this), partition_path, context, origin));
}

void IndexedDBInternalsUI::ForceCloseOrigin(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::FilePath partition_path;
  Origin origin;
  scoped_refptr<IndexedDBContextImpl> context;
  if (!GetOriginData(args, &partition_path, &origin, &context))
    return;

  context->TaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IndexedDBInternalsUI::ForceCloseOriginOnIndexedDBThread,
                     base::Unretained(this), partition_path, context, origin));
}

void IndexedDBInternalsUI::ForceSchemaDowngradeOrigin(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::FilePath partition_path;
  Origin origin;
  scoped_refptr<IndexedDBContextImpl> context;
  if (!GetOriginData(args, &partition_path, &origin, &context))
    return;

  context->TaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IndexedDBInternalsUI::ForceSchemaDowngradeOriginOnIndexedDBThread,
          base::Unretained(this), partition_path, context, origin));
}

void IndexedDBInternalsUI::DownloadOriginDataOnIndexedDBThread(
    const base::FilePath& partition_path,
    const scoped_refptr<IndexedDBContextImpl> context,
    const Origin& origin) {
  DCHECK(context->TaskRunner()->RunsTasksInCurrentSequence());
  // This runs on the IndexedDB task runner to prevent script from reopening
  // the origin while we are zipping.

  // Make sure the database hasn't been deleted since the page was loaded.
  if (!context->HasOrigin(origin))
    return;

  context->ForceClose(origin, IndexedDBContextImpl::FORCE_CLOSE_INTERNALS_PAGE);
  size_t connection_count = context->GetConnectionCount(origin);

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return;

  // This will get cleaned up after the download has completed.
  base::FilePath temp_path = temp_dir.Take();

  std::string origin_id = storage::GetIdentifierFromOrigin(origin);
  base::FilePath zip_path =
      temp_path.AppendASCII(origin_id).AddExtension(FILE_PATH_LITERAL("zip"));

  std::vector<base::FilePath> paths = context->GetStoragePaths(origin);
  zip::ZipWithFilterCallback(context->data_path(), zip_path,
                             base::BindRepeating(AllowWhitelistedPaths, paths));

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&IndexedDBInternalsUI::OnDownloadDataReady,
                                base::Unretained(this), partition_path, origin,
                                temp_path, zip_path, connection_count));
}

void IndexedDBInternalsUI::ForceCloseOriginOnIndexedDBThread(
    const base::FilePath& partition_path,
    const scoped_refptr<IndexedDBContextImpl> context,
    const Origin& origin) {
  DCHECK(context->TaskRunner()->RunsTasksInCurrentSequence());

  // Make sure the database hasn't been deleted since the page was loaded.
  if (!context->HasOrigin(origin))
    return;

  context->ForceClose(origin, IndexedDBContextImpl::FORCE_CLOSE_INTERNALS_PAGE);
  size_t connection_count = context->GetConnectionCount(origin);

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&IndexedDBInternalsUI::OnForcedSchemaDowngrade,
                                base::Unretained(this), partition_path, origin,
                                connection_count));
}

void IndexedDBInternalsUI::ForceSchemaDowngradeOriginOnIndexedDBThread(
    const base::FilePath& partition_path,
    const scoped_refptr<IndexedDBContextImpl> context,
    const Origin& origin) {
  DCHECK(context->TaskRunner()->RunsTasksInCurrentSequence());

  // Make sure the database hasn't been deleted since the page was loaded.
  if (!context->HasOrigin(origin))
    return;

  context->ForceSchemaDowngrade(origin);
  context->ForceClose(
      origin, IndexedDBContextImpl::FORCE_SCHEMA_DOWNGRADE_INTERNALS_PAGE);
  size_t connection_count = context->GetConnectionCount(origin);

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&IndexedDBInternalsUI::OnForcedSchemaDowngrade,
                                base::Unretained(this), partition_path, origin,
                                connection_count));
}

void IndexedDBInternalsUI::OnForcedClose(const base::FilePath& partition_path,
                                         const Origin& origin,
                                         size_t connection_count) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "indexeddb.onForcedClose", base::Value(partition_path.value()),
      base::Value(origin.Serialize()),
      base::Value(static_cast<double>(connection_count)));
}

void IndexedDBInternalsUI::OnForcedSchemaDowngrade(
    const base::FilePath& partition_path,
    const Origin& origin,
    size_t connection_count) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "indexeddb.onForcedSchemaDowngrade", base::Value(partition_path.value()),
      base::Value(origin.Serialize()),
      base::Value(static_cast<double>(connection_count)));
}

void IndexedDBInternalsUI::OnDownloadDataReady(
    const base::FilePath& partition_path,
    const Origin& origin,
    const base::FilePath temp_path,
    const base::FilePath zip_path,
    size_t connection_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const GURL url = GURL(FILE_PATH_LITERAL("file://") + zip_path.value());
  WebContents* web_contents = web_ui()->GetWebContents();
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("indexed_db_internals_handler", R"(
        semantics {
          sender: "Indexed DB Internals"
          description:
            "This is an internal Chrome webpage that displays debug "
            "information about IndexedDB usage and data, used by developers."
          trigger: "When a user navigates to chrome://indexeddb-internals/."
          data: "None."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings, but it's only "
            "triggered by navigating to the specified URL."
          policy_exception_justification:
            "Not implemented. Indexed DB is Chrome's internal local data "
            "storage."
        })");
  std::unique_ptr<download::DownloadUrlParameters> dl_params(
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents, url, traffic_annotation));
  content::Referrer referrer = content::Referrer::SanitizeForRequest(
      url, content::Referrer(web_contents->GetLastCommittedURL(),
                             network::mojom::ReferrerPolicy::kDefault));
  dl_params->set_referrer(referrer.url);
  dl_params->set_referrer_policy(
      Referrer::ReferrerPolicyForUrlRequest(referrer.policy));

  // This is how to watch for the download to finish: first wait for it
  // to start, then attach a download::DownloadItem::Observer to observe the
  // state change to the finished state.
  dl_params->set_callback(base::BindOnce(
      &IndexedDBInternalsUI::OnDownloadStarted, base::Unretained(this),
      partition_path, origin, temp_path, connection_count));

  BrowserContext* context = web_contents->GetBrowserContext();
  BrowserContext::GetDownloadManager(context)->DownloadUrl(
      std::move(dl_params));
}

// The entire purpose of this class is to delete the temp file after
// the download is complete.
class FileDeleter : public download::DownloadItem::Observer {
 public:
  explicit FileDeleter(const base::FilePath& temp_dir) : temp_dir_(temp_dir) {}
  ~FileDeleter() override;

  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadOpened(download::DownloadItem* item) override {}
  void OnDownloadRemoved(download::DownloadItem* item) override {}
  void OnDownloadDestroyed(download::DownloadItem* item) override {}

 private:
  const base::FilePath temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(FileDeleter);
};

void FileDeleter::OnDownloadUpdated(download::DownloadItem* item) {
  switch (item->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      break;
    case download::DownloadItem::COMPLETE:
    case download::DownloadItem::CANCELLED:
    case download::DownloadItem::INTERRUPTED: {
      item->RemoveObserver(this);
      delete this;
      break;
    }
    default:
      NOTREACHED();
  }
}

FileDeleter::~FileDeleter() {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                     std::move(temp_dir_), true));
}

void IndexedDBInternalsUI::OnDownloadStarted(
    const base::FilePath& partition_path,
    const Origin& origin,
    const base::FilePath& temp_path,
    size_t connection_count,
    download::DownloadItem* item,
    download::DownloadInterruptReason interrupt_reason) {
  if (interrupt_reason != download::DOWNLOAD_INTERRUPT_REASON_NONE) {
    LOG(ERROR) << "Error downloading database dump: "
               << DownloadInterruptReasonToString(interrupt_reason);
    return;
  }

  item->AddObserver(new FileDeleter(temp_path));
  web_ui()->CallJavascriptFunctionUnsafe(
      "indexeddb.onOriginDownloadReady", base::Value(partition_path.value()),
      base::Value(origin.Serialize()),
      base::Value(static_cast<double>(connection_count)));
}

}  // namespace content
