// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/browser/sms/sms_provider.h"
#include "content/public/common/content_switches.h"
#include "url/gurl.h"
#include "url/origin.h"
#if BUILDFLAG(IS_ANDROID)
#include "content/browser/sms/sms_provider_gms.h"
#endif

namespace content {

SmsProvider::SmsProvider() = default;
SmsProvider::~SmsProvider() = default;

// static
std::unique_ptr<SmsProvider> SmsProvider::Create() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<SmsProviderGms>();
#else
  return nullptr;
#endif
}

void SmsProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SmsProvider::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SmsProvider::NotifyReceive(const std::string& sms,
                                UserConsent consent_requirement) {
  SmsParser::Result result = SmsParser::Parse(sms);
  if (result.IsValid())
    NotifyReceive(result.GetOriginList(), result.one_time_code,
                  consent_requirement);
  RecordParsingStatus(result.parsing_status);
}

void SmsProvider::NotifyReceive(const OriginList& origin_list,
                                const std::string& one_time_code,
                                UserConsent consent_requirement) {
  for (Observer& obs : observers_) {
    bool handled =
        obs.OnReceive(origin_list, one_time_code, consent_requirement);
    if (handled) {
      break;
    }
  }
}

void SmsProvider::NotifyReceiveForTesting(const std::string& sms,
                                          UserConsent requirement) {
  NotifyReceive(sms, requirement);
}

void SmsProvider::NotifyFailure(FailureType failure_type) {
  for (Observer& obs : observers_) {
    bool handled = obs.OnFailure(failure_type);
    if (handled)
      break;
  }
}

void SmsProvider::RecordParsingStatus(SmsParsingStatus status) {
  if (status == SmsParsingStatus::kParsed)
    return;

  switch (status) {
    case SmsParsingStatus::kOTPFormatRegexNotMatch:
      NotifyFailure(FailureType::kSmsNotParsed_OTPFormatRegexNotMatch);
      break;
    case SmsParsingStatus::kHostAndPortNotParsed:
      NotifyFailure(FailureType::kSmsNotParsed_HostAndPortNotParsed);
      break;
    case SmsParsingStatus::kGURLNotValid:
      NotifyFailure(FailureType::kSmsNotParsed_kGURLNotValid);
      break;
    case SmsParsingStatus::kParsed:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

bool SmsProvider::HasObservers() {
  return !observers_.empty();
}

}  // namespace content
