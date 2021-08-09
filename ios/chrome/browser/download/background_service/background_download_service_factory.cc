// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/background_service/background_download_service_factory.h"

#include <utility>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "components/download/internal/background_service/client_set.h"
#include "components/download/internal/background_service/download_store.h"
#include "components/download/internal/background_service/file_monitor_impl.h"
#include "components/download/internal/background_service/ios/background_download_service_impl.h"
#include "components/download/internal/background_service/ios/background_download_task_helper.h"
#include "components/download/internal/background_service/logger_impl.h"
#include "components/download/internal/background_service/model_impl.h"
#include "components/download/internal/background_service/proto/entry.pb.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/clients.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

// The root directory for background download system, under browser state
// directory.
const base::FilePath::CharType kDownloadServiceStorageDir[] =
    FILE_PATH_LITERAL("Download Service");

// The directory for background download database.
const base::FilePath::CharType kEntryDBStorageDir[] =
    FILE_PATH_LITERAL("EntryDB");

// The directory for downloaded files.
const base::FilePath::CharType kFilesStorageDir[] = FILE_PATH_LITERAL("Files");

// static
download::BackgroundDownloadService*
BackgroundDownloadServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<download::BackgroundDownloadService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
BackgroundDownloadServiceFactory*
BackgroundDownloadServiceFactory::GetInstance() {
  static base::NoDestructor<BackgroundDownloadServiceFactory> instance;
  return instance.get();
}

BackgroundDownloadServiceFactory::BackgroundDownloadServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BackgroundDownloadService",
          BrowserStateDependencyManager::GetInstance()) {}

BackgroundDownloadServiceFactory::~BackgroundDownloadServiceFactory() = default;

std::unique_ptr<KeyedService>
BackgroundDownloadServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  auto clients = std::make_unique<download::DownloadClientMap>();
  auto client_set = std::make_unique<download::ClientSet>(std::move(clients));
  base::FilePath storage_dir =
      context->GetStatePath().Append(kDownloadServiceStorageDir);
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  auto entry_db = context->GetProtoDatabaseProvider()->GetDB<protodb::Entry>(
      leveldb_proto::ProtoDbType::DOWNLOAD_STORE,
      storage_dir.Append(kEntryDBStorageDir), background_task_runner);
  auto store = std::make_unique<download::DownloadStore>(std::move(entry_db));
  auto model = std::make_unique<download::ModelImpl>(std::move(store));
  base::FilePath files_storage_dir = storage_dir.Append(kFilesStorageDir);
  auto file_monitor = std::make_unique<download::FileMonitorImpl>(
      files_storage_dir, background_task_runner);
  auto logger = std::make_unique<download::LoggerImpl>();
  auto* log_sink = logger.get();
  return std::make_unique<download::BackgroundDownloadServiceImpl>(
      std::move(client_set), std::move(model),
      download::BackgroundDownloadTaskHelper::Create(), std::move(file_monitor),
      files_storage_dir, std::move(logger), log_sink,
      base::DefaultClock::GetInstance());
}
