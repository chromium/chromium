// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_internals_ui.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "content/grit/dev_ui_content_resources.h"
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
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/base/text/bytes_formatting.h"
#include "url/origin.h"

namespace content {

IndexedDBInternalsUI::IndexedDBInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<IndexedDBInternalsHandler>());
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIIndexedDBInternalsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");
  source->UseStringsJs();
  source->AddResourcePath("indexeddb_internals.js",
                          IDR_INDEXED_DB_INTERNALS_JS);
  source->AddResourcePath("indexeddb_internals.css",
                          IDR_INDEXED_DB_INTERNALS_CSS);
  source->SetDefaultResource(IDR_INDEXED_DB_INTERNALS_HTML);
}

IndexedDBInternalsUI::~IndexedDBInternalsUI() = default;

IndexedDBInternalsHandler::IndexedDBInternalsHandler() = default;

IndexedDBInternalsHandler::~IndexedDBInternalsHandler() = default;

void IndexedDBInternalsHandler::RegisterMessages() {
  // TODO(https://crbug.com/1199077): Fix this name as part of storage key
  // migration.
  web_ui()->RegisterMessageCallback(
      "getAllOrigins",
      base::BindRepeating(&IndexedDBInternalsHandler::GetAllStorageKeys,
                          base::Unretained(this)));
  // TODO(https://crbug.com/1199077): Fix this name as part of storage key
  // migration.
  web_ui()->RegisterMessageCallback(
      "downloadOriginData",
      base::BindRepeating(&IndexedDBInternalsHandler::DownloadStorageKeyData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "forceClose",
      base::BindRepeating(&IndexedDBInternalsHandler::ForceCloseStorageKey,
                          base::Unretained(this)));
}

void IndexedDBInternalsHandler::OnJavascriptDisallowed() {
  weak_factory_.InvalidateWeakPtrs();
}

void IndexedDBInternalsHandler::GetAllStorageKeys(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  AllowJavascript();
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  browser_context->ForEachStoragePartition(
      base::BindRepeating(
          [](base::WeakPtr<IndexedDBInternalsHandler> handler,
             StoragePartition* partition) {
            if (!handler)
              return;
            auto& control = partition->GetIndexedDBControl();
            control.GetAllStorageKeysDetails(base::BindOnce(
                [](base::WeakPtr<IndexedDBInternalsHandler> handler,
                   base::FilePath partition_path, bool incognito,
                   base::Value::List info_list) {
                  if (!handler)
                    return;

                  handler->OnStorageKeysReady(
                      base::Value(std::move(info_list)),
                      incognito ? base::FilePath() : partition_path);
                },
                handler, partition->GetPath()));
          },
          weak_factory_.GetWeakPtr()));
}

void IndexedDBInternalsHandler::OnStorageKeysReady(
    const base::Value& storage_keys,
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(https://crbug.com/1199077): Fix this name as part of storage key
  // migration.
  FireWebUIListener("origins-ready", storage_keys,
                    base::Value(path.AsUTF8Unsafe()));
}

static void FindControl(const base::FilePath& partition_path,
                        StoragePartition** result_partition,
                        storage::mojom::IndexedDBControl** result_control,
                        StoragePartition* storage_partition) {
  if (storage_partition->GetPath() == partition_path) {
    *result_partition = storage_partition;
    *result_control = &storage_partition->GetIndexedDBControl();
  }
}

bool IndexedDBInternalsHandler::GetStorageKeyData(
    const base::Value::List& args,
    std::string* callback_id,
    base::FilePath* partition_path,
    blink::StorageKey* storage_key,
    storage::mojom::IndexedDBControl** control) {
  if (args.size() < 3)
    return false;

  *callback_id = args[0].GetString();
  *partition_path = base::FilePath::FromUTF8Unsafe(args[1].GetString());
  *storage_key =
      blink::StorageKey(url::Origin::Create(GURL(args[2].GetString())));

  return GetStorageKeyControl(*partition_path, *storage_key, control);
}

bool IndexedDBInternalsHandler::GetStorageKeyControl(
    const base::FilePath& path,
    const blink::StorageKey& storage_key,
    storage::mojom::IndexedDBControl** control) {
  // search the storage keys to find the right context
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  StoragePartition* result_partition = nullptr;
  *control = nullptr;
  browser_context->ForEachStoragePartition(
      base::BindRepeating(&FindControl, path, &result_partition, control));

  if (!result_partition || !control)
    return false;

  return true;
}

void IndexedDBInternalsHandler::DownloadStorageKeyData(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string callback_id;
  base::FilePath partition_path;
  blink::StorageKey storage_key;
  storage::mojom::IndexedDBControl* control;
  if (!GetStorageKeyData(args, &callback_id, &partition_path, &storage_key,
                         &control))
    return;

  AllowJavascript();
  DCHECK(control);
  control->ForceClose(
      storage_key, storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
      base::BindOnce(
          [](base::WeakPtr<IndexedDBInternalsHandler> handler,
             blink::StorageKey storage_key,
             storage::mojom::IndexedDBControl* control,
             const std::string& callback_id) {
            // Is the connection count always zero after closing,
            // such that this can be simplified?
            control->GetConnectionCount(
                storage_key,
                base::BindOnce(
                    [](base::WeakPtr<IndexedDBInternalsHandler> handler,
                       blink::StorageKey storage_key,
                       storage::mojom::IndexedDBControl* control,
                       const std::string& callback_id,
                       uint64_t connection_count) {
                      if (!handler)
                        return;

                      control->DownloadStorageKeyData(
                          storage_key,
                          base::BindOnce(
                              &IndexedDBInternalsHandler::OnDownloadDataReady,
                              handler, callback_id, connection_count));
                    },
                    handler, storage_key, control, callback_id));
          },
          weak_factory_.GetWeakPtr(), storage_key, control, callback_id));
}

void IndexedDBInternalsHandler::ForceCloseStorageKey(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string callback_id;
  base::FilePath partition_path;
  blink::StorageKey storage_key;
  storage::mojom::IndexedDBControl* control;
  if (!GetStorageKeyData(args, &callback_id, &partition_path, &storage_key,
                         &control))
    return;

  AllowJavascript();
  control->ForceClose(
      storage_key, storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
      base::BindOnce(
          [](base::WeakPtr<IndexedDBInternalsHandler> handler,
             blink::StorageKey storage_key,
             storage::mojom::IndexedDBControl* control,
             const std::string& callback_id) {
            if (!handler)
              return;
            control->GetConnectionCount(
                storage_key,
                base::BindOnce(&IndexedDBInternalsHandler::OnForcedClose,
                               handler, callback_id));
          },
          weak_factory_.GetWeakPtr(), storage_key, control, callback_id));
}

void IndexedDBInternalsHandler::OnForcedClose(const std::string& callback_id,
                                              uint64_t connection_count) {
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(static_cast<double>(connection_count)));
}

void IndexedDBInternalsHandler::OnDownloadDataReady(
    const std::string& callback_id,
    uint64_t connection_count,
    bool success,
    const base::FilePath& temp_path,
    const base::FilePath& zip_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  const GURL url = GURL("file://" + zip_path.AsUTF8Unsafe());
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
      &IndexedDBInternalsHandler::OnDownloadStarted, base::Unretained(this),
      temp_path, callback_id, connection_count));

  BrowserContext* context = web_contents->GetBrowserContext();
  context->GetDownloadManager()->DownloadUrl(std::move(dl_params));
}

