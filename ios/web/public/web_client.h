// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_CLIENT_H_
#define IOS_WEB_PUBLIC_WEB_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/values.h"
#include "ios/web/public/user_agent.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"
#include "ui/base/layout.h"
#include "url/url_util.h"

namespace base {
class RefCountedMemory;
}

class GURL;

@class UIWebView;
@class NSString;

namespace net {
class SSLInfo;
}

namespace web {

class BrowserState;
class BrowserURLRewriter;
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
  // such schemes, the embedder overrides |AddAdditionalSchemes| and adds to the
  // vectors inside the |Schemes| structure.
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
  virtual base::string16 GetPluginNotSupportedText() const;

  // Returns a string describing the embedder product name and version, of the
  // form "productname/version".  Used as part of the user agent string.
  virtual std::string GetProduct() const;

  // Returns the user agent string for the specified type.
  virtual std::string GetUserAgent(UserAgentType type) const;

  // Returns a string resource given its id.
  virtual base::string16 GetLocalizedString(int message_id) const;

  // Returns the contents of a resource in a StringPiece given the resource id.
  virtual base::StringPiece GetDataResource(int resource_id,
                                            ui::ScaleFactor scale_factor) const;

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

  using StaticServiceMap =
      std::map<std::string, service_manager::EmbeddedServiceInfo>;

  // Registers services to be loaded by the Service Manager.
  virtual void RegisterServices(StaticServiceMap* services) {}

  // Allows the embedder to provide a dictionary loaded from a JSON file
  // resembling a service manifest whose capabilities section will be merged
  // with web's own for |name|. Additional entries will be appended to their
  // respective sections.
  virtual std::unique_ptr<base::Value> GetServiceManifestOverlay(
      base::StringPiece name);

  // Allows the embedder to bind an interface request for a WebState-scoped
  // interface that originated from the main frame of |web_state|. Called if
  // |web_state| could not bind the request for |interface_name| itself.
  virtual void BindInterfaceRequestFromMainFrame(
      WebState* web_state,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) {}

  // Informs the embedder that a certificate error has occurred. |cert_error| is
  // a network error code defined in //net/base/net_error_list.h. If
  // |overridable| is true, the user can ignore the error and continue. The
  // embedder can call the |callback| asynchronously (an argument of true means
  // that |cert_error| should be ignored and web// should load the page).
  virtual void AllowCertificateError(
      WebState* web_state,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool overridable,
      const base::Callback<void(bool)>& callback);

  // Returns the information to display when a navigation error occurs.
  // |error| and |error_html| are always valid pointers. Embedder may set
  // |error_html| to an HTML page containing the details of the error and maybe
  // links to more info.
  virtual void PrepareErrorPage(NSError* error,
                                bool is_post,
                                bool is_off_the_record,
                                NSString** error_html);

  // Allows upper layers to inject experimental flags to the web layer.
  // TODO(crbug.com/734150): Clean up this flag after experiment. If need for a
  // second flag arises before clean up, consider generalizing to an experiment
  // flags struct instead of adding a bool method for each experiment.
  virtual bool IsSlimNavigationManagerEnabled() const;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_CLIENT_H_
