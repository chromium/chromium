// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/pac_file_decider.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/proxy_resolution/dhcp_pac_file_fetcher.h"
#include "net/proxy_resolution/pac_file_fetcher.h"
#include "net/url_request/url_request_context.h"

namespace net {

namespace {

bool LooksLikePacScript(const base::string16& script) {
  // Note: this is only an approximation! It may not always work correctly,
  // however it is very likely that legitimate scripts have this exact string,
  // since they must minimally define a function of this name. Conversely, a
  // file not containing the string is not likely to be a PAC script.
  //
  // An exact test would have to load the script in a javascript evaluator.
  return script.find(base::ASCIIToUTF16("FindProxyForURL")) !=
         base::string16::npos;
}

// This is the hard-coded location used by the DNS portion of web proxy
// auto-discovery.
//
// Note that we not use DNS devolution to find the WPAD host, since that could
// be dangerous should our top level domain registry  become out of date.
//
// Instead we directly resolve "wpad", and let the operating system apply the
// DNS suffix search paths. This is the same approach taken by Firefox, and
// compatibility hasn't been an issue.
//
// For more details, also check out this comment:
// http://code.google.com/p/chromium/issues/detail?id=18575#c20
const char kWpadUrl[] = "http://wpad/wpad.dat";
const int kQuickCheckDelayMs = 1000;

}  // namespace

PacFileDataWithSource::PacFileDataWithSource() = default;
PacFileDataWithSource::~PacFileDataWithSource() = default;
PacFileDataWithSource::PacFileDataWithSource(const PacFileDataWithSource&) =
    default;
PacFileDataWithSource& PacFileDataWithSource::operator=(
    const PacFileDataWithSource&) = default;

base::Value PacFileDecider::PacSource::NetLogParams(
    const GURL& effective_pac_url) const {
  base::Value dict(base::Value::Type::DICTIONARY);
  std::string source;
  switch (type) {
    case PacSource::WPAD_DHCP:
      source = "WPAD DHCP";
      break;
    case PacSource::WPAD_DNS:
      source = "WPAD DNS: ";
      source += effective_pac_url.possibly_invalid_spec();
      break;
    case PacSource::CUSTOM:
      source = "Custom PAC URL: ";
      source += effective_pac_url.possibly_invalid_spec();
      break;
  }
  dict.SetStringKey("source", source);
  return dict;
}

PacFileDecider::PacFileDecider(PacFileFetcher* pac_file_fetcher,
                               DhcpPacFileFetcher* dhcp_pac_file_fetcher,
                               NetLog* net_log)
    : pac_file_fetcher_(pac_file_fetcher),
      dhcp_pac_file_fetcher_(dhcp_pac_file_fetcher),
      current_pac_source_index_(0u),
      pac_mandatory_(false),
      next_state_(STATE_NONE),
      net_log_(
          NetLogWithSource::Make(net_log, NetLogSourceType::PAC_FILE_DECIDER)),
      fetch_pac_bytes_(false),
      quick_check_enabled_(true) {}

PacFileDecider::~PacFileDecider() {
  if (next_state_ != STATE_NONE)
    Cancel();
}

int PacFileDecider::Start(const ProxyConfigWithAnnotation& config,
                          const base::TimeDelta wait_delay,
                          bool fetch_pac_bytes,
                          CompletionOnceCallback callback) {
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(!callback.is_null());
  DCHECK(config.value().HasAutomaticSettings());

  net_log_.BeginEvent(NetLogEventType::PAC_FILE_DECIDER);

  fetch_pac_bytes_ = fetch_pac_bytes;

  // Save the |wait_delay| as a non-negative value.
  wait_delay_ = wait_delay;
  if (wait_delay_ < base::TimeDelta())
    wait_delay_ = base::TimeDelta();

  pac_mandatory_ = config.value().pac_mandatory();
  have_custom_pac_url_ = config.value().has_pac_url();

  pac_sources_ = BuildPacSourcesFallbackList(config.value());
  DCHECK(!pac_sources_.empty());

  traffic_annotation_ =
      net::MutableNetworkTrafficAnnotationTag(config.traffic_annotation());
  next_state_ = STATE_WAIT;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);
  else
    DidComplete();

