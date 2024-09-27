// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/dom_distiller/model/dom_distiller_service_factory.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/ios/distiller_page_factory_ios.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/thread/web_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
// A simple wrapper for DomDistillerService to expose it as a
// KeyedService.
class DomDistillerKeyedService : public KeyedService,
                                 public dom_distiller::DomDistillerService {
 public:
  DomDistillerKeyedService(
      std::unique_ptr<dom_distiller::DistillerFactory> distiller_factory,
      std::unique_ptr<dom_distiller::DistillerPageFactory>
          distiller_page_factory,
      std::unique_ptr<dom_distiller::DistilledPagePrefs> distilled_page_prefs,
      std::unique_ptr<dom_distiller::DistillerUIHandle> distiller_ui_handle)
      : DomDistillerService(std::move(distiller_factory),
                            std::move(distiller_page_factory),
                            std::move(distilled_page_prefs),
                            std::move(distiller_ui_handle)) {}

  DomDistillerKeyedService(const DomDistillerKeyedService&) = delete;
  DomDistillerKeyedService& operator=(const DomDistillerKeyedService&) = delete;

  ~DomDistillerKeyedService() override {}
};
}  // namespace

namespace dom_distiller {

// static
DomDistillerServiceFactory* DomDistillerServiceFactory::GetInstance() {
  static base::NoDestructor<DomDistillerServiceFactory> instance;
  return instance.get();
}

// static
DomDistillerService* DomDistillerServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<DomDistillerKeyedService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

DomDistillerServiceFactory::DomDistillerServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "DomDistillerService",
          BrowserStateDependencyManager::GetInstance()) {
}

DomDistillerServiceFactory::~DomDistillerServiceFactory() {}

std::unique_ptr<KeyedService>
DomDistillerServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  std::unique_ptr<DistillerPageFactory> distiller_page_factory =
      std::make_unique<DistillerPageFactoryIOS>(context);

  std::unique_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory =
      std::make_unique<DistillerURLFetcherFactory>(
          context->GetSharedURLLoaderFactory());

  dom_distiller::proto::DomDistillerOptions options;
  std::unique_ptr<DistillerFactory> distiller_factory =
      std::make_unique<DistillerFactoryImpl>(
          std::move(distiller_url_fetcher_factory), options);
  std::unique_ptr<DistilledPagePrefs> distilled_page_prefs =
      std::make_unique<DistilledPagePrefs>(
          ProfileIOS::FromBrowserState(context)->GetPrefs());

  return std::make_unique<DomDistillerKeyedService>(
      std::move(distiller_factory), std::move(distiller_page_factory),
      std::move(distilled_page_prefs),
      /* distiller_ui_handle */ nullptr);
}

web::BrowserState* DomDistillerServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // Makes normal profile and off-the-record profile use same service instance.
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace dom_distiller
