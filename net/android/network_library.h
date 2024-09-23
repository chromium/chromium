// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_LIBRARY_H_
#define NET_ANDROID_NETWORK_LIBRARY_H_

#include <android/multinetwork.h>
#include <jni.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "net/android/cert_verify_result_android.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mime_util.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/socket/socket_descriptor.h"

namespace net::android {

// Get the list of user-added roots from Android.
// |roots| is a list of DER-encoded user-added roots from Android.
std::vector<std::string> GetUserAddedRoots();

// |cert_chain| is DER encoded chain of certificates, with the server's own
// certificate listed first.
// |auth_type| is as per the Java X509Certificate.checkServerTrusted method.
void VerifyX509CertChain(const std::vector<std::string>& cert_chain,
                         std::string_view auth_type,
                         std::string_view host,
                         CertVerifyStatusAndroid* status,
                         bool* is_issued_by_known_root,
                         std::vector<std::string>* verified_chain);

// Adds a certificate as a root trust certificate to the trust manager.
// |cert| is DER encoded certificate, |len| is its length in bytes.
void AddTestRootCertificate(base::span<const uint8_t> cert);

// Removes all root certificates added by |AddTestRootCertificate| calls.
void ClearTestRootCertificates();

// Returns true if cleartext traffic to |host| is allowed by the app. Always
// true on L and older.
bool IsCleartextPermitted(std::string_view host);

// Returns true if it can determine that only loopback addresses are configured.
// i.e. if only 127.0.0.1 and ::1 are routable.
// Also returns false if it cannot determine this.
bool HaveOnlyLoopbackAddresses();

// Get the mime type (if any) that is associated with the file extension.
// Returns true if a corresponding mime type exists.
bool GetMimeTypeFromExtension(std::string_view extension, std::string* result);

// Returns MCC+MNC (mobile country code + mobile network code) as
// the numeric name of the current registered operator. This function
// potentially blocks the thread, so use with care.
NET_EXPORT std::string GetTelephonyNetworkOperator();

// Returns true if the device is roaming on the currently active network. When
// true, it suggests that use of data may incur extra costs.
NET_EXPORT bool GetIsRoaming();

// Returns true if the system's captive portal probe was blocked for the current
// default data network. The method will return false if the captive portal
// probe was not blocked, the login process to the captive portal has been
// successfully completed, or if the captive portal status can't be determined.
// Requires ACCESS_NETWORK_STATE permission. Only available on Android
// Marshmallow and later versions. Returns false on earlier versions.
NET_EXPORT bool GetIsCaptivePortal();

// Gets the SSID of the currently associated WiFi access point if there is one,
// and it is available. SSID may not be available if the app does not have
// permissions to access it. On Android M+, the app accessing SSID needs to have
// ACCESS_COARSE_LOCATION or ACCESS_FINE_LOCATION. If there is no WiFi access
// point or its SSID is unavailable, an empty string is returned.
NET_EXPORT_PRIVATE std::string GetWifiSSID();

// Call WifiManager.setWifiEnabled.
NET_EXPORT_PRIVATE void SetWifiEnabledForTesting(bool enabled);

// Returns the signal strength level (between 0 and 4, both inclusive) of the
// currently registered Wifi connection. If the value is unavailable, an
// empty value is returned.
NET_EXPORT_PRIVATE std::optional<int32_t> GetWifiSignalLevel();

// Gets the DNS servers for the current default network and puts them in
// `dns_servers`. Sets `dns_over_tls_active` and `dns_over_tls_hostname` based
// on the private DNS settings. `dns_over_tls_hostname` will only be non-empty
// if `dns_over_tls_active` is true.
// Only callable on Marshmallow and newer releases.
// Returns false when a valid server config could not be read.
NET_EXPORT_PRIVATE bool GetCurrentDnsServers(
    std::vector<IPEndPoint>* dns_servers,
    bool* dns_over_tls_active,
    std::string* dns_over_tls_hostname,
    std::vector<std::string>* search_suffixes);
using DnsServerGetter =
    base::RepeatingCallback<bool(std::vector<IPEndPoint>* dns_servers,
                                 bool* dns_over_tls_active,
                                 std::string* dns_over_tls_hostname,
                                 std::vector<std::string>* search_suffixes)>;

// Works as GetCurrentDnsServers but gets info specific to `network` instead
// of the current default network.
// Only callable on Pie and newer releases.
// Returns false when a valid server config could not be read.
NET_EXPORT_PRIVATE bool GetDnsServersForNetwork(
    std::vector<IPEndPoint>* dns_servers,
    bool* dns_over_tls_active,
    std::string* dns_over_tls_hostname,
    std::vector<std::string>* search_suffixes,
    handles::NetworkHandle network);

// Reports to the framework that the current default network appears to have
// connectivity issues. This may serve as a signal for the OS to consider
// switching to a different default network. Returns |true| if successfully
// reported to the OS, or |false| if not supported.
NET_EXPORT_PRIVATE bool ReportBadDefaultNetwork();

// Apply TrafficStats tag |tag| and UID |uid| to |socket|. Future network
// traffic used by |socket| will be attributed to |uid| and |tag|.
NET_EXPORT_PRIVATE void TagSocket(SocketDescriptor socket,
                                  uid_t uid,
                                  int32_t tag);

// Binds this socket to `network`. All data traffic on the socket will be sent
// and received via `network`. This call will fail if `network` has
// disconnected. Communication using this socket will fail if `network`
// disconnects.
// Returns a net error code.
NET_EXPORT_PRIVATE int BindToNetwork(SocketDescriptor socket,
                                     handles::NetworkHandle network);

// Perform hostname resolution via the DNS servers associated with `network`.
// All arguments are used identically as those passed to Android NDK API
// android_getaddrinfofornetwork:
// https://developer.android.com/ndk/reference/group/networking#group___networking_1ga0ae9e15612e6411855e295476a98ceee
NET_EXPORT_PRIVATE int GetAddrInfoForNetwork(handles::NetworkHandle network,
                                             const char* node,
                                             const char* service,
                                             const struct addrinfo* hints,
                                             struct addrinfo** res);

}  // namespace net::android

#endif  // NET_ANDROID_NETWORK_LIBRARY_H_
