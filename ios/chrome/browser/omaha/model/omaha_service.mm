// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_service.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/i18n/time_formatting.h"
#import "base/ios/device_util.h"
#import "base/location.h"
#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/field_trial.h"
#import "base/no_destructor.h"
#import "base/rand_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/system/sys_info.h"
#import "base/time/time.h"
#import "base/values.h"
#import "build/branding_buildflags.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/upgrade/model/upgrade_constants.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/public/provider/chrome/browser/omaha/omaha_api.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/base/backoff_entry.h"
#import "net/base/load_flags.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "third_party/libxml/chromium/xml_writer.h"
#import "url/gurl.h"

namespace {
// Number of hours to wait between successful requests.
const int kHoursBetweenRequests = 5;
// Minimal time to wait between retry requests.
const int kPostRetryBaseSeconds = 3600;
// Maximal time to wait between retry requests.
const int64_t kPostRetryMaxSeconds = 6 * kPostRetryBaseSeconds;

const char kCurrentArch[] = "arm64";

// 2 is used because 0 is a magic value for Time, and 1 was the pre-M29 value
// which was migrated to a specific date (crbug.com/270124).
const int64_t kUnknownInstallDate = 2;

// Default last sent application version when none has been sent yet.
const char kDefaultLastSentVersion[] = "0.0.0.0";

// Key for saving states in the UserDefaults.
NSString* const kNextTriesTimesKey = @"ChromeOmahaServiceNextTries";
NSString* const kCurrentPingKey = @"ChromeOmahaServiceCurrentPing";
NSString* const kNumberTriesKey = @"ChromeOmahaServiceNumberTries";
NSString* const kLastSentVersionKey = @"ChromeOmahaServiceLastSentVersion";
NSString* const kLastSentTimeKey = @"ChromeOmahaServiceLastSentTime";
NSString* const kRetryRequestIdKey = @"ChromeOmahaServiceRetryRequestId";
NSString* const kLastServerDateKey = @"ChromeOmahaServiceLastServerDate";

class XmlElement {
 public:
  XmlElement(XmlWriter& writer, const std::string& name)
      : writer_(&writer), name_(name) {
    const bool ok = writer_->StartElement(name_);
    DCHECK(ok);

    ios::provider::SetOmahaExtraAttributes(
        name_,
        base::BindRepeating(&XmlElement::AddAttribute, base::Unretained(this)));
  }

  ~XmlElement() {
    if (writer_) {
      const bool ok = writer_->EndElement();
      DCHECK(ok);
    }

    writer_ = nullptr;
  }

  XmlElement(const XmlElement&) = delete;
  XmlElement& operator=(const XmlElement&) = delete;

  XmlElement(XmlElement&& other) : writer_(nullptr), name_(other.name_) {
    std::swap(writer_, other.writer_);
  }

  XmlElement& operator=(XmlElement&&) = delete;

  XmlElement AddElement(const std::string& name) {
    return XmlElement(*writer_, name);
  }

  void AddAttribute(const std::string& name, const std::string& value) {
    const bool ok = writer_->AddAttribute(name, value);
    DCHECK(ok);
  }

 private:
  raw_ptr<XmlWriter> writer_ = nullptr;
  const std::string name_;
};

class XmlWrapper {
 public:
  XmlWrapper() {
    writer_.StartWriting();
    writer_.StopIndenting();
  }

  ~XmlWrapper() = default;

  XmlWrapper(const XmlWrapper&) = delete;
  XmlWrapper& operator=(const XmlWrapper&) = delete;

  XmlElement AddElement(const std::string& name) {
    return XmlElement(writer_, name);
  }

  std::string GetContentAsString() {
    writer_.StopWriting();
    return writer_.GetWrittenString();
  }

 private:
  XmlWriter writer_;
};

}  // namespace

#pragma mark -

// XML parser for the server response.
@interface ResponseParser : NSObject<NSXMLParserDelegate> {
  BOOL _hasError;
  BOOL _responseIsParsed;
  BOOL _appIsParsed;
  BOOL _updateCheckIsParsed;
  BOOL _urlIsParsed;
  BOOL _manifestIsParsed;
  BOOL _eventIsParsed;
  BOOL _dayStartIsParsed;
  NSString* _appId;
  int _serverDate;
  std::unique_ptr<UpgradeRecommendedDetails> _updateInformation;
}

