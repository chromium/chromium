// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/mock_sspi_library_win.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

// Comparator so we can use CredHandle and CtxtHandle with std::set. Both of
// those classes are typedefs for _SecHandle.
bool operator<(const _SecHandle left, const _SecHandle right) {
  return left.dwUpper < right.dwUpper || left.dwLower < right.dwLower;
}

namespace net {

namespace {

int uniquifier_ = 0;

struct MockCredential {
  base::string16 source_principal;
  base::string16 package;
  bool has_explicit_credentials = false;
  int uniquifier = ++uniquifier_;

  // CredHandle and CtxtHandle both shared the following definition:
  //
  // typedef struct _SecHandle {
  //   ULONG_PTR       dwLower;
  //   ULONG_PTR       dwUpper;
  // } SecHandle, * PSecHandle;
  //
  // ULONG_PTR type can hold a pointer. This function stuffs |this| into dwUpper
  // and adds a uniquifier to dwLower. This ensures that all PCredHandles issued
  // by this method during the lifetime of this process is unique.
  void StoreInHandle(PCredHandle handle) {
    DCHECK(uniquifier > 0);
    EXPECT_FALSE(SecIsValidHandle(handle));

    handle->dwLower = uniquifier;
    handle->dwUpper = reinterpret_cast<ULONG_PTR>(this);

    DCHECK(SecIsValidHandle(handle));
  }

  static MockCredential* FromHandle(PCredHandle handle) {
    return reinterpret_cast<MockCredential*>(handle->dwUpper);
  }
};

struct MockContext {
  MockCredential* credential = nullptr;
  base::string16 target_principal;
  int uniquifier = ++uniquifier_;
  int rounds = 0;

  // CredHandle and CtxtHandle both shared the following definition:
  //
  // typedef struct _SecHandle {
  //   ULONG_PTR       dwLower;
  //   ULONG_PTR       dwUpper;
  // } SecHandle, * PSecHandle;
  //
  // ULONG_PTR type can hold a pointer. This function stuffs |this| into dwUpper
  // and adds a uniquifier to dwLower. This ensures that all PCredHandles issued
  // by this method during the lifetime of this process is unique.
  void StoreInHandle(PCtxtHandle handle) {
    EXPECT_FALSE(SecIsValidHandle(handle));
    DCHECK(uniquifier > 0);

    handle->dwLower = uniquifier;
    handle->dwUpper = reinterpret_cast<ULONG_PTR>(this);

    DCHECK(SecIsValidHandle(handle));
  }

  std::string ToString() const {
    return base::StringPrintf(
        "%s's token #%d for %S",
        base::UTF16ToUTF8(credential->source_principal).c_str(), rounds + 1,
        base::as_wcstr(target_principal));
  }

