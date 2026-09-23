// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_backend.h"

#import <Foundation/Foundation.h>

#import <string>
#import <utility>

#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/i18n/time_formatting.h"
#import "base/ios/device_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/no_destructor.h"
#import "base/strings/string_number_conversions.h"
#import "base/system/sys_info.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "base/version.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/omaha/model/omaha_persistent_state.h"
#import "ios/chrome/browser/omaha/model/omaha_ping.h"
#import "ios/chrome/browser/omaha/model/omaha_response.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/public/provider/chrome/browser/omaha/omaha_api.h"
#import "net/base/backoff_entry.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "url/gurl.h"

namespace {

// Constant used to represent an unknown last server date.
inline constexpr int kUnknownLastServerDate = -2;

// Duration between successful requests.
inline constexpr base::TimeDelta kIntervalBetweenRequests = base::Hours(5);

// Maximum size of acceptable response from Omaha server.
inline constexpr size_t kMaxResponseSize = 1024 * 1024;

// Configuration of the exponential backoff.
inline constexpr net::BackoffEntry::Policy kBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = base::Hours(1).InMilliseconds(),
    .multiply_factor = 2.0,
    .jitter_factor = 0.1,
    .maximum_backoff_ms = base::Hours(6).InMicroseconds(),
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false,
};

// Returns whether `lhs` is equal to `rhs`.
constexpr bool IsEqual(const base::Version& lhs, const base::Version& rhs) {
  if (!lhs.IsValid() || !rhs.IsValid()) {
    return !lhs.IsValid() && !rhs.IsValid();
  }

  return lhs == rhs;
}

// Returns whether `last_version` is older than `current_version`.
constexpr bool IsOlder(const base::Version& last_version,
                       const base::Version& current_version) {
  return !last_version.IsValid() || last_version < current_version;
}

// Returns the time to wait before the next attempt.
base::TimeDelta GetBackOff(int number_of_failures) {
  net::BackoffEntry backoff_entry(&kBackoffPolicy);
  for (int i = 0; i < number_of_failures; ++i) {
    backoff_entry.InformOfRequest(false);
  }
  return backoff_entry.GetTimeUntilRelease();
}

// Returns the debug representation of `value`.
base::Value ToDebug(std::string value) {
  return base::Value(std::move(value));
}

base::Value ToDebug(base::Time value) {
  return base::Value(base::TimeFormatShortDateAndTime(value));
}

base::Value ToDebug(int value) {
  return base::Value(base::NumberToString(value));
}

base::Value ToDebug(bool value) {
  return base::Value(base::NumberToString(value ? 1 : 0));
}

base::Value ToDebug(base::TimeDelta value) {
  return base::Value(base::NumberToString(value.InSeconds()));
}

base::Value ToDebug(const base::Version& version) {
  return base::Value(version.GetString());
}

}  // namespace

OmahaBackend::OmahaBackend(std::string locale_lang,
                           base::Time app_install,
                           GURL omaha_server_url,
                           bool auto_schedule)
    : locale_lang_(std::move(locale_lang)),
      app_install_(std::move(app_install)),
      omaha_server_url_(std::move(omaha_server_url)),
      auto_schedule_(auto_schedule) {
  CHECK(!app_install_.is_null());
  CHECK(omaha_server_url_.is_valid());

  // The OmahaBacked instance may be created on a different sequence from
  // the sequence it is used (see OmahaService for such usage). Detach it
  // from the sequence it has been created at the end of the constructor.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

OmahaBackend::~OmahaBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (notification_registration_handle_) {
    id handle = std::exchange(notification_registration_handle_, nil);
    [[NSNotificationCenter defaultCenter] removeObserver:handle];
  }
}

