// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/model/background_service/background_download_service_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "components/download/internal/background_service/client_set.h"
#include "components/download/internal/background_service/download_store.h"
#include "components/download/internal/background_service/file_monitor_impl.h"
#include "components/download/internal/background_service/init_aware_background_download_service.h"
#include "components/download/internal/background_service/ios/background_download_service_impl.h"
#include "components/download/internal/background_service/ios/background_download_task_helper.h"
#include "components/download/internal/background_service/logger_impl.h"
#include "components/download/internal/background_service/model_impl.h"
#include "components/download/internal/background_service/proto/entry.pb.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/clients.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "ios/chrome/browser/optimization_guide/model/prediction_model_download_client.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

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
BackgroundDownloadServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<download::BackgroundDownloadService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
  DCHECK(!context->IsOffTheRecord());
  auto clients = std::make_unique<download::DownloadClientMap>();
  // Clients should be registered here.
  if (optimization_guide::features::IsModelDownloadingEnabled()) {
    auto prediction_model_download_client =
        std::make_unique<optimization_guide::PredictionModelDownloadClient>(
            ProfileIOS::FromBrowserState(context));
    clients->insert(std::make_pair(
        download::DownloadClient::OPTIMIZATION_GUIDE_PREDICTION_MODELS,
        std::move(prediction_model_download_client)));
  }
  return BuildServiceWithClients(context, std::move(clients));
}

std::unique_ptr<KeyedService>
BackgroundDownloadServiceFactory::BuildServiceWithClients(
    web::BrowserState* context,
    std::unique_ptr<download::DownloadClientMap> clients) const {
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
  auto* logger_ptr = logger.get();
  auto service = std::make_unique<download::BackgroundDownloadServiceImpl>(
      std::move(client_set), std::move(model),
      download::BackgroundDownloadTaskHelper::Create(), std::move(file_monitor),
      files_storage_dir, std::move(logger), logger_ptr,
      base::DefaultClock::GetInstance());
  logger_ptr->SetLogSource(service.get());
  auto init_aware_service =
      std::make_unique<download::InitAwareBackgroundDownloadService>(
          std::move(service));
  return init_aware_service;
}
