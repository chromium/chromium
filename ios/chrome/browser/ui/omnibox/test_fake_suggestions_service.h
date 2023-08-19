// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_TEST_FAKE_SUGGESTIONS_SERVICE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_TEST_FAKE_SUGGESTIONS_SERVICE_H_

#import "base/files/file_path.h"
#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "base/scoped_observation.h"
#import "components/omnibox/browser/remote_suggestions_service.h"
#import "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {
class TestURLLoaderFactory;
}  // namespace network

class TemplateURLService;
class FakeSuggestionsDatabase;

class TestFakeSuggestionsService : public RemoteSuggestionsService::Observer {
 public:
  static TestFakeSuggestionsService* GetInstance();

  TestFakeSuggestionsService(const TestFakeSuggestionsService&) = delete;
  TestFakeSuggestionsService& operator=(const TestFakeSuggestionsService&) =
      delete;
  ~TestFakeSuggestionsService() override;

  void SetUp(RemoteSuggestionsService* remote_suggestions_service,
             TemplateURLService* template_url_service,
             const base::FilePath& file_path);

  void TearDown(RemoteSuggestionsService* remote_suggestions_service,
                network::mojom::URLLoaderFactory* url_loader_factory);

  // RemoteSuggestionsService::Observer:
  void OnSuggestRequestCreated(
      const base::UnguessableToken& request_id,
      const network::ResourceRequest* request) override;

 private:
  friend class base::NoDestructor<TestFakeSuggestionsService>;

  TestFakeSuggestionsService();

  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  std::unique_ptr<FakeSuggestionsDatabase> fake_suggestions_database_;

  base::ScopedObservation<RemoteSuggestionsService,
                          RemoteSuggestionsService::Observer>
      remote_suggestions_service_observation_{this};

  base::WeakPtrFactory<TestFakeSuggestionsService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_TEST_FAKE_SUGGESTIONS_SERVICE_H_
