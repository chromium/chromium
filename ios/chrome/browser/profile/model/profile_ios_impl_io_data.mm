// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_ios_impl_io_data.h"

#import <memory>
#import <set>
#import <utility>

#import "base/barrier_closure.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/net_log/chrome_net_log.h"
#import "components/prefs/json_pref_store.h"
#import "components/prefs/pref_filter.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/net/model/http_server_properties_factory.h"
#import "ios/chrome/browser/net/model/ios_chrome_network_delegate.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/profile/model/ios_chrome_url_request_context_getter.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/cookie_util/cookie_util.h"
#import "ios/net/cookies/ns_http_system_cookie_store.h"
#import "ios/net/cookies/system_cookie_store.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/base/cache_type.h"
#import "net/cookies/cookie_store.h"
#import "net/http/http_cache.h"
#import "net/http/http_network_session.h"
#import "net/http/http_server_properties.h"
#import "net/http/transport_security_state.h"
#import "net/url_request/url_request_context_builder.h"

ProfileIOSImplIOData::Handle::Handle(ProfileIOS* profile)
    : io_data_(new ProfileIOSImplIOData),
      profile_(profile),
      initialized_(false) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(profile);
}

ProfileIOSImplIOData::Handle::~Handle() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  io_data_->ShutdownOnUIThread(GetAllContextGetters());
}

void ProfileIOSImplIOData::Handle::Init(
    const base::FilePath& cookie_path,
    const base::FilePath& cache_path,
    int cache_max_size,
    const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(!io_data_->lazy_params_);

  LazyParams* lazy_params = new LazyParams();

  lazy_params->cookie_path = cookie_path;
  lazy_params->cache_path = cache_path;
  lazy_params->cache_max_size = cache_max_size;
  io_data_->lazy_params_.reset(lazy_params);

  // Keep track of profile path and cache sizes separately so we can use them
  // on demand when creating storage isolated URLRequestContextGetters.
  io_data_->profile_path_ = profile_path;
  io_data_->app_cache_max_size_ = cache_max_size;

  io_data_->InitializeMetricsEnabledStateOnUIThread();
}

scoped_refptr<IOSChromeURLRequestContextGetter>
ProfileIOSImplIOData::Handle::CreateMainRequestContextGetter(
    ProtocolHandlerMap* protocol_handlers,
    PrefService* local_state,
    IOSChromeIOThread* io_thread) const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  LazyInitialize();
  DCHECK(!main_request_context_getter_.get());
  main_request_context_getter_ =
      IOSChromeURLRequestContextGetter::Create(io_data_, protocol_handlers);

  return main_request_context_getter_;
}

ProfileIOSIOData* ProfileIOSImplIOData::Handle::io_data() const {
  LazyInitialize();
  return io_data_;
}

void ProfileIOSImplIOData::Handle::ClearNetworkingHistorySince(
    base::Time time,
    base::OnceClosure completion) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  LazyInitialize();

  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ProfileIOSImplIOData::ClearNetworkingHistorySinceOnIOThread,
          base::Unretained(io_data_), time, std::move(completion)));
}

void ProfileIOSImplIOData::Handle::LazyInitialize() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (initialized_) {
    return;
  }

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  io_data_->InitializeOnUIThread(profile_);
}

std::unique_ptr<ProfileIOSIOData::IOSChromeURLRequestContextGetterVector>
ProfileIOSImplIOData::Handle::GetAllContextGetters() {
  IOSChromeURLRequestContextGetterMap::iterator iter;
  std::unique_ptr<IOSChromeURLRequestContextGetterVector> context_getters(
      new IOSChromeURLRequestContextGetterVector());

  iter = app_request_context_getter_map_.begin();
  for (; iter != app_request_context_getter_map_.end(); ++iter) {
    context_getters->push_back(iter->second);
  }

  if (main_request_context_getter_.get()) {
    context_getters->push_back(main_request_context_getter_);
  }

  return context_getters;
}

ProfileIOSImplIOData::LazyParams::LazyParams() : cache_max_size(0) {}

ProfileIOSImplIOData::LazyParams::~LazyParams() {}

ProfileIOSImplIOData::ProfileIOSImplIOData()
    : ProfileIOSIOData(ProfileIOSType::REGULAR_PROFILE),
      app_cache_max_size_(0) {}

ProfileIOSImplIOData::~ProfileIOSImplIOData() {}

void ProfileIOSImplIOData::InitializeInternal(
    net::URLRequestContextBuilder* context_builder,
    ProfileParams* profile_params) const {
  // Set up a persistent store for use by the network stack on the IO thread.
  base::FilePath network_json_store_filepath(
      profile_path_.Append(kIOSChromeNetworkPersistentStateFilename));
  network_json_store_ = new JsonPrefStore(
      network_json_store_filepath, std::unique_ptr<PrefFilter>(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  network_json_store_->ReadPrefsAsync(nullptr);

  IOSChromeIOThread* const io_thread = profile_params->io_thread;

  context_builder->SetHttpServerProperties(
      HttpServerPropertiesFactory::CreateHttpServerProperties(
          network_json_store_, io_thread->net_log()));

  DCHECK(!lazy_params_->cookie_path.empty());
  cookie_util::CookieStoreConfig ios_cookie_config(
      lazy_params_->cookie_path,
      cookie_util::CookieStoreConfig::RESTORED_SESSION_COOKIES,
      cookie_util::CookieStoreConfig::COOKIE_STORE_IOS);
  auto cookie_store = cookie_util::CreateCookieStore(
      ios_cookie_config, std::move(profile_params->system_cookie_store),
      io_thread->net_log());

  context_builder->SetCookieStore(std::move(cookie_store));
  net::URLRequestContextBuilder::HttpCacheParams cache_params;
  cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::DISK;
  cache_params.max_size = lazy_params_->cache_max_size;
  cache_params.path = lazy_params_->cache_path;
  context_builder->EnableHttpCache(cache_params);

  lazy_params_.reset();
}

void ProfileIOSImplIOData::ClearNetworkingHistorySinceOnIOThread(
    base::Time time,
    base::OnceClosure completion) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(initialized());
  auto barrier =
      base::BarrierClosure(2, base::BindOnce(
                                  [](base::OnceClosure callback) {
                                    web::GetUIThreadTaskRunner({})->PostTask(
                                        FROM_HERE, std::move(callback));
                                  },
                                  std::move(completion)));

  main_request_context()
      ->transport_security_state()
      ->DeleteAllDynamicDataBetween(time, base::Time::Max(), barrier);
  main_request_context()->http_server_properties()->Clear(barrier);
}
