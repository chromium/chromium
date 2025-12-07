// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/favicon_web_state_dispatcher_impl.h"

#import "base/task/sequenced_task_runner.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

namespace {
// Default delay to download the favicon when the WebState is handed back.
constexpr base::TimeDelta kDefaultDelayFavicon = base::Seconds(10);
}  // namespace

namespace reading_list {

FaviconWebStateDispatcherImpl::FaviconWebStateDispatcherImpl(
    ProfileIOS* profile,
    base::TimeDelta keep_alive)
    : FaviconWebStateDispatcher(),
      profile_(profile),
      keep_alive_(keep_alive),
      weak_ptr_factory_(this) {}

FaviconWebStateDispatcherImpl::FaviconWebStateDispatcherImpl(
    ProfileIOS* profile)
    : FaviconWebStateDispatcherImpl(profile, kDefaultDelayFavicon) {}

FaviconWebStateDispatcherImpl::~FaviconWebStateDispatcherImpl() {}

std::unique_ptr<web::WebState>
FaviconWebStateDispatcherImpl::RequestWebState() {
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(web::WebState::CreateParams(profile_));

  favicon::WebFaviconDriver::CreateForWebState(
      web_state.get(), ios::FaviconServiceFactory::GetForProfile(
                           profile_, ServiceAccessType::EXPLICIT_ACCESS));

  return web_state;
}

void FaviconWebStateDispatcherImpl::ReleaseAll() {
  web_states_.clear();
}

void FaviconWebStateDispatcherImpl::ReturnWebState(
    std::unique_ptr<web::WebState> web_state) {
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  web_states_.insert(std::make_pair(web_state_id, std::move(web_state)));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FaviconWebStateDispatcherImpl::ReleaseWebStateWithId,
                     weak_ptr_factory_.GetWeakPtr(), web_state_id),
      keep_alive_);
}

void FaviconWebStateDispatcherImpl::ReleaseWebStateWithId(
    web::WebStateID web_state_id) {
  // Release the WebState if it has not yet been released via ReleaseAll().
  auto iter = web_states_.find(web_state_id);
  if (iter != web_states_.end()) {
    web_states_.erase(iter);
  }
}

}  // namespace reading_list
