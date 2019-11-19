// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_IOS_CHROME_URL_REQUEST_CONTEXT_GETTER_H_
#define IOS_CHROME_BROWSER_NET_IOS_CHROME_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "ios/chrome/browser/net/net_types.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

class ChromeBrowserStateIOData;
class IOSChromeURLRequestContextFactory;

// A net::URLRequestContextGetter subclass used by the browser. This returns a
// subclass of net::URLRequestContext which can be used to store extra
// information about requests.
//
// Most methods are expected to be called on the UI thread, except for
// the destructor and GetURLRequestContext().
class IOSChromeURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  // Constructs a ChromeURLRequestContextGetter that will use |factory| to
  // create the URLRequestContext.
  explicit IOSChromeURLRequestContextGetter(
      std::unique_ptr<IOSChromeURLRequestContextFactory> factory);

  // Note that GetURLRequestContext() can only be called from the IO
  // thread (it will assert otherwise).
  // GetIOTaskRunner however can be called from any thread.
  //
  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  // Create an instance for use with an 'original' (non-OTR) profile. This is
  // expected to get called on the UI thread.
  static IOSChromeURLRequestContextGetter* Create(
      const ChromeBrowserStateIOData* io_data,
      ProtocolHandlerMap* protocol_handlers);

  // Discard reference to URLRequestContext and inform observers of shutdown.
  // Must be called before destruction. May only be called on IO thread.
  void NotifyContextShuttingDown();

 private:
  ~IOSChromeURLRequestContextGetter() override;

  // Deferred logic for creating a URLRequestContext.
  // Access only from the IO thread.
  std::unique_ptr<IOSChromeURLRequestContextFactory> factory_;

  // NULL before initialization and after invalidation.
  // Otherwise, it is the URLRequestContext instance that
  // was lazily created by GetURLRequestContext().
  // Access only from the IO thread.
  net::URLRequestContext* url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeURLRequestContextGetter);
};

#endif  // IOS_CHROME_BROWSER_NET_IOS_CHROME_URL_REQUEST_CONTEXT_GETTER_H_
