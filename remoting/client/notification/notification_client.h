// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_NOTIFICATION_NOTIFICATION_CLIENT_H_
#define REMOTING_CLIENT_NOTIFICATION_NOTIFICATION_CLIENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"

namespace remoting {

class JsonFetcher;
struct NotificationMessage;

// Class for fetching a notification from the server so that the caller can
// show that on some UI component when the app is launched.
class NotificationClient final {
 public:
  using NotificationCallback =
      base::OnceCallback<void(std::optional<NotificationMessage>)>;

  explicit NotificationClient(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);

  NotificationClient(const NotificationClient&) = delete;
  NotificationClient& operator=(const NotificationClient&) = delete;

  ~NotificationClient();

  // Fetches notifications from the server and calls |callback| with the
  // best matched notification. If notifications failed to fetch or no matching
  // notification is found then std::nullopt will be returned. |callback| will
  // be silently dropped if |this| is deleted before the notification is
  // fetched.
  // |user_email| is used to determine if the notification is available to the
  // user during percentage rollout. If |user_email| is empty (i.e. user not
  // logged in), the notification percentage must be exactly 100 for the
  // notification to become available.
  void GetNotification(const std::string& user_email,
                       NotificationCallback callback);

 private:
  friend class NotificationClientTest;

  // Constructor for unittest.
  NotificationClient(std::unique_ptr<JsonFetcher> fetcher,
                     const std::string& current_platform,
                     const std::string& current_version,
                     const std::string& current_os_version,
                     const std::string& locale,
                     bool should_ignore_dev_messages);

  void OnRulesFetched(const std::string& user_email,
                      NotificationCallback callback,
                      std::optional<base::Value> rules);

  // Returns non-empty NotificationMessage if the rule is parsed successfully
  // and the rule should apply to the user. |message_text| and |link_text| will
  // not be set and caller needs to call FetchTranslatedText to fill them up.
  std::optional<NotificationMessage> ParseAndMatchRule(
      const base::Value::Dict& rule,
      const std::string& user_email,
      std::string* out_message_text_filename,
      std::string* out_link_text_filename);

  void FetchTranslatedTexts(const std::string& message_text_filename,
                            const std::string& link_text_filename,
                            std::optional<NotificationMessage> partial_message,
                            NotificationCallback done);

  std::unique_ptr<JsonFetcher> fetcher_;
  std::string current_platform_;
  std::string current_version_;
  std::string current_os_version_;
  std::string locale_;
  bool should_ignore_dev_messages_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_NOTIFICATION_NOTIFICATION_CLIENT_H_
