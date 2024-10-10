// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_jobs.h"

#include <cups/ipp.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/version.h"
#include "printing/backend/cups_deleters.h"
#include "printing/backend/cups_helper.h"
#include "printing/backend/cups_ipp_helper.h"
#include "printing/backend/cups_weak_functions.h"
#include "printing/printer_status.h"

namespace printing {
namespace {

using PReason = PrinterStatus::PrinterReason::Reason;
using PSeverity = PrinterStatus::PrinterReason::Severity;

// printer attributes
constexpr char kPrinterUri[] = "printer-uri";
constexpr char kPrinterState[] = "printer-state";
constexpr char kPrinterStateReasons[] = "printer-state-reasons";
constexpr char kPrinterStateMessage[] = "printer-state-message";

constexpr std::string_view kPrinterMakeAndModel = "printer-make-and-model";
constexpr std::string_view kIppVersionsSupported = "ipp-versions-supported";
constexpr std::string_view kIppFeaturesSupported = "ipp-features-supported";
constexpr std::string_view kDocumentFormatSupported =
    "document-format-supported";
constexpr std::string_view kOauthAuthorizationServerUri =
    "oauth-authorization-server-uri";
constexpr std::string_view kOauthAuthorizationScope =
    "oauth-authorization-scope";

// job attributes
constexpr char kJobUri[] = "job-uri";
constexpr char kJobId[] = "job-id";
constexpr char kJobState[] = "job-state";
constexpr char kJobStateReasons[] = "job-state-reasons";
constexpr char kJobStateMessage[] = "job-state-message";
constexpr char kJobImpressionsCompleted[] = "job-impressions-completed";
constexpr char kTimeAtProcessing[] = "time-at-processing";

// request parameters
constexpr char kRequestedAttributes[] = "requested-attributes";
constexpr char kWhichJobs[] = "which-jobs";
constexpr char kLimit[] = "limit";

// request values
constexpr char kCompleted[] = "completed";
constexpr char kNotCompleted[] = "not-completed";

// ipp features
constexpr char kIppEverywhere[] = "ipp-everywhere";

// job state reason values
constexpr char kJobCompletedWithErrors[] = "job-completed-with-errors";
constexpr char kCupsHeldForAuthentication[] = "cups-held-for-authentication";

// printer state severities
constexpr char kSeverityReport[] = "report";
constexpr char kSeverityWarn[] = "warning";
constexpr char kSeverityError[] = "error";

// printer state reason values
constexpr char kNone[] = "none";
constexpr char kMediaNeeded[] = "media-needed";
constexpr char kMediaJam[] = "media-jam";
constexpr char kMovingToPaused[] = "moving-to-paused";
constexpr char kPaused[] = "paused";
constexpr char kShutdown[] = "shutdown";
constexpr char kConnectingToDevice[] = "connecting-to-device";
constexpr char kTimedOut[] = "timed-out";
constexpr char kStopping[] = "stopping";
constexpr char kStoppedPartly[] = "stopped-partly";
constexpr char kTonerLow[] = "toner-low";
constexpr char kTonerEmpty[] = "toner-empty";
constexpr char kSpoolAreaFull[] = "spool-area-full";
constexpr char kCoverOpen[] = "cover-open";
constexpr char kInterlockOpen[] = "interlock-open";
constexpr char kDoorOpen[] = "door-open";
constexpr char kInputTrayMissing[] = "input-tray-missing";
constexpr char kMediaLow[] = "media-low";
constexpr char kMediaEmpty[] = "media-empty";
constexpr char kOutputTrayMissing[] = "output-tray-missing";
constexpr char kOutputAreaAlmostFull[] = "output-area-almost-full";
constexpr char kOutputAreaFull[] = "output-area-full";
constexpr char kMarkerSupplyLow[] = "marker-supply-low";
constexpr char kMarkerSupplyEmpty[] = "marker-supply-empty";
constexpr char kMarkerWasteAlmostFull[] = "marker-waste-almost-full";
constexpr char kMarkerWasteFull[] = "marker-waste-full";
constexpr char kFuserOverTemp[] = "fuser-over-temp";
constexpr char kFuserUnderTemp[] = "fuser-under-temp";
constexpr char kOpcNearEol[] = "opc-near-eol";
constexpr char kOpcLifeOver[] = "opc-life-over";
constexpr char kDeveloperLow[] = "developer-low";
constexpr char kDeveloperEmpty[] = "developer-empty";
constexpr char kInterpreterResourceUnavailable[] =
    "interpreter-resource-unavailable";
constexpr char kCupsPkiExpired[] = "cups-pki-expired";

constexpr char kIppScheme[] = "ipp";
constexpr char kIppsScheme[] = "ipps";

// Timeout for establishing a HTTP connection in milliseconds.  Anecdotally,
// some print servers are slow and can use the extra time.
constexpr int kHttpConnectTimeoutMs = 1000;

constexpr std::array<const char* const, 3> kPrinterAttributes{
    {kPrinterState, kPrinterStateReasons, kPrinterStateMessage}};

constexpr std::array<const char* const, 9> kPrinterInfoAndStatus{
    {kPrinterMakeAndModel.data(), kIppVersionsSupported.data(),
     kIppFeaturesSupported.data(), kDocumentFormatSupported.data(),
     kPrinterState, kPrinterStateReasons, kPrinterStateMessage,
     kOauthAuthorizationServerUri.data(), kOauthAuthorizationScope.data()}};

// Converts an IPP attribute `attr` to the appropriate JobState enum.
CupsJob::JobState ToJobState(ipp_attribute_t* attr) {
  DCHECK_EQ(IPP_TAG_ENUM, ippGetValueTag(attr));
  int state = ippGetInteger(attr, 0);
  switch (state) {
    case IPP_JOB_ABORTED:
      return CupsJob::ABORTED;
    case IPP_JOB_CANCELLED:
      return CupsJob::CANCELED;
    case IPP_JOB_COMPLETED:
      return CupsJob::COMPLETED;
    case IPP_JOB_HELD:
      return CupsJob::HELD;
    case IPP_JOB_PENDING:
      return CupsJob::PENDING;
    case IPP_JOB_PROCESSING:
      return CupsJob::PROCESSING;
    case IPP_JOB_STOPPED:
      return CupsJob::STOPPED;
    default:
      NOTREACHED() << "Unidentifed state " << state;
  }
}

// Returns the Reason corresponding to the string `reason`.  Returns
// `PReason::kUnknownReason` if the string is not recognized.
PrinterStatus::PrinterReason::Reason ToReason(std::string_view reason) {
  // Returns a lookup map from strings to PrinterReason::Reason.
  static constexpr auto kLabelToReasonMap =
      base::MakeFixedFlatMap<std::string_view, PReason>({
          {kNone, PReason::kNone},
          {kMediaNeeded, PReason::kMediaNeeded},
          {kMediaJam, PReason::kMediaJam},
          {kMovingToPaused, PReason::kMovingToPaused},
          {kPaused, PReason::kPaused},
          {kShutdown, PReason::kShutdown},
          {kConnectingToDevice, PReason::kConnectingToDevice},
          {kTimedOut, PReason::kTimedOut},
          {kStopping, PReason::kStopping},
          {kStoppedPartly, PReason::kStoppedPartly},
          {kTonerLow, PReason::kTonerLow},
          {kTonerEmpty, PReason::kTonerEmpty},
          {kSpoolAreaFull, PReason::kSpoolAreaFull},
          {kCoverOpen, PReason::kCoverOpen},
          {kInterlockOpen, PReason::kInterlockOpen},
          {kDoorOpen, PReason::kDoorOpen},
          {kInputTrayMissing, PReason::kInputTrayMissing},
          {kMediaLow, PReason::kMediaLow},
          {kMediaEmpty, PReason::kMediaEmpty},
          {kOutputTrayMissing, PReason::kOutputTrayMissing},
          {kOutputAreaAlmostFull, PReason::kOutputAreaAlmostFull},
          {kOutputAreaFull, PReason::kOutputAreaFull},
          {kMarkerSupplyLow, PReason::kMarkerSupplyLow},
          {kMarkerSupplyEmpty, PReason::kMarkerSupplyEmpty},
          {kMarkerWasteAlmostFull, PReason::kMarkerWasteAlmostFull},
          {kMarkerWasteFull, PReason::kMarkerWasteFull},
          {kFuserOverTemp, PReason::kFuserOverTemp},
          {kFuserUnderTemp, PReason::kFuserUnderTemp},
          {kOpcNearEol, PReason::kOpcNearEol},
          {kOpcLifeOver, PReason::kOpcLifeOver},
          {kDeveloperLow, PReason::kDeveloperLow},
          {kDeveloperEmpty, PReason::kDeveloperEmpty},
          {kInterpreterResourceUnavailable,
           PReason::kInterpreterResourceUnavailable},
          {kCupsPkiExpired, PReason::kCupsPkiExpired},
      });

  const auto entry = kLabelToReasonMap.find(reason);
  return entry != kLabelToReasonMap.end() ? entry->second
                                          : PReason::kUnknownReason;
}

// Returns the Severity corresponding to `severity`.  Returns UNKNOWN_SEVERITY
// if the strin gis not recognized.
PSeverity ToSeverity(std::string_view severity) {
  if (severity == kSeverityError)
    return PSeverity::kError;

  if (severity == kSeverityWarn)
    return PSeverity::kWarning;

  if (severity == kSeverityReport)
    return PSeverity::kReport;

  return PSeverity::kUnknownSeverity;
}

// Parses the `reason` string into a PrinterReason.  Splits the string based on
// the last '-' to determine severity.  If a recognized severity is not
// included, severity is assumed to be ERROR per RFC2911.
PrinterStatus::PrinterReason ToPrinterReason(std::string_view reason) {
  PrinterStatus::PrinterReason parsed;

  if (reason == kNone) {
    parsed.reason = PReason::kNone;
    parsed.severity = PSeverity::kUnknownSeverity;
    return parsed;
  }

  size_t last_dash = reason.rfind('-');
  auto severity = PSeverity::kUnknownSeverity;
  if (last_dash != std::string_view::npos) {
    // try to parse the last part of the string as the severity.
    severity = ToSeverity(reason.substr(last_dash + 1));
  }

  if (severity == PSeverity::kUnknownSeverity) {
    // Severity is unknown.  No severity in the reason.
    // Per spec, if there is no severity, severity is error.
    parsed.severity = PSeverity::kError;
    parsed.reason = ToReason(reason);
  } else {
    parsed.severity = severity;
    // reason is the beginning of the string
    parsed.reason = ToReason(reason.substr(0, last_dash));
  }

  return parsed;
}

// Populates `collection` with the collection of strings in `attr`.
void ParseCollection(ipp_attribute_t* attr,
                     std::vector<std::string>* collection) {
  int count = ippGetCount(attr);
  for (int i = 0; i < count; i++) {
    const char* const value = ippGetString(attr, i, nullptr);
    if (value) {
      collection->push_back(value);
    }
  }
}

// Parse a field for the CupsJob `job` from IPP attribute `attr` using the
// attribute name `name`.
void ParseField(ipp_attribute_t* attr, std::string_view name, CupsJob* job) {
  DCHECK(!name.empty());
  if (name == kJobId) {
    job->id = ippGetInteger(attr, 0);
  } else if (name == kJobImpressionsCompleted) {
    job->current_pages = ippGetInteger(attr, 0);
  } else if (name == kJobState) {
    job->state = ToJobState(attr);
  } else if (name == kJobStateReasons) {
    ParseCollection(attr, &(job->state_reasons));
  } else if (name == kJobStateMessage) {
    const char* message_string = ippGetString(attr, 0, nullptr);
    if (message_string) {
      job->state_message = message_string;
    }
  } else if (name == kTimeAtProcessing) {
    job->processing_started = ippGetInteger(attr, 0);
  }
}

// Returns a new CupsJob allocated in `jobs` with `printer_id` populated.
CupsJob* NewJob(const std::string& printer_id, std::vector<CupsJob>* jobs) {
  jobs->emplace_back();
  CupsJob* job = &jobs->back();
  job->printer_id = printer_id;
  return job;
}

void ParseJobs(ipp_t* response,
               const std::string& printer_id,
               ipp_attribute_t* starting_attr,
               std::vector<CupsJob>* jobs) {
  // We know this is a non-empty job section.  Start parsing fields for at least
  // one job.
  CupsJob* current_job = NewJob(printer_id, jobs);
  for (ipp_attribute_t* attr = starting_attr; attr != nullptr;
       attr = ippNextAttribute(response)) {
    const char* const attribute_name = ippGetName(attr);
    // Separators indicate a new job.  Separators have empty names.
    if (!attribute_name || strlen(attribute_name) == 0) {
      current_job = NewJob(printer_id, jobs);
      continue;
    }

    // Continue to populate the job fileds.
    ParseField(attr, attribute_name, current_job);
  }
}

// Extracts PrinterInfo fields from `response` and populates `printer_info`.
// Returns true if at least printer-make-and-model and ipp-versions-supported
// were read.
bool ParsePrinterInfo(ipp_t* response, PrinterInfo* printer_info) {
  // Set to true when parsing of one of oauth-authorization-* attributes fails.
  bool oauth_error = false;

  for (ipp_attribute_t* attr = ippFirstAttribute(response); attr != nullptr;
       attr = ippNextAttribute(response)) {
    const char* const value = ippGetName(attr);
    if (!value) {
      continue;
    }
    std::string_view name(value);
    if (name == kPrinterMakeAndModel) {
      int tag = ippGetValueTag(attr);
      if (tag != IPP_TAG_TEXT && tag != IPP_TAG_TEXTLANG) {
        LOG(WARNING) << "printer-make-and-model value tag is " << tag << ".";
      }
      const char* make_and_model_string = ippGetString(attr, 0, nullptr);
      if (make_and_model_string) {
        printer_info->make_and_model = make_and_model_string;
      }
    } else if (name == kIppVersionsSupported) {
      std::vector<std::string> ipp_versions;
      ParseCollection(attr, &ipp_versions);
      for (const std::string& version : ipp_versions) {
        base::Version major_minor(version);
        if (major_minor.IsValid()) {
          printer_info->ipp_versions.push_back(major_minor);
        }
      }
    } else if (name == kIppFeaturesSupported) {
      std::vector<std::string> features;
      ParseCollection(attr, &features);
      printer_info->ipp_everywhere = base::Contains(features, kIppEverywhere);
    } else if (name == kDocumentFormatSupported) {
      ParseCollection(attr, &printer_info->document_formats);
    } else if (name == kOauthAuthorizationServerUri) {
      int tag = ippGetValueTag(attr);
      if (tag != IPP_TAG_URI) {
        LOG(WARNING) << "oauth-authorization-server-uri value tag is " << tag
                     << ".";
      }
      const char* oauth_server_string = ippGetString(attr, 0, nullptr);
      if (oauth_server_string) {
        printer_info->oauth_server = oauth_server_string;
      } else {
        oauth_error = true;
        LOG(WARNING) << "Cannot parse oauth-authorization-server-uri.";
      }
    } else if (name == kOauthAuthorizationScope) {
      int tag = ippGetValueTag(attr);
      if (tag != IPP_TAG_NAME) {
        LOG(WARNING) << "oauth-authorization-scope value tag is " << tag << ".";
      }
      const char* oauth_scope_string = ippGetString(attr, 0, nullptr);
      if (oauth_scope_string) {
        printer_info->oauth_scope = oauth_scope_string;
      } else {
        oauth_error = true;
        LOG(WARNING) << "Cannot parse oauth-authorization-scope.";
      }
    }
  }

  if (printer_info->ipp_versions.empty()) {
    // ipp-versions-supported is missing from the response.  This is IPP 1.0.
    printer_info->ipp_versions.push_back(base::Version({1, 0}));
  }

  if (!printer_info->oauth_scope.empty() &&
      printer_info->oauth_server.empty()) {
    oauth_error = true;
  }
  if (oauth_error) {
    printer_info->oauth_server.clear();
    printer_info->oauth_scope.clear();
  }

  // All IPP versions require make and model to be populated so we use it to
  // verify that we parsed the response.
  return !printer_info->make_and_model.empty();
}

// Returns true if `status` represents a complete failure in the IPP request.
bool StatusError(ipp_status_e status) {
  return status != IPP_STATUS_OK &&
         status != IPP_STATUS_OK_IGNORED_OR_SUBSTITUTED;
}

}  // namespace

CupsJob::CupsJob() = default;

CupsJob::CupsJob(const CupsJob& other) = default;

CupsJob::~CupsJob() = default;

bool CupsJob::ContainsStateReason(CupsJob::JobStateReason reason) const {
  return base::Contains(state_reasons, ToJobStateReasonString(reason));
}

PrinterInfo::PrinterInfo() = default;

PrinterInfo::~PrinterInfo() = default;

const std::string_view ToJobStateReasonString(
    CupsJob::JobStateReason state_reason) {
  switch (state_reason) {
    case CupsJob::JobStateReason::kJobCompletedWithErrors:
      return kJobCompletedWithErrors;
    case CupsJob::JobStateReason::kCupsHeldForAuthentication:
      return kCupsHeldForAuthentication;
  }
  return "";
}

std::string PrinterUriFromName(const std::string& id) {
  return base::StringPrintf("ipp://localhost/printers/%s", id.c_str());
}

void ParseJobsResponse(ipp_t* response,
                       const std::string& printer_id,
                       std::vector<CupsJob>* jobs) {
  // Advance the position in the response to the jobs section.
  ipp_attribute_t* attr = ippFirstAttribute(response);
  while (attr != nullptr && ippGetGroupTag(attr) != IPP_TAG_JOB) {
    attr = ippNextAttribute(response);
  }

  if (attr != nullptr) {
    ParseJobs(response, printer_id, attr, jobs);
  }
}

// Returns an IPP response for a Get-Printer-Attributes request to `http`.  For
// print servers, `printer_uri` is used as the printer-uri value.
// `resource_path` specifies the path portion of the server URI.
// `num_attributes` is the number of attributes in `attributes` which should be
// a list of IPP attributes.  `status` is updated with status code for the
// request.  A successful request will have the `status` IPP_STATUS_OK.
ScopedIppPtr GetPrinterAttributes(http_t* http,
                                  const std::string& printer_uri,
                                  const std::string& resource_path,
                                  int num_attributes,
                                  const char* const* attributes,
                                  ipp_status_t* status) {
  DCHECK(http);

  // CUPS expects a leading slash for resource names.  Add one if it's missing.
  std::string rp = !resource_path.empty() && resource_path.front() == '/'
                       ? resource_path
                       : "/" + resource_path;

  auto request = WrapIpp(ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES));
  // We support IPP up to 2.2 but are compatible down to v1.1.
  ippSetVersion(request.get(), 1, 1);

