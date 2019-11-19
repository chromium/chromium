// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/android/dummy_spnego_authenticator.h"

#include "base/android/jni_string.h"
#include "base/base64.h"
#include "base/stl_util.h"
#include "net/net_test_jni_headers/DummySpnegoAuthenticator_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::JavaParamRef;

namespace net {

// iso.org.dod.internet.security.mechanism.snego (1.3.6.1.5.5.2)
// From RFC 4178, which uses SNEGO not SPNEGO.
static const unsigned char kSpnegoOid[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x02};
gss_OID_desc CHROME_GSS_SPNEGO_MECH_OID_DESC_VAL = {
    base::size(kSpnegoOid), const_cast<unsigned char*>(kSpnegoOid)};

gss_OID CHROME_GSS_SPNEGO_MECH_OID_DESC = &CHROME_GSS_SPNEGO_MECH_OID_DESC_VAL;

namespace {

// gss_OID helpers.
// NOTE: gss_OID's do not own the data they point to, which should be static.
void ClearOid(gss_OID dest) {
  if (!dest)
    return;
  dest->length = 0;
  dest->elements = NULL;
}

void SetOid(gss_OID dest, const void* src, size_t length) {
  if (!dest)
    return;
  ClearOid(dest);
  if (!src)
    return;
  dest->length = length;
  if (length)
    dest->elements = const_cast<void*>(src);
}

void CopyOid(gss_OID dest, const gss_OID_desc* src) {
  if (!dest)
    return;
  ClearOid(dest);
  if (!src)
    return;
  SetOid(dest, src->elements, src->length);
}

}  // namespace

namespace test {

GssContextMockImpl::GssContextMockImpl()
    : lifetime_rec(0), ctx_flags(0), locally_initiated(0), open(0) {
  ClearOid(&mech_type);
}

GssContextMockImpl::GssContextMockImpl(const GssContextMockImpl& other)
    : src_name(other.src_name),
      targ_name(other.targ_name),
      lifetime_rec(other.lifetime_rec),
      ctx_flags(other.ctx_flags),
      locally_initiated(other.locally_initiated),
      open(other.open) {
  CopyOid(&mech_type, &other.mech_type);
}

GssContextMockImpl::GssContextMockImpl(const char* src_name_in,
                                       const char* targ_name_in,
                                       uint32_t lifetime_rec_in,
                                       const gss_OID_desc& mech_type_in,
                                       uint32_t ctx_flags_in,
                                       int locally_initiated_in,
                                       int open_in)
    : src_name(src_name_in ? src_name_in : ""),
      targ_name(targ_name_in ? targ_name_in : ""),
      lifetime_rec(lifetime_rec_in),
      ctx_flags(ctx_flags_in),
      locally_initiated(locally_initiated_in),
      open(open_in) {
  CopyOid(&mech_type, &mech_type_in);
}

GssContextMockImpl::~GssContextMockImpl() {
  ClearOid(&mech_type);
}

}  // namespace test

namespace android {

DummySpnegoAuthenticator::SecurityContextQuery::SecurityContextQuery(
    const std::string& in_expected_package,
    uint32_t in_response_code,
    uint32_t in_minor_response_code,
    const test::GssContextMockImpl& in_context_info,
    const std::string& in_expected_input_token,
    const std::string& in_output_token)
    : expected_package(in_expected_package),
      response_code(in_response_code),
      minor_response_code(in_minor_response_code),
      context_info(in_context_info),
      expected_input_token(in_expected_input_token),
      output_token(in_output_token) {
}

DummySpnegoAuthenticator::SecurityContextQuery::SecurityContextQuery(
    const std::string& in_expected_package,
    uint32_t in_response_code,
    uint32_t in_minor_response_code,
    const test::GssContextMockImpl& in_context_info,
    const char* in_expected_input_token,
    const char* in_output_token)
    : expected_package(in_expected_package),
      response_code(in_response_code),
      minor_response_code(in_minor_response_code),
      context_info(in_context_info) {
  if (in_expected_input_token)
    expected_input_token = in_expected_input_token;
  if (in_output_token)
    output_token = in_output_token;
}

DummySpnegoAuthenticator::SecurityContextQuery::SecurityContextQuery()
    : response_code(0), minor_response_code(0) {
}

DummySpnegoAuthenticator::SecurityContextQuery::SecurityContextQuery(
    const SecurityContextQuery& other) = default;

DummySpnegoAuthenticator::SecurityContextQuery::~SecurityContextQuery() {
}

base::android::ScopedJavaLocalRef<jstring>
DummySpnegoAuthenticator::SecurityContextQuery::GetTokenToReturn(
    JNIEnv* env,
    const JavaParamRef<jobject>& /*obj*/) {
  return base::android::ConvertUTF8ToJavaString(env, output_token.c_str());
}
int DummySpnegoAuthenticator::SecurityContextQuery::GetResult(
    JNIEnv* /*env*/,
    const JavaParamRef<jobject>& /*obj*/) {
  return response_code;
}

void DummySpnegoAuthenticator::SecurityContextQuery::CheckGetTokenArguments(
    JNIEnv* env,
    const JavaParamRef<jobject>& /*obj*/,
    const JavaParamRef<jstring>& j_incoming_token) {
  std::string incoming_token =
      base::android::ConvertJavaStringToUTF8(env, j_incoming_token);
  EXPECT_EQ(expected_input_token, incoming_token);
}

// Needed to satisfy "complex class" clang requirements.
DummySpnegoAuthenticator::DummySpnegoAuthenticator() {
}

DummySpnegoAuthenticator::~DummySpnegoAuthenticator() {
}

void DummySpnegoAuthenticator::EnsureTestAccountExists() {
  Java_DummySpnegoAuthenticator_ensureTestAccountExists(
      base::android::AttachCurrentThread());
}

void DummySpnegoAuthenticator::RemoveTestAccounts() {
  Java_DummySpnegoAuthenticator_removeTestAccounts(
      base::android::AttachCurrentThread());
}

void DummySpnegoAuthenticator::ExpectSecurityContext(
    const std::string& expected_package,
    uint32_t response_code,
    uint32_t minor_response_code,
    const test::GssContextMockImpl& context_info,
    const std::string& expected_input_token,
    const std::string& output_token) {
  SecurityContextQuery query(expected_package, response_code,
                             minor_response_code, context_info,
                             expected_input_token, output_token);
  expected_security_queries_.push_back(query);
  Java_DummySpnegoAuthenticator_setNativeAuthenticator(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

long DummySpnegoAuthenticator::GetNextQuery(
    JNIEnv* /*env*/,
    const JavaParamRef<jobject>& /* obj */) {
  CheckQueueNotEmpty();
  current_query_ = expected_security_queries_.front();
  expected_security_queries_.pop_front();
  return reinterpret_cast<intptr_t>(&current_query_);
}

void DummySpnegoAuthenticator::CheckQueueNotEmpty() {
  ASSERT_FALSE(expected_security_queries_.empty());
}

}  // namespace android
}  // namespace net
