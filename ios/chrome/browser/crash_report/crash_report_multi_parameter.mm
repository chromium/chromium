// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_report_multi_parameter.h"

#include <memory>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/crash_report/crash_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Maximum size of a breakpad parameter. The length of the dictionary serialized
// into JSON cannot exceed this length. See declaration in (BreakPad.h) for
// details.
const int kMaximumBreakpadValueSize = 255;
}

@implementation CrashReportMultiParameter {
  crash_reporter::CrashKeyString<256>* _key;
  base::Value _dictionary;
}

- (instancetype)initWithKey:(crash_reporter::CrashKeyString<256>&)key {
  if ((self = [super init])) {
    _dictionary = base::Value(base::Value::Type::DICTIONARY);
    _key = &key;
  }
  return self;
}

- (void)removeValue:(base::StringPiece)key {
  _dictionary.RemoveKey(key);
  [self updateCrashReport];
}

- (void)setValue:(base::StringPiece)key withValue:(int)value {
  _dictionary.SetIntKey(key, value);
  [self updateCrashReport];
}

- (void)incrementValue:(base::StringPiece)key {
  const int value = _dictionary.FindIntKey(key).value_or(0);
  _dictionary.SetIntKey(key, value + 1);
  [self updateCrashReport];
}

- (void)decrementValue:(base::StringPiece)key {
  const absl::optional<int> maybe_value = _dictionary.FindIntKey(key);
  if (maybe_value.has_value()) {
    const int value = maybe_value.value();
    if (value <= 1) {
      _dictionary.RemoveKey(key);
    } else {
      _dictionary.SetIntKey(key, value - 1);
    }
    [self updateCrashReport];
  }
}

- (void)updateCrashReport {
  std::string stateAsJson;
  base::JSONWriter::Write(_dictionary, &stateAsJson);
  if (stateAsJson.length() > kMaximumBreakpadValueSize) {
    NOTREACHED();
    return;
  }
  _key->Set(stateAsJson);
}

@end
