// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/chrome_browser_state_impl_io_data.h"

#include <memory>
#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/net_log/chrome_net_log.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_constants.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#include "ios/chrome/browser/net/cookie_util.h"
#include "ios/chrome/browser/net/http_server_properties_factory.h"
#include "ios/chrome/browser/net/ios_chrome_network_delegate.h"
#include "ios/chrome/browser/net/ios_chrome_url_request_context_getter.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/net/cookies/cookie_store_ios.h"
#import "ios/net/cookies/ns_http_system_cookie_store.h"
#import "ios/net/cookies/system_cookie_store.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/base/cache_type.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromeBrowserStateImplIOData::Handle::Handle(
    ios::ChromeBrowserState* browser_state)
    : io_data_(new ChromeBrowserStateImplIOData),
      browser_state_(browser_state),
      initialized_(false) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(browser_state);
}

ChromeBrowserStateImplIOData::Handle::~Handle() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  io_data_->ShutdownOnUIThread(GetAllContextGetters());
}

void ChromeBrowserStateImplIOData::Handle::Init(
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
ChromeBrowserStateImplIOData::Handle::CreateMainRequestContextGetter(
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

ChromeBrowserStateIOData* ChromeBrowserStateImplIOData::Handle::io_data()
    const {
  LazyInitialize();
  return io_data_;
}

void ChromeBrowserStateImplIOData::Handle::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  LazyInitialize();

  base::PostTask(
      FROM_HERE, {web::WebThread::IO},
      base::BindOnce(
          &ChromeBrowserStateImplIOData::ClearNetworkingHistorySinceOnIOThread,
          base::Unretained(io_data_), time, completion));
}

void ChromeBrowserStateImplIOData::Handle::LazyInitialize() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (initialized_)
    return;

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  io_data_->InitializeOnUIThread(browser_state_);
}

std::unique_ptr<
    ChromeBrowserStateIOData::IOSChromeURLRequestContextGetterVector>
ChromeBrowserStateImplIOData::Handle::GetAllContextGetters() {
  IOSChromeURLRequestContextGetterMap::iterator iter;
  std::unique_ptr<IOSChromeURLRequestContextGetterVector> context_getters(
      new IOSChromeURLRequestContextGetterVector());

  iter = app_request_context_getter_map_.begin();
  for (; iter != app_request_context_getter_map_.end(); ++iter)
    context_getters->push_back(iter->second);

  if (main_request_context_getter_.get())
    context_getters->push_back(main_request_context_getter_);

  return context_getters;
}

ChromeBrowserStateImplIOData::LazyParams::LazyParams() : cache_max_size(0) {}

ChromeBrowserStateImplIOData::LazyParams::~LazyParams() {}

ChromeBrowserStateImplIOData::ChromeBrowserStateImplIOData()
    : ChromeBrowserStateIOData(
          ios::ChromeBrowserStateType::REGULAR_BROWSER_STATE),
      app_cache_max_size_(0) {}

ChromeBrowserStateImplIOData::~ChromeBrowserStateImplIOData() {}

void ChromeBrowserStateImplIOData::InitializeInternal(
    std::unique_ptr<IOSChromeNetworkDelegate> chrome_network_delegate,
    ProfileParams* profile_params,
    ProtocolHandlerMap* protocol_handlers) const {
  // Set up a persistent store for use by the network stack on the IO thread.
  base::FilePath network_json_store_filepath(
      profile_path_.Append(kIOSChromeNetworkPersistentStateFilename));
  network_json_store_ = new JsonPrefStore(
      network_json_store_filepath, std::unique_ptr<PrefFilter>(),
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  network_json_store_->ReadPrefsAsync(nullptr);

  net::URLRequestContext* main_context = main_request_context();

  IOSChromeIOThread* const io_thread = profile_params->io_thread;
  IOSChromeIOThread::Globals* const io_thread_globals = io_thread->globals();

  ApplyProfileParamsToContext(main_context);

  set_http_server_properties(
      HttpServerPropertiesFactory::CreateHttpServerProperties(
          network_json_store_, io_thread->net_log()));

  main_context->set_transport_security_state(transport_security_state());

  main_context->set_net_log(io_thread->net_log());

  network_delegate_ = std::move(chrome_network_delegate);

  main_context->set_network_delegate(network_delegate_.get());

  main_context->set_http_server_properties(http_server_properties());

  main_context->set_host_resolver(io_thread_globals->host_resolver.get());

  main_context->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());

  main_context->set_proxy_resolution_service(proxy_resolution_service());

  DCHECK(!lazy_params_->cookie_path.empty());
  cookie_util::CookieStoreConfig ios_cookie_config(
      lazy_params_->cookie_path,
      cookie_util::CookieStoreConfig::RESTORED_SESSION_COOKIES,
      cookie_util::CookieStoreConfig::COOKIE_STORE_IOS,
      cookie_config::GetCookieCryptoDelegate());
  main_cookie_store_ = cookie_util::CreateCookieStore(
      ios_cookie_config, std::move(profile_params->system_cookie_store),
      io_thread->net_log());

  if (profile_params->path.BaseName().value() ==
      kIOSChromeInitialBrowserState) {
    // Enable metrics on the default profile, not secondary profiles.
    static_cast<net::CookieStoreIOS*>(main_cookie_store_.get())
        ->SetMetricsEnabled();
  }

  main_context->set_cookie_store(main_cookie_store_.get());

  std::unique_ptr<net::HttpCache::BackendFactory> main_backend(
      new net::HttpCache::DefaultBackend(
          net::DISK_CACHE, net::CACHE_BACKEND_BLOCKFILE,
          lazy_params_->cache_path, lazy_params_->cache_max_size));
  http_network_session_ = CreateHttpNetworkSession(*profile_params);
  main_http_factory_ = CreateMainHttpFactory(http_network_session_.get(),
                                             std::move(main_backend));
  main_context->set_http_transaction_factory(main_http_factory_.get());

  main_job_factory_ = std::make_unique<net::URLRequestJobFactoryImpl>();
  InstallProtocolHandlers(main_job_factory_.get(), protocol_handlers);

  main_context->set_job_factory(main_job_factory_.get());

  lazy_params_.reset();
}

void ChromeBrowserStateImplIOData::ClearNetworkingHistorySinceOnIOThread(
    base::Time time,
    const base::Closure& completion) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(initialized());
  DCHECK(transport_security_state());
  auto barrier = base::BarrierClosure(
      2, base::BindOnce(
             [](base::Closure completion) {
               base::PostTask(FROM_HERE, base::TaskTraits(web::WebThread::UI),
                              std::move(completion));
             },
             std::move(completion)));

  transport_security_state()->DeleteAllDynamicDataSince(time, barrier);
  http_server_properties()->Clear(barrier);
}
