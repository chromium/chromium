// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A library to manage RLZ information for access-points shared
// across different client applications.

#include "rlz/lib/rlz_lib.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/backoff_entry.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/financial_ping.h"
#include "rlz/lib/lib_values.h"
#include "rlz/lib/net_response_check.h"
#include "rlz/lib/rlz_value_store.h"
#include "rlz/lib/string_utils.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "rlz/lib/machine_deal_win.h"
#endif

namespace {

// Event information returned from ping response.
struct ReturnedEvent {
  rlz_lib::AccessPoint access_point;
  rlz_lib::Event event_type;
};

// Helper functions

bool IsAccessPointSupported(rlz_lib::AccessPoint point) {
  switch (point) {
  case rlz_lib::NO_ACCESS_POINT:
  case rlz_lib::LAST_ACCESS_POINT:

  case rlz_lib::MOBILE_IDLE_SCREEN_BLACKBERRY:
  case rlz_lib::MOBILE_IDLE_SCREEN_WINMOB:
  case rlz_lib::MOBILE_IDLE_SCREEN_SYMBIAN:
    // These AP's are never available on Windows PCs.
    return false;

  case rlz_lib::IE_DEFAULT_SEARCH:
  case rlz_lib::IE_HOME_PAGE:
  case rlz_lib::IETB_SEARCH_BOX:
  case rlz_lib::QUICK_SEARCH_BOX:
  case rlz_lib::GD_DESKBAND:
  case rlz_lib::GD_SEARCH_GADGET:
  case rlz_lib::GD_WEB_SERVER:
  case rlz_lib::GD_OUTLOOK:
  case rlz_lib::CHROME_OMNIBOX:
  case rlz_lib::CHROME_HOME_PAGE:
    // TODO: Figure out when these settings are set to Google.

  default:
    return true;
  }
}

// Current RLZ can only use [a-zA-Z0-9_\-]
// We will be more liberal and allow some additional chars, but not url meta
// chars.
bool IsGoodRlzChar(const char ch) {
  if (base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch))
    return true;

  switch (ch) {
    case '_':
    case '-':
    case '!':
    case '@':
    case '$':
    case '*':
    case '(':
    case ')':
    case ';':
    case '.':
    case '<':
    case '>':
    return true;
  }

  return false;
}

// This function will remove bad rlz chars and also limit the max rlz to some
// reasonable size.  It also assumes that normalized_rlz is at least
// kMaxRlzLength+1 long.
std::string NormalizeRlz(std::string_view raw_rlz) {
  std::string normalized_rlz;
  for (char current : raw_rlz.substr(0, rlz_lib::kMaxRlzLength)) {
    normalized_rlz.push_back(IsGoodRlzChar(current) ? current : '.');
  }
  return normalized_rlz;
}

void GetEventsFromResponseString(
    const std::string& response_line,
    const std::string& field_header,
    std::vector<ReturnedEvent>* event_array) {
  // Get the string of events.
  std::string events = response_line.substr(field_header.size());
  base::TrimWhitespaceASCII(events, base::TRIM_LEADING, &events);

  int events_length = events.find_first_of("\r\n ");
  if (events_length < 0)
    events_length = events.size();
  events = events.substr(0, events_length);

  // Break this up into individual events
  int event_end_index = -1;
  do {
    int event_begin = event_end_index + 1;
    event_end_index = events.find(rlz_lib::kEventsCgiSeparator, event_begin);
    int event_end = event_end_index;
    if (event_end < 0)
      event_end = events_length;

    std::string event_string = events.substr(event_begin,
                                             event_end - event_begin);
    if (event_string.size() != 3)  // 3 = 2(AP) + 1(E)
      continue;

    rlz_lib::AccessPoint point = rlz_lib::NO_ACCESS_POINT;
    rlz_lib::Event event = rlz_lib::INVALID_EVENT;
    std::string_view event_sv(event_string);
    if (!GetAccessPointFromName(event_sv.substr(0, 2), &point) ||
        point == rlz_lib::NO_ACCESS_POINT) {
      continue;
    }

    if (!GetEventFromName(event_sv.substr(2), &event) ||
        event == rlz_lib::INVALID_EVENT) {
      continue;
    }

    ReturnedEvent current_event = {point, event};
    event_array->push_back(current_event);
  } while (event_end_index >= 0);
}

