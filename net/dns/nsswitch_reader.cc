// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/nsswitch_reader.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <netdb.h>
#endif  // defined (OS_POSIX)

namespace net {

namespace {

#ifdef _PATH_NSSWITCH_CONF
constexpr base::FilePath::CharType kNsswitchPath[] =
    FILE_PATH_LITERAL(_PATH_NSSWITCH_CONF);
#else
constexpr base::FilePath::CharType kNsswitchPath[] =
    FILE_PATH_LITERAL("/etc/nsswitch.conf");
#endif

// Choose 1 MiB as the largest handled filesize. Arbitrarily chosen as seeming
// large enough to handle any reasonable file contents and similar to the size
// limit for HOSTS files (32 MiB).
constexpr size_t kMaxFileSize = 1024 * 1024;

std::string ReadNsswitch() {
  std::string file;
  if (!base::ReadFileToStringWithMaxSize(base::FilePath(kNsswitchPath), &file,
                                         kMaxFileSize))
    return "";

  return file;
}

base::StringPiece SkipRestOfLine(base::StringPiece text) {
  base::StringPiece::size_type line_end = text.find('\n');
  if (line_end == base::StringPiece::npos)
    return "";
  return text.substr(line_end);
}

// In case of multiple entries for `database_name`, finds only the first.
base::StringPiece FindDatabase(base::StringPiece text,
                               base::StringPiece database_name) {
  DCHECK(!text.empty());
  DCHECK(!database_name.empty());
  DCHECK(!base::StartsWith(database_name, "#"));
  DCHECK(!base::IsAsciiWhitespace(database_name.front()));
  DCHECK(base::EndsWith(database_name, ":"));

  while (!text.empty()) {
    text = base::TrimWhitespaceASCII(text, base::TrimPositions::TRIM_LEADING);

    if (base::StartsWith(text, database_name,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      DCHECK(!base::StartsWith(text, "#"));

      text = text.substr(database_name.size());
      base::StringPiece::size_type line_end = text.find('\n');
      if (line_end != base::StringPiece::npos)
        text = text.substr(0, line_end);

      return base::TrimWhitespaceASCII(text, base::TrimPositions::TRIM_ALL);
    }

    text = SkipRestOfLine(text);
  }

  return "";
}

NsswitchReader::ServiceAction TokenizeAction(base::StringPiece column) {
  NsswitchReader::ServiceAction result = {/*negated=*/false,
                                          NsswitchReader::Status::kUnknown,
                                          NsswitchReader::Action::kUnknown};

  if (column.front() != '[' || column.back() != ']')
    return result;
  column = column.substr(1, column.size() - 2);

  std::vector<base::StringPiece> split = base::SplitStringPiece(
      column, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split.size() != 2)
    return result;

  if (split[0].size() >= 2 && split[0].front() == '!') {
    result.negated = true;
    split[0] = split[0].substr(1);
  }

  if (base::EqualsCaseInsensitiveASCII(split[0], "SUCCESS")) {
    result.status = NsswitchReader::Status::kSuccess;
  } else if (base::EqualsCaseInsensitiveASCII(split[0], "NOTFOUND")) {
    result.status = NsswitchReader::Status::kNotFound;
  } else if (base::EqualsCaseInsensitiveASCII(split[0], "UNAVAIL")) {
    result.status = NsswitchReader::Status::kUnavailable;
  } else if (base::EqualsCaseInsensitiveASCII(split[0], "TRYAGAIN")) {
    result.status = NsswitchReader::Status::kTryAgain;
  }

  if (base::EqualsCaseInsensitiveASCII(split[1], "RETURN")) {
    result.action = NsswitchReader::Action::kReturn;
  } else if (base::EqualsCaseInsensitiveASCII(split[1], "CONTINUE")) {
    result.action = NsswitchReader::Action::kContinue;
  } else if (base::EqualsCaseInsensitiveASCII(split[1], "MERGE")) {
    result.action = NsswitchReader::Action::kMerge;
  }

  return result;
}

std::vector<NsswitchReader::ServiceSpecification> TokenizeDatabase(
    base::StringPiece database) {
  std::vector<NsswitchReader::ServiceSpecification> tokenized;

  for (const auto& column : base::SplitStringPiece(
           database, base::kWhitespaceASCII, base::KEEP_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    DCHECK(!column.empty());

    // Note: Assuming comments can only be started at the start of a column.
    if (base::StartsWith(column, "#")) {
      // Once a comment is hit, the rest of the database is comment.
      return tokenized;
    }

    if (column.front() == '[') {
      // Actions are expected to come after a service.
      if (tokenized.empty()) {
        tokenized.emplace_back(NsswitchReader::Service::kUnknown);
      }

      tokenized.back().actions.push_back(TokenizeAction(column));
    } else if (base::EqualsCaseInsensitiveASCII(column, "files")) {
      tokenized.emplace_back(NsswitchReader::Service::kFiles);
    } else if (base::EqualsCaseInsensitiveASCII(column, "dns")) {
      tokenized.emplace_back(NsswitchReader::Service::kDns);
    } else if (base::EqualsCaseInsensitiveASCII(column, "mdns")) {
      tokenized.emplace_back(NsswitchReader::Service::kMdns);
    } else if (base::EqualsCaseInsensitiveASCII(column, "mdns4")) {
      tokenized.emplace_back(NsswitchReader::Service::kMdns4);
    } else if (base::EqualsCaseInsensitiveASCII(column, "mdns6")) {
      tokenized.emplace_back(NsswitchReader::Service::kMdns6);
    } else if (base::EqualsCaseInsensitiveASCII(column, "mdns_minimal")) {
      tokenized.emplace_back(NsswitchReader::Service::kMdnsMinimal);
    } else if (base::EqualsCaseInsensitiveASCII(column, "mdns4_minimal")) {
      tokenized.emplace_back(NsswitchReader::Service::kMdns4Minimal);
    } else if (base::EqualsCaseInsensitiveASCII(column, "mdns6_minimal")) {
      tokenized.emplace_back(NsswitchReader::Service::kMdns6Minimal);
    } else if (base::EqualsCaseInsensitiveASCII(column, "myhostname")) {
      tokenized.emplace_back(NsswitchReader::Service::kMyHostname);
    } else if (base::EqualsCaseInsensitiveASCII(column, "resolve")) {
      tokenized.emplace_back(NsswitchReader::Service::kResolve);
    } else if (base::EqualsCaseInsensitiveASCII(column, "nis")) {
      tokenized.emplace_back(NsswitchReader::Service::kNis);
    } else {
      tokenized.emplace_back(NsswitchReader::Service::kUnknown);
    }
  }

  return tokenized;
}

std::vector<NsswitchReader::ServiceSpecification> GetDefaultHosts() {
  return {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)};
}

}  // namespace

NsswitchReader::ServiceSpecification::ServiceSpecification(
    Service service,
    std::vector<ServiceAction> actions)
    : service(service), actions(std::move(actions)) {}

NsswitchReader::ServiceSpecification::~ServiceSpecification() = default;

NsswitchReader::ServiceSpecification::ServiceSpecification(
    const ServiceSpecification&) = default;

NsswitchReader::ServiceSpecification&
NsswitchReader::ServiceSpecification::operator=(const ServiceSpecification&) =
    default;

NsswitchReader::ServiceSpecification::ServiceSpecification(
    ServiceSpecification&&) = default;

NsswitchReader::ServiceSpecification&
NsswitchReader::ServiceSpecification::operator=(ServiceSpecification&&) =
    default;

NsswitchReader::NsswitchReader()
    : file_read_call_(base::BindRepeating(&ReadNsswitch)) {}

NsswitchReader::~NsswitchReader() = default;

std::vector<NsswitchReader::ServiceSpecification>
NsswitchReader::ReadAndParseHosts() {
  std::string file = file_read_call_.Run();
  if (file.empty())
    return GetDefaultHosts();

  base::StringPiece hosts = FindDatabase(file, "hosts:");
  if (hosts.empty())
    return GetDefaultHosts();

  return TokenizeDatabase(hosts);
}

}  // namespace net