  ippAddString(request.get(), IPP_TAG_OPERATION, IPP_TAG_URI, kPrinterUri,
               nullptr, printer_uri.c_str());

  ippAddStrings(request.get(), IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                kRequestedAttributes, num_attributes, nullptr, attributes);

  DCHECK_EQ(ippValidateAttributes(request.get()), 1);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  auto response = WrapIpp(cupsDoRequest(http, request.release(), rp.c_str()));
  *status = ippGetStatusCode(response.get());

  return response;
}

void ParsePrinterStatus(ipp_t* response, PrinterStatus* printer_status) {
  *printer_status = PrinterStatus();

  for (ipp_attribute_t* attr = ippFirstAttribute(response); attr != nullptr;
       attr = ippNextAttribute(response)) {
    const char* const value = ippGetName(attr);
    if (!value) {
      continue;
    }
    std::string_view name(value);

    if (name == kPrinterState) {
      DCHECK_EQ(IPP_TAG_ENUM, ippGetValueTag(attr));
      printer_status->state = static_cast<ipp_pstate_t>(ippGetInteger(attr, 0));
    } else if (name == kPrinterStateReasons) {
      std::vector<std::string> reason_strings;
      ParseCollection(attr, &reason_strings);
      for (const std::string& reason : reason_strings) {
        printer_status->reasons.push_back(ToPrinterReason(reason));
      }
    } else if (name == kPrinterStateMessage) {
      const char* message_string = ippGetString(attr, 0, nullptr);
      if (message_string) {
        printer_status->message = message_string;
      }
    }
  }
}

