// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/omaha/omaha_service.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/ios/device_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/arch_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/browser_state_metrics/browser_state_metrics.h"
#include "ios/chrome/browser/install_time_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/upgrade/upgrade_recommended_details.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/omaha/omaha_service_provider.h"
#include "ios/public/provider/chrome/browser/omaha/omaha_xml_writer.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#include "libxml/xmlwriter.h"
#include "net/base/backoff_entry.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Number of hours to wait between successful requests.
const int kHoursBetweenRequests = 5;
// Minimal time to wait between retry requests.
const CFTimeInterval kPostRetryBaseSeconds = 3600;
// Maximal time to wait between retry requests.
const CFTimeInterval kPostRetryMaxSeconds = 6 * kPostRetryBaseSeconds;

// Default last sent application version when none has been sent yet.
const char kDefaultLastSentVersion[] = "0.0.0.0";

// Key for saving states in the UserDefaults.
NSString* const kNextTriesTimesKey = @"ChromeOmahaServiceNextTries";
NSString* const kCurrentPingKey = @"ChromeOmahaServiceCurrentPing";
NSString* const kNumberTriesKey = @"ChromeOmahaServiceNumberTries";
NSString* const kLastSentVersionKey = @"ChromeOmahaServiceLastSentVersion";
NSString* const kLastSentTimeKey = @"ChromeOmahaServiceLastSentTime";
NSString* const kRetryRequestIdKey = @"ChromeOmahaServiceRetryRequestId";

class XmlWrapper : public OmahaXmlWriter {
 public:
  XmlWrapper()
      : buffer_(xmlBufferCreate()),
        writer_(xmlNewTextWriterMemory(buffer_, /* compression */ 0)) {
    DCHECK(buffer_);
    DCHECK(writer_);
  }

  ~XmlWrapper() override {
    xmlFreeTextWriter(writer_);
    xmlBufferFree(buffer_);
  }

  void StartElement(const char* name) override {
    DCHECK(name);
    int result = xmlTextWriterStartElement(
        writer_, reinterpret_cast<const xmlChar*>(name));
    DCHECK_GE(result, 0);
  }

  void EndElement() override {
    int result = xmlTextWriterEndElement(writer_);
    DCHECK_GE(result, 0);
  }

  void WriteAttribute(const char* name, const char* value) override {
    DCHECK(name);
    int result = xmlTextWriterWriteAttribute(
        writer_, reinterpret_cast<const xmlChar*>(name),
        reinterpret_cast<const xmlChar*>(value));
    DCHECK_GE(result, 0);
  }

  void Finalize() override {
    int result = xmlTextWriterEndDocument(writer_);
    DCHECK_GE(result, 0);
  }

  std::string GetContentAsString() override {
    return std::string(reinterpret_cast<char*>(buffer_->content));
  }

 private:
  xmlBufferPtr buffer_;
  xmlTextWriterPtr writer_;

  DISALLOW_COPY_AND_ASSIGN(XmlWrapper);
};

}  // namespace

#pragma mark -

// XML parser for the server response.
@interface ResponseParser : NSObject<NSXMLParserDelegate> {
  BOOL hasError_;
  BOOL responseIsParsed_;
  BOOL appIsParsed_;
  BOOL updateCheckIsParsed_;
  BOOL urlIsParsed_;
  BOOL manifestIsParsed_;
  BOOL pingIsParsed_;
  BOOL eventIsParsed_;
  NSString* appId_;
  std::unique_ptr<UpgradeRecommendedDetails> updateInformation_;
}

// Initialization method. |appId| is the application id one expects to find in
// the response message.
- (instancetype)initWithAppId:(NSString*)appId;

// Returns YES if the message has been correctly parsed.
- (BOOL)isCorrect;

// If an upgrade is possible, returns the details of the notification to send.
// Otherwise, return NULL.
- (UpgradeRecommendedDetails*)upgradeRecommendedDetails;

@end

@implementation ResponseParser

- (instancetype)initWithAppId:(NSString*)appId {
  if (self = [super init]) {
    appId_ = appId;
  }
  return self;
}

- (BOOL)isCorrect {
  // A response should have either a ping ACK or an event ACK, depending on the
  // contents of the request.
  return !hasError_ && (pingIsParsed_ || eventIsParsed_);
}

