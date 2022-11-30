// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_VERIFY_PROC_WIN_H_
#define NET_CERT_CERT_VERIFY_PROC_WIN_H_

#include "stdint.h"

#include <vector>

#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "net/cert/cert_verify_proc.h"

namespace net {

// Performs certificate path construction and validation using Windows'
// CryptoAPI.
class NET_EXPORT CertVerifyProcWin : public CertVerifyProc {
 public:
  // Diagnostic data related to Windows cert validation.
  class NET_EXPORT ResultDebugData : public base::SupportsUserData::Data {
   public:
    ResultDebugData(base::Time authroot_this_update,
                    std::vector<uint8_t> authroot_sequence_number);
    ResultDebugData(const ResultDebugData&);
    ~ResultDebugData() override;

    static const ResultDebugData* Get(const base::SupportsUserData* debug_data);
    static void Create(base::Time authroot_this_update,
                       std::vector<uint8_t> authroot_sequence_number,
                       base::SupportsUserData* debug_data);

    // base::SupportsUserData::Data implementation:
    std::unique_ptr<Data> Clone() override;

    // The ThisUpdate field from the AuthRoot store in the registry. Note,
    // if a user has not received any AuthRoot updates, such as updates being
    // disabled or connectivity issues for WinHTTP, this will return a
    // `base::Time` that `is_null()`. Specifically, if a user is running with
    // the RTM version of AuthRoot (e.g. as stored in crypt32.dll), this will
    // not be filled.
    const base::Time& authroot_this_update() const {
      return authroot_this_update_;
    }

    // The Sequence Number from the AuthRoot store in the registry. See the
    // remarks in `authroot_this_update()` for situations where this may not
    // be filled.
    const std::vector<uint8_t>& authroot_sequence_number() const {
      return authroot_sequence_number_;
    }

   private:
    base::Time authroot_this_update_;
    std::vector<uint8_t> authroot_sequence_number_;
  };

  CertVerifyProcWin();

  bool SupportsAdditionalTrustAnchors() const override;

 protected:
  ~CertVerifyProcWin() override;

 private:
  int VerifyInternal(X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     const std::string& sct_list,
                     int flags,
                     CRLSet* crl_set,
                     const CertificateList& additional_trust_anchors,
                     CertVerifyResult* verify_result,
                     const NetLogWithSource& net_log) override;
};

}  // namespace net

#endif  // NET_CERT_CERT_VERIFY_PROC_WIN_H_