// The entire purpose of this class is to delete the temp file after
// the download is complete.
class FileDeleter : public download::DownloadItem::Observer {
 public:
  explicit FileDeleter(const base::FilePath& temp_dir) : temp_dir_(temp_dir) {}

  FileDeleter(const FileDeleter&) = delete;
  FileDeleter& operator=(const FileDeleter&) = delete;

  ~FileDeleter() override;

  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadOpened(download::DownloadItem* item) override {}
  void OnDownloadRemoved(download::DownloadItem* item) override {}
  void OnDownloadDestroyed(download::DownloadItem* item) override {}

 private:
  const base::FilePath temp_dir_;
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
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(base::GetDeletePathRecursivelyCallback(),
                     std::move(temp_dir_)));
}

void IndexedDBInternalsHandler::OnDownloadStarted(
    const base::FilePath& temp_path,
    const std::string& callback_id,
    size_t connection_count,
    download::DownloadItem* item,
    download::DownloadInterruptReason interrupt_reason) {
  if (interrupt_reason != download::DOWNLOAD_INTERRUPT_REASON_NONE) {
    LOG(ERROR) << "Error downloading database dump: "
               << DownloadInterruptReasonToString(interrupt_reason);
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  item->AddObserver(new FileDeleter(temp_path));
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(static_cast<double>(connection_count)));
}

}  // namespace content
