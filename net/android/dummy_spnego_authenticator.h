// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_DUMMY_SPNEGO_AUTHENTICATOR_H_
#define NET_ANDROID_DUMMY_SPNEGO_AUTHENTICATOR_H_

#include <jni.h>
#include <stdint.h>

#include <list>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr_exclusion.h"

// Provides an interface for controlling the DummySpnegoAuthenticator service.
// This includes a basic stub of the Mock GSSAPI library, so that OS independent
// Negotiate authentication tests can be run on Android.
namespace net {

// These constant values are arbitrary, and different from the real GSSAPI
// values, but must match those used in DummySpnegoAuthenticator.java
#define GSS_S_COMPLETE 0
#define GSS_S_CONTINUE_NEEDED 1
#define GSS_S_FAILURE 2

typedef struct gss_OID_desc_struct {
  uint32_t length;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION void* elements;
} gss_OID_desc, *gss_OID;

extern gss_OID CHROME_GSS_SPNEGO_MECH_OID_DESC;

namespace test {

// Copy of class in Mock GSSAPI library.
class GssContextMockImpl {
 public:
  GssContextMockImpl();
  GssContextMockImpl(const GssContextMockImpl& other);
  GssContextMockImpl(const char* src_name,
                     const char* targ_name,
                     uint32_t lifetime_rec,
                     const gss_OID_desc& mech_type,
                     uint32_t ctx_flags,
                     int locally_initiated,
                     int open);
  ~GssContextMockImpl();

  void Assign(const GssContextMockImpl& other);

  std::string src_name;
  std::string targ_name;
  int32_t lifetime_rec;
  gss_OID_desc mech_type;
  int32_t ctx_flags;
  int locally_initiated;
  int open;
};

}  // namespace test

namespace android {

// Interface to Java DummySpnegoAuthenticator.
class DummySpnegoAuthenticator {
 public:
  struct SecurityContextQuery {
    SecurityContextQuery(const std::string& expected_package,
                         uint32_t response_code,
                         uint32_t minor_response_code,
                         const test::GssContextMockImpl& context_info,
                         const std::string& expected_input_token,
                         const std::string& output_token);
    SecurityContextQuery(const std::string& expected_package,
                         uint32_t response_code,
                         uint32_t minor_response_code,
                         const test::GssContextMockImpl& context_info,
                         const char* expected_input_token,
                         const char* output_token);
    SecurityContextQuery();
    SecurityContextQuery(const SecurityContextQuery& other);
    ~SecurityContextQuery();

    // Note that many of these fields only exist for compatibility with the
    // non-Android version of the tests. Only the response_code and tokens are
    // used or checked on Android.
    std::string expected_package;
    uint32_t response_code;
    uint32_t minor_response_code;
    test::GssContextMockImpl context_info;
    std::string expected_input_token;
    std::string output_token;

    // Java callable members
    base::android::ScopedJavaLocalRef<jstring> GetTokenToReturn(JNIEnv* env);
    int GetResult(JNIEnv* env);

    // Called from Java to check the arguments passed to the GetToken. Has to
    // be in C++ since these tests are driven by googletest, and can only report
    // failures through the googletest C++ API.
    void CheckGetTokenArguments(
        JNIEnv* env,
        const base::android::JavaParamRef<jstring>& incoming_token);
  };

  DummySpnegoAuthenticator();

  ~DummySpnegoAuthenticator();

  void ExpectSecurityContext(const std::string& expected_package,
                             uint32_t response_code,
                             uint32_t minor_response_code,
                             const test::GssContextMockImpl& context_info,
                             const std::string& expected_input_token,
                             const std::string& output_token);

  static void EnsureTestAccountExists();
  static void RemoveTestAccounts();

  long GetNextQuery(JNIEnv* env);

 private:
  // Abandon the test if the query queue is empty. Has to be a void function to
  // allow use of ASSERT_FALSE.
  void CheckQueueNotEmpty();

  std::list<SecurityContextQuery> expected_security_queries_;
  // Needed to keep the current query alive once it has been pulled from the
  // queue. This is simpler than transferring its ownership to Java.
  SecurityContextQuery current_query_;
};

}  // namespace android

using MockAuthLibrary = android::DummySpnegoAuthenticator;

}  // namespace net

#endif  // NET_ANDROID_DUMMY_SPNEGO_AUTHENTICATOR_H_
