// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_NOTIFICATION_NOTIFICATION_CLIENT_H_
#define REMOTING_CLIENT_NOTIFICATION_NOTIFICATION_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"

namespace base {
class Value;
}  // namespace base

namespace remoting {

class JsonFetcher;
struct NotificationMessage;

// Class for fetching a notification from the server so that the caller can
// show that on some UI component when the app is launched.
class NotificationClient final {
 public:
  using NotificationCallback =
      base::OnceCallback<void(base::Optional<NotificationMessage>)>;

  explicit NotificationClient(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);
  ~NotificationClient();

  // Fetches notifications from the server and calls |callback| with the
  // best matched notification. If notifications failed to fetch or no matching
  // notification is found then base::nullopt will be returned. |callback| will
  // be silently dropped if |this| is deleted before the notification is
  // fetched.
  void GetNotification(const std::string& user_email,
                       NotificationCallback callback);

 private:
  friend class NotificationClientTest;

  // Constructor for unittest.
  NotificationClient(std::unique_ptr<JsonFetcher> fetcher,
                     const std::string& current_platform,
                     const std::string& current_version,
                     const std::string& locale,
                     bool should_ignore_dev_messages);

  void OnRulesFetched(const std::string& user_email,
                      NotificationCallback callback,
                      base::Optional<base::Value> rules);

  // Returns non-empty NotificationMessage if the rule is parsed successfully
  // and the rule should apply to the user. |message_text| and |link_text| will
  // not be set and caller needs to call FetchTranslatedText to fill them up.
  base::Optional<NotificationMessage> ParseAndMatchRule(
      const base::Value& rule,
      const std::string& user_email,
      std::string* out_message_text_filename,
      std::string* out_link_text_filename);

  void FetchTranslatedTexts(const std::string& message_text_filename,
                            const std::string& link_text_filename,
                            base::Optional<NotificationMessage> partial_message,
                            NotificationCallback done);

  std::unique_ptr<JsonFetcher> fetcher_;
  std::string current_platform_;
  std::string current_version_;
  std::string locale_;
  bool should_ignore_dev_messages_;

  DISALLOW_COPY_AND_ASSIGN(NotificationClient);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_NOTIFICATION_NOTIFICATION_CLIENT_H_