// Initialization method. `appId` is the application id one expects to find in
// the response message.
- (instancetype)initWithAppId:(NSString*)appId;

// Returns YES if the message has been correctly parsed.
- (BOOL)isCorrect;

// If an upgrade is available, returns the details of the notification to send,
// and returns if Chrome is up to date.
- (UpgradeRecommendedDetails*)upgradeRecommendedDetails;

// If the response was successfully parsed, returns the date according to the
// server.
- (int)serverDate;

@end

@implementation ResponseParser

- (instancetype)initWithAppId:(NSString*)appId {
  if ((self = [super init])) {
    _appId = appId;
  }
  return self;
}

- (BOOL)isCorrect {
  // A response should have either an updatecheck ACK or an event ACK,
  // depending on the contents of the request.
  return !_hasError && (_updateCheckIsParsed || _eventIsParsed);
}

- (UpgradeRecommendedDetails*)upgradeRecommendedDetails {
  return _updateInformation.get();
}

- (int)serverDate {
  return _serverDate;
}

// This method is parsing a message with the following type:
// <response...>
//   <daystart elapsed_days="???" .../>
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
  if (_hasError)
    return;

  // Array of uninteresting tags in the Omaha xml response.
  NSArray* ignoredTagNames =
      @[ @"action", @"actions", @"package", @"packages", @"ping", @"urls" ];
  if ([ignoredTagNames containsObject:elementName])
    return;

  if (!_responseIsParsed) {
    if ([elementName isEqualToString:@"response"] &&
        [[attributeDict valueForKey:@"protocol"] isEqualToString:@"3.0"] &&
        [[attributeDict valueForKey:@"server"] isEqualToString:@"prod"]) {
      _responseIsParsed = YES;
    } else {
      _hasError = YES;
    }
  } else if (!_dayStartIsParsed) {
    if ([elementName isEqualToString:@"daystart"]) {
      _dayStartIsParsed = YES;
      _serverDate = [[attributeDict valueForKey:@"elapsed_days"] integerValue];
    } else {
      _hasError = YES;
    }
  } else if (!_appIsParsed) {
    if ([elementName isEqualToString:@"app"] &&
        [[attributeDict valueForKey:@"status"] isEqualToString:@"ok"] &&
        [[attributeDict valueForKey:@"appid"] isEqualToString:_appId]) {
      _appIsParsed = YES;
    } else {
      _hasError = YES;
    }
  } else if (!_eventIsParsed && !_updateCheckIsParsed) {
    if ([elementName isEqualToString:@"updatecheck"]) {
      _updateCheckIsParsed = YES;
      NSString* status = [attributeDict valueForKey:@"status"];
      _updateInformation = std::make_unique<UpgradeRecommendedDetails>();
      if ([status isEqualToString:@"noupdate"]) {
        // No update is available on the Market, so we won't get a <url> or
        // <manifest> tag.
        _urlIsParsed = YES;
        _manifestIsParsed = YES;
        _updateInformation->is_up_to_date = true;
        [[NSUserDefaults standardUserDefaults] setBool:true
                                                forKey:kIOSChromeUpToDateKey];
      } else if ([status isEqualToString:@"ok"]) {
        _updateInformation->is_up_to_date = false;
        [[NSUserDefaults standardUserDefaults] setBool:false
                                                forKey:kIOSChromeUpToDateKey];
      } else {
        _updateInformation = nullptr;
        _hasError = YES;
      }
    } else if ([elementName isEqualToString:@"event"]) {
      if ([[attributeDict valueForKey:@"status"] isEqualToString:@"ok"]) {
        _eventIsParsed = YES;
      } else {
        _hasError = YES;
      }
    } else {
      _hasError = YES;
    }
  } else if (!_urlIsParsed) {
    if ([elementName isEqualToString:@"url"] &&
        [[attributeDict valueForKey:@"codebase"] length] > 0) {
      _urlIsParsed = YES;
      DCHECK(_updateInformation);
      NSString* url = [attributeDict valueForKey:@"codebase"];
      if ([[url substringFromIndex:([url length] - 1)] isEqualToString:@"/"])
        url = [url substringToIndex:([url length] - 1)];
      _updateInformation->upgrade_url = GURL(base::SysNSStringToUTF8(url));
      if (!_updateInformation->upgrade_url.is_valid())
        _hasError = YES;
    } else {
      _hasError = YES;
    }
  } else if (!_manifestIsParsed) {
    if ([elementName isEqualToString:@"manifest"] &&
        [attributeDict valueForKey:@"version"]) {
      _manifestIsParsed = YES;
      DCHECK(_updateInformation);
      _updateInformation->next_version =
          base::SysNSStringToUTF8([attributeDict valueForKey:@"version"]);
    } else {
      _hasError = YES;
    }
  } else {
    _hasError = YES;
  }
}