- (UpgradeRecommendedDetails*)upgradeRecommendedDetails {
  return updateInformation_.get();
}

// This method is parsing a message with the following type:
// <response...>
//   <daystart.../>
//   <app...>
//     <updatecheck status="ok">
//       <urls>
//         <url codebase="???"/>
//       </urls>
//       <manifest version="???">
//         <packages>
//           <package hash="0" name="Chrome" required="true" size="0"/>
//         </packages>
//         <actions>
//           <action event="update" run="Chrome"/>
//           <action event="postinstall"/>
//         </actions>
//       </manifest>
//     </updatecheck>
//     <ping.../>
//   </app>
// </response>
// --- OR ---
// <response...>
//   <daystart.../>
//   <app...>
//     <event.../>
//   </app>
// </response>
// See http://code.google.com/p/omaha/wiki/ServerProtocol for details.
- (void)parser:(NSXMLParser*)parser
    didStartElement:(NSString*)elementName
       namespaceURI:(NSString*)namespaceURI
      qualifiedName:(NSString*)qualifiedName
         attributes:(NSDictionary*)attributeDict {
  if (hasError_)
    return;

  // Array of uninteresting tags in the Omaha xml response.
  NSArray* ignoredTagNames =
      @[ @"action", @"actions", @"daystart", @"package", @"packages", @"urls" ];
  if ([ignoredTagNames containsObject:elementName])
    return;

  if (!responseIsParsed_) {
    if ([elementName isEqualToString:@"response"] &&
        [[attributeDict valueForKey:@"protocol"] isEqualToString:@"3.0"] &&
        [[attributeDict valueForKey:@"server"] isEqualToString:@"prod"]) {
      responseIsParsed_ = YES;
    } else {
      hasError_ = YES;
    }
  } else if (!appIsParsed_) {
    if ([elementName isEqualToString:@"app"] &&
        [[attributeDict valueForKey:@"status"] isEqualToString:@"ok"] &&
        [[attributeDict valueForKey:@"appid"] isEqualToString:appId_]) {
      appIsParsed_ = YES;
    } else {
      hasError_ = YES;
    }
  } else if (!eventIsParsed_ && !updateCheckIsParsed_) {
    if ([elementName isEqualToString:@"updatecheck"]) {
      updateCheckIsParsed_ = YES;
      NSString* status = [attributeDict valueForKey:@"status"];
      if ([status isEqualToString:@"noupdate"]) {
        // No update is available on the Market, so we won't get a <url> or
        // <manifest> tag.
        urlIsParsed_ = YES;
        manifestIsParsed_ = YES;
      } else if ([status isEqualToString:@"ok"]) {
        updateInformation_ = std::make_unique<UpgradeRecommendedDetails>();
      } else {
        hasError_ = YES;
      }
    } else if ([elementName isEqualToString:@"event"]) {
      if ([[attributeDict valueForKey:@"status"] isEqualToString:@"ok"]) {
        eventIsParsed_ = YES;
      } else {
        hasError_ = YES;
      }
    } else {
      hasError_ = YES;
    }
  } else if (!urlIsParsed_) {
    if ([elementName isEqualToString:@"url"] &&
        [[attributeDict valueForKey:@"codebase"] length] > 0) {
      urlIsParsed_ = YES;
      DCHECK(updateInformation_);
      NSString* url = [attributeDict valueForKey:@"codebase"];
      if ([[url substringFromIndex:([url length] - 1)] isEqualToString:@"/"])
        url = [url substringToIndex:([url length] - 1)];
      updateInformation_.get()->upgrade_url =
          GURL(base::SysNSStringToUTF8(url));
      if (!updateInformation_.get()->upgrade_url.is_valid())
        hasError_ = YES;
    } else {
      hasError_ = YES;
    }
  } else if (!manifestIsParsed_) {
    if ([elementName isEqualToString:@"manifest"] &&
        [attributeDict valueForKey:@"version"]) {
      manifestIsParsed_ = YES;
      DCHECK(updateInformation_);
      updateInformation_.get()->next_version =
          base::SysNSStringToUTF8([attributeDict valueForKey:@"version"]);
    } else {
      hasError_ = YES;
    }
  } else if (!pingIsParsed_) {
    if ([elementName isEqualToString:@"ping"] &&
        [[attributeDict valueForKey:@"status"] isEqualToString:@"ok"]) {
      pingIsParsed_ = YES;
    } else {
      hasError_ = YES;
    }
  } else {
    hasError_ = YES;
  }
}

