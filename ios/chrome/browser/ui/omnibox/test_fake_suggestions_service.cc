// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/test_fake_suggestions_service.h"

#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/ui/omnibox/fake_suggestions_database.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"

TestFakeSuggestionsService::TestFakeSuggestionsService() = default;

TestFakeSuggestionsService::~TestFakeSuggestionsService() = default;

// static
TestFakeSuggestionsService* TestFakeSuggestionsService::GetInstance() {
  static base::NoDestructor<TestFakeSuggestionsService> instance;
  return instance.get();
}

void TestFakeSuggestionsService::SetUp(
    RemoteSuggestionsService* remote_suggestions_service,
    TemplateURLService* template_url_service,
    const base::FilePath& file_path) {
  remote_suggestions_service_observation_.Observe(remote_suggestions_service);

  test_url_loader_factory_ = std::make_unique<network::TestURLLoaderFactory>();
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          test_url_loader_factory_.get());
  remote_suggestions_service->set_url_loader_factory_for_testing(
      shared_url_loader_factory);

  fake_suggestions_database_ =
      std::make_unique<FakeSuggestionsDatabase>(template_url_service);
  fake_suggestions_database_->LoadSuggestionsFromFile(file_path);
}

void TestFakeSuggestionsService::TearDown(
    RemoteSuggestionsService* remote_suggestions_service,
    network::mojom::URLLoaderFactory* url_loader_factory) {
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          url_loader_factory);

  remote_suggestions_service->set_url_loader_factory_for_testing(
      shared_url_loader_factory);

  remote_suggestions_service_observation_.Reset();
  test_url_loader_factory_.reset();
  fake_suggestions_database_.reset();
}

void TestFakeSuggestionsService::OnSuggestRequestCreated(
    const base::UnguessableToken& request_id,
    const network::ResourceRequest* request) {
  DCHECK(fake_suggestions_database_);
  DCHECK(test_url_loader_factory_);

  if (fake_suggestions_database_->HasFakeSuggestions(request->url)) {
    std::string fake_suggestions =
        fake_suggestions_database_->GetFakeSuggestions(request->url);
    test_url_loader_factory_->AddResponse(request->url.spec(), fake_suggestions,
                                          net::HTTP_OK);
  }
}