@end

// static
bool OmahaService::IsEnabled() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return !tests_hook::DisableUpdateService();
#else
  return false;
#endif
}

// static
OmahaService* OmahaService::GetInstance() {
  // base::NoDestructor creates its OmahaService as soon as this method is
  // entered for the first time. In build variants where Omaha is disabled, that
  // can lead to a scenario where the OmahaService is started but never
  // stopped. Guard against this by ensuring that GetInstance() can only be
  // called when Omaha is enabled.
  DCHECK(IsEnabled());

  static base::NoDestructor<OmahaService> instance;
  return instance.get();
}

// static
void OmahaService::Start(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                             pending_url_loader_factory,
                         const UpgradeRecommendedCallback& callback) {
  DCHECK(pending_url_loader_factory);
  DCHECK(!callback.is_null());

  if (!OmahaService::IsEnabled()) {
    return;
  }

  OmahaService* service = GetInstance();
  service->StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  if (IsOmahaServiceRefactorEnabled()) {
    base::RepeatingCallback<void(const UpgradeRecommendedDetails&)>
        wrapped_callback_that_notifies_observers = base::BindRepeating(
            [](OmahaService* service, UpgradeRecommendedCallback callback,
               const UpgradeRecommendedDetails& details) {
              // `OmahaService` is never destroyed due to `NoDestructor`,
              // ensuring the `base::Unretained(service)` reference below
              // remains valid throughout its lifetime.
              service->task_runner_->PostTask(
                  FROM_HERE,
                  base::BindOnce(&OmahaService::NotifyObservers,
                                 base::Unretained(service), details));

              callback.Run(details);
            },
            service, callback);

    service->set_upgrade_recommended_callback(
        wrapped_callback_that_notifies_observers);
  } else {
    service->set_upgrade_recommended_callback(callback);
  }

  // This should only be called once.
  DCHECK(!service->pending_url_loader_factory_ ||
         !service->url_loader_factory_);
  service->pending_url_loader_factory_ = std::move(pending_url_loader_factory);
  service->locale_lang_ = GetApplicationContext()->GetApplicationLocale();
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&OmahaService::SendOrScheduleNextPing,
                                base::Unretained(service)));
}

// static
void OmahaService::CheckNow(OneOffCallback callback) {
  DCHECK(!callback.is_null());

  if (OmahaService::IsEnabled()) {
    OmahaService* service = GetInstance();
    DUMP_WILL_BE_CHECK(service->started_);
    // TODO(crbug.com/40070635): Remove when early callers are removed.
    if (!service->started_) {
      return;
    }

    if (IsOmahaServiceRefactorEnabled()) {
      CHECK(service->task_runner_);

      base::OnceCallback<void(UpgradeRecommendedDetails)>
          wrapped_callback_that_notifies_observers = base::BindOnce(
              [](OmahaService* service, OneOffCallback callback,
                 const UpgradeRecommendedDetails details) {
                // `OmahaService` is never destroyed due to `NoDestructor`,
                // ensuring the `base::Unretained(service)` reference below
                // remains valid throughout its lifetime.
                service->task_runner_->PostTask(
                    FROM_HERE,
                    base::BindOnce(&OmahaService::NotifyObservers,
                                   base::Unretained(service), details));

                std::move(callback).Run(details);
              },
              service, std::move(callback));

      web::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&OmahaService::CheckNowOnIOThread,
                         base::Unretained(service),
                         std::move(wrapped_callback_that_notifies_observers)));
    } else {
      web::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&OmahaService::CheckNowOnIOThread,
                         base::Unretained(service), std::move(callback)));
    }
  }
}