@end

// static
OmahaService* OmahaService::GetInstance() {
  return base::Singleton<OmahaService>::get();
}

// static
void OmahaService::Start(std::unique_ptr<network::SharedURLLoaderFactoryInfo>
                             url_loader_factory_info,
                         const UpgradeRecommendedCallback& callback) {
  DCHECK(url_loader_factory_info);
  DCHECK(!callback.is_null());
  OmahaService* result = GetInstance();
  result->set_upgrade_recommended_callback(callback);
  // This should only be called once.
  DCHECK(!result->url_loader_factory_info_ || !result->url_loader_factory_);
  result->url_loader_factory_info_ = std::move(url_loader_factory_info);
  result->locale_lang_ = GetApplicationContext()->GetApplicationLocale();
  base::PostTaskWithTraits(FROM_HERE, {web::WebThread::IO},
                           base::Bind(&OmahaService::SendOrScheduleNextPing,
                                      base::Unretained(result)));
}

OmahaService::OmahaService()
    : schedule_(true),
      application_install_date_(0),
      sending_install_event_(false) {
  Initialize();
}

OmahaService::OmahaService(bool schedule)
    : schedule_(schedule),
      application_install_date_(0),
      sending_install_event_(false) {
  Initialize();
}

OmahaService::~OmahaService() {}

void OmahaService::Initialize() {
  // Initialize the provider at the same time as the rest of the service.
  ios::GetChromeBrowserProvider()->GetOmahaServiceProvider()->Initialize();

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  next_tries_time_ = base::Time::FromCFAbsoluteTime(
      [defaults doubleForKey:kNextTriesTimesKey]);
  current_ping_time_ =
      base::Time::FromCFAbsoluteTime([defaults doubleForKey:kCurrentPingKey]);
  number_of_tries_ = [defaults integerForKey:kNumberTriesKey];
  last_sent_time_ =
      base::Time::FromCFAbsoluteTime([defaults doubleForKey:kLastSentTimeKey]);
  NSString* lastSentVersion = [defaults stringForKey:kLastSentVersionKey];
  if (lastSentVersion) {
    last_sent_version_ =
        base::Version(base::SysNSStringToUTF8(lastSentVersion));
  } else {
    last_sent_version_ = base::Version(kDefaultLastSentVersion);
  }

  application_install_date_ =
      GetApplicationContext()->GetLocalState()->GetInt64(
          metrics::prefs::kInstallDate);
  DCHECK(application_install_date_);

  // Whether data should be persisted again to the user preferences.
  bool persist_again = false;

  base::Time now = base::Time::Now();
  // If |last_sent_time_| is in the future, the clock has been tampered with.
  // Reset |last_sent_time_| to now.
  if (last_sent_time_ > now) {
    last_sent_time_ = now;
    persist_again = true;
  }

  // If the |next_tries_time_| is more than kHoursBetweenRequests hours away,
  // there is a possibility that the clock has been tampered with. Reschedule
  // the ping to be the usual interval after the last successful one.
  if (next_tries_time_ - now >
      base::TimeDelta::FromHours(kHoursBetweenRequests)) {
    next_tries_time_ =
        last_sent_time_ + base::TimeDelta::FromHours(kHoursBetweenRequests);
    persist_again = true;
  }

  // Fire a ping as early as possible if the version changed.
  const base::Version& current_version = version_info::GetVersion();
  if (last_sent_version_ < current_version) {
    next_tries_time_ = base::Time::Now() - base::TimeDelta::FromSeconds(1);
    number_of_tries_ = 0;
    persist_again = true;
  }

  if (persist_again)
    PersistStates();
}

// static
void OmahaService::GetDebugInformation(
    const base::Callback<void(base::DictionaryValue*)> callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {web::WebThread::IO},
      base::Bind(&OmahaService::GetDebugInformationOnIOThread,
                 base::Unretained(GetInstance()), callback));
}

