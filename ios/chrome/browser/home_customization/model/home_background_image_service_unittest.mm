// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"

#import "base/functional/callback.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/mock_callback.h"
#import "base/test/task_environment.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/themes/ntp_background_data.h"
#import "components/themes/ntp_background_service.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::SaveArg;

namespace {
const char kTestImageUrl[] = "https://wallpapers.co/some_image";
const char kTestImageUrl2[] = "https://wallpapers.co/some_image_2";
const char kTestActionUrl[] = "https://wallpapers.co/some_image/learn_more";
}  // namespace

class HomeBackgroundImageServiceTest : public PlatformTest {
 public:
  HomeBackgroundImageServiceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        application_locale_storage_(
            std::make_unique<ApplicationLocaleStorage>()) {
    service_ = std::make_unique<NtpBackgroundService>(
        application_locale_storage_.get(), test_shared_loader_factory_);
    model_ = std::make_unique<HomeBackgroundImageService>(service_.get());
  }

  void SetUpResponseWithNetworkSuccess(
      const GURL& load_url,
      const std::string& response = std::string()) {
    test_url_loader_factory_.AddResponse(load_url.spec(), response);
  }

  void SetUpResponseWithData(const GURL& load_url,
                             const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {}));
    SetUpResponseWithNetworkSuccess(load_url, response);
  }

  void SetUpResponseWithNetworkError(const GURL& load_url) {
    test_url_loader_factory_.AddResponse(load_url.spec(), std::string(),
                                         net::HTTP_NOT_FOUND);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<ApplicationLocaleStorage> application_locale_storage_;
  std::unique_ptr<NtpBackgroundService> service_;
  std::unique_ptr<HomeBackgroundImageService> model_;
};

TEST_F(HomeBackgroundImageServiceTest, SuccessFetchCollectionsImagesResponse) {
  // Add a collection
  ntp::background::Collection collection;
  collection.set_collection_id("shapes");
  collection.set_collection_name("Shapes Title");
  collection.add_preview()->set_image_url(kTestImageUrl);
  ntp::background::GetCollectionsResponse response;
  *response.add_collections() = collection;
  std::string response_string;
  response.SerializeToString(&response_string);
  SetUpResponseWithData(service_.get()->GetCollectionsLoadURLForTesting(),
                        response_string);

  // Add an image to the collection
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url(kTestImageUrl);
  image.add_attribution()->set_text("attribution text");
  image.set_action_url(kTestActionUrl);
  ntp::background::GetImagesInCollectionResponse image_response;
  *image_response.add_images() = image;
  std::string image_response_string;
  image_response.SerializeToString(&image_response_string);
  SetUpResponseWithData(service_.get()->GetImagesURLForTesting(),
                        image_response_string);

  HomeBackgroundImageService::CollectionImageMap collections_images;
  base::MockCallback<HomeBackgroundImageService::CollectionsImagesCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(_))
      .WillOnce(DoAll(SaveArg<0>(&collections_images)));

  base::RunLoop run_loop;
  model_.get()->FetchCollectionsImages(
      mock_callback.Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(collections_images.size(), 1u);
  std::string collection_name = std::get<0>(collections_images[0]);
  std::vector<CollectionImage> images = std::get<1>(collections_images[0]);
  std::string expected_image_url =
      std::string(kTestImageUrl) + GetImageOptions();
  std::string expected_thumbnail_image_url =
      std::string(kTestImageUrl) + GetThumbnailImageOptions();
  EXPECT_EQ(collection_name, "Shapes Title");
  EXPECT_EQ(images.size(), 1u);
  EXPECT_EQ(images[0].collection_id, "shapes");
  EXPECT_EQ(images[0].asset_id, 12345u);
  EXPECT_EQ(images[0].image_url, GURL(expected_image_url));
  EXPECT_EQ(images[0].thumbnail_image_url, GURL(expected_thumbnail_image_url));
  EXPECT_EQ(images[0].attribution[0], "attribution text");
  EXPECT_EQ(images[0].attribution_action_url, GURL(kTestActionUrl));
}

