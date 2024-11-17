// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/dns_hosts.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "build/build_config.h"
#include "net/base/cronet_buildflags.h"
#include "net/base/url_util.h"
#include "net/dns/dns_util.h"
#include "url/url_canon.h"

namespace net {

namespace {

// Parses the contents of a hosts file.  Returns one token (IP or hostname) at
// a time.  Doesn't copy anything; accepts the file as a std::string_view and
// returns tokens as StringPieces.
class HostsParser {
 public:
  explicit HostsParser(std::string_view text, ParseHostsCommaMode comma_mode)
      : text_(text),
        data_(text.data()),
        end_(text.size()),
        comma_mode_(comma_mode) {}

  HostsParser(const HostsParser&) = delete;
  HostsParser& operator=(const HostsParser&) = delete;

  // Advances to the next token (IP or hostname).  Returns whether another
  // token was available.  |token_is_ip| and |token| can be used to find out
  // the type and text of the token.
  bool Advance() {
    bool next_is_ip = (pos_ == 0);
    while (pos_ < end_ && pos_ != std::string::npos) {
      switch (text_[pos_]) {
        case ' ':
        case '\t':
          SkipWhitespace();
          break;

        case '\r':
        case '\n':
          next_is_ip = true;
          pos_++;
          break;

        case '#':
          SkipRestOfLine();
          break;

        case ',':
          if (comma_mode_ == PARSE_HOSTS_COMMA_IS_WHITESPACE) {
            SkipWhitespace();
            break;
          }

          // If comma_mode_ is COMMA_IS_TOKEN, fall through:
          [[fallthrough]];

        default: {
          size_t token_start = pos_;
          SkipToken();
          size_t token_end = (pos_ == std::string::npos) ? end_ : pos_;

          token_ =
              std::string_view(data_ + token_start, token_end - token_start);
          token_is_ip_ = next_is_ip;

          return true;
        }
      }
    }

    return false;
  }

  // Fast-forwards the parser to the next line.  Should be called if an IP
  // address doesn't parse, to avoid wasting time tokenizing hostnames that
  // will be ignored.
  void SkipRestOfLine() { pos_ = text_.find("\n", pos_); }

  // Returns whether the last-parsed token is an IP address (true) or a
  // hostname (false).
  bool token_is_ip() { return token_is_ip_; }

  // Returns the text of the last-parsed token as a std::string_view referencing
  // the same underlying memory as the std::string_view passed to the
  // constructor. Returns an empty std::string_view if no token has been parsed
  // or the end of the input string has been reached.
  std::string_view token() { return token_; }

 private:
  void SkipToken() {
    switch (comma_mode_) {
      case PARSE_HOSTS_COMMA_IS_TOKEN:
        pos_ = text_.find_first_of(" \t\n\r#", pos_);
        break;
      case PARSE_HOSTS_COMMA_IS_WHITESPACE:
        pos_ = text_.find_first_of(" ,\t\n\r#", pos_);
        break;
    }
  }

  void SkipWhitespace() {
    switch (comma_mode_) {
      case PARSE_HOSTS_COMMA_IS_TOKEN:
        pos_ = text_.find_first_not_of(" \t", pos_);
        break;
      case PARSE_HOSTS_COMMA_IS_WHITESPACE:
        pos_ = text_.find_first_not_of(" ,\t", pos_);
        break;
    }
  }

  const std::string_view text_;
  const char* data_;
  const size_t end_;

  size_t pos_ = 0;
  std::string_view token_;
  bool token_is_ip_ = false;