// static
base::TimeDelta OmahaService::GetBackOff(uint8_t number_of_tries) {
  // Configuration for the service exponential backoff
  static net::BackoffEntry::Policy kBackoffPolicy = {
      0,                             // num_errors_to_ignore
      kPostRetryBaseSeconds * 1000,  // initial_delay_ms
      2.0,                           // multiply_factor
      0.1,                           // jitter_factor
      kPostRetryMaxSeconds * 1000,   // maximum_backoff_ms
      -1,                            // entry_lifetime_ms
      false                          // always_use_initial_delay
  };

  net::BackoffEntry backoff_entry(&kBackoffPolicy);
  for (int i = 0; i < number_of_tries; ++i) {
    backoff_entry.InformOfRequest(false);
  }

  return backoff_entry.GetTimeUntilRelease();
}

std::string OmahaService::GetPingContent(const std::string& requestId,
                                         const std::string& sessionId,
                                         const std::string& versionName,
                                         const std::string& channelName,
                                         const base::Time& installationTime,
                                         PingContent pingContent) {
  OmahaServiceProvider* provider =
      ios::GetChromeBrowserProvider()->GetOmahaServiceProvider();

  XmlWrapper xml_wrapper;
  xml_wrapper.StartElement("request");
  xml_wrapper.WriteAttribute("protocol", "3.0");
  xml_wrapper.WriteAttribute("version", "iOS-1.0.0.0");
  xml_wrapper.WriteAttribute("ismachine", "1");
  xml_wrapper.WriteAttribute("requestid", requestId.c_str());
  xml_wrapper.WriteAttribute("sessionid", sessionId.c_str());
  provider->AppendExtraAttributes("request", &xml_wrapper);
  xml_wrapper.WriteAttribute("hardware_class",
                             ios::device_util::GetPlatform().c_str());
  // Set up <os platform="ios"... />
  xml_wrapper.StartElement("os");
  xml_wrapper.WriteAttribute("platform", "ios");
  xml_wrapper.WriteAttribute("version",
                             base::SysInfo::OperatingSystemVersion().c_str());
  xml_wrapper.WriteAttribute("arch", arch_util::kCurrentArch);
  xml_wrapper.EndElement();

  bool is_first_install =
      pingContent == INSTALL_EVENT &&
      last_sent_version_ == base::Version(kDefaultLastSentVersion);

  // Set up <app version="" ...>
  xml_wrapper.StartElement("app");
  if (pingContent == INSTALL_EVENT) {
    std::string previous_version =
        is_first_install ? "" : last_sent_version_.GetString();
    xml_wrapper.WriteAttribute("version", previous_version.c_str());
    xml_wrapper.WriteAttribute("nextversion", versionName.c_str());
  } else {
    xml_wrapper.WriteAttribute("version", versionName.c_str());
    xml_wrapper.WriteAttribute("nextversion", "");
  }
  xml_wrapper.WriteAttribute("lang", locale_lang_.c_str());
  xml_wrapper.WriteAttribute("brand", provider->GetBrandCode().c_str());
  xml_wrapper.WriteAttribute("client", "");
  std::string application_id = provider->GetApplicationID();
  xml_wrapper.WriteAttribute("appid", application_id.c_str());
  std::string install_age;
  if (is_first_install) {
    install_age = "-1";
  } else if (!installationTime.is_null() &&
             installationTime.ToTimeT() !=
                 install_time_util::kUnknownInstallDate) {
    install_age = base::StringPrintf(
        "%d", (base::Time::Now() - installationTime).InDays());
  }
  provider->AppendExtraAttributes("app", &xml_wrapper);
  // If the install date is unknown, send nothing.
  if (!install_age.empty())
    xml_wrapper.WriteAttribute("installage", install_age.c_str());

  if (pingContent == INSTALL_EVENT) {
    // Add an install complete event.
    xml_wrapper.StartElement("event");
    if (is_first_install) {
      xml_wrapper.WriteAttribute("eventtype", "2");  // install
    } else {
      xml_wrapper.WriteAttribute("eventtype", "3");  // update
    }
    xml_wrapper.WriteAttribute("eventresult", "1");  // succeeded
    xml_wrapper.EndElement();
  } else {
    // Set up <updatecheck/>
    xml_wrapper.StartElement("updatecheck");
    xml_wrapper.WriteAttribute("tag", channelName.c_str());
    xml_wrapper.EndElement();

    // Set up <ping active=1/>
    xml_wrapper.StartElement("ping");
    xml_wrapper.WriteAttribute("active", "1");
    xml_wrapper.EndElement();
  }

  // End app.
  xml_wrapper.EndElement();
  // End request.
  xml_wrapper.EndElement();

  xml_wrapper.Finalize();
  return xml_wrapper.GetContentAsString();
}

