// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "headless/lib/browser/headless_browser_context_options.h"
#include "headless/lib/browser/headless_request_context_manager.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_export.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace headless {
class HeadlessBrowserImpl;
class HeadlessClientHintsControllerDelegate;
class HeadlessWebContentsImpl;

class HEADLESS_EXPORT HeadlessBrowserContextImpl final
    : public HeadlessBrowserContext,
      public content::BrowserContext {
 public:
  HeadlessBrowserContextImpl(const HeadlessBrowserContextImpl&) = delete;
  HeadlessBrowserContextImpl& operator=(const HeadlessBrowserContextImpl&) =
      delete;

  ~HeadlessBrowserContextImpl() override;

  static HeadlessBrowserContextImpl* From(
      HeadlessBrowserContext* browser_context);
  static HeadlessBrowserContextImpl* From(
      content::BrowserContext* browser_context);

  static std::unique_ptr<HeadlessBrowserContextImpl> Create(
      HeadlessBrowserContext::Builder* builder);

  // HeadlessBrowserContext implementation:
  HeadlessWebContents::Builder CreateWebContentsBuilder() override;
  std::vector<HeadlessWebContents*> GetAllWebContents() override;
  HeadlessWebContents* GetWebContentsForDevToolsAgentHostId(
      const std::string& devtools_agent_host_id) override;
  void Close() override;
  const std::string& Id() override;

  // BrowserContext implementation:
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  base::FilePath GetPath() override;
  bool IsOffTheRecord() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  ::storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PlatformNotificationService* GetPlatformNotificationService()
      override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::StorageNotificationService* GetStorageNotificationService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  content::ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;
  content::OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate()
      override;

  HeadlessWebContents* CreateWebContents(HeadlessWebContents::Builder* builder);
  // Register web contents which were created not through Headless API
  // (calling window.open() is a best example for this).
  void RegisterWebContents(
      std::unique_ptr<HeadlessWebContentsImpl> web_contents);
  void DestroyWebContents(HeadlessWebContentsImpl* web_contents);

  HeadlessBrowserImpl* browser() const;
  const HeadlessBrowserContextOptions* options() const;

  void ConfigureNetworkContextParams(
      bool in_memory,
      const base::FilePath& relative_partition_path,
      ::network::mojom::NetworkContextParams* network_context_params,
      ::cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params);

 private:
  HeadlessBrowserContextImpl(
      HeadlessBrowserImpl* browser,
      std::unique_ptr<HeadlessBrowserContextOptions> context_options);

  // Performs initialization of the HeadlessBrowserContextImpl while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  raw_ptr<HeadlessBrowserImpl> browser_;  // Not owned.
  std::unique_ptr<HeadlessBrowserContextOptions> context_options_;
  base::FilePath path_;

  std::unordered_map<std::string, std::unique_ptr<HeadlessWebContents>>
      web_contents_map_;

  std::unique_ptr<content::PermissionControllerDelegate>
      permission_controller_delegate_;

  std::unique_ptr<HeadlessRequestContextManager> request_context_manager_;
  std::unique_ptr<SimpleFactoryKey> simple_factory_key_;

  std::unique_ptr<content::OriginTrialsControllerDelegate>
      origin_trials_controller_delegate_;

  std::unique_ptr<HeadlessClientHintsControllerDelegate> hints_delegate_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_
