// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_GETTER_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_GETTER_H_

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "build/build_config.h"
#include "net/base/net_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

#if BUILDFLAG(IS_IOS)
namespace web {
class NetworkContextOwner;
}
#endif  // BUILDFLAG(IS_IOS)

namespace net {
class URLRequestContext;
class URLRequestContextGetterObserver;

struct URLRequestContextGetterTraits;

// Interface for retrieving an URLRequestContext.
class NET_EXPORT URLRequestContextGetter
    : public base::RefCountedThreadSafe<URLRequestContextGetter,
                                        URLRequestContextGetterTraits> {
 public:
  URLRequestContextGetter(const URLRequestContextGetter&) = delete;
  URLRequestContextGetter& operator=(const URLRequestContextGetter&) = delete;

  // Returns the URLRequestContextGetter's URLRequestContext. Must only be
  // called on the network task runner. Once NotifyContextShuttingDown() is
  // invoked, must always return nullptr. Does not transfer ownership of
  // the URLRequestContext.
  virtual URLRequestContext* GetURLRequestContext() = 0;

  // Returns a SingleThreadTaskRunner corresponding to the thread on
  // which the network IO happens (the thread on which the returned
  // URLRequestContext may be used).
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<URLRequestContextGetter,
                                          URLRequestContextGetterTraits>;
  friend class base::DeleteHelper<URLRequestContextGetter>;
  friend struct URLRequestContextGetterTraits;

  URLRequestContextGetter();
  virtual ~URLRequestContextGetter();

  // Called to indicate the URLRequestContext is about to be shutdown, so
  // observers need to abort any URLRequests they own.  The implementation of
  // this class is responsible for making sure this gets called.
  //
  // Must be called once and only once *before* context tear down begins, so any
  // pending requests can be torn down safely. Right before calling this method,
  // subclasses must ensure GetURLRequestContext returns nullptr, to protect
  // against reentrancy.
  void NotifyContextShuttingDown();

 private:
  // AddObserver and RemoveObserver are deprecated. Friend URLFetcherCore and
  // web::NetworkContextOwner to restrict visibility.
  friend class URLFetcherCore;

#if BUILDFLAG(IS_IOS)
  friend class web::NetworkContextOwner;
#endif  // BUILDFLAG(IS_IOS)

  // Adds / removes an observer to watch for shutdown of |this|'s context. Must
  // only be called on network thread. May not be called once
  // GetURLRequestContext() starts returning nullptr.
  void AddObserver(URLRequestContextGetterObserver* observer);
  void RemoveObserver(URLRequestContextGetterObserver* observer);

  // OnDestruct is used to ensure deletion on the thread on which the request
  // IO happens.
  void OnDestruct() const;

  base::ObserverList<URLRequestContextGetterObserver>::Unchecked observer_list_;
};

struct URLRequestContextGetterTraits {
  static void Destruct(const URLRequestContextGetter* context_getter) {
    context_getter->OnDestruct();
  }
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_GETTER_H_
