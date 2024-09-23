// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_OPT_RECORD_RDATA_H_
#define NET_DNS_OPT_RECORD_RDATA_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "net/base/net_export.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"

namespace net {

// OPT record format (https://tools.ietf.org/html/rfc6891):
class NET_EXPORT_PRIVATE OptRecordRdata : public RecordRdata {
 public:
  static std::unique_ptr<OptRecordRdata> Create(std::string_view data);

  class NET_EXPORT_PRIVATE Opt {
   public:
    static constexpr size_t kHeaderSize = 4;  // sizeof(code) + sizeof(size)

    Opt() = delete;
    explicit Opt(std::string data);

    Opt(const Opt& other) = delete;
    Opt& operator=(const Opt& other) = delete;
    Opt(Opt&& other) = delete;
    Opt& operator=(Opt&& other) = delete;
    virtual ~Opt() = default;

    bool operator==(const Opt& other) const;
    bool operator!=(const Opt& other) const;

    virtual uint16_t GetCode() const = 0;
    std::string_view data() const { return data_; }

   private:
    bool IsEqual(const Opt& other) const;
    std::string data_;
  };

  class NET_EXPORT_PRIVATE EdeOpt : public Opt {
   public:
    static const uint16_t kOptCode = dns_protocol::kEdnsExtendedDnsError;

    // The following errors are defined by in the IANA registry.
    // https://www.iana.org/assignments/dns-parameters/dns-parameters.xhtml#extended-dns-error-codes
    enum EdeInfoCode {
      kOtherError,
      kUnsupportedDnskeyAlgorithm,
      kUnsupportedDsDigestType,
      kStaleAnswer,
      kForgedAnswer,
      kDnssecIndeterminate,
      kDnssecBogus,
      kSignatureExpired,
      kSignatureNotYetValid,
      kDnskeyMissing,
      kRrsigsMissing,
      kNoZoneKeyBitSet,
      kNsecMissing,
      kCachedError,
      kNotReady,
      kBlocked,
      kCensored,
      kFiltered,
      kProhibited,
      kStaleNxdomainAnswer,
      kNotAuthoritative,
      kNotSupported,
      kNoReachableAuthority,
      kNetworkError,
      kInvalidData,
      kSignatureExpiredBeforeValid,
      kTooEarly,
      kUnsupportedNsec3IterationsValue,
      // Note: kUnrecognizedErrorCode is not defined by RFC 8914.
      // Used when error code does not match existing RFC error code.
      kUnrecognizedErrorCode
    };

    EdeOpt(uint16_t info_code, std::string extra_text);

    EdeOpt(const EdeOpt& other) = delete;
    EdeOpt& operator=(const EdeOpt& other) = delete;
    EdeOpt(EdeOpt&& other) = delete;
    EdeOpt& operator=(EdeOpt&& other) = delete;
    ~EdeOpt() override;

    // Attempts to parse an EDE option from `data`. Returns nullptr on failure.
    static std::unique_ptr<EdeOpt> Create(std::string data);

    uint16_t GetCode() const override;
    uint16_t info_code() const { return info_code_; }
    std::string_view extra_text() const { return extra_text_; }

    EdeInfoCode GetEnumFromInfoCode() const;

    // Convert a uint16_t to an EdeInfoCode enum.
    static EdeInfoCode GetEnumFromInfoCode(uint16_t info_code);

   private:
    EdeOpt();

    uint16_t info_code_;
    std::string extra_text_;
  };

  class NET_EXPORT_PRIVATE PaddingOpt : public Opt {
   public:
    static const uint16_t kOptCode = dns_protocol::kEdnsPadding;

    PaddingOpt() = delete;
    // Construct a PaddingOpt with the specified padding string.
    explicit PaddingOpt(std::string padding);
    // Constructs PaddingOpt with '\0' character padding of specified length.
    // Note: This padding_len only specifies the length of the data section.
    // Users must take into account the header length `Opt::kHeaderSize`
    explicit PaddingOpt(uint16_t padding_len);

    PaddingOpt(const PaddingOpt& other) = delete;
    PaddingOpt& operator=(const PaddingOpt& other) = delete;
    PaddingOpt(PaddingOpt&& other) = delete;
    PaddingOpt& operator=(PaddingOpt&& other) = delete;
    ~PaddingOpt() override;

    uint16_t GetCode() const override;
  };

  class NET_EXPORT_PRIVATE UnknownOpt : public Opt {
   public:
    UnknownOpt() = delete;
    UnknownOpt(const UnknownOpt& other) = delete;
    UnknownOpt& operator=(const UnknownOpt& other) = delete;
    UnknownOpt(UnknownOpt&& other) = delete;
    UnknownOpt& operator=(UnknownOpt&& other) = delete;
    ~UnknownOpt() override;

    // Create UnknownOpt with option code and data.
    // Cannot instantiate UnknownOpt directly in order to prevent Opt with
    // dedicated class class (ex. EdeOpt) from being stored in UnknownOpt.
    // object.
    // This method must purely be used for testing.
    // Only the parser can instantiate an UnknownOpt object (via friend
    // classes).
    static std::unique_ptr<UnknownOpt> CreateForTesting(uint16_t code,
                                                        std::string data);

    uint16_t GetCode() const override;

   private:
    UnknownOpt(uint16_t code, std::string data);

    uint16_t code_;

    friend std::unique_ptr<OptRecordRdata> OptRecordRdata::Create(
        std::string_view data);
  };

  static constexpr uint16_t kOptsWithDedicatedClasses[] = {
      dns_protocol::kEdnsPadding, dns_protocol::kEdnsExtendedDnsError};

  static const uint16_t kType = dns_protocol::kTypeOPT;

  OptRecordRdata();

  OptRecordRdata(const OptRecordRdata&) = delete;
  OptRecordRdata& operator=(const OptRecordRdata&) = delete;

  OptRecordRdata(OptRecordRdata&& other) = delete;
  OptRecordRdata& operator=(OptRecordRdata&& other) = delete;

  ~OptRecordRdata() override;

  bool operator==(const OptRecordRdata& other) const;
  bool operator!=(const OptRecordRdata& other) const;

  // Checks whether two OptRecordRdata objects are equal. This comparison takes
  // into account the order of insertion. Two OptRecordRdata objects with
  // identical Opt records inserted in a different order will not be equal.
  bool IsEqual(const RecordRdata* other) const override;

  uint16_t Type() const override;
  const std::vector<char>& buf() const { return buf_; }
  const std::multimap<uint16_t, const std::unique_ptr<const Opt>>& opts()
      const {
    return opts_;
  }

  // Add specified Opt to rdata. Updates raw buffer as well.
  void AddOpt(const std::unique_ptr<Opt> opt);

  // Checks if an Opt with the specified opt_code is contained.
  bool ContainsOptCode(uint16_t opt_code) const;

  size_t OptCount() const { return opts_.size(); }

  // Returns all options sorted by option code, using insertion order to break
  // ties.
  std::vector<const Opt*> GetOpts() const;

  // Returns all EDE options in insertion order.
  std::vector<const EdeOpt*> GetEdeOpts() const;

  // Returns all Padding options in insertion order.
  std::vector<const PaddingOpt*> GetPaddingOpts() const;

 private:
  // Opt objects are stored in a multimap; key is the opt code.
  std::multimap<uint16_t, const std::unique_ptr<const Opt>> opts_;
  std::vector<char> buf_;
};

}  // namespace net

#endif  // NET_DNS_OPT_RECORD_RDATA_H_
