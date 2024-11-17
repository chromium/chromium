// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_BACKEND_H_
#define CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_BACKEND_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/values.h"
#include "content/browser/webui/url_data_manager.h"
#include "net/http/http_response_headers.h"

class GURL;

namespace base {
class RefCountedMemory;
}

namespace content {

class BrowserContext;
class URLDataSourceImpl;

// URLDataManagerBackend is used internally by ChromeURLDataManager on the UI
// thread. In most cases you can use the API in ChromeURLDataManager and ignore
// this class. URLDataManagerBackend is owned by BrowserContext.
class CONTENT_EXPORT URLDataManagerBackend
    : public base::SupportsUserData::Data {
 public:
  typedef int RequestID;
  using DataSourceMap = std::map<std::string, scoped_refptr<URLDataSourceImpl>>;

  URLDataManagerBackend();

  URLDataManagerBackend(const URLDataManagerBackend&) = delete;
  URLDataManagerBackend& operator=(const URLDataManagerBackend&) = delete;

  ~URLDataManagerBackend() override;

  static URLDataManagerBackend* GetForBrowserContext(BrowserContext* context);

  // Adds a DataSource to the collection of data sources.
  void AddDataSource(URLDataSourceImpl* source);

  void UpdateWebUIDataSource(const std::string& source_name,
                             const base::Value::Dict& update);

  // DataSource invokes this. Sends the data to the URLRequest. |bytes| may be
  // null, which signals an error handling the request.
  void DataAvailable(RequestID request_id, base::RefCountedMemory* bytes);

  // Look up the data source for the request. Returns the source if it is found,
  // else NULL.
  URLDataSourceImpl* GetDataSourceFromURL(const GURL& url);

  const DataSourceMap& data_sources() const { return data_sources_; }

  // Creates and sets the response headers for the given request.
  static scoped_refptr<net::HttpResponseHeaders> GetHeaders(
      URLDataSourceImpl* source,
      const GURL& url,
      const std::string& origin);

  // Returns whether |url| passes some sanity checks and is a valid GURL.
  static bool CheckURLIsValid(const GURL& url);

  // Check if the given integer is a valid network error code.
  static bool IsValidNetworkErrorCode(int error_code);

  // Returns the schemes that are used by WebUI (i.e. the set from content and
  // its embedder).
  static std::vector<std::string> GetWebUISchemes();

  // When this is true, GetWebUISchemes() bypasses the caching of its result
  // and always recomputes the list of WebUI schemes.  This should be used in
  // tests that inject custom WebUI schemes, which may otherwise not be seen.
  static void SetDisallowWebUISchemeCachingForTesting(bool disallow_caching);

 private:
  // Custom sources of data, keyed by source path (e.g. "favicon").
  DataSourceMap data_sources_;

  // Vends weak pointers to URLDataSources, allowing them to continue referring
  // to the backend that originally owned them, even if they've been replaced
  // and detached from the backend. This allows outstanding asynchronous queries
  // to be served and routed to the backend to which they were original issued.
  base::WeakPtrFactory<URLDataManagerBackend> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_BACKEND_H_
