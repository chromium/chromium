// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CONFIG_SERVICE_H_
#define NET_SSL_SSL_CONFIG_SERVICE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "net/base/net_export.h"
#include "net/ssl/ssl_config.h"

namespace net {

struct NET_EXPORT SSLContextConfig {
  SSLContextConfig();
  SSLContextConfig(const SSLContextConfig&);
  SSLContextConfig(SSLContextConfig&&);
  ~SSLContextConfig();
  SSLContextConfig& operator=(const SSLContextConfig&);
  SSLContextConfig& operator=(SSLContextConfig&&);

  // The minimum and maximum protocol versions that are enabled.
  // (Use the SSL_PROTOCOL_VERSION_xxx enumerators defined in ssl_config.h.)
  // SSL 2.0 and SSL 3.0 are not supported. If version_max < version_min, it
  // means no protocol versions are enabled.
  uint16_t version_min = kDefaultSSLVersionMin;
  uint16_t version_max = kDefaultSSLVersionMax;

  // Presorted list of cipher suites which should be explicitly prevented from
  // being used in addition to those disabled by the net built-in policy.
  //
  // Though cipher suites are sent in TLS as "uint8_t CipherSuite[2]", in
  // big-endian form, they should be declared in host byte order, with the
  // first uint8_t occupying the most significant byte.
  // Ex: To disable TLS_RSA_WITH_RC4_128_MD5, specify 0x0004, while to
  // disable TLS_ECDH_ECDSA_WITH_RC4_128_SHA, specify 0xC002.
  std::vector<uint16_t> disabled_cipher_suites;

  // If true, enables TLS 1.3 downgrade hardening for connections using
  // local trust anchors. (Hardening for known roots is always enabled.)
  bool tls13_hardening_for_local_anchors_enabled = false;
};

// The interface for retrieving global SSL configuration.  This interface
// does not cover setting the SSL configuration, as on some systems, the
// SSLConfigService objects may not have direct access to the configuration, or
// live longer than the configuration preferences.
class NET_EXPORT SSLConfigService {
 public:
  // Observer is notified when SSL config settings have changed.
  class NET_EXPORT Observer {
   public:
    // Notify observers if SSL settings have changed.
    virtual void OnSSLContextConfigChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  SSLConfigService();
  virtual ~SSLConfigService();

  // May not be thread-safe, should only be called on the IO thread.
  virtual SSLContextConfig GetSSLContextConfig() = 0;

  // Returns true if connections to |hostname| can reuse, or are permitted to
  // reuse, connections on which a client cert has been negotiated. Note that
  // this must return true for both hostnames being pooled - that is to say this
  // function must return true for both the hostname of the existing connection
  // and the potential hostname to pool before allowing the connection to be
  // reused.
  //
  // NOTE: Pooling connections with ambient authority can create security issues
  // with that ambient authority and privacy issues in that embedders (and
  // users) may not have been consulted to send a client cert to |hostname|.
  // Implementations of this method should only return true if they have
  // received affirmative consent (e.g. through preferences or Enterprise
  // policy).
  //
  // NOTE: For Web Platform clients, this violates the Fetch Standard's policies
  // around connection pools: https://fetch.spec.whatwg.org/#connections.
  // Implementations that return true should take steps to limit the Web
  // Platform visibility of this, such as only allowing it to be used for
  // Enterprise or internal configurations.
  //
  // DEPRECATED: For the reasons above, this method is temporary and will be
  // removed in a future release. Please leave a comment on
  // https://crbug.com/855690 if you believe this is needed.
  virtual bool CanShareConnectionWithClientCerts(
      const std::string& hostname) const = 0;

  // Add an observer of this service.
  void AddObserver(Observer* observer);

  // Remove an observer of this service.
  void RemoveObserver(Observer* observer);

  // Calls the OnSSLContextConfigChanged method of registered observers. Should
  // only be called on the IO thread.
  void NotifySSLContextConfigChange();

  // Checks if the config-service managed fields in two SSLContextConfigs are
  // the same.
  static bool SSLContextConfigsAreEqualForTesting(
      const SSLContextConfig& config1,
      const SSLContextConfig& config2);

 protected:
  // Process before/after config update. If |force_notification| is true,
  // NotifySSLContextConfigChange will be called regardless of whether
  // |orig_config| and |new_config| are equal.
  void ProcessConfigUpdate(const SSLContextConfig& orig_config,
                           const SSLContextConfig& new_config,
                           bool force_notification);

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace net

#endif  // NET_SSL_SSL_CONFIG_SERVICE_H_
