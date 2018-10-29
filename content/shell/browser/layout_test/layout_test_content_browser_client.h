// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_BROWSER_CLIENT_H_

#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/layout_test/fake_bluetooth_chooser.mojom.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"

namespace content {

class FakeBluetoothChooser;
class LayoutTestBrowserContext;
class MockClipboardHost;
class MockPlatformNotificationService;

class LayoutTestContentBrowserClient : public ShellContentBrowserClient {
 public:
  // Gets the current instance.
  static LayoutTestContentBrowserClient* Get();

  LayoutTestContentBrowserClient();
  ~LayoutTestContentBrowserClient() override;

  LayoutTestBrowserContext* GetLayoutTestBrowserContext();
  void SetPopupBlockingEnabled(bool block_popups_);
  void ResetMockClipboardHost();

  // Retrieves the last created FakeBluetoothChooser instance.
  std::unique_ptr<FakeBluetoothChooser> GetNextFakeBluetoothChooser();

  // ContentBrowserClient overrides.
  void RenderProcessWillLaunch(
      RenderProcessHost* host,
      service_manager::mojom::ServiceRequest* service_request) override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      RenderProcessHost* render_process_host) override;
  void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                           WebPreferences* prefs) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  BrowserMainParts* CreateBrowserMainParts(
      const MainFunctionParams& parameters) override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      storage::OptionalQuotaSettingsCallback callback) override;
  bool DoesSiteRequireDedicatedProcess(BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;
  std::unique_ptr<OverlayWindow> CreateWindowForPictureInPicture(
      PictureInPictureWindowController* controller) override;

  PlatformNotificationService* GetPlatformNotificationService() override;

  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const GURL& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;
  bool ShouldEnableStrictSiteIsolation() override;
  bool CanIgnoreCertificateErrorIfNeeded() override;

  // ShellContentBrowserClient overrides.
  void ExposeInterfacesToFrame(
      service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
          registry) override;
  scoped_refptr<LoginDelegate> CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info,
      content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
      const content::GlobalRequestID& request_id,
      bool is_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;

 private:
  // Creates and stores a FakeBluetoothChooser instance.
  void CreateFakeBluetoothChooser(mojom::FakeBluetoothChooserRequest request);
  void BindClipboardHost(blink::mojom::ClipboardHostRequest request);

  std::unique_ptr<MockPlatformNotificationService>
      mock_platform_notification_service_;
  bool block_popups_ = false;

  // Stores the next instance of FakeBluetoothChooser that is to be returned
  // when GetNextFakeBluetoothChooser is called.
  std::unique_ptr<FakeBluetoothChooser> next_fake_bluetooth_chooser_;
  std::unique_ptr<MockClipboardHost> mock_clipboard_host_;
};

}  // content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_BROWSER_CLIENT_H_
