// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/web_js_error_report_processor.h"

#import <string>

#import "base/apple/bridging.h"
#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/no_destructor.h"
#import "base/strings/escape.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "build/branding_buildflags.h"
#import "components/variations/variations_crash_keys.h"
#import "ios/web/js_features/window_error/ios_javascript_error_report.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_client.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "url/gurl.h"

using ParameterMap = std::map<std::string, std::string>;

namespace {

// The max length of the crash endpoint response.
constexpr int kCrashEndpointResponseMaxSizeInBytes = 1024;

// The url of the crash report upload endpoint.
constexpr char kCrashEndpointUrl[] = "https://clients2.google.com/cr/report";

constexpr char kNewlineSeparator[] = "\n";

const char kWebJsErrorReportProcessorKeyName[] =
    "web_js_error_report_processor";

// Results of attempted error report uploads.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IOSJavascriptErrorReportProcessorResult {
  // The JSErrorReportProcessor was not available.
  kErrorReportNotSentProcessorUnavailable = 0,
  // The error report is missing the API value and was not sent.
  kErrorReportNotSentApiMissing = 1,
  // The error was already recently reported so the duplicate report will not be
  // uploaded.
  kErrorReportNotSentDuplicate = 2,
  // The error report upload failed.
  kErrorReportUploadFailed = 3,
  // The error report upload succeeded.
  kErrorReportUploadSucceeded = 4,
  kMaxValue = kErrorReportUploadSucceeded,
};

// Returns the OS version of the currently running application.
std::string GetOsVersion() {
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);
  return base::StrCat({base::NumberToString(os_major_version), ".",
                       base::NumberToString(os_minor_version), ".",
                       base::NumberToString(os_bugfix_version)});
}

// Adds experiment IDs to the crash report `params`.
void AddExperimentIds(ParameterMap& params) {
  variations::ExperimentListInfo experiment_info =
      variations::GetExperimentListInfo();

  params[variations::kNumExperimentsKey] =
      base::NumberToString(experiment_info.num_experiments);
  params[variations::kExperimentListKey] = experiment_info.experiment_list;
}

// Returns a query string for the crash report upload given `params`.
std::string BuildPostRequestQueryString(const ParameterMap& params) {
  std::vector<std::string> query_parts;
  for (const auto& kv : params) {
    query_parts.push_back(base::StrCat(
        {kv.first, "=",
         base::EscapeQueryParamValue(kv.second, /*use_plus=*/false)}));
  }
  return base::JoinString(query_parts, "&");
}

// Adds the report parameters which remain consistent for this application run
// to `report_params`
void AddStandardReportParams(ParameterMap& report_params) {
  static base::NoDestructor<ParameterMap> general_params([]() {
    ParameterMap params;
    params["type"] = "JavascriptError";
    params["isFatal"] = "no";
    params["plat"] = "iOS";

    @autoreleasepool {
      params["os"] = base::SysInfo::OperatingSystemName();
      params["os_version"] = GetOsVersion();

      NSBundle* outer_bundle = base::apple::OuterBundle();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      params["prod"] = "Chrome_iOS";
#else
      NSString* product = base::apple::ObjCCast<NSString>(
          [outer_bundle objectForInfoDictionaryKey:base::apple::CFToNSPtrCast(
                                                       kCFBundleNameKey)]);
      params["prod"] = base::SysNSStringToUTF8(product).append("_iOS");
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // Empty means stable.
      const bool allow_empty_channel = true;
#else
      const bool allow_empty_channel = false;
#endif
      NSString* channel = base::apple::ObjCCast<NSString>(
          [outer_bundle objectForInfoDictionaryKey:@"KSChannelID"]);
      // Must be a developer build.
      if (!allow_empty_channel && (!channel || !channel.length)) {
        channel = @"developer";
      }
      params["channel"] = base::SysNSStringToUTF8(channel);

      NSString* version =
          base::apple::ObjCCast<NSString>([base::apple::FrameworkBundle()
              objectForInfoDictionaryKey:@"CFBundleVersion"]);
      params["ver"] = base::SysNSStringToUTF8(version);
    }  // @autoreleasepool

    return params;
  }());

  report_params.insert(general_params.get()->begin(),
                       general_params.get()->end());
}

// Removes stack frames which are not from injected feature scripts.
std::string RedactStack(std::string stack) {
  std::vector<std::string_view> kept_frames;
  for (const auto& frame :
       base::SplitStringPiece(stack, kNewlineSeparator, base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (frame.find("@user-script:") != std::string::npos) {
      kept_frames.push_back(frame);
    }
  }
  return base::JoinString(kept_frames, kNewlineSeparator);
}

}  // namespace