TEST_F(HomeBackgroundImageServiceTest,
       SuccessMultipleFetchCollectionsImagesResponse) {
  // Add a collection
  ntp::background::Collection collection;
  collection.set_collection_id("shapes");
  collection.set_collection_name("Shapes Title");
  collection.add_preview()->set_image_url(kTestImageUrl);

  ntp::background::Collection second_collection;
  second_collection.set_collection_id("nature");
  second_collection.set_collection_name("Nature Title");
  second_collection.add_preview()->set_image_url(kTestImageUrl2);

  ntp::background::GetCollectionsResponse response;
  *response.add_collections() = collection;
  *response.add_collections() = second_collection;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service_.get()->GetCollectionsLoadURLForTesting(),
                        response_string);

  // Add an image to the collection
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url(kTestImageUrl);
  image.add_attribution()->set_text("attribution text");
  image.set_action_url(kTestActionUrl);
  ntp::background::GetImagesInCollectionResponse image_response;
  *image_response.add_images() = image;
  std::string image_response_string;
  image_response.SerializeToString(&image_response_string);

  SetUpResponseWithData(service_.get()->GetImagesURLForTesting(),
                        image_response_string);

  HomeBackgroundImageService::CollectionImageMap collections_images;
  base::MockCallback<HomeBackgroundImageService::CollectionsImagesCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(_))
      .WillOnce(DoAll(SaveArg<0>(&collections_images)));

  base::RunLoop run_loop;
  model_.get()->FetchCollectionsImages(
      mock_callback.Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(collections_images.size(), 2u);
  std::string collection_name = std::get<0>(collections_images[0]);
  std::vector<CollectionImage> images = std::get<1>(collections_images[0]);
  std::string expected_image_url =
      std::string(kTestImageUrl) + GetImageOptions();
  std::string expected_thumbnail_image_url =
      std::string(kTestImageUrl) + GetThumbnailImageOptions();
  EXPECT_EQ(collection_name, "Shapes Title");
  EXPECT_EQ(images.size(), 1u);
  EXPECT_EQ(images[0].collection_id, "shapes");

  std::string second_collection_name = std::get<0>(collections_images[1]);
  std::vector<CollectionImage> second_image =
      std::get<1>(collections_images[1]);
  EXPECT_EQ(second_collection_name, "Nature Title");
  EXPECT_EQ(second_image.size(), 1u);
  EXPECT_EQ(second_image[0].collection_id, "nature");
}

TEST_F(HomeBackgroundImageServiceTest, BadCollectionResponse) {
  SetUpResponseWithData(service_.get()->GetCollectionsLoadURLForTesting(),
                        "bad serialized GetCollectionsResponse");

  HomeBackgroundImageService::CollectionImageMap collections_images;
  base::MockCallback<HomeBackgroundImageService::CollectionsImagesCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(_))
      .WillOnce(DoAll(SaveArg<0>(&collections_images)));

  base::RunLoop run_loop;
  model_.get()->FetchCollectionsImages(
      mock_callback.Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(collections_images.size(), 0u);
}

TEST_F(HomeBackgroundImageServiceTest, CollectionImagesNetworkErrorResponse) {
  // Add a collection
  ntp::background::Collection collection;
  collection.set_collection_id("shapes");
  collection.set_collection_name("Shapes Title");
  collection.add_preview()->set_image_url(kTestImageUrl);
  ntp::background::GetCollectionsResponse response;
  *response.add_collections() = collection;
  std::string response_string;
  response.SerializeToString(&response_string);
  SetUpResponseWithData(service_.get()->GetCollectionsLoadURLForTesting(),
                        response_string);

  SetUpResponseWithNetworkError(service_.get()->GetImagesURLForTesting());

  HomeBackgroundImageService::CollectionImageMap collections_images;
  base::MockCallback<HomeBackgroundImageService::CollectionsImagesCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(_))
      .WillOnce(DoAll(SaveArg<0>(&collections_images)));

  base::RunLoop run_loop;
  model_.get()->FetchCollectionsImages(
      mock_callback.Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(collections_images.size(), 0u);
}

TEST_F(HomeBackgroundImageServiceTest, CollectionNetworkErrorResponse) {
  // Add a collection
  SetUpResponseWithNetworkError(
      service_.get()->GetCollectionsLoadURLForTesting());

  HomeBackgroundImageService::CollectionImageMap collections_images;
  base::MockCallback<HomeBackgroundImageService::CollectionsImagesCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(_))
      .WillOnce(DoAll(SaveArg<0>(&collections_images)));

  base::RunLoop run_loop;
  model_.get()->FetchCollectionsImages(
      mock_callback.Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(collections_images.size(), 0u);
}