void OmahaService::AddObserver(OmahaServiceObserver* observer) {
  if (OmahaService::IsEnabled()) {
    GetInstance()->RegisterObserver(observer);
  }
}

void OmahaService::RemoveObserver(OmahaServiceObserver* observer) {
  if (OmahaService::IsEnabled()) {
    GetInstance()->UnregisterObserver(observer);
  }
}

void OmahaService::RegisterObserver(OmahaServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsOmahaServiceRefactorEnabled());

  observers_.AddObserver(observer);
}

void OmahaService::UnregisterObserver(OmahaServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsOmahaServiceRefactorEnabled());

  observers_.RemoveObserver(observer);
}

void OmahaService::CheckNowOnIOThread(OneOffCallback callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(!callback.is_null());

  DCHECK(one_off_check_callback_.is_null());
  one_off_check_callback_ = std::move(callback);

  // If there is not an ongoing ping, send one.
  if (!url_loader_) {
    SendPing();
  } else {
    // The one off ping is taking the scheduled one, so the scheduled ping is
    // now "canceled".
    scheduled_ping_canceled_ = true;
  }
}

OmahaService::OmahaService() : OmahaService(/*schedule=*/true) {}

OmahaService::OmahaService(bool schedule)
    : started_(false),
      schedule_(schedule),
      application_install_date_(0),
      sending_install_event_(false) {}

OmahaService::~OmahaService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.ServiceWillShutdown(this);
  }

  DCHECK(observers_.empty());
}

void OmahaService::StartInternal(
    const scoped_refptr<base::SequencedTaskRunner> task_runner) {
  if (started_) {
    return;
  }
  started_ = true;
  task_runner_ = task_runner;

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
  last_server_date_ = [defaults integerForKey:kLastServerDateKey];
  if (last_server_date_ == 0) {
    // If there is no last server date, this is a first active. However, it
    // may be following a reinstall. To avoid overcounting from neutrinos,
    // transmit -2 ("unknown").
    last_server_date_ = -2;
  }

  application_install_date_ =
      GetApplicationContext()->GetLocalState()->GetInt64(
          metrics::prefs::kInstallDate);
  DCHECK(application_install_date_);

  // Whether data should be persisted again to the user preferences.
  bool persist_again = false;

  base::Time now = base::Time::Now();
  // If `last_sent_time_` is in the future, the clock has been tampered with.
  // Reset `last_sent_time_` to now.
  if (last_sent_time_ > now) {
    last_sent_time_ = now;
    persist_again = true;
  }

  // If the `next_tries_time_` is more than kHoursBetweenRequests hours away,
  // there is a possibility that the clock has been tampered with. Reschedule
  // the ping to be the usual interval after the last successful one.
  if (next_tries_time_ - now > base::Hours(kHoursBetweenRequests)) {
    next_tries_time_ = last_sent_time_ + base::Hours(kHoursBetweenRequests);
    persist_again = true;
  }

  // Fire a ping as early as possible if the version changed.
  const base::Version& current_version = version_info::GetVersion();
  if (last_sent_version_ < current_version) {
    next_tries_time_ = base::Time::Now() - base::Seconds(1);
    number_of_tries_ = 0;
    persist_again = true;
  }

  if (persist_again)
    PersistStates();
}

// static
void OmahaService::GetDebugInformation(
    base::OnceCallback<void(base::Value::Dict)> callback) {
  if (OmahaService::IsEnabled()) {
    OmahaService* service = GetInstance();
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&OmahaService::GetDebugInformationOnIOThread,
                       base::Unretained(service), std::move(callback)));

  } else {
    // Invoke the callback with an empty response.
    web::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::Value::Dict()));
  }
}

