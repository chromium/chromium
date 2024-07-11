// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_IO_THREAD_SHARED_URL_LOADER_FACTORY_OWNER_H_
#define CONTENT_TEST_IO_THREAD_SHARED_URL_LOADER_FACTORY_OWNER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

class GURL;

namespace content {

// Class to own the SharedURLLoaderFactory for use on the IO thread.
//
// Created on the UI thread and destroyed on the IO thread.
class IOThreadSharedURLLoaderFactoryOwner {
 public:
  using IOThreadSharedURLLoaderFactoryOwnerPtr =
      std::unique_ptr<IOThreadSharedURLLoaderFactoryOwner,
                      BrowserThread::DeleteOnIOThread>;

  // To be called on the UI thread. Will block and finish initialization on the
  // IO thread.
  static IOThreadSharedURLLoaderFactoryOwnerPtr Create(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> info);

  IOThreadSharedURLLoaderFactoryOwner(
      const IOThreadSharedURLLoaderFactoryOwner&) = delete;
  IOThreadSharedURLLoaderFactoryOwner& operator=(
      const IOThreadSharedURLLoaderFactoryOwner&) = delete;

  // Load the given |url| with the internal |shared_url_loader_factory_| on IO
  // thread and return the |net::Error| code.
  int LoadBasicRequestOnIOThread(const GURL& url);

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<IOThreadSharedURLLoaderFactoryOwner>;

  explicit IOThreadSharedURLLoaderFactoryOwner(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> info);
  ~IOThreadSharedURLLoaderFactoryOwner();

  // Lives on the IO thread.
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

}  // namespace content

#endif  // CONTENT_TEST_IO_THREAD_SHARED_URL_LOADER_FACTORY_OWNER_H_
