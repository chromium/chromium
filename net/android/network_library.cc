// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_library.h"

#include <dlfcn.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/native_library.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "net/base/net_errors.h"
#include "net/dns/public/dns_protocol.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "net/net_jni_headers/AndroidNetworkLibrary_jni.h"
#include "net/net_jni_headers/DnsStatus_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaArrayOfByteArrayToStringVector;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfByteArray;
using base::android::ToJavaByteArray;

namespace net::android {

std::vector<std::string> GetUserAddedRoots() {
  std::vector<std::string> roots;
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> roots_byte_array =
      Java_AndroidNetworkLibrary_getUserAddedRoots(env);
  JavaArrayOfByteArrayToStringVector(env, roots_byte_array, &roots);
  return roots;
}

void VerifyX509CertChain(const std::vector<std::string>& cert_chain,
                         std::string_view auth_type,
                         std::string_view host,
                         CertVerifyStatusAndroid* status,
                         bool* is_issued_by_known_root,
                         std::vector<std::string>* verified_chain) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> chain_byte_array =
      ToJavaArrayOfByteArray(env, cert_chain);
  DCHECK(!chain_byte_array.is_null());

  ScopedJavaLocalRef<jstring> auth_string =
      ConvertUTF8ToJavaString(env, auth_type);
  DCHECK(!auth_string.is_null());

  ScopedJavaLocalRef<jstring> host_string =
      ConvertUTF8ToJavaString(env, host);
  DCHECK(!host_string.is_null());

  ScopedJavaLocalRef<jobject> result =
      Java_AndroidNetworkLibrary_verifyServerCertificates(
          env, chain_byte_array, auth_string, host_string);

  ExtractCertVerifyResult(result, status, is_issued_by_known_root,
                          verified_chain);
}

void AddTestRootCertificate(base::span<const uint8_t> cert) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> cert_array = ToJavaByteArray(env, cert);
  DCHECK(!cert_array.is_null());
  Java_AndroidNetworkLibrary_addTestRootCertificate(env, cert_array);
}

void ClearTestRootCertificates() {
  JNIEnv* env = AttachCurrentThread();
  Java_AndroidNetworkLibrary_clearTestRootCertificates(env);
}

bool IsCleartextPermitted(std::string_view host) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> host_string = ConvertUTF8ToJavaString(env, host);
  return Java_AndroidNetworkLibrary_isCleartextPermitted(env, host_string);
}

bool HaveOnlyLoopbackAddresses() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  JNIEnv* env = AttachCurrentThread();
  return Java_AndroidNetworkLibrary_haveOnlyLoopbackAddresses(env);
}

bool GetMimeTypeFromExtension(std::string_view extension, std::string* result) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> extension_string =
      ConvertUTF8ToJavaString(env, extension);
  ScopedJavaLocalRef<jstring> ret =
      Java_AndroidNetworkLibrary_getMimeTypeFromExtension(env,
                                                          extension_string);

  if (!ret.obj())
    return false;
  *result = ConvertJavaStringToUTF8(ret);
  return true;
}

std::string GetTelephonyNetworkOperator() {
  return base::android::ConvertJavaStringToUTF8(
      Java_AndroidNetworkLibrary_getNetworkOperator(
          base::android::AttachCurrentThread()));
}

bool GetIsRoaming() {
  return Java_AndroidNetworkLibrary_getIsRoaming(
      base::android::AttachCurrentThread());
}

bool GetIsCaptivePortal() {
  return Java_AndroidNetworkLibrary_getIsCaptivePortal(
      base::android::AttachCurrentThread());
}

std::string GetWifiSSID() {
  return base::android::ConvertJavaStringToUTF8(
      Java_AndroidNetworkLibrary_getWifiSSID(
          base::android::AttachCurrentThread()));
}

void SetWifiEnabledForTesting(bool enabled) {
  Java_AndroidNetworkLibrary_setWifiEnabledForTesting(
      base::android::AttachCurrentThread(), enabled);
}

std::optional<int32_t> GetWifiSignalLevel() {
  const int count_buckets = 5;
  int signal_strength = Java_AndroidNetworkLibrary_getWifiSignalLevel(
      base::android::AttachCurrentThread(), count_buckets);
  if (signal_strength < 0)
    return std::nullopt;
  DCHECK_LE(0, signal_strength);
  DCHECK_GE(count_buckets - 1, signal_strength);

  return signal_strength;
}

