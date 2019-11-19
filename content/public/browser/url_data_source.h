// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_H_
#define CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"

class GURL;

namespace base {
class RefCountedMemory;
}

namespace content {
class BrowserContext;
class ResourceContext;

// A URLDataSource is an object that can answer requests for WebUI data
// asynchronously. An implementation of URLDataSource should handle calls to
// StartDataRequest() by starting its (implementation-specific) asynchronous
// request for the data, then running the callback given in that method to
// notify.
class CONTENT_EXPORT URLDataSource {
 public:
  // Adds a URL data source to |browser_context|.
  static void Add(BrowserContext* browser_context,
                  std::unique_ptr<URLDataSource> source);

  // Gets a reference to the URL data source for |url| and runs |callback| with
  // it as an argument.
  // TODO (rbpotter): Remove this function when the OOBE page Polymer 2
  // migration is complete.
  static void GetSourceForURL(BrowserContext* browser_context,
                              const GURL& url,
                              base::OnceCallback<void(URLDataSource*)>);

  // Parse |url| to get the path which will be used to resolve the request. The
  // path is the remaining portion after the scheme and hostname, without the
  // leading slash.
  static std::string URLToRequestPath(const GURL& url);

  virtual ~URLDataSource() {}

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source. For
  // sources where a scheme is used instead of the hostname as the unique
  // identifier, the suffix "://" must be added to the return value, eg. for a
  // URLDataSource which would display resources with URLs on the form
  // your-scheme://anything , GetSource() must return "your-scheme://".
  virtual std::string GetSource() = 0;

  // Used by StartDataRequest so that the child class can return the data when
  // it's available.
  typedef base::Callback<void(scoped_refptr<base::RefCountedMemory>)>
      GotDataCallback;

  // Must be called on the task runner specified by TaskRunnerForRequestPath,
  // or the IO thread if TaskRunnerForRequestPath returns nullptr.
  //
  // Called by URLDataSource to request data at |url|. The child class should
  // run |callback| when the data is available or if the request could not be
  // satisfied. This can be called either in this callback or asynchronously
  // with the response. |wc_getter| can be called on the UI thread to return the
  // WebContents for this request if it originates from a render frame. If it
  // originated from a worker or if the frame has destructed it will return
  // null.
  virtual void StartDataRequest(const GURL& url,
                                const WebContents::Getter& wc_getter,
                                const GotDataCallback& callback) = 0;

  // The following methods are all called on the IO thread.

  // Return the mimetype that should be sent with this response, or empty
  // string to specify no mime type.
  virtual std::string GetMimeType(const std::string& path) = 0;

  // Returns the TaskRunner on which the delegate wishes to have
  // StartDataRequest called to handle the request for |path|. The default
  // implementation returns BrowserThread::UI. If the delegate does not care
  // which thread StartDataRequest is called on, this should return nullptr.
  // It may be beneficial to return nullptr for requests that are safe to handle
  // directly on the IO thread.  This can improve performance by satisfying such
  // requests more rapidly when there is a large amount of UI thread contention.
  // Or the delegate can return a specific thread's TaskRunner if they wish.
  virtual scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerForRequestPath(
      const std::string& path);

  // Returns true if the URLDataSource should replace an existing URLDataSource
  // with the same name that has already been registered. The default is true.
  //
  // WARNING: this is invoked on the IO thread.
  //
  // TODO: nuke this and convert all callers to not replace.
  virtual bool ShouldReplaceExistingSource();

  // Returns true if responses from this URLDataSource can be cached.
  virtual bool AllowCaching();

  // If you are overriding the following two methods, then you have a bug.
  // It is not acceptable to disable content-security-policy on chrome:// pages
  // to permit functionality excluded by CSP, such as inline script.
  // Instead, you must go back and change your WebUI page so that it is
  // compliant with the policy. This typically involves ensuring that all script
  // is delivered through the data manager backend. Do not disable CSP on your
  // page without first contacting the chrome security team.
  virtual bool ShouldAddContentSecurityPolicy();
  // For pre-existing code, enabling CSP with relaxed script-src attributes
  // may be marginally better than disabling CSP outright.
  // Do not override this method without first contacting the chrome security
  // team.
  // By default, "script-src chrome://resources 'self';" is added to CSP.
  // Override to change this.
  virtual std::string GetContentSecurityPolicyScriptSrc();

  // It is OK to override the following methods to a custom CSP directive
  // thereby slightly reducing the protection applied to the page.

  // By default, "object-src 'none';" is added to CSP. Override to change this.
  virtual std::string GetContentSecurityPolicyObjectSrc();
  // By default, "child-src 'none';" is added to CSP. Override to change this.
  virtual std::string GetContentSecurityPolicyChildSrc();
  // By default empty. Override to change this.
  virtual std::string GetContentSecurityPolicyStyleSrc();
  // By default empty. Override to change this.
  virtual std::string GetContentSecurityPolicyImgSrc();
  // By default empty. Override to change this.
  virtual std::string GetContentSecurityPolicyWorkerSrc();

  // By default, the "X-Frame-Options: DENY" header is sent. To stop this from
  // happening, return false. It is OK to return false as needed.
  virtual bool ShouldDenyXFrameOptions();

  // By default, only chrome: and devtools: requests are allowed.
  // Override in specific WebUI data sources to enable for additional schemes or
  // to implement fancier access control.  Typically used in concert with
  // ContentBrowserClient::GetAdditionalWebUISchemes() to permit additional
  // WebUI scheme support for an embedder.
  virtual bool ShouldServiceRequest(const GURL& url,
                                    ResourceContext* resource_context,
                                    int render_process_id);

  // By default, Content-Type: header is not sent along with the response.
  // To start sending mime type returned by GetMimeType in HTTP headers,
  // return true. It is useful when tunneling response served from this data
  // source programmatically. Or when AppCache is enabled for this source as it
  // is for devtools.
  virtual bool ShouldServeMimeTypeAsContentTypeHeader();

  // This method is called when the request contains "Origin:" header. The value
  // of the header is passed in |origin| parameter. If the returned value is not
  // empty, it is used as a value for "Access-Control-Allow-Origin:" response
  // header, otherwise the header is not set. This method should return either
  // |origin|, or "*", or "none", or empty string.
  // Default implementation returns an empty string.
  virtual std::string GetAccessControlAllowOriginForOrigin(
      const std::string& origin);

  // Called on the UI thread. For the shared resource, disables using Polymer 2
  // for requests from |host|, even if WebUIPolymer2 is enabled. Assumes this
  // method is only called from one host.
  // TODO (rbpotter): Remove this function when the OOBE page Polymer 2
  // migration is complete.
  virtual void DisablePolymer2ForHost(const std::string& host);

  // Whether i18n template expression replacement should be allowed in HTML
  // templates within JS files.
  virtual bool ShouldReplaceI18nInJS();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_H_
