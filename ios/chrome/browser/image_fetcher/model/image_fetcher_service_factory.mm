// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/image_fetcher/model/image_fetcher_service_factory.h"

#import <utility>

#import "base/files/file_path.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/time/default_clock.h"
#import "components/image_fetcher/core/cache/image_cache.h"
#import "components/image_fetcher/core/cache/image_data_store_disk.h"
#import "components/image_fetcher/core/cache/image_metadata_store_leveldb.h"
#import "components/image_fetcher/core/image_fetcher_service.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

ImageFetcherServiceFactory::ImageFetcherServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "ImageFetcherService") {}

std::unique_ptr<KeyedService>
ImageFetcherServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  base::DefaultClock* clock = base::DefaultClock::GetInstance();

  base::FilePath cache_path = profile->GetStatePath();
  auto metadata_store =
      std::make_unique<image_fetcher::ImageMetadataStoreLevelDB>(
          profile->GetProtoDatabaseProvider(), cache_path, task_runner, clock);
  auto data_store = std::make_unique<image_fetcher::ImageDataStoreDisk>(
      cache_path, task_runner);

  scoped_refptr<image_fetcher::ImageCache> image_cache =
      base::MakeRefCounted<image_fetcher::ImageCache>(
          std::move(data_store), std::move(metadata_store), profile->GetPrefs(),
          clock, task_runner);

  return std::make_unique<image_fetcher::ImageFetcherService>(
      image_fetcher::CreateIOSImageDecoder(),
      profile->GetSharedURLLoaderFactory(), std::move(image_cache),
      /*readonly=*/false);
}
