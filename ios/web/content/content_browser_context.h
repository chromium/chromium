// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_CONTENT_BROWSER_CONTEXT_H_
#define IOS_WEB_CONTENT_CONTENT_BROWSER_CONTEXT_H_

#import "base/memory/raw_ptr.h"
#import "build/blink_buildflags.h"
#import "content/public/browser/browser_context.h"
#import "ios/web/public/browser_state.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace content {
class BrowserContext;
}

namespace web {

class BrowserContextHolder;

// Wraps a web::BrowserState. The lifetime of instances is tied to that
// of the corresponding ContentBrowserContext (via SharedUserData).
// This is a temporary solution to unblock other areas of ios/web/content. This
// class needs to be refactored so BrowserContext and BrowserState can be
// properly layered on one another.
class ContentBrowserContext : public content::BrowserContext {
 public:
  static content::BrowserContext* FromBrowserState(
      web::BrowserState* browser_state);
  explicit ContentBrowserContext(web::BrowserState* browser_state);
  ~ContentBrowserContext() override;

  // BrowserContext implementation.
  base::FilePath GetPath() override;
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  bool IsOffTheRecord() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PlatformNotificationService* GetPlatformNotificationService()
      override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::StorageNotificationService* GetStorageNotificationService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  content::ContentIndexProvider* GetContentIndexProvider() override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::FederatedIdentityApiPermissionContextDelegate*
  GetFederatedIdentityApiPermissionContext() override;
  content::FederatedIdentityPermissionContextDelegate*
  GetFederatedIdentityPermissionContext() override;
  content::ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;
  content::OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate()
      override;

 private:
  friend class BrowserContextHolder;

  bool ignore_certificate_errors() const { return false; }

  // Remove when refactored to depend on browser_start_->GetStatePath()
  base::FilePath browser_path_;

  // Performs initialization of the ContentBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();
  void FinishInitWhileIOAllowed();
  raw_ptr<web::BrowserState> browser_state_ = nullptr;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_CONTENT_BROWSER_CONTEXT_H_
