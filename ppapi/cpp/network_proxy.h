// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_NETWORK_PROXY_H_
#define PPAPI_CPP_NETWORK_PROXY_H_

#include <stdint.h>

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"

namespace pp {

/// This class provides a way to determine the appropriate proxy settings for
/// for a given URL.
///
/// Permissions: Apps permission <code>socket</code> with subrule
/// <code>resolve-proxy</code> is required for using this API.
/// For more details about network communication permissions, please see:
/// http://developer.chrome.com/apps/app_network.html
class NetworkProxy {
 public:
  /// Returns true if the browser supports this API, false otherwise.
  static bool IsAvailable();

  /// Retrieves the proxy that will be used for the given URL. The result will
  /// be a string in PAC format. For more details about PAC format, please see
  /// http://en.wikipedia.org/wiki/Proxy_auto-config
  ///
  /// @param[in] instance An <code>InstanceHandle</code> identifying one
  /// instance of a module.
  ///
  /// @param[in] url A string <code>Var</code> containing a URL.
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code> to be
  /// called upon completion. It will be passed a string <code>Var</code>
  /// containing the appropriate PAC string for <code>url</code>.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  static int32_t GetProxyForURL(
      const InstanceHandle& instance,
      const Var& url,
      const pp::CompletionCallbackWithOutput<Var>& callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_NETWORK_PROXY_H_
