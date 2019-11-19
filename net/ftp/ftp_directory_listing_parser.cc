// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/ftp/ftp_directory_listing_parser_ls.h"
#include "net/ftp/ftp_directory_listing_parser_vms.h"
#include "net/ftp/ftp_directory_listing_parser_windows.h"
#include "net/ftp/ftp_server_type.h"

namespace net {

namespace {

// Fills in |raw_name| for all |entries| using |encoding|. Returns network
// error code.
int FillInRawName(const std::string& encoding,
                  std::vector<FtpDirectoryListingEntry>* entries) {
  for (size_t i = 0; i < entries->size(); i++) {
    if (!base::UTF16ToCodepage(entries->at(i).name, encoding.c_str(),
                               base::OnStringConversionError::SUBSTITUTE,
                               &entries->at(i).raw_name)) {
      return ERR_ENCODING_CONVERSION_FAILED;
    }
  }

  return OK;
}

// Parses |text| as an FTP directory listing. Fills in |entries|
// and |server_type| and returns network error code.
int ParseListing(const base::string16& text,
                 const base::string16& newline_separator,
                 const std::string& encoding,
                 const base::Time& current_time,
                 std::vector<FtpDirectoryListingEntry>* entries,
                 FtpServerType* server_type) {
  std::vector<base::string16> lines = base::SplitStringUsingSubstr(
      text, newline_separator, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  struct {
    base::Callback<bool(void)> callback;
    FtpServerType server_type;
  } parsers[] = {
    {
      base::Bind(&ParseFtpDirectoryListingLs, lines, current_time, entries),
      SERVER_LS
    },
    {
      base::Bind(&ParseFtpDirectoryListingWindows, lines, entries),
      SERVER_WINDOWS
    },
    {
      base::Bind(&ParseFtpDirectoryListingVms, lines, entries),
      SERVER_VMS
    },
  };

  for (size_t i = 0; i < base::size(parsers); i++) {
    entries->clear();
    if (parsers[i].callback.Run()) {
      *server_type = parsers[i].server_type;
      return FillInRawName(encoding, entries);
    }
  }

  entries->clear();
  return ERR_UNRECOGNIZED_FTP_DIRECTORY_LISTING_FORMAT;
}

// Detects encoding of |text| and parses it as an FTP directory listing.
// Fills in |entries| and |server_type| and returns network error code.
int DecodeAndParse(const std::string& text,
                   const base::Time& current_time,
                   std::vector<FtpDirectoryListingEntry>* entries,
                   FtpServerType* server_type) {
  std::string encoding;
  if (!base::DetectEncoding(text, &encoding))
    return ERR_ENCODING_DETECTION_FAILED;
  const char* encoding_name = encoding.c_str();

  base::string16 converted_text;
  if (base::CodepageToUTF16(text, encoding_name,
                            base::OnStringConversionError::SUBSTITUTE,
                            &converted_text)) {
    const char* const kNewlineSeparators[] = {"\n", "\r\n"};

    for (size_t j = 0; j < base::size(kNewlineSeparators); j++) {
      int rv = ParseListing(converted_text,
                            base::ASCIIToUTF16(kNewlineSeparators[j]),
                            encoding_name, current_time, entries, server_type);
      if (rv == OK)
        return rv;
    }
  }

  entries->clear();
  *server_type = SERVER_UNKNOWN;
  return ERR_UNRECOGNIZED_FTP_DIRECTORY_LISTING_FORMAT;
}

}  // namespace

FtpDirectoryListingEntry::FtpDirectoryListingEntry()
    : type(UNKNOWN),
      size(-1) {
}

int ParseFtpDirectoryListing(const std::string& text,
                             const base::Time& current_time,
                             std::vector<FtpDirectoryListingEntry>* entries) {
  FtpServerType server_type = SERVER_UNKNOWN;
  int rv = DecodeAndParse(text, current_time, entries, &server_type);
  return rv;
}

}  // namespace net
