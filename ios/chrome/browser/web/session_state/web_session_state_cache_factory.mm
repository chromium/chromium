// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/session_state/web_session_state_cache_factory.h"

#import "base/bind.h"
#import "base/logging.h"
#import "base/memory/ptr_util.h"
#import "base/no_destructor.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/all_web_state_list_observation_registrar.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache_web_state_list_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// C++ wrapper around WebSessionStateCache, owning the WebSessionStateCache and
// allowing it bind it to an ChromeBrowserState as a KeyedService.
class WebSessionStateCacheWrapper : public KeyedService {
 public:
  explicit WebSessionStateCacheWrapper(
      ChromeBrowserState* browser_state,
      WebSessionStateCache* web_session_state_cache);

  WebSessionStateCacheWrapper(const WebSessionStateCacheWrapper&) = delete;
  WebSessionStateCacheWrapper& operator=(const WebSessionStateCacheWrapper&) =
      delete;

  ~WebSessionStateCacheWrapper() override;

  WebSessionStateCache* web_session_state_cache() {
    return web_session_state_cache_;
  }

  // KeyedService implementation.
  void Shutdown() override;

 private:
  __strong WebSessionStateCache* web_session_state_cache_;
  std::unique_ptr<AllWebStateListObservationRegistrar> registrar_;
};

WebSessionStateCacheWrapper::WebSessionStateCacheWrapper(
    ChromeBrowserState* browser_state,
    WebSessionStateCache* web_session_state_cache)
    : web_session_state_cache_(web_session_state_cache) {
  DCHECK(web_session_state_cache);
  registrar_ = std::make_unique<AllWebStateListObservationRegistrar>(
      browser_state, std::make_unique<WebSessionStateCacheWebStateListObserver>(
                         web_session_state_cache));
}

WebSessionStateCacheWrapper::~WebSessionStateCacheWrapper() {
  DCHECK(!web_session_state_cache_);
}

void WebSessionStateCacheWrapper::Shutdown() {
  registrar_.reset();
  [web_session_state_cache_ shutdown];
  web_session_state_cache_ = nil;
}

std::unique_ptr<KeyedService> BuildWebSessionStateCacheWrapper(
    web::BrowserState* context) {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<WebSessionStateCacheWrapper>(
      chrome_browser_state,
      [[WebSessionStateCache alloc] initWithBrowserState:chrome_browser_state]);
}
}  // namespace

// static
WebSessionStateCache* WebSessionStateCacheFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  WebSessionStateCacheWrapper* wrapper =
      static_cast<WebSessionStateCacheWrapper*>(
          GetInstance()->GetServiceForBrowserState(browser_state, true));
  return wrapper ? wrapper->web_session_state_cache() : nil;
}

// static
WebSessionStateCacheFactory* WebSessionStateCacheFactory::GetInstance() {
  static base::NoDestructor<WebSessionStateCacheFactory> instance;
  return instance.get();
}

WebSessionStateCacheFactory::WebSessionStateCacheFactory()
    : BrowserStateKeyedServiceFactory(
          "WebSessionStateCache",
          BrowserStateDependencyManager::GetInstance()) {}

WebSessionStateCacheFactory::~WebSessionStateCacheFactory() = default;

std::unique_ptr<KeyedService>
WebSessionStateCacheFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildWebSessionStateCacheWrapper(context);
}

web::BrowserState* WebSessionStateCacheFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
