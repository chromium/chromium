// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NAVIGATION_BODY_LOADER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NAVIGATION_BODY_LOADER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/platform/web_url_error.h"

namespace blink {

// This class is used to load the body of main resource during navigation.
// It is provided by the client which commits a navigation.
// See WebNavigationParams for more details.
class BLINK_EXPORT WebNavigationBodyLoader {
 public:
  class Client {
   public:
    virtual ~Client() {}

    // Notifies about code cache if available. This method will
    // be called zero or one time.
    virtual void BodyCodeCacheReceived(mojo_base::BigBuffer data) = 0;

    // Notifies about more data available. Called multiple times.
    // If main resource is empty, can be not called at all.
    virtual void BodyDataReceived(base::span<const char> data) = 0;

    // Called once at the end. If something went wrong, |error| will be set.
    // No more calls are issued after this one.
    virtual void BodyLoadingFinished(
        base::TimeTicks completion_time,
        int64_t total_encoded_data_length,
        int64_t total_encoded_body_length,
        int64_t total_decoded_body_length,
        bool should_report_corb_blocking,
        const base::Optional<WebURLError>& error) = 0;
  };

  // It should be safe to destroy WebNavigationBodyLoader at any moment,
  // including from inside any client notification.
  virtual ~WebNavigationBodyLoader() {}

  // While deferred, no more data will be read and no notifications
  // will be called on the client. This method can be called
  // multiples times, at any moment.
  virtual void SetDefersLoading(bool defers) = 0;

  // Starts loading the body. Client must be non-null, and will receive
  // the body, code cache and final result.
  virtual void StartLoadingBody(Client*, bool use_isolated_code_cache) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NAVIGATION_BODY_LOADER_H_
