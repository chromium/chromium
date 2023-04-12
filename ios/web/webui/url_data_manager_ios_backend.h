// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_URL_DATA_MANAGER_IOS_BACKEND_H_
#define IOS_WEB_WEBUI_URL_DATA_MANAGER_IOS_BACKEND_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/supports_user_data.h"
#include "ios/web/public/webui/url_data_source_ios.h"
#include "ios/web/webui/url_data_manager_ios.h"
#include "net/url_request/url_request_job_factory.h"

class GURL;

namespace base {
class RefCountedMemory;
}

namespace web {
class BrowserState;
class URLDataSourceIOSImpl;
class URLRequestChromeJob;

// URLDataManagerIOSBackend is used internally by URLDataManagerIOS on
// the IO thread. In most cases you can use the API in URLDataManagerIOS
// and ignore this class. URLDataManagerIOSBackend is owned by BrowserState.
class URLDataManagerIOSBackend : public base::SupportsUserData::Data {
 public:
  typedef int RequestID;

  URLDataManagerIOSBackend();

  URLDataManagerIOSBackend(const URLDataManagerIOSBackend&) = delete;
  URLDataManagerIOSBackend& operator=(const URLDataManagerIOSBackend&) = delete;

  ~URLDataManagerIOSBackend() override;

  // Invoked to create the protocol handler for chrome://. `is_incognito` should
  // be set for incognito browser states. Called on the UI thread.
  static std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
  CreateProtocolHandler(BrowserState* browser_state);

  // Adds a DataSource to the collection of data sources.
  void AddDataSource(URLDataSourceIOSImpl* source);

  // DataSource invokes this. Sends the data to the URLRequest.
  void DataAvailable(RequestID request_id, base::RefCountedMemory* bytes);

  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

 private:
  friend class URLRequestChromeJob;

  typedef std::map<std::string, scoped_refptr<URLDataSourceIOSImpl> >
      DataSourceMap;
  typedef std::map<RequestID, URLRequestChromeJob*> PendingRequestMap;

  // Called by the job when it's starting up.
  // Returns false if `url` is not a URL managed by this object.
  bool StartRequest(const net::URLRequest* request, URLRequestChromeJob* job);

  // Helper function to call StartDataRequest on `source`'s delegate. This is
  // needed because while we want to call URLDataSourceIOSDelegate's method, we
  // need to add a refcount on the source.
  static void CallStartRequest(scoped_refptr<URLDataSourceIOSImpl> source,
                               const std::string& path,
                               int request_id);

  // Remove a request from the list of pending requests.
  void RemoveRequest(URLRequestChromeJob* job);

  // Returns true if the job exists in `pending_requests_`. False otherwise.
  // Called by ~URLRequestChromeJob to verify that `pending_requests_` is kept
  // up to date.
  bool HasPendingJob(URLRequestChromeJob* job) const;

  // Look up the data source for the request. Returns the source if it is found,
  // else NULL.
  URLDataSourceIOSImpl* GetDataSourceFromURL(const GURL& url);

  // Custom sources of data, keyed by source path (e.g. "favicon").
  DataSourceMap data_sources_;

  // All pending URLRequestChromeJobs, keyed by ID of the request.
  // URLRequestChromeJob calls into this object when it's constructed and
  // destructed to ensure that the pointers in this map remain valid.
  PendingRequestMap pending_requests_;

  // The ID we'll use for the next request we receive.
  RequestID next_request_id_;
};

}  // namespace web

#endif  // IOS_WEB_WEBUI_URL_DATA_MANAGER_IOS_BACKEND_H_