  return rv;
}

void PacFileDecider::OnShutdown() {
  // Don't do anything if idle.
  if (next_state_ == STATE_NONE)
    return;

  // Just cancel any pending work.
  Cancel();
}

const ProxyConfigWithAnnotation& PacFileDecider::effective_config() const {
  DCHECK_EQ(STATE_NONE, next_state_);
  return effective_config_;
}

const PacFileDataWithSource& PacFileDecider::script_data() const {
  DCHECK_EQ(STATE_NONE, next_state_);
  return script_data_;
}

// Initialize the fallback rules.
// (1) WPAD (DHCP).
// (2) WPAD (DNS).
// (3) Custom PAC URL.
PacFileDecider::PacSourceList PacFileDecider::BuildPacSourcesFallbackList(
    const ProxyConfig& config) const {
  PacSourceList pac_sources;
  if (config.auto_detect()) {
    pac_sources.push_back(PacSource(PacSource::WPAD_DHCP, GURL(kWpadUrl)));
    pac_sources.push_back(PacSource(PacSource::WPAD_DNS, GURL(kWpadUrl)));
  }
  if (config.has_pac_url())
    pac_sources.push_back(PacSource(PacSource::CUSTOM, config.pac_url()));
  return pac_sources;
}

void PacFileDecider::OnIOCompletion(int result) {
  DCHECK_NE(STATE_NONE, next_state_);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    DidComplete();
    std::move(callback_).Run(rv);
  }
}

int PacFileDecider::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_WAIT:
        DCHECK_EQ(OK, rv);
        rv = DoWait();
        break;
      case STATE_WAIT_COMPLETE:
        rv = DoWaitComplete(rv);
        break;
      case STATE_QUICK_CHECK:
        DCHECK_EQ(OK, rv);
        rv = DoQuickCheck();
        break;
      case STATE_QUICK_CHECK_COMPLETE:
        rv = DoQuickCheckComplete(rv);
        break;
      case STATE_FETCH_PAC_SCRIPT:
        DCHECK_EQ(OK, rv);
        rv = DoFetchPacScript();
        break;
      case STATE_FETCH_PAC_SCRIPT_COMPLETE:
        rv = DoFetchPacScriptComplete(rv);
        break;
      case STATE_VERIFY_PAC_SCRIPT:
        DCHECK_EQ(OK, rv);
        rv = DoVerifyPacScript();
        break;
      case STATE_VERIFY_PAC_SCRIPT_COMPLETE:
        rv = DoVerifyPacScriptComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int PacFileDecider::DoWait() {
  next_state_ = STATE_WAIT_COMPLETE;

  // If no waiting is required, continue on to the next state.
  if (wait_delay_.ToInternalValue() == 0)
    return OK;

  // Otherwise wait the specified amount of time.
  wait_timer_.Start(FROM_HERE, wait_delay_, this,
                    &PacFileDecider::OnWaitTimerFired);
  net_log_.BeginEvent(NetLogEventType::PAC_FILE_DECIDER_WAIT);
  return ERR_IO_PENDING;
}

int PacFileDecider::DoWaitComplete(int result) {
  DCHECK_EQ(OK, result);
  if (wait_delay_.ToInternalValue() != 0) {
    net_log_.EndEventWithNetErrorCode(NetLogEventType::PAC_FILE_DECIDER_WAIT,
                                      result);
  }
  if (quick_check_enabled_ && current_pac_source().type == PacSource::WPAD_DNS)
    next_state_ = STATE_QUICK_CHECK;
  else
    next_state_ = GetStartState();
  return OK;
}