std::string OmahaService::GetCurrentPingContent() {
  const base::Version& current_version = version_info::GetVersion();
  sending_install_event_ = last_sent_version_ < current_version;
  PingContent ping_content =
      sending_install_event_ ? INSTALL_EVENT : USAGE_PING;

  // An install retry ping only makes sense if an install event must be send.
  DCHECK(sending_install_event_ || !IsNextPingInstallRetry());
  std::string request_id = GetNextPingRequestId(ping_content);
  return GetPingContent(request_id, ios::device_util::GetRandomId(),
                        version_info::GetVersionNumber(), GetChannelString(),
                        base::Time::FromTimeT(application_install_date_),
                        ping_content);
}

void OmahaService::SendPing() {
  // Check that no request is in progress.
  DCHECK(!url_loader_);

  GURL url(ios::GetChromeBrowserProvider()
               ->GetOmahaServiceProvider()
               ->GetUpdateServerURL());
  if (!url.is_valid()) {
    return;
  }

  // There are 2 situations here:
  // 1) production code, where |url_loader_factory_info_| is used.
  // 2) testing code, where the |url_loader_factory_| creation is triggered by
  // the test.
  if (url_loader_factory_info_) {
    DCHECK(!url_loader_factory_);
    url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(url_loader_factory_info_));
  }

  DCHECK(url_loader_factory_);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;

  // If this is not the first try, notify the omaha server.
  if (number_of_tries_ && IsNextPingInstallRetry()) {
    resource_request->headers.SetHeader(
        "X-RequestAge",
        base::StringPrintf(
            "%lld", (base::Time::Now() - current_ping_time_).InSeconds()));
  }

  // Update last fail time and number of tries, so that if anything fails
  // catastrophically, the fail is taken into account.
  if (number_of_tries_ < 30)
    ++number_of_tries_;
  next_tries_time_ = base::Time::Now() + GetBackOff(number_of_tries_);
  PersistStates();

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 NO_TRAFFIC_ANNOTATION_YET);
  url_loader_->AttachStringForUpload(GetCurrentPingContent(), "text/xml");
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&OmahaService::OnURLLoadComplete, base::Unretained(this)));
}

void OmahaService::SendOrScheduleNextPing() {
  base::Time now = base::Time::Now();
  if (next_tries_time_ <= now) {
    SendPing();
    return;
  }
  if (schedule_) {
    timer_.Start(
        FROM_HERE, next_tries_time_ - now,
        base::BindOnce(&OmahaService::SendPing, base::Unretained(this)));
  }
}

void OmahaService::PersistStates() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  [defaults setDouble:next_tries_time_.ToCFAbsoluteTime()
               forKey:kNextTriesTimesKey];
  [defaults setDouble:current_ping_time_.ToCFAbsoluteTime()
               forKey:kCurrentPingKey];
  [defaults setDouble:last_sent_time_.ToCFAbsoluteTime()
               forKey:kLastSentTimeKey];
  [defaults setInteger:number_of_tries_ forKey:kNumberTriesKey];
  [defaults setObject:base::SysUTF8ToNSString(last_sent_version_.GetString())
               forKey:kLastSentVersionKey];

  // Save critical state information for usage reporting.
  [defaults synchronize];
}

