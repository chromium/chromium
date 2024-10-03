// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_CONTENT_BROWSER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/web_test/common/fake_bluetooth_chooser.mojom-forward.h"
#include "content/web_test/common/web_test.mojom-forward.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/blink/public/mojom/compute_pressure/web_pressure_manager_automation.mojom-forward.h"
#include "third_party/blink/public/mojom/cookie_manager/cookie_manager_automation.mojom-forward.h"
#include "third_party/blink/public/mojom/device_posture/device_posture_provider_automation.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_automation.mojom-forward.h"
#include "third_party/blink/public/mojom/sensor/web_sensor_provider_automation.mojom-forward.h"
#include "third_party/blink/public/mojom/storage_access/storage_access_automation.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request_automation.mojom-forward.h"

namespace blink {
namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace content {
class FakeBluetoothChooser;
class FakeBluetoothChooserFactory;
class FakeBluetoothDelegate;
class MockBadgeService;
class MockClipboardHost;
class WebTestBrowserContext;
class WebTestSensorProviderManager;

class WebTestContentBrowserClient : public ShellContentBrowserClient {
 public:
  // Gets the current instance.
  static WebTestContentBrowserClient* Get();

  WebTestContentBrowserClient();
  ~WebTestContentBrowserClient() override;

  WebTestBrowserContext* GetWebTestBrowserContext();
  void SetPopupBlockingEnabled(bool block_popups_);
  void ResetMockClipboardHosts();

  // Retrieves the last created FakeBluetoothChooser instance.
  std::unique_ptr<FakeBluetoothChooser> GetNextFakeBluetoothChooser();
  void ResetFakeBluetoothDelegate();

  void ResetWebSensorProviderAutomation();

  // ContentBrowserClient overrides.
  void BrowserChildProcessHostCreated(BrowserChildProcessHost* host) override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      RenderProcessHost* render_process_host) override;
  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
  void OverrideWebkitPrefs(WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::unique_ptr<BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  std::vector<url::Origin> GetOriginsRequiringDedicatedProcess() override;
  std::unique_ptr<VideoOverlayWindow> CreateWindowForVideoPictureInPicture(
      VideoPictureInPictureWindowController* controller) override;
  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const url::Origin& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;
  void RegisterBrowserInterfaceBindersForFrame(
      RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  bool CanAcceptUntrustedExchangesIfNeeded() override;
  BluetoothDelegate* GetBluetoothDelegate() override;
  content::TtsPlatform* GetTtsPlatform() override;
  std::unique_ptr<LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const content::GlobalRequestID& request_id,
      bool is_request_for_primary_main_frame,
      bool is_request_for_navigation,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
#if BUILDFLAG(IS_WIN)
  bool PreSpawnChild(sandbox::TargetConfig* config,
                     sandbox::mojom::Sandbox sandbox_type,
                     ChildSpawnFlags flags) override;
#endif
  std::string GetAcceptLangs(BrowserContext* context) override;
  bool IsInterestGroupAPIAllowed(content::RenderFrameHost* render_frame_host,
                                 InterestGroupApiOperation operation,
                                 const url::Origin& top_frame_origin,
                                 const url::Origin& api_origin) override;
  bool IsPrivacySandboxReportingDestinationAttested(
      content::BrowserContext* browser_context,
      const url::Origin& destination_origin,
      content::PrivacySandboxInvokingAPI invoking_api) override;
  void GetHyphenationDictionary(
      base::OnceCallback<void(const base::FilePath&)>) override;
  void RegisterMojoBinderPoliciesForSameOriginPrerendering(
      MojoBinderPolicyMap& policy_map) override;
  void RegisterMojoBinderPoliciesForPreview(
      MojoBinderPolicyMap& policy_map) override;

 private:
  // ShellContentBrowserClient overrides.
  void ConfigureNetworkContextParamsForShell(
      BrowserContext* context,
      network::mojom::NetworkContextParams* context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override;

  // Creates and stores a FakeBluetoothChooserFactory instance.
  void CreateFakeBluetoothChooserFactory(
      mojo::PendingReceiver<mojom::FakeBluetoothChooserFactory> receiver);
  void BindClipboardHost(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver);

  void BindBadgeService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::BadgeService> receiver);

  void BindPermissionAutomation(
      mojo::PendingReceiver<blink::test::mojom::PermissionAutomation> receiver);

  void BindStorageAccessAutomation(
      mojo::PendingReceiver<blink::test::mojom::StorageAccessAutomation>
          receiver);

  void BindCookieManagerAutomation(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::test::mojom::CookieManagerAutomation>
          receiver);

  void BindDevicePostureProviderAutomation(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::test::mojom::DevicePostureProviderAutomation>
          receiver);

  void BindFedCmAutomation(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::test::mojom::FederatedAuthRequestAutomation>
          receiver);

  void BindWebSensorProviderAutomation(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::test::mojom::WebSensorProviderAutomation>
          receiver);

  void BindWebPressureManagerAutomation(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::test::mojom::WebPressureManagerAutomation>
          receiver);

  void BindWebTestControlHost(
      int render_process_id,
      mojo::PendingAssociatedReceiver<mojom::WebTestControlHost> receiver);

  void BindNonAssociatedWebTestControlHost(
      mojo::PendingReceiver<mojom::NonAssociatedWebTestControlHost> receiver);

  bool block_popups_ = true;

  // Stores the FakeBluetoothChooserFactory that produces FakeBluetoothChoosers.
  std::unique_ptr<FakeBluetoothChooserFactory> fake_bluetooth_chooser_factory_;
  std::unique_ptr<FakeBluetoothDelegate> fake_bluetooth_delegate_;
  std::unique_ptr<MockClipboardHost> mock_clipboard_host_;
  std::unique_ptr<MockBadgeService> mock_badge_service_;
  mojo::UniqueReceiverSet<blink::test::mojom::DevicePostureProviderAutomation>
      device_posture_provider_managers_;
  std::unique_ptr<WebTestSensorProviderManager> sensor_provider_manager_;
  mojo::UniqueReceiverSet<blink::test::mojom::CookieManagerAutomation>
      cookie_managers_;
  mojo::UniqueReceiverSet<blink::test::mojom::FederatedAuthRequestAutomation>
      fedcm_managers_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_CONTENT_BROWSER_CLIENT_H_
