// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_CLIENT_H_
#define IOS_WEB_PUBLIC_WEB_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "ios/web/common/user_agent.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

class GURL;

@protocol CRWFindSession;
@protocol UITraitEnvironment;
@class NSString;

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

  // Returns text to be displayed for an unsupported plugin.
  virtual std::u16string GetPluginNotSupportedText() const;

  // Returns the user agent string for the specified type.
  virtual std::string GetUserAgent(UserAgentType type) const;

  // Returns a string resource given its id.
  virtual std::u16string GetLocalizedString(int message_id) const;

  // Returns the contents of a resource in a StringPiece given the resource id.
  virtual base::StringPiece GetDataResource(
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

  // Gives the embedder a chance to provide the JavaScript to be injected into
  // the web view as early as possible. Result must not be nil.
  // The script returned will be injected in all frames (main and subframes).
  //
  // TODO(crbug.com/703964): Change the return value to NSArray<NSString*> to
  // improve performance.
  virtual NSString* GetDocumentStartScriptForAllFrames(
      BrowserState* browser_state) const;

  // Gives the embedder a chance to provide the JavaScript to be injected into
  // the web view as early as possible. Result must not be nil.
  // The script returned will only be injected in the main frame.
  //
  // TODO(crbug.com/703964): Change the return value to NSArray<NSString*> to
  // improve performance.
  virtual NSString* GetDocumentStartScriptForMainFrame(
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
                                const absl::optional<net::SSLInfo>& info,
                                int64_t navigation_id,
                                base::OnceCallback<void(NSString*)> callback);

  // Instructs the embedder to return a container that is attached to a window.
  virtual UIView* GetWindowedContainer();

  // Enables the logic to handle long press context menu with UIContextMenu.
  virtual bool EnableLongPressUIContextMenu() const;

  // Allows WKWebViews to be inspected using Safari's Web Inspector.
  // TODO(crbug.com/1418431): Remove this method when Web Inspector is enabled
  // unconditionally.
  virtual bool EnableWebInspector() const;

  // Returns the UserAgentType that should be used by default for the web
  // content, based on the `web_state`.
  virtual UserAgentType GetDefaultUserAgent(web::WebState* web_state,
                                            const GURL& url) const;

  // Logs the default mode used (Mobile or Desktop). This is supposed to be
  // called only if the user didn't force the mode.
  virtual void LogDefaultUserAgent(web::WebState* web_state,
                                   const GURL& url) const;

  // Returns true if URL was restored via session restoration cache.
  virtual bool RestoreSessionFromCache(web::WebState* web_state) const;

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

  // Provides a searchable object for the given `web_state` instance.
  virtual id<CRWFindSession> CreateFindSessionForWebState(
      web::WebState* web_state) const API_AVAILABLE(ios(16));

  // Starts a text search in `web_state`.
  virtual void StartTextSearchInWebState(web::WebState* web_state);

  // Stops the ongoing text search in `web_state`.
  virtual void StopTextSearchInWebState(web::WebState* web_state);

  // Returns true if mixed content on HTTPS documents should be upgraded if
  // possible.
  virtual bool IsMixedContentAutoupgradeEnabled(
      web::BrowserState* browser_state) const;

  // Returns true if browser lockdown mode is enabled. Default return value is
  // false.
  virtual bool IsBrowserLockdownModeEnabled(web::BrowserState* browser_state);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_CLIENT_H_