namespace {

bool GetDnsServersInternal(JNIEnv* env,
                           const base::android::JavaRef<jobject>& dns_status,
                           std::vector<IPEndPoint>* dns_servers,
                           bool* dns_over_tls_active,
                           std::string* dns_over_tls_hostname,
                           std::vector<std::string>* search_suffixes) {
  // Parse the DNS servers.
  std::vector<std::vector<uint8_t>> dns_servers_data;
  base::android::JavaArrayOfByteArrayToBytesVector(
      env, Java_DnsStatus_getDnsServers(env, dns_status), &dns_servers_data);
  for (const std::vector<uint8_t>& dns_address_data : dns_servers_data) {
    IPAddress dns_address(dns_address_data);
    IPEndPoint dns_server(dns_address, dns_protocol::kDefaultPort);
    dns_servers->push_back(dns_server);
  }

  *dns_over_tls_active = Java_DnsStatus_getPrivateDnsActive(env, dns_status);
  *dns_over_tls_hostname = base::android::ConvertJavaStringToUTF8(
      Java_DnsStatus_getPrivateDnsServerName(env, dns_status));

  std::string search_suffixes_str = base::android::ConvertJavaStringToUTF8(
      Java_DnsStatus_getSearchDomains(env, dns_status));
  *search_suffixes =
      base::SplitString(search_suffixes_str, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  return !dns_servers->empty();
}

}  // namespace

bool GetCurrentDnsServers(std::vector<IPEndPoint>* dns_servers,
                          bool* dns_over_tls_active,
                          std::string* dns_over_tls_hostname,
                          std::vector<std::string>* search_suffixes) {
  DCHECK_GE(base::android::BuildInfo::GetInstance()->sdk_int(),
            base::android::SDK_VERSION_MARSHMALLOW);

  JNIEnv* env = AttachCurrentThread();
  // Get the DNS status for the current default network.
  ScopedJavaLocalRef<jobject> result =
      Java_AndroidNetworkLibrary_getCurrentDnsStatus(env);
  if (result.is_null())
    return false;
  return GetDnsServersInternal(env, result, dns_servers, dns_over_tls_active,
                               dns_over_tls_hostname, search_suffixes);
}

bool GetDnsServersForNetwork(std::vector<IPEndPoint>* dns_servers,
                             bool* dns_over_tls_active,
                             std::string* dns_over_tls_hostname,
                             std::vector<std::string>* search_suffixes,
                             handles::NetworkHandle network) {
  DCHECK_GE(base::android::BuildInfo::GetInstance()->sdk_int(),
            base::android::SDK_VERSION_P);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_AndroidNetworkLibrary_getDnsStatusForNetwork(env, network);
  if (result.is_null())
    return false;
  return GetDnsServersInternal(env, result, dns_servers, dns_over_tls_active,
                               dns_over_tls_hostname, search_suffixes);
}

bool ReportBadDefaultNetwork() {
  return Java_AndroidNetworkLibrary_reportBadDefaultNetwork(
      AttachCurrentThread());
}

void TagSocket(SocketDescriptor socket, uid_t uid, int32_t tag) {
  Java_AndroidNetworkLibrary_tagSocket(AttachCurrentThread(), socket, uid, tag);
}

namespace {

using LollipopSetNetworkForSocket = int (*)(unsigned net_id, int socket_fd);
using MarshmallowSetNetworkForSocket = int (*)(int64_t net_id, int socket_fd);

MarshmallowSetNetworkForSocket GetMarshmallowSetNetworkForSocket() {
  // On Android M and newer releases use supported NDK API.
  base::FilePath file(base::GetNativeLibraryName("android"));
  // See declaration of android_setsocknetwork() here:
  // http://androidxref.com/6.0.0_r1/xref/development/ndk/platforms/android-M/include/android/multinetwork.h#65
  // Function cannot be called directly as it will cause app to fail to load on
  // pre-marshmallow devices.
  void* dl = dlopen(file.value().c_str(), RTLD_NOW);
  return reinterpret_cast<MarshmallowSetNetworkForSocket>(
      dlsym(dl, "android_setsocknetwork"));
}

LollipopSetNetworkForSocket GetLollipopSetNetworkForSocket() {
  // On Android L use setNetworkForSocket from libnetd_client.so. Android's netd
  // client library should always be loaded in our address space as it shims
  // socket().
  base::FilePath file(base::GetNativeLibraryName("netd_client"));
  // Use RTLD_NOW to match Android's prior loading of the library:
  // http://androidxref.com/6.0.0_r5/xref/bionic/libc/bionic/NetdClient.cpp#37
  // Use RTLD_NOLOAD to assert that the library is already loaded and avoid
  // doing any disk IO.
  void* dl = dlopen(file.value().c_str(), RTLD_NOW | RTLD_NOLOAD);
  return reinterpret_cast<LollipopSetNetworkForSocket>(
      dlsym(dl, "setNetworkForSocket"));
}

}  // namespace

int BindToNetwork(SocketDescriptor socket, handles::NetworkHandle network) {
  DCHECK_NE(socket, kInvalidSocket);
  if (network == handles::kInvalidNetworkHandle)
    return ERR_INVALID_ARGUMENT;

  // Android prior to Lollipop didn't have support for binding sockets to
  // networks.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_LOLLIPOP)
    return ERR_NOT_IMPLEMENTED;

  int rv;
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_MARSHMALLOW) {
    static MarshmallowSetNetworkForSocket marshmallow_set_network_for_socket =
        GetMarshmallowSetNetworkForSocket();
    if (!marshmallow_set_network_for_socket)
      return ERR_NOT_IMPLEMENTED;
    rv = marshmallow_set_network_for_socket(network, socket);
    if (rv)
      rv = errno;
  } else {
    static LollipopSetNetworkForSocket lollipop_set_network_for_socket =
        GetLollipopSetNetworkForSocket();
    if (!lollipop_set_network_for_socket)
      return ERR_NOT_IMPLEMENTED;
    rv = -lollipop_set_network_for_socket(network, socket);
  }
  // If |network| has since disconnected, |rv| will be ENONET.  Surface this as
  // ERR_NETWORK_CHANGED, rather than MapSystemError(ENONET) which gives back
  // the less descriptive ERR_FAILED.
  if (rv == ENONET)
    return ERR_NETWORK_CHANGED;
  return MapSystemError(rv);
}