// Event storage functions.
bool RecordStatefulEvent(rlz_lib::Product product, rlz_lib::AccessPoint point,
                         rlz_lib::Event event) {
  rlz_lib::ScopedRlzValueStoreLock lock;
  rlz_lib::RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(rlz_lib::RlzValueStore::kWriteAccess))
    return false;

  // Write the new event to the value store.
  std::string_view point_name = GetAccessPointName(point);
  std::string_view event_name = GetEventName(event);
  if (point_name.empty() || event_name.empty()) {
    return false;
  }

  std::string new_event_value = base::StrCat({point_name, event_name});
  return store->AddStatefulEvent(product, new_event_value);
}

std::optional<std::string> GetProductEventsAsCgiHelper(
    rlz_lib::Product product,
    rlz_lib::RlzValueStore* store) {
  // Read stored events.
  std::vector<std::string> events = store->ReadProductEvents(product);
  if (events.empty()) {
    return std::nullopt;
  }

  std::ranges::sort(events);

  return base::StrCat(
      {rlz_lib::kEventsCgiVariable, "=",
       base::JoinString(events, std::string(1, rlz_lib::kEventsCgiSeparator))});
}

}  // namespace

namespace rlz_lib {

bool SetURLLoaderFactory(network::mojom::URLLoaderFactory* factory) {
  return FinancialPing::SetURLLoaderFactory(factory);
}

std::optional<std::string> GetProductEventsAsCgi(Product product) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kReadAccess)) {
    return std::nullopt;
  }

  return GetProductEventsAsCgiHelper(product, store);
}

bool RecordProductEvent(Product product, AccessPoint point, Event event) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kWriteAccess))
    return false;

  // Get this event's value.
  std::string_view point_name = GetAccessPointName(point);
  std::string_view event_name = GetEventName(event);
  if (point_name.empty() || event_name.empty()) {
    return false;
  }

  std::string new_event_value = base::StrCat({point_name, event_name});

  // Check whether this event is a stateful event. If so, don't record it.
  if (store->IsStatefulEvent(product, new_event_value)) {
    // For a stateful event we skip recording, this function is also
    // considered successful.
    return true;
  }

  // Write the new event to the value store.
  return store->AddProductEvent(product, new_event_value);
}

bool ClearProductEvent(Product product, AccessPoint point, Event event) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kWriteAccess))
    return false;

  // Get the event's value store value and delete it.
  std::string_view point_name = GetAccessPointName(point);
  std::string_view event_name = GetEventName(event);
  if (point_name.empty() || event_name.empty()) {
    return false;
  }

  std::string event_value = base::StrCat({point_name, event_name});
  return store->ClearProductEvent(product, event_value);
}

// RLZ storage functions.

std::optional<std::string> GetAccessPointRlz(AccessPoint point) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kReadAccess)) {
    return std::nullopt;
  }

  if (!IsAccessPointSupported(point)) {
    return std::nullopt;
  }

  return store->ReadAccessPointRlz(point);
}

bool SetAccessPointRlz(AccessPoint point, const char* new_rlz) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kWriteAccess))
    return false;

  if (!new_rlz) {
    ASSERT_STRING("SetAccessPointRlz: Invalid buffer");
    return false;
  }

  // Return false if the access point is not set to Google.
  if (!IsAccessPointSupported(point)) {
    ASSERT_STRING(("SetAccessPointRlz: "
                "Cannot set RLZ for unsupported access point."));
    return false;
  }

  // Verify the RLZ length.
  size_t rlz_length = strlen(new_rlz);
  if (rlz_length > kMaxRlzLength) {
    ASSERT_STRING("SetAccessPointRlz: RLZ length is exceeds max allowed.");
    return false;
  }

  std::string normalized_rlz = NormalizeRlz(new_rlz);
  VERIFY(strlen(new_rlz) == rlz_length);

  // Setting RLZ to empty == clearing.
  if (normalized_rlz.empty()) {
    return store->ClearAccessPointRlz(point);
  }
  return store->WriteAccessPointRlz(point, normalized_rlz);
}