int PacFileDecider::DoQuickCheck() {
  DCHECK(quick_check_enabled_);
  if (!pac_file_fetcher_ || !pac_file_fetcher_->GetRequestContext() ||
      !pac_file_fetcher_->GetRequestContext()->host_resolver()) {
    // If we have no resolver, skip QuickCheck altogether.
    next_state_ = GetStartState();
    return OK;
  }

  std::string host = current_pac_source().url.host();

  HostResolver::ResolveHostParameters parameters;
  // We use HIGHEST here because proxy decision blocks doing any other requests.
  parameters.initial_priority = HIGHEST;
  // Only resolve via the system resolver for maximum compatibility with DNS
  // suffix search paths, because for security, we are relying on suffix search
  // paths rather than WPAD-standard DNS devolution.
  parameters.source = HostResolverSource::SYSTEM;

  HostResolver* host_resolver =
      pac_file_fetcher_->GetRequestContext()->host_resolver();
  // It's safe to use an empty NetworkIsolationKey() here, since this is only
  // for fetching the PAC script, so can't usefully leak data to web-initiated
  // requests (Which can't use an empty NIK for resolving IPs other than that of
  // the proxy).
  resolve_request_ = host_resolver->CreateRequest(
      HostPortPair(host, 80), NetworkIsolationKey(), net_log_, parameters);

  CompletionRepeatingCallback callback = base::BindRepeating(
      &PacFileDecider::OnIOCompletion, base::Unretained(this));

  next_state_ = STATE_QUICK_CHECK_COMPLETE;
  quick_check_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kQuickCheckDelayMs),
      base::BindOnce(callback, ERR_NAME_NOT_RESOLVED));

  return resolve_request_->Start(callback);
}

int PacFileDecider::DoQuickCheckComplete(int result) {
  DCHECK(quick_check_enabled_);
  resolve_request_.reset();
  quick_check_timer_.Stop();
  if (result != OK)
    return TryToFallbackPacSource(result);
  next_state_ = GetStartState();
  return result;
}

int PacFileDecider::DoFetchPacScript() {
  DCHECK(fetch_pac_bytes_);

  next_state_ = STATE_FETCH_PAC_SCRIPT_COMPLETE;

  const PacSource& pac_source = current_pac_source();

  GURL effective_pac_url;
  DetermineURL(pac_source, &effective_pac_url);

  net_log_.BeginEvent(NetLogEventType::PAC_FILE_DECIDER_FETCH_PAC_SCRIPT, [&] {
    return pac_source.NetLogParams(effective_pac_url);
  });

  if (pac_source.type == PacSource::WPAD_DHCP) {
    if (!dhcp_pac_file_fetcher_) {
      net_log_.AddEvent(NetLogEventType::PAC_FILE_DECIDER_HAS_NO_FETCHER);
      return ERR_UNEXPECTED;
    }

    return dhcp_pac_file_fetcher_->Fetch(
        &pac_script_,
        base::BindOnce(&PacFileDecider::OnIOCompletion, base::Unretained(this)),
        net_log_, NetworkTrafficAnnotationTag(traffic_annotation_));
  }

  if (!pac_file_fetcher_) {
    net_log_.AddEvent(NetLogEventType::PAC_FILE_DECIDER_HAS_NO_FETCHER);
    return ERR_UNEXPECTED;
  }

  return pac_file_fetcher_->Fetch(
      effective_pac_url, &pac_script_,
      base::BindOnce(&PacFileDecider::OnIOCompletion, base::Unretained(this)),
      NetworkTrafficAnnotationTag(traffic_annotation_));
}

int PacFileDecider::DoFetchPacScriptComplete(int result) {
  DCHECK(fetch_pac_bytes_);

  net_log_.EndEventWithNetErrorCode(
      NetLogEventType::PAC_FILE_DECIDER_FETCH_PAC_SCRIPT, result);
  if (result != OK)
    return TryToFallbackPacSource(result);

  next_state_ = STATE_VERIFY_PAC_SCRIPT;
  return result;
}

int PacFileDecider::DoVerifyPacScript() {
  next_state_ = STATE_VERIFY_PAC_SCRIPT_COMPLETE;

  // This is just a heuristic. Ideally we would try to parse the script.
  if (fetch_pac_bytes_ && !LooksLikePacScript(pac_script_))
    return ERR_PAC_SCRIPT_FAILED;

  return OK;
}