void OmahaBackend::Start(
    OmahaPersistentState initial_state,
    PendingURLLoaderFactoryCallback pending_url_loader_factory,
    UpgradeRecommendedCallback upgrade_recommended_callback,
    SavePersistentStateCallback save_persistent_state_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!url_loader_factory_) << "Start(...) must only be called once.";

  CHECK(!pending_url_loader_factory.is_null());
  CHECK(!save_persistent_state_callback.is_null());
  upgrade_recommended_callback_ = std::move(upgrade_recommended_callback);
  save_persistent_state_callback_ = std::move(save_persistent_state_callback);

  url_loader_factory_ = std::move(pending_url_loader_factory).Run();
  CHECK(url_loader_factory_) << "The factory must return a value.";

  // If the last server date is not set, consider it "unknown" to avoid
  // overcounting in case the date was lost due to a storage issue.
  if (initial_state.last_server_date == 0) {
    initial_state.last_server_date = kUnknownLastServerDate;
  }

  // OmahaService used to require the last sent version to be valid and
  // used "0.0.0.0" to represent a missing last version sent. Convert
  // this value back to base::Version().
  //
  // TODO(crbug.com/555698908): remove this and "base/no_destructor.h"
  // include when OmahaService is re-implemented to use OmahaBackend.
  static base::NoDestructor<base::Version> kDefaultLastSentVersion("0.0.0.0");
  if (IsEqual(initial_state.last_sent_version, *kDefaultLastSentVersion)) {
    initial_state.last_sent_version = base::Version();
  }

  // Copy the initial state after fixing last server date and last sent
  // version as difference in those fields does not require saving the
  // state again (as they only correpond to different way to represent
  // default values on disk vs in memory).
  current_state_ = initial_state;
  bool should_persist_state = false;

  const auto now = base::Time::Now();
  if (current_state_.last_response_time > now) {
    // If the last response time is in the future, the clock has been tampered
    // with. Reset the last reponse time to now.
    current_state_.last_response_time = now;
    should_persist_state = true;
  }

  if (current_state_.next_ping_time - now > kIntervalBetweenRequests) {
    // If the time for the next ping is too far away in the future, the clock
    // has been tampered with. Reschedule the ping to have the usual interval.
    current_state_.next_ping_time =
        current_state_.last_response_time + kIntervalBetweenRequests;
    should_persist_state = true;
  }

  if (IsOlder(current_state_.last_sent_version, version_info::GetVersion())) {
    // Fire a ping as early as possible if the version changed.
    current_state_.next_ping_time = base::Time::Now();
    current_state_.number_of_failures = 0;
    should_persist_state = true;
  }

  if (should_persist_state) {
    PersistState();
  }

  ScheduleNextPing();
}

void OmahaBackend::CheckNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(url_loader_factory_) << "CheckNow() called before Start(...).";
  if (current_url_loader_) {
    // If there is already a ping in progress, nothing to do.
    return;
  }

  SendPing();
}

base::DictValue OmahaBackend::GetDebugInformation() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::DictValue result;

  result.Set("message", ToDebug(GetDefaultPingContent()));
  result.Set("last_sent_time", ToDebug(current_state_.last_response_time));
  result.Set("next_tries_time", ToDebug(current_state_.next_ping_time));
  result.Set("current_ping_time", ToDebug(current_state_.last_ping_time));
  result.Set("last_sent_version", ToDebug(current_state_.last_sent_version));
  result.Set("number_of_tries", ToDebug(current_state_.number_of_failures));

  const bool timer_running = timer_.IsRunning();
  result.Set("timer_running", ToDebug(timer_running));
  if (timer_running) {
    const base::Time desired_run_time =
        base::Time::Now() +
        (timer_.desired_run_time() - base::TimeTicks::Now());

    result.Set("timer_current_delay", ToDebug(timer_.GetCurrentDelay()));
    result.Set("timer_desired_run_time", ToDebug(desired_run_time));
  }

  return result;
}

void OmahaBackend::ScheduleNextPing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!timer_.IsRunning());

  if (!auto_schedule_) {
    // If scheduling is disabled (which can happen for tests that simulate
    // network requests/response) then do not start the timer.
    return;
  }

  timer_.Start(FROM_HERE, current_state_.next_ping_time - base::Time::Now(),
               base::BindOnce(&OmahaBackend::SendPing, base::Unretained(this)));

  if (!notification_registration_handle_) {
    // Once the timer is started, register with the NSNotificationCenter.
    notification_registration_handle_ = [[NSNotificationCenter defaultCenter]
        addObserverForName:@"UIApplicationWillEnterForegroundNotification"
                    object:nil
                     queue:nil
                usingBlock:base::CallbackToBlock(base::BindPostTask(
                               base::SequencedTaskRunner::GetCurrentDefault(),
                               base::IgnoreArgs<NSNotification*>(
                                   base::BindRepeating(
                                       &OmahaBackend::ResyncTimerIfNeeded,
                                       weak_ptr_factory_.GetWeakPtr()))))];
  }
}

void OmahaBackend::SendPing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!current_url_loader_);

  // Cancel any pending timer (it will be restarted when the response from
  // the server is received and parsed). This is safe to call if the timer
  // is not running.
  timer_.Stop();

  // Prepare the request to the Omaha server.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = omaha_server_url_;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->method = "POST";

  if (current_state_.number_of_failures &&
      current_state_.current_request_id != "") {
    // If this is not the first try, notify the omaha server.
    const auto elapsed = base::Time::Now() - current_state_.last_ping_time;
    request->headers.SetHeader("X-RequestAge",
                               base::NumberToString(elapsed.InSeconds()));
  }

  // Update the state before sending the request, and then persist it.
  // This ensure that if the application crashes before the response
  // is received, the backoff will be respected.
  base::Version current_version = version_info::GetVersion();
  const OmahaPingEvent event = GetPingEventForVersion(current_version);

  std::string request_id = ios::device_util::GetRandomId();
  if (event == OmahaPingEvent::kUsagePing) {
    // For usage ping, the request id should not be reused.
    current_state_.current_request_id = "";
  } else {
    // If there is already a saved request id, re-use it, otherwise
    // save the newly generated request id (so that it is reused if
    // the current ping fails).
    if (!current_state_.current_request_id.empty()) {
      request_id = current_state_.current_request_id;
    } else {
      current_state_.current_request_id = request_id;
    }
  }

  if (current_state_.number_of_failures < 30) {
    ++current_state_.number_of_failures;
  }
  const base::TimeDelta backoff = GetBackOff(current_state_.number_of_failures);
  current_state_.next_ping_time = base::Time::Now() + backoff;

  // Persist the state before starting the network request, so that if
  // anything fails catastrophically, the data has time to be persisted.
  PersistState();

  // Start the request.
  current_url_loader_ = network::SimpleURLLoader::Create(
      std::move(request), NO_TRAFFIC_ANNOTATION_YET);
  current_url_loader_->AttachStringForUpload(
      GetPingContent(event, std::move(current_version), std::move(request_id)),
      "text/xml");
  current_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&OmahaBackend::OnServerResponse, base::Unretained(this)),
      kMaxResponseSize);
}

