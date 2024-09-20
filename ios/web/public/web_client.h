// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_CLIENT_H_
#define IOS_WEB_PUBLIC_WEB_CLIENT_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "ios/web/common/user_agent.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace base {
class RefCountedMemory;
}

class GURL;

@protocol CRWFindSession;
@protocol UITraitEnvironment;
@class NSString;
@class NSData;
@class UIView;

namespace net {
class SSLInfo;
}

namespace web {

class BrowserState;
class BrowserURLRewriter;
class JavaScriptFeature;
class WebClient;
class WebMainParts;
class WebState;

// Setter and getter for the client.  The client should be set early, before any
// web code is called.
void SetWebClient(WebClient* client);
WebClient* GetWebClient();

// Interface that the embedder of the web layer implements.
class WebClient {
 public:
  WebClient();
  virtual ~WebClient();

  // Allows the embedder to set a custom WebMainParts implementation for the
  // browser startup code.
  virtual std::unique_ptr<WebMainParts> CreateWebMainParts();

  // Gives the embedder a chance to perform tasks before a web view is created.
  virtual void PreWebViewCreation() const {}

  // An embedder may support schemes that are otherwise unknown to lower-level
  // components. To control how /net/url and other components interpret urls of
  // such schemes, the embedder overrides `AddAdditionalSchemes` and adds to the
  // vectors inside the `Schemes` structure.
  struct Schemes {
    Schemes();
    ~Schemes();
    // "Standard" (RFC3986 syntax) schemes.
    std::vector<std::string> standard_schemes;
    // See https://www.w3.org/TR/powerful-features/#is-origin-trustworthy.
    std::vector<std::string> secure_schemes;
  };

  // Gives the embedder a chance to register its own standard and secure url
  // schemes early on in the startup sequence.
  virtual void AddAdditionalSchemes(Schemes* schemes) const {}

  // Returns the embedding application locale string.
  virtual std::string GetApplicationLocale() const;

  // Returns true if URL has application specific schema. Embedder must return
  // true for every custom app specific schema it supports. For example Chromium
  // browser would return true for "chrome://about" URL.
  virtual bool IsAppSpecificURL(const GURL& url) const;

  // Returns the user agent string for the specified type.
  virtual std::string GetUserAgent(UserAgentType type) const;

  // Returns a string resource given its id.
  virtual std::u16string GetLocalizedString(int message_id) const;

  // Returns the contents of a resource in a std::string_view given the resource
  // id.
  virtual std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) const;

  // Returns the raw bytes of a scale independent data resource.
  virtual base::RefCountedMemory* GetDataResourceBytes(int resource_id) const;

  // Returns a list of additional WebUI schemes, if any. These additional
  // schemes act as aliases to the about: scheme. The additional schemes may or
  // may not serve specific WebUI pages depending on the particular
  // URLDataSourceIOS and its override of
  // URLDataSourceIOS::ShouldServiceRequest. For all schemes returned here,
  // view-source is allowed.
  virtual void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) {}

  // Gives the embedder a chance to add url rewriters to the BrowserURLRewriter
  // singleton.
  virtual void PostBrowserURLRewriterCreation(BrowserURLRewriter* rewriter) {}

  // Gives the embedder a chance to provide custom JavaScriptFeatures.
  virtual std::vector<JavaScriptFeature*> GetJavaScriptFeatures(
      BrowserState* browser_state) const;

  // Allows the embedder to bind an interface request for a WebState-scoped
  // interface that originated from the main frame of `web_state`. Called if
  // `web_state` could not bind the receiver itself.
  virtual void BindInterfaceReceiverFromMainFrame(
      WebState* web_state,
      mojo::GenericPendingReceiver receiver) {}

  // Calls the given `callback` with the contents of an error page to display
  // when a navigation error occurs. `error` is always a valid pointer. The
  // string passed to `callback` will be nil if no error page should be
  // displayed. Otherwise, this string will contain the details of the error
  // and maybe links to more info. `info` will have a value for SSL cert errors
  // and otherwise be nullopt. `navigation_id` is passed into this method so
  // that in the case of an SSL cert error, the blocking page can be associated
  // with the tab.
  virtual void PrepareErrorPage(WebState* web_state,
                                const GURL& url,
                                NSError* error,
                                bool is_post,
                                bool is_off_the_record,
                                const std::optional<net::SSLInfo>& info,
                                int64_t navigation_id,
                                base::OnceCallback<void(NSString*)> callback);

  // Instructs the embedder to return a container that is attached to a window.
  virtual UIView* GetWindowedContainer();

  // Enables the web-exposed Fullscreen API.
  virtual bool EnableFullscreenAPI() const;

  // Enables the logic to handle long press context menu with UIContextMenu.
  virtual bool EnableLongPressUIContextMenu() const;

  // Allows WKWebViews to be inspected using Safari's Web Inspector.
  virtual bool EnableWebInspector(web::BrowserState* browser_state) const;

  // Returns the UserAgentType that should be used by default for the web
  // content, based on the `web_state`.
  virtual UserAgentType GetDefaultUserAgent(web::WebState* web_state,
                                            const GURL& url) const;

  // Logs the default mode used (Mobile or Desktop). This is supposed to be
  // called only if the user didn't force the mode.
  virtual void LogDefaultUserAgent(web::WebState* web_state,
                                   const GURL& url) const;

  // Correct missing NTP and reading list virtualURLs and titles. Native session
  // restoration may not properly restore these items.
  virtual void CleanupNativeRestoreURLs(web::WebState* web_state) const;

  // Notify the embedder that `web_state` will display a prompt for the user.
  // Note that the implementation of this method may destroy `web state`.
  virtual void WillDisplayMediaCapturePermissionPrompt(
      web::WebState* web_state) const;

  // Returns whether `url1` and `url2` are actually pointing to the same page.
  virtual bool IsPointingToSameDocument(const GURL& url1,
                                        const GURL& url2) const;

  // Returns true if browser lockdown mode is enabled. Default return value is
  // false.
  virtual bool IsBrowserLockdownModeEnabled();

  // Sets OS lockdown mode preference value. By default, no preference value is
  // set.
  virtual void SetOSLockdownModeEnabled(bool enabled);

  virtual bool IsInsecureFormWarningEnabled(
      web::BrowserState* browser_state) const;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_CLIENT_H_
