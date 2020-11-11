// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_internals_ui.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/task/post_task.h"
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
#include "ui/base/text/bytes_formatting.h"
#include "url/origin.h"

using url::Origin;

namespace content {

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
  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIIndexedDBInternalsHost);
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

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, source);
}

IndexedDBInternalsUI::~IndexedDBInternalsUI() {}

void IndexedDBInternalsUI::GetAllOrigins(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  BrowserContext::ForEachStoragePartition(
      browser_context,
      base::BindRepeating(
          [](base::WeakPtr<IndexedDBInternalsUI> ui,
             StoragePartition* partition) {
            if (!ui)
              return;
            auto& control = partition->GetIndexedDBControl();
            control.GetAllOriginsDetails(base::BindOnce(
                [](base::WeakPtr<IndexedDBInternalsUI> ui,
                   base::FilePath partition_path, bool incognito,
                   base::Value info_list) {
                  if (!ui)
                    return;

                  ui->OnOriginsReady(
                      info_list, incognito ? base::FilePath() : partition_path);
                },
                ui, partition->GetPath()));
          },
          weak_factory_.GetWeakPtr()));
}

void IndexedDBInternalsUI::OnOriginsReady(const base::Value& origins,
                                          const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_ui()->CallJavascriptFunctionUnsafe("indexeddb.onOriginsReady", origins,
                                         base::Value(path.value()));
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

bool IndexedDBInternalsUI::GetOriginData(
    const base::ListValue* args,
    base::FilePath* partition_path,
    Origin* origin,
    storage::mojom::IndexedDBControl** control) {
  base::FilePath::StringType path_string;
  if (!args->GetString(0, &path_string))
    return false;
  *partition_path = base::FilePath(path_string);

  std::string url_string;
  if (!args->GetString(1, &url_string))
    return false;

  *origin = Origin::Create(GURL(url_string));

  return GetOriginControl(*partition_path, *origin, control);
}

bool IndexedDBInternalsUI::GetOriginControl(
    const base::FilePath& path,
    const Origin& origin,
    storage::mojom::IndexedDBControl** control) {
  // search the origins to find the right context
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  StoragePartition* result_partition = nullptr;
  *control = nullptr;
  BrowserContext::ForEachStoragePartition(
      browser_context,
      base::BindRepeating(&FindControl, path, &result_partition, control));

  if (!result_partition || !control)
    return false;

  return true;
}

void IndexedDBInternalsUI::DownloadOriginData(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::FilePath partition_path;
  Origin origin;
  storage::mojom::IndexedDBControl* control;
  if (!GetOriginData(args, &partition_path, &origin, &control))
    return;

  DCHECK(control);
  control->ForceClose(
      origin, storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
      base::BindOnce(
          [](base::WeakPtr<IndexedDBInternalsUI> ui, Origin origin,
             base::FilePath partition_path,
             storage::mojom::IndexedDBControl* control) {
            // Is the connection count always zero after closing,
            // such that this can be simplified?
            control->GetConnectionCount(
                origin,
                base::BindOnce(
                    [](base::WeakPtr<IndexedDBInternalsUI> ui, Origin origin,
                       base::FilePath partition_path,
                       storage::mojom::IndexedDBControl* control,
                       uint64_t connection_count) {
                      if (!ui)
                        return;

                      control->DownloadOriginData(
                          origin,
                          base::BindOnce(
                              &IndexedDBInternalsUI::OnDownloadDataReady, ui,
                              partition_path, origin, connection_count));
                    },
                    ui, origin, partition_path, control));
          },
          weak_factory_.GetWeakPtr(), origin, partition_path, control));
}

void IndexedDBInternalsUI::ForceCloseOrigin(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::FilePath partition_path;
  Origin origin;
  storage::mojom::IndexedDBControl* control;
  if (!GetOriginData(args, &partition_path, &origin, &control))
    return;

  control->ForceClose(
      origin, storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
      base::BindOnce(
          [](base::WeakPtr<IndexedDBInternalsUI> ui,
             base::FilePath partition_path, Origin origin,
             storage::mojom::IndexedDBControl* control) {
            if (!ui)
              return;
            control->GetConnectionCount(
                origin, base::BindOnce(&IndexedDBInternalsUI::OnForcedClose, ui,
                                       partition_path, origin));
          },
          weak_factory_.GetWeakPtr(), partition_path, origin, control));
}

void IndexedDBInternalsUI::OnForcedClose(const base::FilePath& partition_path,
                                         const Origin& origin,
                                         uint64_t connection_count) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "indexeddb.onForcedClose", base::Value(partition_path.value()),
      base::Value(origin.Serialize()),
      base::Value(static_cast<double>(connection_count)));
}

void IndexedDBInternalsUI::OnDownloadDataReady(
    const base::FilePath& partition_path,
    const Origin& origin,
    uint64_t connection_count,
    bool success,
    const base::FilePath& temp_path,
    const base::FilePath& zip_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success)
    return;

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
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(base::GetDeletePathRecursivelyCallback(),
                     std::move(temp_dir_)));
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
