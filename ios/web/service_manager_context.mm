// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/service_manager_context.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "ios/web/grit/ios_web_resources.h"
#include "ios/web/public/service_manager_connection.h"
#include "ios/web/public/service_names.mojom.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/service_manager_connection_impl.h"
#include "services/catalog/manifest_provider.h"
#include "services/catalog/public/cpp/manifest_parsing_util.h"
#include "services/catalog/public/mojom/constants.mojom.h"
#include "services/service_manager/connect_params.h"
#include "services/service_manager/embedder/manifest_utils.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/service_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

struct ManifestInfo {
  const char* name;
  int resource_id;
};

// A ManifestProvider which resolves application names to builtin manifest
// resources for the catalog service to consume.
class BuiltinManifestProvider : public catalog::ManifestProvider {
 public:
  BuiltinManifestProvider() {}
  ~BuiltinManifestProvider() override {}

  void AddServiceManifest(base::StringPiece name, int resource_id) {
    std::string contents =
        GetWebClient()
            ->GetDataResource(resource_id, ui::ScaleFactor::SCALE_FACTOR_NONE)
            .as_string();
    DCHECK(!contents.empty());

    std::unique_ptr<base::Value> manifest_value =
        base::JSONReader::Read(contents);
    DCHECK(manifest_value);

    std::unique_ptr<base::Value> overlay_value =
        GetWebClient()->GetServiceManifestOverlay(name);

    service_manager::MergeManifestWithOverlay(manifest_value.get(),
                                              overlay_value.get());
    auto insertion_result = manifests_.insert(
        std::make_pair(name.as_string(), std::move(manifest_value)));
    DCHECK(insertion_result.second) << "Duplicate manifest entry: " << name;
  }

 private:
  // catalog::ManifestProvider:
  std::unique_ptr<base::Value> GetManifest(const std::string& name) override {
    auto it = manifests_.find(name);
    return it != manifests_.end() ? it->second->CreateDeepCopy() : nullptr;
  }

  std::map<std::string, std::unique_ptr<base::Value>> manifests_;

  DISALLOW_COPY_AND_ASSIGN(BuiltinManifestProvider);
};

}  // namespace

// State which lives on the IO thread and drives the ServiceManager.
class ServiceManagerContext::InProcessServiceManagerContext
    : public base::RefCountedThreadSafe<InProcessServiceManagerContext> {
 public:
  InProcessServiceManagerContext() {}

  void Start(
      service_manager::mojom::ServicePtrInfo packaged_services_service_info,
      std::unique_ptr<BuiltinManifestProvider> manifest_provider) {
    base::PostTaskWithTraits(
        FROM_HERE, {WebThread::IO},
        base::BindOnce(&InProcessServiceManagerContext::StartOnIOThread, this,
                       base::Passed(&manifest_provider),
                       base::Passed(&packaged_services_service_info)));
  }

  void ShutDown() {
    base::PostTaskWithTraits(
        FROM_HERE, {WebThread::IO},
        base::BindOnce(&InProcessServiceManagerContext::ShutDownOnIOThread,
                       this));
  }

 private:
  friend class base::RefCountedThreadSafe<InProcessServiceManagerContext>;

  ~InProcessServiceManagerContext() {}

  // Creates the ServiceManager and registers the packaged services service
  // with it, connecting the other end of the packaged services serviceto
  // |packaged_services_service_info|.
  void StartOnIOThread(
      std::unique_ptr<BuiltinManifestProvider> manifest_provider,
      service_manager::mojom::ServicePtrInfo packaged_services_service_info) {
    manifest_provider_ = std::move(manifest_provider);
    service_manager_ = std::make_unique<service_manager::ServiceManager>(
        nullptr, nullptr, manifest_provider_.get());

    service_manager::mojom::ServicePtr packaged_services_service;
    packaged_services_service.Bind(std::move(packaged_services_service_info));
    service_manager_->RegisterService(
        service_manager::Identity(mojom::kPackagedServicesServiceName,
                                  service_manager::mojom::kRootUserID),
        std::move(packaged_services_service), nullptr);
  }

  void ShutDownOnIOThread() {
    service_manager_.reset();
    manifest_provider_.reset();
  }

  std::unique_ptr<BuiltinManifestProvider> manifest_provider_;
  std::unique_ptr<service_manager::ServiceManager> service_manager_;

  DISALLOW_COPY_AND_ASSIGN(InProcessServiceManagerContext);
};

ServiceManagerContext::ServiceManagerContext() {
  service_manager::mojom::ServiceRequest packaged_services_request;
  std::unique_ptr<BuiltinManifestProvider> manifest_provider =
      std::make_unique<BuiltinManifestProvider>();

  const std::array<ManifestInfo, 3> manifests = {{
      {mojom::kBrowserServiceName, IDR_MOJO_WEB_BROWSER_MANIFEST},
      {mojom::kPackagedServicesServiceName,
       IDR_MOJO_WEB_PACKAGED_SERVICES_MANIFEST},
      {catalog::mojom::kServiceName, IDR_MOJO_CATALOG_MANIFEST},
  }};
  for (const ManifestInfo& manifest : manifests) {
    manifest_provider->AddServiceManifest(manifest.name, manifest.resource_id);
  }
  in_process_context_ = base::MakeRefCounted<InProcessServiceManagerContext>();

  service_manager::mojom::ServicePtr packaged_services_service;
  packaged_services_request = mojo::MakeRequest(&packaged_services_service);
  in_process_context_->Start(packaged_services_service.PassInterface(),
                             std::move(manifest_provider));

  packaged_services_connection_ = ServiceManagerConnection::Create(
      std::move(packaged_services_request),
      base::CreateSingleThreadTaskRunnerWithTraits({WebThread::IO}));

  service_manager::mojom::ServicePtr root_browser_service;
  ServiceManagerConnection::Set(ServiceManagerConnection::Create(
      mojo::MakeRequest(&root_browser_service),
      base::CreateSingleThreadTaskRunnerWithTraits({WebThread::IO})));
  auto* browser_connection = ServiceManagerConnection::Get();

  service_manager::mojom::PIDReceiverPtr pid_receiver;
  packaged_services_connection_->GetConnector()->StartService(
      service_manager::Identity(mojom::kBrowserServiceName,
                                service_manager::mojom::kRootUserID),
      std::move(root_browser_service), mojo::MakeRequest(&pid_receiver));
  pid_receiver->SetPID(base::GetCurrentProcId());

  // Embed any services from //ios/web here.

  // Embed services from the client of //ios/web.
  WebClient::StaticServiceMap services;
  GetWebClient()->RegisterServices(&services);
  for (const auto& entry : services) {
    packaged_services_connection_->AddEmbeddedService(entry.first,
                                                      entry.second);
  }

  packaged_services_connection_->Start();

  browser_connection->Start();
}

ServiceManagerContext::~ServiceManagerContext() {
  // NOTE: The in-process ServiceManager MUST be destroyed before the browser
  // process-wide ServiceManagerConnection. Otherwise it's possible for the
  // ServiceManager to receive connection requests for service:ios_web_browser
  // which it may attempt to service by launching a new instance of the browser.
  if (in_process_context_)
    in_process_context_->ShutDown();
  if (ServiceManagerConnection::Get())
    ServiceManagerConnection::Destroy();
}

}  // namespace web
