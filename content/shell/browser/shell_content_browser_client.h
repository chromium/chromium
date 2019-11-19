// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/shell/browser/shell_speech_recognition_manager_delegate.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "storage/browser/quota/quota_settings.h"

namespace content {

class ShellBrowserContext;
class ShellBrowserMainParts;

std::string GetShellUserAgent();
std::string GetShellLanguage();
blink::UserAgentMetadata GetShellUserAgentMetadata();

class ShellContentBrowserClient : public ContentBrowserClient {
 public:
  // Gets the current instance.
  static ShellContentBrowserClient* Get();

  ShellContentBrowserClient();
  ~ShellContentBrowserClient() override;

  // ContentBrowserClient overrides.
  std::unique_ptr<BrowserMainParts> CreateBrowserMainParts(
      const MainFunctionParams& parameters) override;
  bool IsHandledURL(const GURL& url) override;
  void BindInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) override;
  void RunServiceInstance(
      const service_manager::Identity& identity,
      mojo::PendingReceiver<service_manager::mojom::Service>* receiver)
      override;
  bool ShouldTerminateOnServiceQuit(
      const service_manager::Identity& id) override;
  base::Optional<service_manager::Manifest> GetServiceManifestOverlay(
      base::StringPiece name) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::string GetAcceptLangs(BrowserContext* context) override;
  std::string GetDefaultDownloadName() override;
  WebContentsViewDelegate* GetWebContentsViewDelegate(
      WebContents* web_contents) override;
  scoped_refptr<content::QuotaPermissionContext> CreateQuotaPermissionContext()
      override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      storage::OptionalQuotaSettingsCallback callback) override;
  GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  base::OnceClosure SelectClientCertificate(
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<ClientCertificateDelegate> delegate) override;
  SpeechRecognitionManagerDelegate* CreateSpeechRecognitionManagerDelegate()
      override;
  base::FilePath GetFontLookupTableCacheDir() override;
  DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;
  void OpenURL(SiteInstance* site_instance,
               const OpenURLParams& params,
               base::OnceCallback<void(WebContents*)> callback) override;
  std::unique_ptr<LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      const content::GlobalRequestID& request_id,
      bool is_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;

  std::string GetUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;

#if defined(OS_LINUX) || defined(OS_ANDROID)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_WIN)
  bool PreSpawnRenderer(sandbox::TargetPolicy* policy,
                        RendererSpawnFlags flags) override;
#endif

  mojo::Remote<network::mojom::NetworkContext> CreateNetworkContext(
      BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path) override;
  std::vector<base::FilePath> GetNetworkContextsParentDirectory() override;

  ShellBrowserContext* browser_context();
  ShellBrowserContext* off_the_record_browser_context();
  ShellBrowserMainParts* shell_browser_main_parts() {
    return shell_browser_main_parts_;
  }

  // Used for content_browsertests.
  void set_select_client_certificate_callback(
      base::OnceClosure select_client_certificate_callback) {
    select_client_certificate_callback_ =
        std::move(select_client_certificate_callback);
  }
  void set_should_terminate_on_service_quit_callback(
      base::OnceCallback<bool(const service_manager::Identity&)> callback) {
    should_terminate_on_service_quit_callback_ = std::move(callback);
  }
  void set_login_request_callback(
      base::OnceCallback<void(bool is_main_frame)> login_request_callback) {
    login_request_callback_ = std::move(login_request_callback);
  }

 protected:
  virtual void ExposeInterfacesToFrame(
      service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
          registry);

  void set_browser_main_parts(ShellBrowserMainParts* parts) {
    shell_browser_main_parts_ = parts;
  }

 private:
  base::OnceClosure select_client_certificate_callback_;
  base::OnceCallback<bool(const service_manager::Identity&)>
      should_terminate_on_service_quit_callback_;
  base::OnceCallback<void(bool is_main_frame)> login_request_callback_;

  std::unique_ptr<
      service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>>
      frame_interfaces_;

  // Owned by content::BrowserMainLoop.
  ShellBrowserMainParts* shell_browser_main_parts_;
};

// The delay for sending reports when running with --run-web-tests
constexpr base::TimeDelta kReportingDeliveryIntervalTimeForWebTests =
    base::TimeDelta::FromMilliseconds(100);

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
