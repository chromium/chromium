// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_FEEDBACK_DATA_H_
#define REMOTING_CLIENT_FEEDBACK_DATA_H_

#include <map>
#include <string>

#include "remoting/base/chromoting_event.h"

namespace remoting {

// Class that stores additional data to be sent with the user feedback.
class FeedbackData {
 public:
  enum class Key {
    SESSION_PREVIOUS_STATE,
    SESSION_STATE,
    SESSION_ERROR,
    SESSION_MODE,
    SESSION_CREDENTIALS_TYPE,
    SESSION_HOST_OS,
    SESSION_HOST_OS_VERSION,
    SESSION_HOST_VERSION,
    SESSION_PERFORMANCE_STATS,
    SESSION_PEER_CONNECTION_STATS,
  };

  FeedbackData();

  FeedbackData(const FeedbackData&) = delete;
  FeedbackData& operator=(const FeedbackData&) = delete;

  ~FeedbackData();

  void SetData(Key key, const std::string& data);

  void FillWithChromotingEvent(const ChromotingEvent& event);

  const std::map<Key, std::string>& data() const { return data_; }

  static std::string KeyToString(Key key);

 private:
  std::map<Key, std::string> data_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_FEEDBACK_DATA_H_
