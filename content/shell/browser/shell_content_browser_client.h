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

namespace content {

class ResourceDispatcherHostDelegate;
class ShellBrowserContext;
class ShellBrowserMainParts;

class ShellContentBrowserClient : public ContentBrowserClient {
 public:
  // Gets the current instance.
  static ShellContentBrowserClient* Get();

  ShellContentBrowserClient();
  ~ShellContentBrowserClient() override;

  // ContentBrowserClient overrides.
  BrowserMainParts* CreateBrowserMainParts(
      const MainFunctionParams& parameters) override;
  bool DoesSiteRequireDedicatedProcess(BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;
  bool IsHandledURL(const GURL& url) override;
  void BindInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) override;
  void RegisterInProcessServices(StaticServiceMap* services,
                                 ServiceManagerConnection* connection) override;
  void RegisterOutOfProcessServices(OutOfProcessServiceMap* services) override;
  bool ShouldTerminateOnServiceQuit(
      const service_manager::Identity& id) override;
  std::unique_ptr<base::Value> GetServiceManifestOverlay(
      base::StringPiece name) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  void AdjustUtilityServiceProcessCommandLine(
      const service_manager::Identity& identity,
      base::CommandLine* command_line) override;
  std::string GetAcceptLangs(BrowserContext* context) override;
  void ResourceDispatcherHostCreated() override;
  std::string GetDefaultDownloadName() override;
  WebContentsViewDelegate* GetWebContentsViewDelegate(
      WebContents* web_contents) override;
  QuotaPermissionContext* CreateQuotaPermissionContext() override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      storage::OptionalQuotaSettingsCallback callback) override;
  GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  void SelectClientCertificate(
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<ClientCertificateDelegate> delegate) override;
  SpeechRecognitionManagerDelegate* CreateSpeechRecognitionManagerDelegate()
      override;
  net::NetLog* GetNetLog() override;
  DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;
  void OpenURL(BrowserContext* browser_context,
               const OpenURLParams& params,
               const base::Callback<void(WebContents*)>& callback) override;
  scoped_refptr<LoginDelegate> CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info,
      content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
      const content::GlobalRequestID& request_id,
      bool is_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;

#if defined(OS_LINUX) || defined(OS_ANDROID)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_WIN)
  bool PreSpawnRenderer(sandbox::TargetPolicy* policy) override;
#endif

  ShellBrowserContext* browser_context();
  ShellBrowserContext* off_the_record_browser_context();
  ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate() {
    return resource_dispatcher_host_delegate_.get();
  }
  ShellBrowserMainParts* shell_browser_main_parts() {
    return shell_browser_main_parts_;
  }

  // Used for content_browsertests.
  void set_select_client_certificate_callback(
      base::Closure select_client_certificate_callback) {
    select_client_certificate_callback_ =
        std::move(select_client_certificate_callback);
  }
  void set_should_terminate_on_service_quit_callback(
      base::Callback<bool(const service_manager::Identity&)> callback) {
    should_terminate_on_service_quit_callback_ = std::move(callback);
  }
  void set_login_request_callback(
      base::Callback<void()> login_request_callback) {
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
  std::unique_ptr<ResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

  base::Closure select_client_certificate_callback_;
  base::Callback<bool(const service_manager::Identity&)>
      should_terminate_on_service_quit_callback_;
  base::Callback<void()> login_request_callback_;

  std::unique_ptr<
      service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>>
      frame_interfaces_;

  ShellBrowserMainParts* shell_browser_main_parts_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