  const ParseHostsCommaMode comma_mode_;
};

void ParseHostsWithCommaMode(const std::string& contents,
                             DnsHosts* dns_hosts,
                             ParseHostsCommaMode comma_mode) {
  CHECK(dns_hosts);

  std::string_view ip_text;
  IPAddress ip;
  AddressFamily family = ADDRESS_FAMILY_IPV4;
  HostsParser parser(contents, comma_mode);
  while (parser.Advance()) {
    if (parser.token_is_ip()) {
      std::string_view new_ip_text = parser.token();
      // Some ad-blocking hosts files contain thousands of entries pointing to
      // the same IP address (usually 127.0.0.1).  Don't bother parsing the IP
      // again if it's the same as the one above it.
      if (new_ip_text != ip_text) {
        IPAddress new_ip;
        if (new_ip.AssignFromIPLiteral(parser.token())) {
          ip_text = new_ip_text;
          ip = new_ip;
          family = (ip.IsIPv4()) ? ADDRESS_FAMILY_IPV4 : ADDRESS_FAMILY_IPV6;
        } else {
          parser.SkipRestOfLine();
        }
      }
    } else {
      url::CanonHostInfo canonicalization_info;
      std::string canonicalized_host =
          CanonicalizeHost(parser.token(), &canonicalization_info);

      // Skip if token is invalid for host canonicalization, or if it
      // canonicalizes as an IP address.
      if (canonicalization_info.family != url::CanonHostInfo::NEUTRAL)
        continue;

      DnsHostsKey key(std::move(canonicalized_host), family);
      if (!IsCanonicalizedHostCompliant(key.first))
        continue;
      IPAddress* mapped_ip = &(*dns_hosts)[key];
      if (mapped_ip->empty())
        *mapped_ip = ip;
      // else ignore this entry (first hit counts)
    }
  }
}

}  // namespace

void ParseHostsWithCommaModeForTesting(const std::string& contents,
                                       DnsHosts* dns_hosts,
                                       ParseHostsCommaMode comma_mode) {
  ParseHostsWithCommaMode(contents, dns_hosts, comma_mode);
}

void ParseHosts(const std::string& contents, DnsHosts* dns_hosts) {
  ParseHostsCommaMode comma_mode;
#if BUILDFLAG(IS_APPLE)
  // Mac OS X allows commas to separate hostnames.
  comma_mode = PARSE_HOSTS_COMMA_IS_WHITESPACE;
#else
  // Linux allows commas in hostnames.
  comma_mode = PARSE_HOSTS_COMMA_IS_TOKEN;
#endif

  ParseHostsWithCommaMode(contents, dns_hosts, comma_mode);

  // TODO(crbug.com/40874231): Remove this when we have enough data.
  base::UmaHistogramCounts100000("Net.DNS.DnsHosts.Count", dns_hosts->size());

#if !BUILDFLAG(CRONET_BUILD)
  // Cronet disables tracing and doesn't provide an implementation of
  // base::trace_event::EstimateMemoryUsage for DnsHosts. Having this
  // conditional is preferred over a fake implementation to avoid reporting fake
  // metrics.
  base::UmaHistogramMemoryKB(
      "Net.DNS.DnsHosts.EstimateMemoryUsage",
      base::trace_event::EstimateMemoryUsage(*dns_hosts));
#endif  // !BUILDFLAG(CRONET_BUILD)
}

DnsHostsParser::~DnsHostsParser() = default;

DnsHostsFileParser::DnsHostsFileParser(base::FilePath hosts_file_path)
    : hosts_file_path_(std::move(hosts_file_path)) {}

DnsHostsFileParser::~DnsHostsFileParser() = default;

bool DnsHostsFileParser::ParseHosts(DnsHosts* dns_hosts) const {
  dns_hosts->clear();
  // Missing file indicates empty HOSTS.
  if (!base::PathExists(hosts_file_path_))
    return true;

  std::optional<int64_t> size = base::GetFileSize(hosts_file_path_);
  if (!size.has_value()) {
    return false;
  }

  // Reject HOSTS files larger than |kMaxHostsSize| bytes.
  const int64_t kMaxHostsSize = 1 << 25;  // 32MB

  // TODO(crbug.com/40874231): Remove this when we have enough data.
  base::UmaHistogramCustomCounts("Net.DNS.DnsHosts.FileSize", size.value(), 1,
                                 kMaxHostsSize * 2, 50);
  if (size.value() > kMaxHostsSize) {
    return false;
  }

  std::string contents;
  if (!base::ReadFileToString(hosts_file_path_, &contents))
    return false;

  net::ParseHosts(contents, dns_hosts);
  return true;
}

}  // namespace net
