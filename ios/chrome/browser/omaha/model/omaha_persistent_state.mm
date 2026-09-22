// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_persistent_state.h"

#import <Foundation/Foundation.h>

#import <string>
#import <string_view>
#import <type_traits>

#import "base/memory/raw_ref.h"
#import "base/strings/sys_string_conversions.h"

namespace {

// Keys used for saving the state in the NSUserDefaults.
NSString* const kNextPingTimeKey = @"ChromeOmahaServiceNextTries";
NSString* const kLastPingTimeKey = @"ChromeOmahaServiceCurrentPing";
NSString* const kLastResponseTimeKey = @"ChromeOmahaServiceLastSentTime";
NSString* const kLastSentVersionKey = @"ChromeOmahaServiceLastSentVersion";
NSString* const kCurrentRequestIdKey = @"ChromeOmahaServiceRetryRequestId";
NSString* const kNumberOfFailuresKey = @"ChromeOmahaServiceNumberTries";
NSString* const kLastServerDateKey = @"ChromeOmahaServiceLastServerDate";

// Default last application version when none have been sent yet.
inline constexpr std::string_view kDefaultLastSendVersion = "0.0.0.0";

// Helper class used to serialize a value to NSUserDefaults.
template <typename T, typename MaybeConstT>
class ValueSerializer {
 public:
  constexpr ValueSerializer(MaybeConstT& ref, NSString* key)
      : store_(ref), key_(key) {}

  ValueSerializer(const ValueSerializer&) = delete;
  ValueSerializer& operator=(const ValueSerializer&) = delete;

  constexpr ~ValueSerializer() = default;

  // Save value to NSUserDefaults.
  constexpr void WriteTo(NSUserDefaults* defaults) const {
    if (IsDefaultValue()) {
      [defaults removeObjectForKey:key_];
      return;
    }

    if constexpr (std::is_same_v<T, int>) {
      [defaults setInteger:*store_ forKey:key_];
    }

    if constexpr (std::is_same_v<T, base::Time>) {
      [defaults setDouble:store_->ToCFAbsoluteTime() forKey:key_];
    }

    if constexpr (std::is_same_v<T, std::string>) {
      [defaults setObject:base::SysUTF8ToNSString(*store_) forKey:key_];
    }

    if constexpr (std::is_same_v<T, base::Version>) {
      [defaults setObject:base::SysUTF8ToNSString(store_->GetString())
                   forKey:key_];
    }
  }

  // Load value from NSUserDefaults.
  constexpr void LoadFrom(NSUserDefaults* defaults)
    requires(std::is_same_v<T, MaybeConstT>)
  {
    if constexpr (std::is_same_v<T, int>) {
      *store_ = [defaults integerForKey:key_];
    }

    if constexpr (std::is_same_v<T, base::Time>) {
      *store_ = base::Time::FromCFAbsoluteTime([defaults doubleForKey:key_]);
    }

    if constexpr (std::is_same_v<T, std::string>) {
      *store_ = base::SysNSStringToUTF8([defaults stringForKey:key_]);
    }

    if constexpr (std::is_same_v<T, base::Version>) {
      std::string value = base::SysNSStringToUTF8([defaults stringForKey:key_]);
      *store_ = base::Version(!value.empty() ? std::string_view(value)
                                             : kDefaultLastSendVersion);
    }
  }

  // Returns whether the value is default.
  constexpr bool IsDefaultValue() const {
    if constexpr (std::is_same_v<T, int>) {
      return *store_ == 0;
    }

    if constexpr (std::is_same_v<T, base::Time>) {
      return *store_ == base::Time();
    }

    if constexpr (std::is_same_v<T, std::string>) {
      return store_->empty();
    }

    if constexpr (std::is_same_v<T, base::Version>) {
      return !store_->IsValid() ||
             *store_ == base::Version(kDefaultLastSendVersion);
    }
  }

 private:
  raw_ref<MaybeConstT> store_;
  NSString* key_;
};

// Deduction helper.
template <typename MaybeConstT>
constexpr auto Serializer(MaybeConstT& ref, NSString* key) {
  using T = std::remove_const_t<MaybeConstT>;
  return ValueSerializer<T, MaybeConstT>(ref, key);
}

// Visit all member variable of `state`.
template <typename Functor, typename MaybeConstOmahaPersistentState>
constexpr void Visit(MaybeConstOmahaPersistentState& state, Functor functor) {
  functor(Serializer(state.next_ping_time, kNextPingTimeKey));
  functor(Serializer(state.last_ping_time, kLastPingTimeKey));
  functor(Serializer(state.last_response_time, kLastResponseTimeKey));
  functor(Serializer(state.last_sent_version, kLastSentVersionKey));
  functor(Serializer(state.current_request_id, kCurrentRequestIdKey));
  functor(Serializer(state.number_of_failures, kNumberOfFailuresKey));
  functor(Serializer(state.last_server_date, kLastServerDateKey));
}

}  // namespace

// static
OmahaPersistentState OmahaPersistentState::LoadFrom(NSUserDefaults* defaults) {
  OmahaPersistentState state;
  Visit(state, [=](auto serializer) { serializer.LoadFrom(defaults); });
  return state;
}

// static
void OmahaPersistentState::SaveTo(NSUserDefaults* defaults,
                                  const OmahaPersistentState& state) {
  Visit(state, [=](auto serializer) { serializer.WriteTo(defaults); });

  // Save critical state information for usage reporting.
  [defaults synchronize];
}