void OmahaService::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  // Reset the loader.
  url_loader_.reset();

  if (!response_body) {
    DLOG(WARNING) << "Error contacting the Omaha server";
    SendOrScheduleNextPing();
    return;
  }

  NSData* xml = [NSData dataWithBytes:response_body->data()
                               length:response_body->length()];
  NSXMLParser* parser = [[NSXMLParser alloc] initWithData:xml];
  const std::string application_id = ios::GetChromeBrowserProvider()
                                         ->GetOmahaServiceProvider()
                                         ->GetApplicationID();
  ResponseParser* delegate = [[ResponseParser alloc]
      initWithAppId:base::SysUTF8ToNSString(application_id)];
  parser.delegate = delegate;

  if (![parser parse] || ![delegate isCorrect]) {
    DLOG(ERROR) << "Unable to parse XML response from Omaha server.";
    SendOrScheduleNextPing();
    return;
  }
  // Handle success.
  number_of_tries_ = 0;
  // Schedule the next request. If requset that just finished was an install
  // notification, send an active ping immediately.
  next_tries_time_ = sending_install_event_
                         ? base::Time::Now()
                         : base::Time::Now() + base::TimeDelta::FromHours(
                                                   kHoursBetweenRequests);
  current_ping_time_ = next_tries_time_;
  last_sent_time_ = base::Time::Now();
  last_sent_version_ = version_info::GetVersion();
  sending_install_event_ = false;
  ClearInstallRetryRequestId();
  PersistStates();
  SendOrScheduleNextPing();

  // Send notification for updates if needed.
  UpgradeRecommendedDetails* details = [delegate upgradeRecommendedDetails];
  if (details) {
    base::PostTaskWithTraits(
        FROM_HERE, {web::WebThread::UI},
        base::Bind(upgrade_recommended_callback_, *details));
  }
}

void OmahaService::GetDebugInformationOnIOThread(
    const base::Callback<void(base::DictionaryValue*)> callback) {
  auto result = std::make_unique<base::DictionaryValue>();

  result->SetString("message", GetCurrentPingContent());
  result->SetString("last_sent_time",
                    base::TimeFormatShortDateAndTime(last_sent_time_));
  result->SetString("next_tries_time",
                    base::TimeFormatShortDateAndTime(next_tries_time_));
  result->SetString("current_ping_time",
                    base::TimeFormatShortDateAndTime(current_ping_time_));
  result->SetString("last_sent_version", last_sent_version_.GetString());
  result->SetString("number_of_tries",
                    base::StringPrintf("%d", number_of_tries_));
  result->SetString("timer_running",
                    base::StringPrintf("%d", timer_.IsRunning()));
  result->SetString(
      "timer_current_delay",
      base::StringPrintf("%llds", timer_.GetCurrentDelay().InSeconds()));
  result->SetString("timer_desired_run_time",
                    base::TimeFormatShortDateAndTime(
                        base::Time::Now() +
                        (timer_.desired_run_time() - base::TimeTicks::Now())));

  // Sending the value to the callback.
  base::PostTaskWithTraits(FROM_HERE, {web::WebThread::UI},
                           base::Bind(callback, base::Owned(result.release())));
}

bool OmahaService::IsNextPingInstallRetry() {
  return [[NSUserDefaults standardUserDefaults]
             stringForKey:kRetryRequestIdKey] != nil;
}

std::string OmahaService::GetNextPingRequestId(PingContent ping_content) {
  NSString* stored_id =
      [[NSUserDefaults standardUserDefaults] stringForKey:kRetryRequestIdKey];
  if (stored_id) {
    DCHECK(ping_content == INSTALL_EVENT);
    return base::SysNSStringToUTF8(stored_id);
  } else {
    std::string identifier = ios::device_util::GetRandomId();
    if (ping_content == INSTALL_EVENT)
      OmahaService::SetInstallRetryRequestId(identifier);
    return identifier;
  }
}

void OmahaService::SetInstallRetryRequestId(const std::string& request_id) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:base::SysUTF8ToNSString(request_id)
               forKey:kRetryRequestIdKey];
  // Save critical state information for usage reporting.
  [defaults synchronize];
}

void OmahaService::ClearInstallRetryRequestId() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kRetryRequestIdKey];
  // Clear critical state information for usage reporting.
  [defaults synchronize];
}

void OmahaService::InitializeURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

void OmahaService::ClearPersistentStateForTests() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kNextTriesTimesKey];
  [defaults removeObjectForKey:kCurrentPingKey];
  [defaults removeObjectForKey:kNumberTriesKey];
  [defaults removeObjectForKey:kLastSentVersionKey];
  [defaults removeObjectForKey:kLastSentTimeKey];
  [defaults removeObjectForKey:kRetryRequestIdKey];
}