namespace web {

WebJsErrorReportProcessor::WebJsErrorReportProcessor(
    BrowserState* browser_state)
    : browser_state_(browser_state) {}
WebJsErrorReportProcessor::~WebJsErrorReportProcessor() = default;

// static
WebJsErrorReportProcessor* WebJsErrorReportProcessor::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);

  WebJsErrorReportProcessor* feature = static_cast<WebJsErrorReportProcessor*>(
      browser_state->GetUserData(kWebJsErrorReportProcessorKeyName));
  if (!feature) {
    feature = new WebJsErrorReportProcessor(browser_state);
    browser_state->SetUserData(kWebJsErrorReportProcessorKeyName,
                               base::WrapUnique(feature));
  }
  return feature;
}

// static
void WebJsErrorReportProcessor::LogProcessorUnavailable() {
  UMA_HISTOGRAM_ENUMERATION("IOS.JavaScript.ErrorReportProcessor",
                            IOSJavascriptErrorReportProcessorResult::
                                kErrorReportNotSentProcessorUnavailable);
}

void WebJsErrorReportProcessor::ReportJavaScriptError(
    ScriptErrorDetails details) {
  IOSJavaScriptErrorReport report;
  report.source_system =
      IOSJavaScriptErrorReport::SourceSystem::kScriptErrorMessageHandler;
  report.from_main_frame = details.is_main_frame;
  report.api = details.api;
  report.error_message = details.message;
  report.stack_trace = RedactStack(details.stack);
  report.crash_keys = details.crash_keys;
  report.page_url = details.url.GetWithEmptyPath().spec();

  std::string filename = details.url.ExtractFileName();
  if (filename.length()) {
    std::optional<std::pair<std::string_view, std::string_view>> parts =
        base::RSplitStringOnce(filename, '.');
    if (parts.has_value()) {
      report.page_url_file_extension = std::string(parts->second);
    }
  }

  SendErrorReport(std::move(report));
}

void WebJsErrorReportProcessor::ReportJavaScriptExecutionFailed(
    std::string api,
    url::Origin origin,
    NSError* error,
    bool from_main_frame) {
  IOSJavaScriptErrorReport report;
  report.source_system =
      IOSJavaScriptErrorReport::SourceSystem::kNativeScriptExecutionFailed;
  report.api = api;
  report.from_main_frame = from_main_frame;
  report.page_url = origin.Serialize();

  std::string exception =
      base::SysNSStringToUTF8(error.userInfo[@"WKJavaScriptExceptionMessage"]);
  if (!exception.empty()) {
    report.error_message = exception;
  } else {
    report.error_message =
        base::SysNSStringToUTF8(error.userInfo[NSLocalizedDescriptionKey]);
  }
  report.error_domain = base::SysNSStringToUTF8(error.domain);
  report.error_code = error.code;

  SendErrorReport(std::move(report));
}