bool UpdateExistingAccessPointRlz(const std::string& brand) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kWriteAccess))
    return false;
  return store->UpdateExistingAccessPointRlz(brand);
}

// Financial Server pinging functions.

std::optional<std::string> FormFinancialPingRequest(
    Product product,
    const AccessPoint* access_points,
    const char* product_signature,
    const char* product_brand,
    const char* product_id,
    const char* product_lang,
    bool exclude_machine_id) {
  std::string request;
  if (!FinancialPing::FormRequest(product, access_points, product_signature,
                                  product_brand, product_id, product_lang,
                                  exclude_machine_id, &request)) {
    return std::nullopt;
  }
  return request;
}

// Complex helpers built on top of other functions.

bool ParseFinancialPingResponse(Product product, const char* response) {
  // Update the last ping time irrespective of success.
  FinancialPing::UpdateLastPingTime(product);
  // Parse the ping response - update RLZs, clear events.
  return ParsePingResponse(product, response);
}

bool SendFinancialPing(Product product,
                       const AccessPoint* access_points,
                       const char* product_signature,
                       const char* product_brand,
                       const char* product_id,
                       const char* product_lang,
                       bool exclude_machine_id) {
  return SendFinancialPing(product, access_points, product_signature,
                           product_brand, product_id, product_lang,
                           exclude_machine_id, false);
}

bool SendFinancialPing(Product product,
                       const AccessPoint* access_points,
                       const char* product_signature,
                       const char* product_brand,
                       const char* product_id,
                       const char* product_lang,
                       bool exclude_machine_id,
                       const bool skip_time_check) {
  // Create the financial ping request.
  std::string request;
  if (!FinancialPing::FormRequest(product, access_points, product_signature,
                                  product_brand, product_id, product_lang,
                                  exclude_machine_id, &request))
    return false;

  // Check if the time is right to ping.
  if (!FinancialPing::IsPingTime(product, skip_time_check))
    return false;

  // Send out the ping, update the last ping time irrespective of success.
  FinancialPing::UpdateLastPingTime(product);

  SYSLOG(INFO) << "Attempting to send RLZ ping brand=" << product_brand;
  std::string response;
  FinancialPing::PingResponse res =
      FinancialPing::PingServer(request.c_str(), &response);
  if (res != FinancialPing::PING_SUCCESSFUL) {
    SYSLOG(INFO) << "Failed sending RLZ";
    return false;
  }

  SYSLOG(INFO) << "Succeeded in sending RLZ ping";
  return ParsePingResponse(product, response.c_str());
}

