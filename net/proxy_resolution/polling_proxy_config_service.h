// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_POLLING_PROXY_CONFIG_SERVICE_H_
#define NET_PROXY_RESOLUTION_POLLING_PROXY_CONFIG_SERVICE_H_

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class ProxyConfigWithAnnotation;

// PollingProxyConfigService is a base class for creating ProxyConfigService
// implementations that use polling to notice when settings have change.
//
// It runs code to get the current proxy settings on a background worker
// thread, and notifies registered observers when the value changes.
class NET_EXPORT_PRIVATE PollingProxyConfigService : public ProxyConfigService {
 public:
  PollingProxyConfigService(const PollingProxyConfigService&) = delete;
  PollingProxyConfigService& operator=(const PollingProxyConfigService&) =
      delete;

  // ProxyConfigService implementation:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  ConfigAvailability GetLatestProxyConfig(
      ProxyConfigWithAnnotation* config) override;
  void OnLazyPoll() override;
  bool UsesPolling() override;

 protected:
  // Function for retrieving the current proxy configuration.
  // Implementors must be threadsafe as the function will be invoked from
  // worker threads.
  using GetConfigFunction =
      base::RepeatingCallback<void(const NetworkTrafficAnnotationTag,
                                   ProxyConfigWithAnnotation*)>;

  // Creates a polling-based ProxyConfigService which will test for new
  // settings at most every |poll_interval| time by calling |get_config_func|
  // on a worker thread.
  PollingProxyConfigService(
      base::TimeDelta poll_interval,
      GetConfigFunction get_config_func,
      const NetworkTrafficAnnotationTag& traffic_annotation);

  ~PollingProxyConfigService() override;

  // Polls for changes by posting a task to the worker pool.
  void CheckForChangesNow();

 private:
  class Core;
  scoped_refptr<Core> core_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_POLLING_PROXY_CONFIG_SERVICE_H_