  static MockContext* FromHandle(PCtxtHandle handle) {
    return reinterpret_cast<MockContext*>(handle->dwUpper);
  }
};

}  // namespace

MockSSPILibrary::MockSSPILibrary(const wchar_t* package)
    : SSPILibrary(package) {}

MockSSPILibrary::~MockSSPILibrary() {
  EXPECT_TRUE(expected_package_queries_.empty());
  EXPECT_TRUE(expected_freed_packages_.empty());
  EXPECT_TRUE(active_credentials_.empty());
  EXPECT_TRUE(active_contexts_.empty());
}

SECURITY_STATUS MockSSPILibrary::AcquireCredentialsHandle(
    LPWSTR pszPrincipal,
    unsigned long fCredentialUse,
    void* pvLogonId,
    void* pvAuthData,
    SEC_GET_KEY_FN pGetKeyFn,
    void* pvGetKeyArgument,
    PCredHandle phCredential,
    PTimeStamp ptsExpiry) {
  DCHECK(!SecIsValidHandle(phCredential));
  auto* credential = new MockCredential;
  credential->source_principal = pszPrincipal ? base::as_u16cstr(pszPrincipal)
                                              : STRING16_LITERAL("<Default>");
  credential->package = base::as_u16cstr(package_name_.c_str());
  credential->has_explicit_credentials = !!pvAuthData;

  credential->StoreInHandle(phCredential);

  if (ptsExpiry) {
    ptsExpiry->LowPart = 0xBAA5B780;
    ptsExpiry->HighPart = 0x01D54E17;
  }

  active_credentials_.insert(*phCredential);
  return SEC_E_OK;
}

SECURITY_STATUS MockSSPILibrary::InitializeSecurityContext(
    PCredHandle phCredential,
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
    PTimeStamp ptsExpiry) {
  MockContext* new_context = new MockContext;
  new_context->credential = MockCredential::FromHandle(phCredential);
  new_context->target_principal = base::as_u16cstr(pszTargetName);
  new_context->rounds = 0;

  // Always rotate contexts. That way tests will fail if the caller's context
  // management is broken.
  if (phContext && SecIsValidHandle(phContext)) {
    std::unique_ptr<MockContext> old_context{
        MockContext::FromHandle(phContext)};
    EXPECT_EQ(old_context->credential, new_context->credential);
    EXPECT_EQ(1u, active_contexts_.erase(*phContext));

    new_context->rounds = old_context->rounds + 1;
    SecInvalidateHandle(phContext);
  }

  new_context->StoreInHandle(phNewContext);
  active_contexts_.insert(*phNewContext);

  auto token = new_context->ToString();
  PSecBuffer out_buffer = pOutput->pBuffers;
  out_buffer->cbBuffer = std::min<ULONG>(out_buffer->cbBuffer, token.size());
  std::memcpy(out_buffer->pvBuffer, token.data(), out_buffer->cbBuffer);

  if (ptsExpiry) {
    ptsExpiry->LowPart = 0xBAA5B780;
    ptsExpiry->HighPart = 0x01D54E15;
  }
  return SEC_E_OK;
}

SECURITY_STATUS MockSSPILibrary::QueryContextAttributesEx(PCtxtHandle phContext,
                                                          ULONG ulAttribute,
                                                          PVOID pBuffer,
                                                          ULONG cbBuffer) {
  static const SecPkgInfoW kNegotiatedPackage = {
      0,
      0,
      0,
      0,
      const_cast<SEC_WCHAR*>(L"Itsa me Kerberos!!"),
      const_cast<SEC_WCHAR*>(L"I like turtles")};

  auto* context = MockContext::FromHandle(phContext);

  switch (ulAttribute) {
    case SECPKG_ATTR_NATIVE_NAMES: {
      auto* native_names =
          reinterpret_cast<SecPkgContext_NativeNames*>(pBuffer);
      DCHECK_EQ(sizeof(*native_names), cbBuffer);
      native_names->sClientName =
          base::as_writable_wcstr(context->credential->source_principal);
      native_names->sServerName =
          base::as_writable_wcstr(context->target_principal);
      return SEC_E_OK;
    }

    case SECPKG_ATTR_NEGOTIATION_INFO: {
      auto* negotiation_info =
          reinterpret_cast<SecPkgContext_NegotiationInfo*>(pBuffer);
      DCHECK_EQ(sizeof(*negotiation_info), cbBuffer);
      negotiation_info->PackageInfo =
          const_cast<SecPkgInfoW*>(&kNegotiatedPackage);
      negotiation_info->NegotiationState = (context->rounds == 1)
                                               ? SECPKG_NEGOTIATION_COMPLETE
                                               : SECPKG_NEGOTIATION_IN_PROGRESS;
      return SEC_E_OK;
    }

    case SECPKG_ATTR_AUTHORITY: {
      auto* authority = reinterpret_cast<SecPkgContext_Authority*>(pBuffer);
      DCHECK_EQ(sizeof(*authority), cbBuffer);
      authority->sAuthorityName = const_cast<SEC_WCHAR*>(L"Dodgy Server");
      return SEC_E_OK;
    }

    default:
      return SEC_E_UNSUPPORTED_FUNCTION;
  }
}

SECURITY_STATUS MockSSPILibrary::QuerySecurityPackageInfo(
    PSecPkgInfoW* pkgInfo) {
  if (expected_package_queries_.empty()) {
    static SecPkgInfoW kDefaultPkgInfo{
        0, 0, 0, kDefaultMaxTokenLength, nullptr, nullptr};
    *pkgInfo = &kDefaultPkgInfo;
    expected_freed_packages_.insert(&kDefaultPkgInfo);
    return SEC_E_OK;
  }

  PackageQuery package_query = expected_package_queries_.front();
  expected_package_queries_.pop_front();
  *pkgInfo = package_query.package_info;
  if (package_query.response_code == SEC_E_OK)
    expected_freed_packages_.insert(package_query.package_info);
  return package_query.response_code;
}

SECURITY_STATUS MockSSPILibrary::FreeCredentialsHandle(
    PCredHandle phCredential) {
  DCHECK(SecIsValidHandle(phCredential));
  EXPECT_EQ(1u, active_credentials_.erase(*phCredential));
  std::unique_ptr<MockCredential> owned{
      MockCredential::FromHandle(phCredential)};
  SecInvalidateHandle(phCredential);
  return SEC_E_OK;
}

SECURITY_STATUS MockSSPILibrary::DeleteSecurityContext(PCtxtHandle phContext) {
  std::unique_ptr<MockContext> context{MockContext::FromHandle(phContext)};
  EXPECT_EQ(1u, active_contexts_.erase(*phContext));
  SecInvalidateHandle(phContext);
  return SEC_E_OK;
}

SECURITY_STATUS MockSSPILibrary::FreeContextBuffer(PVOID pvContextBuffer) {
  PSecPkgInfoW package_info = static_cast<PSecPkgInfoW>(pvContextBuffer);
  std::set<PSecPkgInfoW>::iterator it = expected_freed_packages_.find(
      package_info);
  EXPECT_TRUE(it != expected_freed_packages_.end());
  expected_freed_packages_.erase(it);
  return SEC_E_OK;
}

void MockSSPILibrary::ExpectQuerySecurityPackageInfo(
    SECURITY_STATUS response_code,
    PSecPkgInfoW package_info) {
  expected_package_queries_.emplace_back(
      PackageQuery{response_code, package_info});
}

}  // namespace net
