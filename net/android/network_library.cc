// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_library.h"

#include <string>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/strings/string_split.h"
#include "net/dns/public/dns_protocol.h"
#include "net/net_jni_headers/AndroidNetworkLibrary_jni.h"
#include "net/net_jni_headers/DnsStatus_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfByteArray;
using base::android::ToJavaByteArray;

namespace net {
namespace android {

void VerifyX509CertChain(const std::vector<std::string>& cert_chain,
                         base::StringPiece auth_type,
                         base::StringPiece host,
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

void AddTestRootCertificate(const uint8_t* cert, size_t len) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> cert_array = ToJavaByteArray(env, cert, len);
  DCHECK(!cert_array.is_null());
  Java_AndroidNetworkLibrary_addTestRootCertificate(env, cert_array);
}

void ClearTestRootCertificates() {
  JNIEnv* env = AttachCurrentThread();
  Java_AndroidNetworkLibrary_clearTestRootCertificates(env);
}

bool IsCleartextPermitted(const std::string& host) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> host_string = ConvertUTF8ToJavaString(env, host);
  return Java_AndroidNetworkLibrary_isCleartextPermitted(env, host_string);
}

bool HaveOnlyLoopbackAddresses() {
  JNIEnv* env = AttachCurrentThread();
  return Java_AndroidNetworkLibrary_haveOnlyLoopbackAddresses(env);
}

bool GetMimeTypeFromExtension(const std::string& extension,
                              std::string* result) {
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

std::string GetTelephonySimOperator() {
  return base::android::ConvertJavaStringToUTF8(
      Java_AndroidNetworkLibrary_getSimOperator(
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

base::Optional<int32_t> GetWifiSignalLevel() {
  const int count_buckets = 5;
  int signal_strength = Java_AndroidNetworkLibrary_getWifiSignalLevel(
      base::android::AttachCurrentThread(), count_buckets);
  if (signal_strength < 0)
    return base::nullopt;
  DCHECK_LE(0, signal_strength);
  DCHECK_GE(count_buckets - 1, signal_strength);

  return signal_strength;
}

bool GetDnsServers(std::vector<IPEndPoint>* dns_servers,
                   bool* dns_over_tls_active,
                   std::string* dns_over_tls_hostname,
                   std::vector<std::string>* search_suffixes) {
  DCHECK_GE(base::android::BuildInfo::GetInstance()->sdk_int(),
            base::android::SDK_VERSION_MARSHMALLOW);

  JNIEnv* env = AttachCurrentThread();
  // Get the DNS status for the active network.
  ScopedJavaLocalRef<jobject> result =
      Java_AndroidNetworkLibrary_getDnsStatus(env, nullptr /* network */);
  if (result.is_null())
    return false;

  // Parse the DNS servers.
  std::vector<std::vector<uint8_t>> dns_servers_data;
  base::android::JavaArrayOfByteArrayToBytesVector(
      env, Java_DnsStatus_getDnsServers(env, result), &dns_servers_data);
  for (const std::vector<uint8_t>& dns_address_data : dns_servers_data) {
    IPAddress dns_address(dns_address_data.data(), dns_address_data.size());
    IPEndPoint dns_server(dns_address, dns_protocol::kDefaultPort);
    dns_servers->push_back(dns_server);
  }

  *dns_over_tls_active = Java_DnsStatus_getPrivateDnsActive(env, result);
  *dns_over_tls_hostname = base::android::ConvertJavaStringToUTF8(
      Java_DnsStatus_getPrivateDnsServerName(env, result));

  std::string search_suffixes_str = base::android::ConvertJavaStringToUTF8(
      Java_DnsStatus_getSearchDomains(env, result));
  *search_suffixes =
      base::SplitString(search_suffixes_str, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  return !dns_servers->empty();
}

bool ReportBadDefaultNetwork() {
  return Java_AndroidNetworkLibrary_reportBadDefaultNetwork(
      AttachCurrentThread());
}

void TagSocket(SocketDescriptor socket, uid_t uid, int32_t tag) {
  Java_AndroidNetworkLibrary_tagSocket(AttachCurrentThread(), socket, uid, tag);
}

}  // namespace android
}  // namespace net
