// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/common/content_export.h"
#include "content/common/web_ui.mojom.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"

namespace content {
class NavigationRequest;
class RenderFrameHost;
class RenderFrameHostImpl;
class WebUIMainFrameObserver;

class CONTENT_EXPORT WebUIImpl : public WebUI, public mojom::WebUIHost {
 public:
  explicit WebUIImpl(WebContents* web_contents);
  explicit WebUIImpl(NavigationRequest* request);
  WebUIImpl(const WebUIImpl&) = delete;
  WebUIImpl& operator=(const WebUIImpl&) = delete;
  ~WebUIImpl() override;

  // A WebUIImpl object is created and owned by the WebUI navigation's
  // NavigationRequest, until a RenderFrameHost has been picked for the
  // navigation, at which point the ownership of the WebUIImpl object is moved
  // to the RenderFrameHost. This function is called when that happens.
  void SetRenderFrameHost(RenderFrameHost* render_frame_host);

  // Called when a RenderFrame is created for a WebUI (reload after a renderer
  // crash) or when a WebUI is created for a RenderFrame (i.e. navigating from
  // chrome://downloads to chrome://bookmarks) or when both are new (i.e.
  // opening a new tab).
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host);

  // Called when the owning RenderFrameHost has started unloading.
  void RenderFrameHostUnloading();

  // Called when the renderer-side frame is destroyed, along with any mojo
  // connections to it. The browser can not attempt to communicate with the
  // renderer afterward.
  void RenderFrameDeleted();

  // Called right after AllowBindings is notified to a RenderFrame.
  void SetUpMojoConnection();

  // Called when a RenderFrame is deleted for a WebUI (i.e. a renderer crash).
  void TearDownMojoConnection();

  // Add a property to the WebUI binding object.
  void SetProperty(const std::string& name, const std::string& value);

  // WebUI implementation:
  WebContents* GetWebContents() override;
  WebUIController* GetController() override;
  RenderFrameHost* GetRenderFrameHost() override;
  void SetController(std::unique_ptr<WebUIController> controller) override;
  float GetDeviceScaleFactor() override;
  const std::u16string& GetOverriddenTitle() override;
  void OverrideTitle(const std::u16string& title) override;
  BindingsPolicySet GetBindings() override;
  void SetBindings(BindingsPolicySet bindings) override;
  const std::vector<std::string>& GetRequestableSchemes() override;
  void AddRequestableScheme(const char* scheme) override;
  void AddMessageHandler(std::unique_ptr<WebUIMessageHandler> handler) override;
  void RegisterMessageCallback(std::string_view message,
                               MessageCallback callback) override;
  void ProcessWebUIMessage(const GURL& source_url,
                           const std::string& message,
                           base::Value::List args) override;
  bool CanCallJavascript() override;
  void CallJavascriptFunctionUnsafe(
      std::string_view function_name,
      base::span<const base::ValueView> args) override;
  std::vector<std::unique_ptr<WebUIMessageHandler>>* GetHandlersForTesting()
      override;

  const mojo::AssociatedRemote<mojom::WebUI>& GetRemoteForTest() const {
    return remote_;
  }
  WebUIMainFrameObserver* GetWebUIMainFrameObserverForTest() const {
    return web_contents_observer_.get();
  }

  bool HasRenderFrameHost() const;

  static blink::mojom::LocalResourceLoaderConfigPtr
  GetLocalResourceLoaderConfigForTesting(URLDataManagerBackend* data_backend);

 private:
  friend class WebUIMainFrameObserver;

  // mojom::WebUIHost
  void Send(const std::string& message, base::Value::List args) override;

  // Execute a string of raw JavaScript on the page.
  void ExecuteJavascript(const std::u16string& javascript);

  // Called internally and by the owned WebUIMainFrameObserver.
  void DisallowJavascriptOnAllHandlers();

  blink::mojom::LocalResourceLoaderConfigPtr GetLocalResourceLoaderConfig();

  // A map of message name -> message handling callback.
  std::map<std::string, MessageCallback> message_callbacks_;

  // Options that may be overridden by individual Web UI implementations. The
  // bool options default to false. See the public getters for more information.
  std::u16string overridden_title_;  // Defaults to empty string.

  // The bindings that should be enabled for this page.
  BindingsPolicySet bindings_ =
      BindingsPolicySet({BindingsPolicyValue::kWebUi});

  // The URL schemes that can be requested by this document.
  std::vector<std::string> requestable_schemes_;

  // Non-owning pointer to the WebContents and RenderFrameHostImpl this WebUI is
  // associated with. It is generally safe, because |web_content_| indirectly
  // owns |frame_host_|, which owns |this|.
  //
  // Note: During the destructor, releasing |controller_| calls content/
  // embedder code. This might delete both synchronously.
  // This lead to one UAF. See https://crbug.com/1308391
  // See regression test:
  // `WebUIImplBrowserTest::SynchronousWebContentDeletionInUnload`
  const raw_ptr<WebContents, DisableDanglingPtrDetection> web_contents_;

  // During WebUI construction, `frame_host_` might stay unset for a while,
  // as the WebUIImpl object is created early in a navigation, and a
  // RenderFrameHost for the navigation might not be created until the final
  // response for the navigation is received in some cases
  // (after `NavigationRequest::OnResponseStarted()`).
  // During WebUI destruction, `frame_host_` is always valid except
  // if the WebContents is destroyed by the WebUIController subclass.
  // See regression test:
  // `WebUIImplBrowserTest::SynchronousWebContentDeletionInUnload`
  base::WeakPtr<RenderFrameHostImpl> frame_host_;

  // The WebUIMessageHandlers we own.
  std::vector<std::unique_ptr<WebUIMessageHandler>> handlers_;

  // Notifies this WebUI about notifications in the main frame.
  const std::unique_ptr<WebUIMainFrameObserver> web_contents_observer_;

  std::unique_ptr<WebUIController> controller_;

  mojo::AssociatedRemote<mojom::WebUI> remote_;
  mojo::AssociatedReceiver<mojom::WebUIHost> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