// TODO: Use something like RSA to make sure the response is
// from a Google server.
bool ParsePingResponse(Product product, const char* response) {
  rlz_lib::ScopedRlzValueStoreLock lock;
  rlz_lib::RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(rlz_lib::RlzValueStore::kWriteAccess))
    return false;

  std::string response_string(response);
  int response_length = -1;
  if (!IsPingResponseValid(response, &response_length))
    return false;

  if (0 == response_length)
    return true;  // Empty response - no parsing.

  std::string events_variable = base::StringPrintf("%s: ", kEventsCgiVariable);
  std::string stateful_events_variable =
      base::StringPrintf("%s: ", kStatefulEventsCgiVariable);

  int rlz_cgi_length = strlen(kRlzCgiVariable);

  // Split response lines. Expected response format is lines of the form:
  // rlzW1: 1R1_____en__252
  int line_end_index = -1;
  do {
    int line_begin = line_end_index + 1;
    line_end_index = response_string.find("\n", line_begin);

    int line_end = line_end_index;
    if (line_end < 0)
      line_end = response_length;

    if (line_end <= line_begin)
      continue;  // Empty line.

    std::string response_line;
    response_line = response_string.substr(line_begin, line_end - line_begin);

    if (base::StartsWith(response_line, kRlzCgiVariable,
                         base::CompareCase::SENSITIVE)) {  // An RLZ.
      int separator_index = -1;
      if ((separator_index = response_line.find(": ")) < 0)
        continue;  // Not a valid key-value pair.

      // Get the access point.
      std::string point_name =
        response_line.substr(3, separator_index - rlz_cgi_length);
      AccessPoint point = NO_ACCESS_POINT;
      if (!GetAccessPointFromName(point_name, &point) ||
          point == NO_ACCESS_POINT) {
        continue;  // Not a valid access point.
      }

      // Get the new RLZ.
      std::string rlz_value(response_line.substr(separator_index + 2));
      base::TrimWhitespaceASCII(rlz_value, base::TRIM_LEADING, &rlz_value);

      size_t rlz_length = rlz_value.find_first_of("\r\n ");
      if (rlz_length == std::string::npos)
        rlz_length = rlz_value.size();

      if (rlz_length > kMaxRlzLength)
        continue;  // Too long.

      if (IsAccessPointSupported(point))
        SetAccessPointRlz(point, rlz_value.substr(0, rlz_length).c_str());
    } else if (base::StartsWith(response_line, events_variable,
                                base::CompareCase::SENSITIVE)) {
      // Clear events which server parsed.
      std::vector<ReturnedEvent> event_array;
      GetEventsFromResponseString(response_line, events_variable, &event_array);
      for (const auto& event : event_array) {
        ClearProductEvent(product, event.access_point, event.event_type);
      }
    } else if (base::StartsWith(response_line, stateful_events_variable,
                                base::CompareCase::SENSITIVE)) {
      // Record any stateful events the server send over.
      std::vector<ReturnedEvent> event_array;
      GetEventsFromResponseString(response_line, stateful_events_variable,
                                  &event_array);
      for (const auto& event : event_array) {
        RecordStatefulEvent(product, event.access_point, event.event_type);
      }
    }
  } while (line_end_index >= 0);

#if BUILDFLAG(IS_WIN)
  // Update the DCC in registry if needed.
  SetMachineDealCodeFromPingResponse(response);
#endif

  return true;
}

std::optional<std::string> GetPingParams(Product product,
                                         const AccessPoint* access_points) {
  if (!access_points) {
    ASSERT_STRING("GetPingParams: access_points is NULL");
    return std::nullopt;
  }

  // Add the RLZ Exchange Protocol version.
  std::string cgi_string(kProtocolCgiArgument);

  // Copy the &rlz= over.
  base::StringAppendF(&cgi_string, "&%s=", kRlzCgiVariable);

  {
    // Now add each of the RLZ's. Keep the lock during all GetAccessPointRlz()
    // calls below.
    ScopedRlzValueStoreLock lock;
    RlzValueStore* store = lock.GetStore();
    if (!store || !store->HasAccess(RlzValueStore::kReadAccess)) {
      return std::nullopt;
    }
    bool first_rlz = true;  // comma before every RLZ but the first.
    // SAFETY: `access_points` is an array of AccessPoints terminated by
    // `NO_ACCESS_POINT` per the function contract.
    for (const AccessPoint* ap = access_points; *ap != NO_ACCESS_POINT;
         UNSAFE_BUFFERS(++ap)) {
      if (std::optional<std::string> rlz = GetAccessPointRlz(*ap)) {
        std::string_view access_point = GetAccessPointName(*ap);
        if (access_point.empty()) {
          continue;
        }

        base::StrAppend(&cgi_string, {first_rlz ? "" : kRlzCgiSeparator,
                                      access_point, kRlzCgiIndicator, *rlz});
        first_rlz = false;
      }
    }

#if BUILDFLAG(IS_WIN)
    // Report the DCC too if present. DCCs are windows-only.
    if (std::optional<std::string> dcc = GetMachineDealCode()) {
      base::StringAppendF(&cgi_string, "&%s=%s", kDccCgiVariable, dcc->c_str());
    }
#endif
  }

  return cgi_string;
}

}  // namespace rlz_lib