void WebJsErrorReportProcessor::SendErrorReport(
    IOSJavaScriptErrorReport error_report) {
  if (error_report.api.empty()) {
    UMA_HISTOGRAM_ENUMERATION(
        "IOS.JavaScript.ErrorReportProcessor",
        IOSJavascriptErrorReportProcessorResult::kErrorReportNotSentApiMissing);
    // Unknown API, do not report error.
    return;
  }

  // Do not send duplicate reports to prevent spamming the crash server with the
  // same errors.
  std::string error_key =
      base::StrCat({error_report.api, "+", error_report.error_message});
  if (!ShouldUploadErrorReport(error_key)) {
    UMA_HISTOGRAM_ENUMERATION(
        "IOS.JavaScript.ErrorReportProcessor",
        IOSJavascriptErrorReportProcessorResult::kErrorReportNotSentDuplicate);
    return;
  }

  ParameterMap params;
  AddStandardReportParams(params);

  params["api"] = error_report.api;
  params["error_message"] = error_report.error_message;
  params["main_frame"] = error_report.from_main_frame ? "yes" : "no";

  constexpr char kSourceSystemParamName[] = "source_system";
  switch (error_report.source_system) {
    case IOSJavaScriptErrorReport::SourceSystem::kUnknown:
      break;
    case IOSJavaScriptErrorReport::SourceSystem::kNativeScriptExecutionFailed:
      params[kSourceSystemParamName] = "native";
      break;
    case IOSJavaScriptErrorReport::SourceSystem::kScriptErrorMessageHandler:
      params[kSourceSystemParamName] = "message_handler";
      break;
  }

  if (web::GetWebClient()->GetJSErrorReportLoggingLevel(browser_state_) ==
      JSErrorReportLoggingLevel::FULL) {
    if (error_report.page_url) {
      params["url0"] = error_report.page_url.value();
    }
    if (error_report.page_url_file_extension) {
      params["url0_file_extension"] =
          error_report.page_url_file_extension.value();
    }
  }

  if (error_report.error_domain) {
    params["error_domain"] = error_report.error_domain.value();
  }
  if (error_report.error_code) {
    params["error_code"] =
        base::NumberToString(error_report.error_code.value());
  }

  if (error_report.crash_keys) {
    for (auto ck = error_report.crash_keys->begin();
         ck != error_report.crash_keys->end(); ++ck) {
      params[base::StrCat({"JS_", ck->first})] = ck->second;
    }
  }

  AddExperimentIds(params);

  const GURL url(base::StrCat(
      {kCrashEndpointUrl, "?", BuildPostRequestQueryString(params)}));
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  const auto traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ios_javascript_report_error", R"(
        semantics {
          sender: "iOS JavaScript error reporter"
          description:
            "iOS Chrome can send JavaScript errors that occur within Chrome "
            "feature JavaScript. If enabled, the error message, along with "
            "information about Chrome and the operating system, is sent to "
            "Google for debugging."
          trigger:
            "A JavaScript error occurs in iOS Chrome's scripts or while "
            "attempting to call these scripts from native code.""
          data:
            "The JavaScript error message, version and channel of Chrome, the "
            "URL of the webpage, and a stack trace of the error."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via 'Automatically send "
            "usage statistics and crash reports to Google' in Chromium's "
            "settings under Advanced, Privacy. This feature is enabled by "
            "default."
          chrome_policy {
            MetricsReportingEnabled {
              policy_options {mode: MANDATORY}
              MetricsReportingEnabled: false
            }
          }
        })");

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      browser_state_->GetSharedURLLoaderFactory();
  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  if (error_report.stack_trace) {
    url_loader->AttachStringForUpload(*error_report.stack_trace, "text/plain");
  }

  network::SimpleURLLoader* loader = url_loader.get();
  loader->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&WebJsErrorReportProcessor::OnRequestComplete,
                     weak_factory_.GetWeakPtr(), std::move(url_loader),
                     error_key),
      kCrashEndpointResponseMaxSizeInBytes);
}

void WebJsErrorReportProcessor::OnRequestComplete(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    const std::string& error_report_key,
    std::optional<std::string> response_body) {
  if (response_body) {
    UMA_HISTOGRAM_ENUMERATION(
        "IOS.JavaScript.ErrorReportProcessor",
        IOSJavascriptErrorReportProcessorResult::kErrorReportUploadSucceeded);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "IOS.JavaScript.ErrorReportProcessor",
        IOSJavascriptErrorReportProcessorResult::kErrorReportUploadFailed);
    // Clear timestamp for this error key since the upload failed.
    recent_error_reports_.erase(error_report_key);
  }
}

bool WebJsErrorReportProcessor::ShouldUploadErrorReport(
    const std::string& error_report_key) {
  constexpr base::TimeDelta kTimeBetweenCleanings = base::Hours(1);
  constexpr base::TimeDelta kTimeBetweenDuplicateReports = base::Hours(1);

  base::Time now = base::Time::Now();
  // Check for cleaning.
  if (last_recent_error_reports_cleaning_.is_null()) {
    // First time in this function, no need to clean.
    last_recent_error_reports_cleaning_ = now;
  } else if (now - kTimeBetweenCleanings >
             last_recent_error_reports_cleaning_) {
    auto it = recent_error_reports_.begin();
    while (it != recent_error_reports_.end()) {
      if (now - kTimeBetweenDuplicateReports > it->second) {
        it = recent_error_reports_.erase(it);
      } else {
        ++it;
      }
    }
    last_recent_error_reports_cleaning_ = now;
  } else if (now < last_recent_error_reports_cleaning_) {
    // Time went backwards, clock must have been adjusted. Assume all our
    // last-send records are meaningless. Clock adjustments should be rare
    // enough that it doesn't matter if we send a few duplicate reports in this
    // case.
    recent_error_reports_.clear();
    last_recent_error_reports_cleaning_ = now;
  }

  auto insert_result = recent_error_reports_.try_emplace(error_report_key, now);
  if (insert_result.second) {
    // No recent reports with this key. Time is already inserted into map.
    return true;
  }

  base::Time& last_error_report = insert_result.first->second;
  if (now - kTimeBetweenDuplicateReports > last_error_report ||
      now < last_error_report) {
    // It's been long enough, send the report. (Or, the clock has been adjusted
    // and we don't really know how long it's been, so send the report.)
    return true;
  }

  return false;
}

}  // namespace web