// static
base::TimeDelta OmahaService::GetBackOff(uint8_t number_of_tries) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
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
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  XmlWrapper xml_wrapper;

  {
    // Set up <request... />
    XmlElement request_element = xml_wrapper.AddElement("request");
    request_element.AddAttribute("protocol", "3.0");
    request_element.AddAttribute("updater", "iOS");
    request_element.AddAttribute("updaterversion", versionName);
    request_element.AddAttribute("updaterchannel", channelName);
    request_element.AddAttribute("ismachine", "1");
    request_element.AddAttribute("requestid", requestId);
    request_element.AddAttribute("sessionid", sessionId);
    request_element.AddAttribute("hardware_class",
                                 base::SysInfo::HardwareModelName());

    {
      // Set up <os platform="ios"... />
      XmlElement os_element = request_element.AddElement("os");
      os_element.AddAttribute("platform", "ios");
      os_element.AddAttribute("version",
                              base::SysInfo::OperatingSystemVersion());
      os_element.AddAttribute("arch", kCurrentArch);
    }

    const bool is_first_install =
        pingContent == INSTALL_EVENT &&
        last_sent_version_ == base::Version(kDefaultLastSentVersion);

    {
      // Set up <app version="" ...>
      XmlElement app_element = request_element.AddElement("app");
      if (pingContent == INSTALL_EVENT) {
        const std::string previous_version =
            is_first_install ? "" : last_sent_version_.GetString();
        app_element.AddAttribute("version", previous_version);
        app_element.AddAttribute("nextversion", versionName);
      } else {
        app_element.AddAttribute("version", versionName);
        app_element.AddAttribute("nextversion", "");
      }
      app_element.AddAttribute("ap", channelName);
      app_element.AddAttribute("lang", locale_lang_);
      app_element.AddAttribute("client", "");

      std::string install_age;
      if (is_first_install) {
        install_age = "-1";
      } else if (!installationTime.is_null() &&
                 installationTime.ToTimeT() != kUnknownInstallDate) {
        install_age = base::StringPrintf(
            "%d", (base::Time::Now() - installationTime).InDays());
      }

      // If the install date is unknown, send nothing.
      if (!install_age.empty())
        app_element.AddAttribute("installage", install_age);

      if (pingContent == INSTALL_EVENT) {
        // Add an install complete event.
        XmlElement event_element = app_element.AddElement("event");
        if (is_first_install) {
          event_element.AddAttribute("eventtype", "2");  // install
        } else {
          event_element.AddAttribute("eventtype", "3");  // update
        }
        event_element.AddAttribute("eventresult", "1");  // succeeded
      } else {
        // Set up <updatecheck/>
        app_element.AddElement("updatecheck");
      }

      {
        // Set up <ping ... />
        const std::string last_server_date =
            base::StringPrintf("%d", last_server_date_);

        XmlElement ping_element = app_element.AddElement("ping");
        ping_element.AddAttribute("active", "1");
        ping_element.AddAttribute("ad", last_server_date);
        ping_element.AddAttribute("rd", last_server_date);
      }
    }
  }

  return xml_wrapper.GetContentAsString();
}

std::string OmahaService::GetCurrentPingContent() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  const base::Version& current_version = version_info::GetVersion();
  sending_install_event_ = last_sent_version_ < current_version;
  PingContent ping_content =
      sending_install_event_ ? INSTALL_EVENT : USAGE_PING;

  // An install retry ping only makes sense if an install event must be send.
  DCHECK(sending_install_event_ || !IsNextPingInstallRetry());
  std::string request_id = GetNextPingRequestId(ping_content);
  return GetPingContent(
      request_id, ios::device_util::GetRandomId(),
      std::string(version_info::GetVersionNumber()), GetChannelString(),
      base::Time::FromTimeT(application_install_date_), ping_content);
}

void OmahaService::NotifyObservers(UpgradeRecommendedDetails details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsOmahaServiceRefactorEnabled());

  for (auto& observer : observers_) {
    observer.UpgradeRecommendedDetailsChanged(details);
  }
}

void OmahaService::SendPing() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  // If a scheduled ping comes during a one off, drop it.
  if (url_loader_ && !one_off_check_callback_.is_null()) {
    scheduled_ping_canceled_ = true;
    return;
  }

  // Check that no request is in progress.
  DCHECK(!url_loader_);

  const GURL url = ios::provider::GetOmahaUpdateServerURL();
  if (!url.is_valid()) {
    return;
  }

  // There are 2 situations here:
  // 1) production code, where `pending_url_loader_factory_` is used.
  // 2) testing code, where the `url_loader_factory_` creation is triggered by
  // the test.
  if (pending_url_loader_factory_) {
    DCHECK(!url_loader_factory_);
    url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(pending_url_loader_factory_));
    DCHECK(url_loader_factory_);
  } else {
    CHECK(url_loader_factory_);
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

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
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
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
  // As a workaround to crbug.com/1247282, dispatch back to the main thread.
  dispatch_async(dispatch_get_main_queue(), ^{
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
    [defaults setInteger:last_server_date_ forKey:kLastServerDateKey];

    // Save critical state information for usage reporting.
    [defaults synchronize];
  });
}

