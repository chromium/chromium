// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_MOCK_SSPI_LIBRARY_WIN_H_
#define NET_HTTP_MOCK_SSPI_LIBRARY_WIN_H_

#include <list>
#include <set>

#include "net/http/http_auth_sspi_win.h"

namespace net {

// The MockSSPILibrary class is intended for unit tests which want to bypass
// the system SSPI library calls.
class MockSSPILibrary : public SSPILibrary {
 public:
  explicit MockSSPILibrary(const wchar_t* package);
  ~MockSSPILibrary() override;

  // Default max token length regardless of package name returned by
  // QuerySecurityPackageInfo() if no expectations are set.
  static constexpr unsigned long kDefaultMaxTokenLength = 1024;

  // SSPILibrary methods:

  // AcquireCredentialsHandle() returns a handle that must be freed using
  // FreeCredentialsHandle(). The credentials handle records the principal name.
  //
  // On return ptsExpiry is set to a constant.
  SECURITY_STATUS AcquireCredentialsHandle(LPWSTR pszPrincipal,
                                           unsigned long fCredentialUse,
                                           void* pvLogonId,
                                           void* pvAuthData,
                                           SEC_GET_KEY_FN pGetKeyFn,
                                           void* pvGetKeyArgument,
                                           PCredHandle phCredential,
                                           PTimeStamp ptsExpiry) override;

  // InitializeSecurityContext() returns a handle in phContext that must be
  // freed via FreeContextBuffer() or by passing it into another
  // InitializeSecurityContext() call.
  //
  // On return ptsExpiry is set to a constant.
  //
  // The output buffer will contain a token consisting of the ASCII string:
  //
  //   "<source principal>'s token #<n> for <target principal>"
  //
  // <source principal> is the security principal derived from explicit
  // credentials that were passed to a prior AcquireCredentialsHandle() call, or
  // the string "<Default>" if ambient credentials were requested.
  //
  // <n> is the 1-based invocation counter for InitializeSecurityContext() for
  // the same context.
  //
  // <target principal> is the contents of the pszTargetName. Note that the
  // function expects the same target name on every invocation.
  SECURITY_STATUS InitializeSecurityContext(PCredHandle phCredential,
                                            PCtxtHandle phContext,
                                            SEC_WCHAR* pszTargetName,
                                            unsigned long fContextReq,
                                            unsigned long Reserved1,
                                            unsigned long TargetDataRep,
                                            PSecBufferDesc pInput,
                                            unsigned long Reserved2,
                                            PCtxtHandle phNewContext,
                                            PSecBufferDesc pOutput,
                                            unsigned long* contextAttr,
                                            PTimeStamp ptsExpiry) override;

  // QueryContextAttributesEx() supports querying the same attributes as
  // required by HttpAuthSSPI.
  SECURITY_STATUS QueryContextAttributesEx(PCtxtHandle phContext,
                                           ULONG ulAttribute,
                                           PVOID pBuffer,
                                           ULONG cbBuffer) override;

  SECURITY_STATUS QuerySecurityPackageInfo(PSecPkgInfoW* pkgInfo) override;
  SECURITY_STATUS FreeCredentialsHandle(PCredHandle phCredential) override;
  SECURITY_STATUS DeleteSecurityContext(PCtxtHandle phContext) override;
  SECURITY_STATUS FreeContextBuffer(PVOID pvContextBuffer) override;

  // Establishes an expectation for a |QuerySecurityPackageInfo()| call.
  //
  // Each expectation established by |ExpectSecurityQueryPackageInfo()| must be
  // matched by a call to |QuerySecurityPackageInfo()| during the lifetime of
  // the MockSSPILibrary. The expectations establish an explicit ordering.
  //
  // |response_code| is used as the return value for
  // |QuerySecurityPackageInfo()|. If |response_code| is SEC_E_OK,
  // an expectation is also set for a call to |FreeContextBuffer()| after
  // the matching |QuerySecurityPackageInfo()| is called.
  //
  // |package_info| is assigned to |*pkgInfo| in |QuerySecurityPackageInfo|.
  // The lifetime of |*package_info| should last at least until the matching
  // |QuerySecurityPackageInfo()| is called.
  void ExpectQuerySecurityPackageInfo(SECURITY_STATUS response_code,
                                      PSecPkgInfoW package_info);

 private:
  struct PackageQuery {
    SECURITY_STATUS response_code;
    PSecPkgInfoW package_info;
  };

  // expected_package_queries contains an ordered list of expected
  // |QuerySecurityPackageInfo()| calls and the return values for those
  // calls.
  std::list<PackageQuery> expected_package_queries_;

  // Set of packages which should be freed.
  std::set<PSecPkgInfoW> expected_freed_packages_;

  // These sets keep track of active credentials and contexts.
  std::set<CredHandle> active_credentials_;
  std::set<CtxtHandle> active_contexts_;
};

using MockAuthLibrary = MockSSPILibrary;

}  // namespace net

#endif  // NET_HTTP_MOCK_SSPI_LIBRARY_WIN_H_