PrinterQueryResult GetPrinterInfo(const std::string& address,
                                  const int port,
                                  const std::string& resource,
                                  bool encrypted,
                                  PrinterInfo* printer_info,
                                  PrinterStatus* printer_status) {
  DCHECK(printer_info);
  DCHECK(printer_status);

  // Lookup the printer IP address.
  http_addrlist_t* addr_list = httpAddrGetList(
      address.c_str(), AF_UNSPEC, base::NumberToString(port).c_str());
  if (!addr_list) {
    LOG(WARNING) << "Unable to resolve IP address from hostname " << address
                 << ": " << cupsLastErrorString();
    return PrinterQueryResult::kHostnameResolution;
  }

  ScopedHttpPtr http = HttpConnect2(
      address.c_str(), port, addr_list, AF_UNSPEC,
      encrypted ? HTTP_ENCRYPTION_ALWAYS : HTTP_ENCRYPTION_IF_REQUESTED, 0,
      kHttpConnectTimeoutMs, nullptr);
  if (!http) {
    LOG(WARNING) << "Could not connect to host " << address << ":" << port
                 << ": " << cupsLastErrorString();
    return PrinterQueryResult::kUnreachable;
  }

  // TODO(crbug.com/821497): Use a library to canonicalize the URL.
  size_t first_non_slash = resource.find_first_not_of('/');
  const std::string path = (first_non_slash == std::string::npos)
                               ? ""
                               : resource.substr(first_non_slash);

  std::string printer_uri =
      base::StringPrintf("%s://%s:%d/%s", encrypted ? kIppsScheme : kIppScheme,
                         address.c_str(), port, path.c_str());

  ipp_status_t status;
  ScopedIppPtr response = GetPrinterAttributes(
      http.get(), printer_uri, resource, kPrinterInfoAndStatus.size(),
      kPrinterInfoAndStatus.data(), &status);
  if (StatusError(status) || response.get() == nullptr) {
    LOG(WARNING) << "Failed to get attributes from " << printer_uri << ": "
                 << base::StringPrintf("0x%04x", status);
    return PrinterQueryResult::kUnknownFailure;
  }

  ParsePrinterStatus(response.get(), printer_status);

  if (ParsePrinterInfo(response.get(), printer_info)) {
    return PrinterQueryResult::kSuccess;
  }
  return PrinterQueryResult::kUnknownFailure;
}