int PacFileDecider::DoVerifyPacScriptComplete(int result) {
  if (result != OK)
    return TryToFallbackPacSource(result);

  const PacSource& pac_source = current_pac_source();

  // Extract the current script data.
  script_data_.from_auto_detect = pac_source.type != PacSource::CUSTOM;
  if (fetch_pac_bytes_) {
    script_data_.data = PacFileData::FromUTF16(pac_script_);
  } else {
    script_data_.data = pac_source.type == PacSource::CUSTOM
                            ? PacFileData::FromURL(pac_source.url)
                            : PacFileData::ForAutoDetect();
  }

  // Let the caller know which automatic setting we ended up initializing the
  // resolver for (there may have been multiple fallbacks to choose from.)
  ProxyConfig config;
  if (current_pac_source().type == PacSource::CUSTOM) {
    config = ProxyConfig::CreateFromCustomPacURL(current_pac_source().url);
    config.set_pac_mandatory(pac_mandatory_);
  } else {
    if (fetch_pac_bytes_) {
      GURL auto_detected_url;

      switch (current_pac_source().type) {
        case PacSource::WPAD_DHCP:
          auto_detected_url = dhcp_pac_file_fetcher_->GetPacURL();
          break;

        case PacSource::WPAD_DNS:
          auto_detected_url = GURL(kWpadUrl);
          break;

        default:
          NOTREACHED();
      }

      config = ProxyConfig::CreateFromCustomPacURL(auto_detected_url);
    } else {
      // The resolver does its own resolution so we cannot know the
      // URL. Just do the best we can and state that the configuration
      // is to auto-detect proxy settings.
      config = ProxyConfig::CreateAutoDetect();
    }
  }

  effective_config_ = ProxyConfigWithAnnotation(
      config, net::NetworkTrafficAnnotationTag(traffic_annotation_));

  return OK;
}

int PacFileDecider::TryToFallbackPacSource(int error) {
  DCHECK_LT(error, 0);

  if (current_pac_source_index_ + 1 >= pac_sources_.size()) {
    // Nothing left to fall back to.
    return error;
  }

  // Advance to next URL in our list.
  ++current_pac_source_index_;

  net_log_.AddEvent(
      NetLogEventType::PAC_FILE_DECIDER_FALLING_BACK_TO_NEXT_PAC_SOURCE);
  if (quick_check_enabled_ && current_pac_source().type == PacSource::WPAD_DNS)
    next_state_ = STATE_QUICK_CHECK;
  else
    next_state_ = GetStartState();

  return OK;
}

PacFileDecider::State PacFileDecider::GetStartState() const {
  return fetch_pac_bytes_ ? STATE_FETCH_PAC_SCRIPT : STATE_VERIFY_PAC_SCRIPT;
}

void PacFileDecider::DetermineURL(const PacSource& pac_source,
                                  GURL* effective_pac_url) {
  DCHECK(effective_pac_url);

  switch (pac_source.type) {
    case PacSource::WPAD_DHCP:
      break;
    case PacSource::WPAD_DNS:
      *effective_pac_url = GURL(kWpadUrl);
      break;
    case PacSource::CUSTOM:
      *effective_pac_url = pac_source.url;
      break;
  }
}

const PacFileDecider::PacSource& PacFileDecider::current_pac_source() const {
  DCHECK_LT(current_pac_source_index_, pac_sources_.size());
  return pac_sources_[current_pac_source_index_];
}

void PacFileDecider::OnWaitTimerFired() {
  OnIOCompletion(OK);
}

void PacFileDecider::DidComplete() {
  net_log_.EndEvent(NetLogEventType::PAC_FILE_DECIDER);
}

void PacFileDecider::Cancel() {
  DCHECK_NE(STATE_NONE, next_state_);

  net_log_.AddEvent(NetLogEventType::CANCELLED);

  switch (next_state_) {
    case STATE_QUICK_CHECK_COMPLETE:
      resolve_request_.reset();
      break;
    case STATE_WAIT_COMPLETE:
      wait_timer_.Stop();
      break;
    case STATE_FETCH_PAC_SCRIPT_COMPLETE:
      pac_file_fetcher_->Cancel();
      break;
    default:
      break;
  }

  next_state_ = STATE_NONE;

  // This is safe to call in any state.
  if (dhcp_pac_file_fetcher_)
    dhcp_pac_file_fetcher_->Cancel();

  DCHECK(!resolve_request_);

  DidComplete();
}

}  // namespace net
