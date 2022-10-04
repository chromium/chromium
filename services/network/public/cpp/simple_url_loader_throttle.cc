// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/simple_url_loader_throttle.h"

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "net/base/network_change_notifier.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/radio_utils.h"
#endif  // BUILDFLAGS(IS_ANDROID)

namespace network {

namespace {

// A SimpleURLLoaderBatcher::Delegate which tries to batch loaders when
// the default network is inactive (e.g. in low power state).
class BatchingDelegate
    : public SimpleURLLoaderThrottle::Delegate,
      public net::NetworkChangeNotifier::DefaultNetworkActiveObserver {
 public:
  explicit BatchingDelegate(SimpleURLLoaderThrottle* owner) : owner_(owner) {}
  ~BatchingDelegate() override {
    if (is_observing_default_network_)
      net::NetworkChangeNotifier::RemoveDefaultNetworkActiveObserver(this);
  }

  bool ShouldThrottle() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (net::NetworkChangeNotifier::IsDefaultNetworkActive())
      return false;

    if (base::PowerMonitor::IsInitialized() &&
        !base::PowerMonitor::IsOnBatteryPower()) {
      return false;
    }

#if BUILDFLAG(IS_ANDROID)
    if (base::android::RadioUtils::GetConnectionType() !=
        base::android::RadioConnectionType::kCell) {
      return false;
    }
#endif  // BUILDFLAGS(IS_ANDROID)

    if (!is_observing_default_network_) {
      net::NetworkChangeNotifier::AddDefaultNetworkActiveObserver(this);
      is_observing_default_network_ = true;
    }

    return true;
  }

 private:
  // net::NetworkChangeNotifier::DefaultNetworkActiveObserver method:
  void OnDefaultNetworkActive() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_observing_default_network_);

    net::NetworkChangeNotifier::RemoveDefaultNetworkActiveObserver(this);
    is_observing_default_network_ = false;

    // `owner_` deletes `this`.
    owner_->OnReadyToStart();
  }

  // Must outlive `this`.
  raw_ptr<SimpleURLLoaderThrottle> const owner_;

  bool is_observing_default_network_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Parses field trial parameters and holds parsed parameters.
class BatchingConfig {
 public:
  BatchingConfig() = default;
  ~BatchingConfig() = default;

  BatchingConfig(const BatchingConfig&) = delete;
  BatchingConfig& operator=(const BatchingConfig&) = delete;
  BatchingConfig(BatchingConfig&&) = delete;
  BatchingConfig& operator=(BatchingConfig&&) = delete;

  bool IsBatchingEnabledForTrafficAnnotation(
      const net::NetworkTrafficAnnotationTag& traffic_annotation) {
    if (!initialized_)
      Initialize();

    return enabled_traffic_annotation_hashes_.contains(
        traffic_annotation.unique_id_hash_code);
  }

  void ResetForTesting() {
    enabled_traffic_annotation_hashes_.clear();
    initialized_ = false;
  }

 private:
  void Initialize() {
    DCHECK(!initialized_);
    DCHECK(enabled_traffic_annotation_hashes_.empty());

    std::string comma_separated_hashes = base::GetFieldTrialParamValueByFeature(
        features::kBatchSimpleURLLoader,
        kBatchSimpleURLLoaderEnabledTrafficAnnotationHashesParam);
    const std::vector<std::string> values =
        base::SplitString(comma_separated_hashes, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    for (const auto& value : values) {
      uint32_t parsed;
      if (!base::StringToUint(value, &parsed))
        continue;
      enabled_traffic_annotation_hashes_.insert(parsed);
    }

    initialized_ = true;
  }

  bool initialized_ = false;
  base::flat_set<uint32_t> enabled_traffic_annotation_hashes_;
};

BatchingConfig& GetBatchingConfig() {
  static base::NoDestructor<BatchingConfig> configuration;
  return *configuration;
}

}  // namespace

// static
bool SimpleURLLoaderThrottle::IsBatchingEnabled(
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  if (!base::FeatureList::IsEnabled(features::kBatchSimpleURLLoader))
    return false;

  if (!GetBatchingConfig().IsBatchingEnabledForTrafficAnnotation(
          traffic_annotation)) {
    return false;
  }

  return true;
}

// static
void SimpleURLLoaderThrottle::ResetConfigForTesting() {
  GetBatchingConfig().ResetForTesting();  // IN-TEST
}

SimpleURLLoaderThrottle::Delegate::Delegate() = default;
SimpleURLLoaderThrottle::Delegate::~Delegate() = default;

SimpleURLLoaderThrottle::SimpleURLLoaderThrottle()
    : delegate_(std::make_unique<BatchingDelegate>(this)) {}

SimpleURLLoaderThrottle::~SimpleURLLoaderThrottle() = default;

void SimpleURLLoaderThrottle::NotifyWhenReady(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());
  DCHECK(!timeout_timer_.IsRunning());
  DCHECK(delegate_);

  bool should_throttle = delegate_->ShouldThrottle();
  base::UmaHistogramBoolean("Network.Radio.SimpleURLLoaderIsThrottled",
                            should_throttle);

  if (!should_throttle) {
    std::move(callback).Run();
    return;
  }

  throttling_start_time_ = base::TimeTicks::Now();
  callback_ = std::move(callback);

  // Unretained is safe because `this` owns `timeout_timer_`.
  timeout_timer_.Start(FROM_HERE, timeout_,
                       base::BindOnce(&SimpleURLLoaderThrottle::OnTimeout,
                                      base::Unretained(this)));
}

void SimpleURLLoaderThrottle::OnReadyToStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_);
  timeout_timer_.Stop();
  delegate_.reset();

  base::TimeDelta throttled_time =
      base::TimeTicks::Now() - throttling_start_time_;
  base::UmaHistogramLongTimes("Network.Radio.SimpleURLLoaderThrottledTime",
                              throttled_time);

  std::move(callback_).Run();
}

void SimpleURLLoaderThrottle::OnTimeout() {
  OnReadyToStart();
}

void SimpleURLLoaderThrottle::SetDelegateForTesting(
    std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void SimpleURLLoaderThrottle::SetTimeoutForTesting(base::TimeDelta timeout) {
  DCHECK(!timeout_timer_.IsRunning());
  timeout_ = timeout;
}

}  // namespace network
