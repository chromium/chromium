// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEBUI_URL_DATA_SOURCE_IOS_H_
#define IOS_WEB_PUBLIC_WEBUI_URL_DATA_SOURCE_IOS_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"

class GURL;

namespace base {
class RefCountedMemory;
}

namespace web {
class BrowserState;

// A URLDataSourceIOS is an object that can answer requests for WebUI data
// asynchronously. An implementation of URLDataSourceIOS should handle calls to
// StartDataRequest() by starting its (implementation-specific) asynchronous
// request for the data, then running the callback given in that method to
// notify.
class URLDataSourceIOS {
 public:
  // Adds a URL data source to `browser_state`.
  static void Add(BrowserState* browser_state, URLDataSourceIOS* source);

  virtual ~URLDataSourceIOS() {}

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source. For
  // sources where a scheme is used instead of the hostname as the unique
  // identifier, the suffix "://" must be added to the return value, eg. for a
  // URLDataSourceIOS which would display resources with URLs on the form
  // your-scheme://anything , GetSource() must return "your-scheme://".
  virtual std::string GetSource() const = 0;

  // Used by StartDataRequest so that the child class can return the data when
  // it's available.
  typedef base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>
      GotDataCallback;

  // Called by URLDataSourceIOS to request data at `path`. The string parameter
  // is the path of the request. The child class should run `callback` when the
  // data is available or if the request could not be satisfied. This can be
  // called either in this callback or asynchronously with the response.
  virtual void StartDataRequest(const std::string& path,
                                GotDataCallback callback) = 0;

  // Return the mimetype that should be sent with this response, or empty
  // string to specify no mime type.
  virtual std::string GetMimeType(const std::string& path) const = 0;

  // The following methods are all called on the IO thread.

  // Returns true if the URLDataSourceIOS should replace an existing
  // URLDataSourceIOS with the same name that has already been registered. The
  // default is true.
  //
  // WARNING: this is invoked on the IO thread.
  //
  // TODO: nuke this and convert all callers to not replace.
  virtual bool ShouldReplaceExistingSource() const;

  // Returns true if i18n replacemenents should be performed in JS files. Needed
  // by UIs that use Web Components.
  virtual bool ShouldReplaceI18nInJS() const;

  // Returns true if responses from this URLDataSourceIOS can be cached.
  virtual bool AllowCaching() const;

  // By default, "object-src 'none';" is added to CSP. Override to change this.
  virtual std::string GetContentSecurityPolicyObjectSrc() const;

  // By default, the "X-Frame-Options: DENY" header is sent. To stop this from
  // happening, return false. It is OK to return false as needed.
  virtual bool ShouldDenyXFrameOptions() const;

  // By default, only chrome: requests are allowed.  Override in specific WebUI
  // data sources to enable for additional schemes or to implement fancier
  // access control.  Typically used in concert with
  // WebClient::GetAdditionalWebUISchemes() to permit additional WebUI scheme
  // support for an embedder.
  virtual bool ShouldServiceRequest(const GURL& url) const;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEBUI_URL_DATA_SOURCE_IOS_H_
