// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/off_the_record_profile_ios_io_data.h"

#import <UIKit/UIKit.h>

#import <utility>

#import "base/check_op.h"
#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "components/net_log/chrome_net_log.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/net/model/ios_chrome_network_delegate.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/profile/model/ios_chrome_url_request_context_getter.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/cookie_util/cookie_util.h"
#import "ios/net/cookies/system_cookie_store.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/cookies/cookie_store.h"
#import "net/disk_cache/disk_cache.h"
#import "net/http/http_cache.h"
#import "net/http/http_network_session.h"
#import "net/http/http_server_properties.h"
#import "net/url_request/url_request_context_builder.h"
#import "net/url_request/url_request_job_factory.h"

namespace {

// Called by the notification center on memory warnings.
void OnMemoryWarningReceived(CFNotificationCenterRef center,
                             void* observer,
                             CFStringRef name,
                             const void* object,
                             CFDictionaryRef userInfo) {
  OffTheRecordProfileIOSIOData::Handle* handle =
      (OffTheRecordProfileIOSIOData::Handle*)observer;
  handle->DoomIncognitoCache();
}

}  // namespace

void OffTheRecordProfileIOSIOData::Handle::DoomIncognitoCache() {
  // The cache for the incognito profile is in RAM.
  scoped_refptr<net::URLRequestContextGetter> getter =
      main_request_context_getter_;
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(^{
        DCHECK_CURRENTLY_ON(web::WebThread::IO);
        net::HttpCache* cache = getter->GetURLRequestContext()
                                    ->http_transaction_factory()
                                    ->GetCache();
        if (!cache->GetCurrentBackend()) {
          return;
        }
        cache->GetCurrentBackend()->DoomAllEntries(base::DoNothing());
      }));
}

OffTheRecordProfileIOSIOData::Handle::Handle(ProfileIOS* profile)
    : io_data_(new OffTheRecordProfileIOSIOData()),
      profile_(profile),
      initialized_(false) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(profile);
  io_data_->cookie_path_ =
      profile->GetStatePath().Append(kIOSChromeCookieFilename);
}

OffTheRecordProfileIOSIOData::Handle::~Handle() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  // Stop listening to notifications.
  CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), this,
                                     nullptr, nullptr);

  io_data_->ShutdownOnUIThread(GetAllContextGetters());
}

scoped_refptr<IOSChromeURLRequestContextGetter>
OffTheRecordProfileIOSIOData::Handle::CreateMainRequestContextGetter(
    ProtocolHandlerMap* protocol_handlers) const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  LazyInitialize();
  DCHECK(!main_request_context_getter_.get());
  main_request_context_getter_ =
      IOSChromeURLRequestContextGetter::Create(io_data_, protocol_handlers);
  return main_request_context_getter_;
}

ProfileIOSIOData* OffTheRecordProfileIOSIOData::Handle::io_data() const {
  LazyInitialize();
  return io_data_;
}

void OffTheRecordProfileIOSIOData::Handle::LazyInitialize() const {
  if (initialized_) {
    return;
  }

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  io_data_->InitializeOnUIThread(profile_);

  // Once initialized, listen to memory warnings.
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(), this, &OnMemoryWarningReceived,
      static_cast<CFStringRef>(
          UIApplicationDidReceiveMemoryWarningNotification),
      nullptr, CFNotificationSuspensionBehaviorCoalesce);
}

std::unique_ptr<
    ProfileIOSIOData::IOSChromeURLRequestContextGetterVector>
OffTheRecordProfileIOSIOData::Handle::GetAllContextGetters() {
  std::unique_ptr<IOSChromeURLRequestContextGetterVector> context_getters(
      new IOSChromeURLRequestContextGetterVector());
  if (main_request_context_getter_.get()) {
    context_getters->push_back(main_request_context_getter_);
  }

  return context_getters;
}

OffTheRecordProfileIOSIOData::OffTheRecordProfileIOSIOData()
    : ProfileIOSIOData(ProfileIOSType::INCOGNITO_PROFILE) {}

OffTheRecordProfileIOSIOData::~OffTheRecordProfileIOSIOData() {}

void OffTheRecordProfileIOSIOData::InitializeInternal(
    net::URLRequestContextBuilder* context_builder,
    ProfileParams* profile_params) const {
  IOSChromeIOThread* const io_thread = profile_params->io_thread;

  context_builder->SetCookieStore(cookie_util::CreateCookieStore(
      cookie_util::CookieStoreConfig(
          cookie_path_,
          cookie_util::CookieStoreConfig::RESTORED_SESSION_COOKIES,
          cookie_util::CookieStoreConfig::COOKIE_STORE_IOS),
      std::move(profile_params->system_cookie_store), io_thread->net_log()));

  net::URLRequestContextBuilder::HttpCacheParams cache_params;
  cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
  context_builder->EnableHttpCache(cache_params);
}
