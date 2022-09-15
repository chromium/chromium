// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/feedback_data.h"

#include "base/values.h"
#include "remoting/base/name_value_map.h"

namespace remoting {

namespace {

const NameMapElement<FeedbackData::Key> kFeedbackDataKeyNames[]{
    {FeedbackData::Key::SESSION_PREVIOUS_STATE, "session-previous-state"},
    {FeedbackData::Key::SESSION_STATE, "session-state"},
    {FeedbackData::Key::SESSION_ERROR, "session-error"},
    {FeedbackData::Key::SESSION_MODE, "session-mode"},
    {FeedbackData::Key::SESSION_HOST_OS, "session-host-os"},
    {FeedbackData::Key::SESSION_HOST_OS_VERSION, "session-host-os-version"},
    {FeedbackData::Key::SESSION_HOST_VERSION, "session-host-version"},
    {FeedbackData::Key::SESSION_CREDENTIALS_TYPE, "session-credentials-type"},

    // TODO(yuweih): Collect session info for these fields.
    {FeedbackData::Key::SESSION_PERFORMANCE_STATS, "session-performance-stats"},
    {FeedbackData::Key::SESSION_PEER_CONNECTION_STATS,
     "session-peer-connection-stats"},
};

template <typename EnumType>
void SetEnumIfNotEmpty(std::map<FeedbackData::Key, std::string>* data,
                       FeedbackData::Key key,
                       const ChromotingEvent& event,
                       const std::string& event_key) {
  const base::Value* value = event.GetValue(event_key);
  if (!value) {
    return;
  }
  auto enum_value = static_cast<EnumType>(value->GetInt());
  const char* string_value = ChromotingEvent::EnumToString(enum_value);
  DCHECK(string_value);
  (*data)[key] = string_value;
}

void SetStringIfNotEmpty(std::map<FeedbackData::Key, std::string>* data,
                         FeedbackData::Key key,
                         const ChromotingEvent& event,
                         const std::string& event_key) {
  const base::Value* value = event.GetValue(event_key);
  if (!value) {
    return;
  }
  (*data)[key] = value->GetString();
}

}  // namespace

FeedbackData::FeedbackData() {}

FeedbackData::~FeedbackData() {}

void FeedbackData::SetData(Key key, const std::string& data) {
  data_[key] = data;
}

void FeedbackData::FillWithChromotingEvent(const ChromotingEvent& event) {
  SetEnumIfNotEmpty<ChromotingEvent::SessionState>(
      &data_, Key::SESSION_PREVIOUS_STATE, event,
      ChromotingEvent::kPreviousSessionStateKey);
  SetEnumIfNotEmpty<ChromotingEvent::SessionState>(
      &data_, Key::SESSION_STATE, event, ChromotingEvent::kSessionStateKey);
  SetEnumIfNotEmpty<ChromotingEvent::ConnectionError>(
      &data_, Key::SESSION_ERROR, event, ChromotingEvent::kConnectionErrorKey);
  SetEnumIfNotEmpty<ChromotingEvent::Mode>(&data_, Key::SESSION_MODE, event,
                                           ChromotingEvent::kModeKey);
  SetEnumIfNotEmpty<ChromotingEvent::Os>(&data_, Key::SESSION_HOST_OS, event,
                                         ChromotingEvent::kHostOsKey);
  SetEnumIfNotEmpty<ChromotingEvent::AuthMethod>(
      &data_, Key::SESSION_CREDENTIALS_TYPE, event,
      ChromotingEvent::kAuthMethodKey);
  SetStringIfNotEmpty(&data_, Key::SESSION_HOST_OS_VERSION, event,
                      ChromotingEvent::kHostOsVersionKey);
  SetStringIfNotEmpty(&data_, Key::SESSION_HOST_VERSION, event,
                      ChromotingEvent::kHostVersionKey);
}

// static
std::string FeedbackData::KeyToString(Key key) {
  return ValueToName(kFeedbackDataKeyNames, key);
}

}  // namespace remoting
