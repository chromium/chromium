// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/content_browser_context.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace web {

const char kBrowserContextDataKey[] = "browser_context";

class BrowserContextHolder : public base::SupportsUserData::Data {
 public:
  static content::BrowserContext* FromBrowserState(
      web::BrowserState* browser_state) {
    BrowserContextHolder* holder = static_cast<BrowserContextHolder*>(
        browser_state->GetUserData(kBrowserContextDataKey));
    if (!holder) {
      std::unique_ptr<BrowserContextHolder> data =
          std::make_unique<BrowserContextHolder>();
      data->browser_context_ =
          base::WrapUnique(new ContentBrowserContext(browser_state));
      browser_state->SetUserData(kBrowserContextDataKey, std::move(data));
      holder = static_cast<BrowserContextHolder*>(
          browser_state->GetUserData(kBrowserContextDataKey));
    }
    CHECK(holder);
    return holder->browser_context_.get();
  }

  BrowserContextHolder() = default;

 private:
  std::unique_ptr<content::BrowserContext> browser_context_;
};

content::BrowserContext* ContentBrowserContext::FromBrowserState(
    web::BrowserState* browser_state) {
  DCHECK(browser_state);
  return BrowserContextHolder::FromBrowserState(browser_state);
}

ContentBrowserContext::ContentBrowserContext(web::BrowserState* browser_state)
    : browser_state_(browser_state) {
  InitWhileIOAllowed();

  // This should depend on browser_state_->GetStatePath(), but it is probably
  // unsafe to do this right now. Instead, use a temp directory until this is
  // refactored.
  browser_path_ =
      base::FilePath(base::SysNSStringToUTF8(NSTemporaryDirectory()))
          .Append("Chromium");
}

ContentBrowserContext::~ContentBrowserContext() {
  NotifyWillBeDestroyed();
  ShutdownStoragePartitions();
}

void ContentBrowserContext::InitWhileIOAllowed() {
  FinishInitWhileIOAllowed();
}

void ContentBrowserContext::FinishInitWhileIOAllowed() {}

std::unique_ptr<content::ZoomLevelDelegate>
ContentBrowserContext::CreateZoomLevelDelegate(const base::FilePath&) {
  return nullptr;
}

base::FilePath ContentBrowserContext::GetPath() {
  return browser_path_;
}

bool ContentBrowserContext::IsOffTheRecord() {
  return browser_state_->IsOffTheRecord();
}

content::DownloadManagerDelegate*
ContentBrowserContext::GetDownloadManagerDelegate() {
  return nullptr;
}

content::BrowserPluginGuestManager* ContentBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy*
ContentBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PlatformNotificationService*
ContentBrowserContext::GetPlatformNotificationService() {
  return nullptr;
}

content::PushMessagingService*
ContentBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
ContentBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate*
ContentBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
ContentBrowserContext::GetPermissionControllerDelegate() {
  return nullptr;
}

content::ClientHintsControllerDelegate*
ContentBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
ContentBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
ContentBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
ContentBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

content::ContentIndexProvider*
ContentBrowserContext::GetContentIndexProvider() {
  return nullptr;
}

content::FederatedIdentityApiPermissionContextDelegate*
ContentBrowserContext::GetFederatedIdentityApiPermissionContext() {
  return nullptr;
}

content::FederatedIdentityPermissionContextDelegate*
ContentBrowserContext::GetFederatedIdentityPermissionContext() {
  return nullptr;
}

content::ReduceAcceptLanguageControllerDelegate*
ContentBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  return nullptr;
}

content::OriginTrialsControllerDelegate*
ContentBrowserContext::GetOriginTrialsControllerDelegate() {
  return nullptr;
}

}  // namespace web