void OmahaService::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
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
  const std::string application_id = ios::provider::GetOmahaApplicationId();
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
  next_tries_time_ =
      sending_install_event_
          ? base::Time::Now()
          : base::Time::Now() + base::Hours(kHoursBetweenRequests);
  current_ping_time_ = next_tries_time_;
  last_sent_time_ = base::Time::Now();
  last_sent_version_ = version_info::GetVersion();
  sending_install_event_ = false;
  last_server_date_ = [delegate serverDate];
  ClearInstallRetryRequestId();
  PersistStates();
  bool need_to_schedule_ping = true;

  // Send notification for updates if needed.
  UpgradeRecommendedDetails* details = [delegate upgradeRecommendedDetails];
  if (details) {
    // Use the correct callback based on if a one-off check is ongoing.
    if (!one_off_check_callback_.is_null()) {
      web::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(one_off_check_callback_), *details));
      // Do not schedule another ping for one-off checks, unless
      // it canceled a scheduled ping.
      need_to_schedule_ping = scheduled_ping_canceled_;
      scheduled_ping_canceled_ = false;
    } else if (!details->is_up_to_date) {
      web::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(upgrade_recommended_callback_, *details));
    }
  }

  // Schedule next ping if necessary.
  if (need_to_schedule_ping) {
    SendOrScheduleNextPing();
  }
}

void OmahaService::GetDebugInformationOnIOThread(
    base::OnceCallback<void(base::Value::Dict)> callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  base::Value::Dict result;

  result.Set("message", GetCurrentPingContent());
  result.Set("last_sent_time",
             base::TimeFormatShortDateAndTime(last_sent_time_));
  result.Set("next_tries_time",
             base::TimeFormatShortDateAndTime(next_tries_time_));
  result.Set("current_ping_time",
             base::TimeFormatShortDateAndTime(current_ping_time_));
  result.Set("last_sent_version", last_sent_version_.GetString());
  result.Set("number_of_tries", base::StringPrintf("%d", number_of_tries_));
  result.Set("timer_running", base::StringPrintf("%d", timer_.IsRunning()));
  result.Set("timer_current_delay",
             base::StringPrintf("%llds", timer_.GetCurrentDelay().InSeconds()));
  result.Set("timer_desired_run_time",
             base::TimeFormatShortDateAndTime(
                 base::Time::Now() +
                 (timer_.desired_run_time() - base::TimeTicks::Now())));

  // Sending the value to the callback.
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

bool OmahaService::IsNextPingInstallRetry() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  return [[NSUserDefaults standardUserDefaults]
             stringForKey:kRetryRequestIdKey] != nil;
}

std::string OmahaService::GetNextPingRequestId(PingContent ping_content) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
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
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:base::SysUTF8ToNSString(request_id)
               forKey:kRetryRequestIdKey];
  // Save critical state information for usage reporting.
  [defaults synchronize];
}

void OmahaService::ClearInstallRetryRequestId() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kRetryRequestIdKey];
  // Clear critical state information for usage reporting.
  [defaults synchronize];
}

void OmahaService::InitializeURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  url_loader_factory_ = url_loader_factory;
}

void OmahaService::ClearPersistentStateForTests() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kNextTriesTimesKey];
  [defaults removeObjectForKey:kCurrentPingKey];
  [defaults removeObjectForKey:kNumberTriesKey];
  [defaults removeObjectForKey:kLastSentVersionKey];
  [defaults removeObjectForKey:kLastSentTimeKey];
  [defaults removeObjectForKey:kRetryRequestIdKey];
  [defaults removeObjectForKey:kLastServerDateKey];
  [defaults removeObjectForKey:kIOSChromeUpToDateKey];
}