std::string OmahaBackend::GetPingContent(OmahaPingEvent event,
                                         base::Version version,
                                         std::string request_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return FormatOmahaPingEvent(
      event, OmahaPingData{
                 .request_id = std::move(request_id),
                 .session_id = ios::device_util::GetRandomId(),
                 .channel_name = GetChannelString(),
                 .locale_lang = locale_lang_,
                 .hardware_class = base::SysInfo::HardwareModelName(),
                 .os_version = base::SysInfo::OperatingSystemVersion(),
                 .current_version = std::move(version),
                 .previous_version = current_state_.last_sent_version,
                 .installation_time = app_install_,
                 .last_server_date = current_state_.last_server_date,
             });
}

std::string OmahaBackend::GetDefaultPingContent() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Version version = version_info::GetVersion();
  const OmahaPingEvent event = GetPingEventForVersion(version);

  std::string request_id;
  if (event == OmahaPingEvent::kInstallEvent) {
    request_id = current_state_.current_request_id;
  }

  if (request_id.empty()) {
    request_id = ios::device_util::GetRandomId();
  }

  return GetPingContent(event, std::move(version), std::move(request_id));
}

void OmahaBackend::ResyncTimerIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!timer_.IsRunning()) {
    // If the timer isn't running, there is nothing to sync.
    return;
  }

  // base::TimeTicks pauses when the device is asleep, which artificially
  // extends long-running timers. Mitigate this by resynchronizing timers
  // to the expected deadline, by checking base::Time instead. Since this
  // uses wall clock time, it may fire early if the device clock changed
  // (e.g. due to daylight time saving). Sending extra pings is harmless,
  // so this is okay.
  const base::Time now = base::Time::Now();
  if (current_state_.next_ping_time <= now) {
    // If the deadling has already passed, then fire it immediately.
    timer_.FireNow();
    return;
  }

  // The deadline is still in the future but may not match what the timer
  // is currently set to. Reset the timer.
  timer_.Start(FROM_HERE, current_state_.next_ping_time - now,
               base::BindOnce(&OmahaBackend::SendPing, base::Unretained(this)));
}

void OmahaBackend::PersistState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  save_persistent_state_callback_.Run(current_state_);
}

OmahaPingEvent OmahaBackend::GetPingEventForVersion(
    const base::Version& version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsOlder(current_state_.last_sent_version, version)
             ? OmahaPingEvent::kInstallEvent
             : OmahaPingEvent::kUsagePing;
}

void OmahaBackend::OnServerResponse(std::optional<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_url_loader_.reset();

  base::expected<OmahaResponse, OmahaParsingError> parsing_result =
      ParseOmahaResponse(ios::provider::GetOmahaApplicationId(),
                         response_body.value_or(std::string{}));

  if (!parsing_result.has_value()) {
    ScheduleNextPing();
    return;
  }

  OmahaResponse response = std::move(parsing_result).value();

  // Log elapsed time since last successful response.
  const base::Time now = base::Time::Now();
  const base::TimeDelta elapsed = now - current_state_.last_response_time;
  base::UmaHistogramCounts1000("IOS.Omaha.HoursSinceLastSuccess",
                               elapsed.InHours());

  // Update the persistent state, and if the request was an installation
  // request, immediately schedule an active ping event.
  base::Version current_version = version_info::GetVersion();
  const base::Time next_ping_time =
      GetPingEventForVersion(current_version) == OmahaPingEvent::kInstallEvent
          ? now
          : now + kIntervalBetweenRequests;

  current_state_.number_of_failures = 0;
  current_state_.next_ping_time = next_ping_time;
  current_state_.last_ping_time = next_ping_time;
  current_state_.last_response_time = now;
  current_state_.last_sent_version = current_version;
  current_state_.last_server_date = response.server_date;
  current_state_.current_request_id = "";
  PersistState();

  // Send notification for updates if needed.
  if (response.details.has_value()) {
    upgrade_recommended_callback_.Run(std::move(response.details).value());
  }

  ScheduleNextPing();
}
