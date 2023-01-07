// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_URL_FETCHER_BLOCK_ADAPTER_H_
#define IOS_WEB_WEBUI_URL_FETCHER_BLOCK_ADAPTER_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace web {

// Class for use of URLLoader from Objective-C with a completion handler block.
class URLFetcherBlockAdapter;
// Block type for URLFetcherBlockAdapter callbacks.
typedef void (^URLFetcherBlockAdapterCompletion)(NSData*,
                                                 URLFetcherBlockAdapter*);

// Class to manage retrieval of WebUI resources.
class URLFetcherBlockAdapter {
 public:
  // Creates URLFetcherBlockAdapter for resource at `url` with
  // `request_context`.
  // `completion_handler` is called with results of the fetch.
  URLFetcherBlockAdapter(
      const GURL& url,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      web::URLFetcherBlockAdapterCompletion completion_handler);
  virtual ~URLFetcherBlockAdapter();
  // Starts the fetch.
  virtual void Start();

  GURL getUrl() { return url_; }

 protected:
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

 private:
  // The URL to fetch.
  const GURL url_;
  // The URL loader factory.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Callback for resource load.
  __strong web::URLFetcherBlockAdapterCompletion completion_handler_;
  // URLLoader for retrieving data from net stack.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  // namespace web

#endif  // IOS_WEB_WEBUI_URL_FETCHER_BLOCK_ADAPTER_H_
