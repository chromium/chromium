// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/nsswitch_reader.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
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
  bool result = base::ReadFileToStringWithMaxSize(base::FilePath(kNsswitchPath),
                                                  &file, kMaxFileSize);
  UMA_HISTOGRAM_BOOLEAN("Net.DNS.DnsConfig.Nsswitch.Read",
                        result || file.size() == kMaxFileSize);
  UMA_HISTOGRAM_BOOLEAN("Net.DNS.DnsConfig.Nsswitch.TooLarge",
                        !result && file.size() == kMaxFileSize);

  if (result)
    return file;

  return "";
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

NsswitchReader::ServiceAction TokenizeAction(base::StringPiece action_column) {
  DCHECK(!action_column.empty());
  DCHECK_EQ(action_column.find(']'), base::StringPiece::npos);
  DCHECK_EQ(action_column.find_first_of(base::kWhitespaceASCII),
            base::StringPiece::npos);

  NsswitchReader::ServiceAction result = {/*negated=*/false,
                                          NsswitchReader::Status::kUnknown,
                                          NsswitchReader::Action::kUnknown};

  std::vector<base::StringPiece> split = base::SplitStringPiece(
      action_column, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
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

std::vector<NsswitchReader::ServiceAction> TokenizeActions(
    base::StringPiece actions) {
  DCHECK(!actions.empty());
  DCHECK_NE(actions.front(), '[');
  DCHECK_EQ(actions.find(']'), base::StringPiece::npos);
  DCHECK(!base::IsAsciiWhitespace(actions.front()));

  std::vector<NsswitchReader::ServiceAction> result;

  for (const auto& action_column : base::SplitStringPiece(
           actions, base::kWhitespaceASCII, base::KEEP_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    DCHECK(!action_column.empty());
    result.push_back(TokenizeAction(action_column));
  }

  return result;
}

NsswitchReader::ServiceSpecification TokenizeService(
    base::StringPiece service_column) {
  DCHECK(!service_column.empty());
  DCHECK_EQ(service_column.find_first_of(base::kWhitespaceASCII),
            base::StringPiece::npos);
  DCHECK_NE(service_column.front(), '[');

  if (base::EqualsCaseInsensitiveASCII(service_column, "files")) {
    return NsswitchReader::ServiceSpecification(
        NsswitchReader::Service::kFiles);
  }
  if (base::EqualsCaseInsensitiveASCII(service_column, "dns")) {
    return NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns);
  }
  if (base::EqualsCaseInsensitiveASCII(service_column, "mdns")) {
    return NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns);
  }
  if (base::EqualsCaseInsensitiveASCII(service_column, "mdns4")) {
    return NsswitchReader::ServiceSpecification(
        NsswitchReader::Service::kMdns4);
  }
  if (base::EqualsCaseInsensitiveASCII(service_column, "mdns6")) {
    return NsswitchReader::ServiceSpecification(
        NsswitchReader::Service::kMdns6);
  }
  if (base::EqualsCaseInsensitiveASCII(service_column, "mdns_minimal")) {
    return NsswitchReader::ServiceSpecification(
        NsswitchReader::Service::kMdnsMinimal);
  }
  if (base::EqualsCaseInsensitiveASCII(service_column, "mdns4_minimal")) {
    return NsswitchReader::ServiceSpecification(
        NsswitchReader::Service::kMdns4Minimal);
  }
  if (base::EqualsCaseInsensitiveASCII(service_column, "mdns6_minimal")) {
    return NsswitchReader::ServiceSpecification(
        NsswitchReader::Service::kMdns6Minimal);
  }
  if (base::EqualsCaseInsensitiveASCII(service_column, "myhostname")) {
    return NsswitchReader::ServiceSpecification(
        NsswitchReader::Service::kMyHostname);
  }
  if (base::EqualsCaseInsensitiveASCII(service_column, "resolve")) {
    return NsswitchReader::ServiceSpecification(
        NsswitchReader::Service::kResolve);
  }
  if (base::EqualsCaseInsensitiveASCII(service_column, "nis")) {
    return NsswitchReader::ServiceSpecification(NsswitchReader::Service::kNis);
  }

  return NsswitchReader::ServiceSpecification(
      NsswitchReader::Service::kUnknown);
}

// Returns the actions string without brackets. `out_num_bytes` returns number
// of bytes in the actions including brackets and trailing whitespace.
base::StringPiece GetActionsStringAndRemoveBrackets(base::StringPiece database,
                                                    size_t& out_num_bytes) {
  DCHECK(!database.empty());
  DCHECK_EQ(database.front(), '[');

  size_t action_end = database.find(']');

  base::StringPiece actions;
  if (action_end == base::StringPiece::npos) {
    actions = database.substr(1);
    out_num_bytes = database.size();
  } else {
    actions = database.substr(1, action_end - 1);
    out_num_bytes = action_end;
  }

  // Ignore repeated '[' at start of `actions`.
  actions =
      base::TrimWhitespaceASCII(actions, base::TrimPositions::TRIM_LEADING);
  while (!actions.empty() && actions.front() == '[') {
    actions = base::TrimWhitespaceASCII(actions.substr(1),
                                        base::TrimPositions::TRIM_LEADING);
  }

  // Include any trailing ']' and whitespace in `out_num_bytes`.
  while (out_num_bytes < database.size() &&
         (database[out_num_bytes] == ']' ||
          base::IsAsciiWhitespace(database[out_num_bytes]))) {
    ++out_num_bytes;
  }

  return actions;
}

std::vector<NsswitchReader::ServiceSpecification> TokenizeDatabase(
    base::StringPiece database) {
  std::vector<NsswitchReader::ServiceSpecification> tokenized;

  while (!database.empty()) {
    DCHECK(!base::IsAsciiWhitespace(database.front()));

    // Note: Assuming comments are not recognized mid-action or mid-service.
    if (database.front() == '#') {
      // Once a comment is hit, the rest of the database is comment.
      return tokenized;
    }

    if (database.front() == '[') {
      // Actions are expected to come after a service.
      if (tokenized.empty()) {
        tokenized.emplace_back(NsswitchReader::Service::kUnknown);
      }

      size_t num_actions_bytes = 0;
      base::StringPiece actions =
          GetActionsStringAndRemoveBrackets(database, num_actions_bytes);

      if (num_actions_bytes == database.size()) {
        database = "";
      } else {
        database = database.substr(num_actions_bytes);
      }

      if (!actions.empty()) {
        std::vector<NsswitchReader::ServiceAction> tokenized_actions =
            TokenizeActions(actions);
        tokenized.back().actions.insert(tokenized.back().actions.end(),
                                        tokenized_actions.begin(),
                                        tokenized_actions.end());
      }
    } else {
      size_t column_end = database.find_first_of(base::kWhitespaceASCII);

      base::StringPiece service_column;
      if (column_end == base::StringPiece::npos) {
        service_column = database;
        database = "";
      } else {
        service_column = database.substr(0, column_end);
        database = database.substr(column_end);
      }

      tokenized.push_back(TokenizeService(service_column));
    }

    database =
        base::TrimWhitespaceASCII(database, base::TrimPositions::TRIM_LEADING);
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
  UMA_HISTOGRAM_BOOLEAN("Net.DNS.DnsConfig.Nsswitch.HostsFound",
                        !hosts.empty());
  if (hosts.empty())
    return GetDefaultHosts();

  return TokenizeDatabase(hosts);
}

}  // namespace net