bool GetPrinterStatus(http_t* http,
                      const std::string& printer_id,
                      PrinterStatus* printer_status) {
  ipp_status_t status;
  const std::string printer_uri = PrinterUriFromName(printer_id);

  ScopedIppPtr response =
      GetPrinterAttributes(http, printer_uri, "/", kPrinterAttributes.size(),
                           kPrinterAttributes.data(), &status);

  if (status != IPP_STATUS_OK) {
    LOG(WARNING) << "Failed to get printer status from " << printer_uri << ": "
                 << cupsLastErrorString();
    return false;
  }

  ParsePrinterStatus(response.get(), printer_status);

  return true;
}

bool GetCupsJobs(http_t* http,
                 const std::string& printer_id,
                 int limit,
                 JobCompletionState which,
                 std::vector<CupsJob>* jobs) {
  DCHECK(http);

  auto request = WrapIpp(ippNewRequest(IPP_OP_GET_JOBS));
  const std::string printer_uri = PrinterUriFromName(printer_id);
  ippAddString(request.get(), IPP_TAG_OPERATION, IPP_TAG_URI, kPrinterUri,
               nullptr, printer_uri.c_str());
  ippAddInteger(request.get(), IPP_TAG_OPERATION, IPP_TAG_INTEGER, kLimit,
                limit);

  std::vector<const char*> job_attributes = {
      kJobUri,          kJobId,           kJobState,
      kJobStateReasons, kJobStateMessage, kJobImpressionsCompleted,
      kTimeAtProcessing};

  ippAddStrings(request.get(), IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                kRequestedAttributes, job_attributes.size(), nullptr,
                job_attributes.data());

  ippAddString(request.get(), IPP_TAG_OPERATION, IPP_TAG_KEYWORD, kWhichJobs,
               nullptr, which == COMPLETED ? kCompleted : kNotCompleted);

  if (ippValidateAttributes(request.get()) != 1) {
    LOG(WARNING) << "Could not validate Get-Jobs ipp request: "
                 << cupsLastErrorString();
    return false;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // cupsDoRequest will delete the request.
  auto response = WrapIpp(cupsDoRequest(http, request.release(), "/"));

  ipp_status_t status = ippGetStatusCode(response.get());

  if (status != IPP_STATUS_OK) {
    LOG(WARNING) << "IPP Error: " << cupsLastErrorString();
    return false;
  }

  ParseJobsResponse(response.get(), printer_id, jobs);

  return true;
}

}  // namespace printing