namespace {

using MarshmallowGetAddrInfoForNetwork = int (*)(int64_t network,
                                                 const char* node,
                                                 const char* service,
                                                 const struct addrinfo* hints,
                                                 struct addrinfo** res);

MarshmallowGetAddrInfoForNetwork GetMarshmallowGetAddrInfoForNetwork() {
  // On Android M and newer releases use supported NDK API.
  base::FilePath file(base::GetNativeLibraryName("android"));
  // See declaration of android_getaddrinfofornetwork() here:
  // https://developer.android.com/ndk/reference/group/networking#android_getaddrinfofornetwork
  // Function cannot be called directly as it will cause app to fail to load on
  // pre-marshmallow devices.
  void* dl = dlopen(file.value().c_str(), RTLD_NOW);
  return reinterpret_cast<MarshmallowGetAddrInfoForNetwork>(
      dlsym(dl, "android_getaddrinfofornetwork"));
}

}  // namespace

NET_EXPORT_PRIVATE int GetAddrInfoForNetwork(handles::NetworkHandle network,
                                             const char* node,
                                             const char* service,
                                             const struct addrinfo* hints,
                                             struct addrinfo** res) {
  if (network == handles::kInvalidNetworkHandle) {
    errno = EINVAL;
    return EAI_SYSTEM;
  }
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_MARSHMALLOW) {
    errno = ENOSYS;
    return EAI_SYSTEM;
  }

  static MarshmallowGetAddrInfoForNetwork get_addrinfo_for_network =
      GetMarshmallowGetAddrInfoForNetwork();
  if (!get_addrinfo_for_network) {
    errno = ENOSYS;
    return EAI_SYSTEM;
  }

  return get_addrinfo_for_network(network, node, service, hints, res);
}

}  // namespace net::android
