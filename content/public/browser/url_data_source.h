// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_H_
#define CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/template_expressions.h"

class GURL;

namespace base {
class RefCountedMemory;
}

namespace content {
class BrowserContext;

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
  using GotDataCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;

  // Called by URLDataSource to request data at |url|. The child class should
  // run |callback| when the data is available or if the request could not be
  // satisfied. This can be called either in this callback or asynchronously
  // with the response. |wc_getter| can be called on the UI thread to return the
  // WebContents for this request if it originates from a render frame. If it
  // originated from a worker or if the frame has destructed it will return
  // null.
  virtual void StartDataRequest(const GURL& url,
                                const WebContents::Getter& wc_getter,
                                GotDataCallback callback) = 0;

  // Return the mimetype that should be sent with this response, or empty
  // string to specify no mime type.
  virtual std::string GetMimeType(const GURL& url) = 0;

  // Returns true if the URLDataSource should replace an existing URLDataSource
  // with the same name that has already been registered. The default is true.
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

  // By default, the following CSPs are added. Override to change this.
  //  - "child-src 'none';"
  //  - "object-src 'none';"
  //  - "frame ancestors: 'none'" is added to the CSP unless
  //    ShouldDenyXFrameOptions() returns false
  //  - "script-src chrome://resources 'self';"
  virtual std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive);

  // By default, neither of these headers are set. Override to change this.
  // TODO(crbug.com/40755309): Consider setting COOP:same-origin and
  // COEP:require-corp as the default instead.
  virtual std::string GetCrossOriginOpenerPolicy();
  virtual std::string GetCrossOriginEmbedderPolicy();
  virtual std::string GetCrossOriginResourcePolicy();

  // By default, the "X-Frame-Options: DENY" header is sent. To stop this from
  // happening, return false. It is OK to return false as needed.
  virtual bool ShouldDenyXFrameOptions();

  // By default, only chrome: and devtools: requests are allowed.
  // Override in specific WebUI data sources to enable for additional schemes or
  // to implement fancier access control.  Typically used in concert with
  // ContentBrowserClient::GetAdditionalWebUISchemes() to permit additional
  // WebUI scheme support for an embedder.
  virtual bool ShouldServiceRequest(const GURL& url,
                                    BrowserContext* browser_context,
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

  // Replacements for i18n or null if no replacements are desired.
  virtual const ui::TemplateReplacements* GetReplacements();

  // Whether i18n template expression replacement should be allowed in HTML
  // templates within JS files.
  virtual bool ShouldReplaceI18nInJS();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_H_
