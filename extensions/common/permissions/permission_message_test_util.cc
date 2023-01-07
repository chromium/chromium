// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permission_message_test_util.h"

#include <stddef.h>

#include <iterator>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace {

PermissionMessages GetMessages(const PermissionSet& permissions,
                               Manifest::Type extension_type) {
  const PermissionMessageProvider* provider = PermissionMessageProvider::Get();
  return provider->GetPermissionMessages(
      provider->GetAllPermissionIDs(permissions, extension_type));
}

std::vector<std::u16string> MakeVectorString16(
    const std::vector<std::string>& vec) {
  std::vector<std::u16string> result;
  for (const std::string& msg : vec)
    result.push_back(base::UTF8ToUTF16(msg));
  return result;
}

std::vector<std::vector<std::u16string>> MakeVectorVectorString16(
    const std::vector<std::vector<std::string>>& vecs) {
  std::vector<std::vector<std::u16string>> result;
  for (const std::vector<std::string>& vec : vecs)
    result.push_back(MakeVectorString16(vec));
  return result;
}

// Returns the vector of messages concatenated into a single string, separated
// by newlines, e.g.: "Bar"\n"Baz"\n
std::string MessagesVectorToString(
    const std::vector<std::u16string>& messages) {
  if (messages.empty())
    return "\n";
  return base::StrCat(
      {"\"", base::UTF16ToUTF8(base::JoinString(messages, u"\"\n\"")), "\"\n"});
}

std::string MessagesToString(const PermissionMessages& messages) {
  std::vector<std::u16string> messages_vec;
  for (const PermissionMessage& msg : messages)
    messages_vec.push_back(msg.message());
  return MessagesVectorToString(messages_vec);
}

bool CheckThatSubmessagesMatch(
    const std::u16string& message,
    const std::vector<std::u16string>& expected_submessages,
    const std::vector<std::u16string>& actual_submessages) {
  bool result = true;

  std::vector<std::u16string> expected_sorted(expected_submessages);
  std::sort(expected_sorted.begin(), expected_sorted.end());

  std::vector<std::u16string> actual_sorted(actual_submessages);
  std::sort(actual_sorted.begin(), actual_sorted.end());
  if (expected_sorted != actual_sorted) {
    // This is always a failure, even within an EXPECT_FALSE.
    // Message: Expected submessages for "Message" to be { "Foo" }, but got
    // { "Bar", "Baz" }
    ADD_FAILURE() << "Expected submessages for \"" << message << "\" to be:\n"
                  << MessagesVectorToString(expected_sorted) << "But got:\n"
                  << MessagesVectorToString(actual_sorted);
    result = false;
  }

  return result;
}

testing::AssertionResult VerifyHasPermissionMessageImpl(
    const std::u16string& expected_message,
    const std::vector<std::u16string>& expected_submessages,
    const PermissionMessages& actual_messages) {
  auto message_it = base::ranges::find(actual_messages, expected_message,
                                       &PermissionMessage::message);
  bool found = message_it != actual_messages.end();
  if (!found) {
    // Message: Expected messages to contain "Foo", but got { "Bar", "Baz" }
    return testing::AssertionFailure() << "Expected messages to contain \""
                                       << expected_message << "\", but got "
                                       << MessagesToString(actual_messages);
  }

  if (!CheckThatSubmessagesMatch(expected_message, expected_submessages,
                                 message_it->submessages())) {
    return testing::AssertionFailure();
  }

  // Message: Expected messages NOT to contain "Foo", but got { "Bar", "Baz" }
  return testing::AssertionSuccess() << "Expected messages NOT to contain \""
                                     << expected_message << "\", but got "
                                     << MessagesToString(actual_messages);
}

testing::AssertionResult VerifyPermissionMessagesWithSubmessagesImpl(
    const std::vector<std::u16string>& expected_messages,
    const std::vector<std::vector<std::u16string>>& expected_submessages,
    const PermissionMessages& actual_messages,
    bool check_order) {
  CHECK_EQ(expected_messages.size(), expected_submessages.size());
  if (expected_messages.size() != actual_messages.size()) {
    // Message: Expected 2 messages { "Bar", "Baz" }, but got 0 {}
    return testing::AssertionFailure()
           << "Expected " << expected_messages.size() << " messages:\n"
           << MessagesVectorToString(expected_messages) << "But got "
           << actual_messages.size() << " messages:\n"
           << MessagesToString(actual_messages);
  }

  if (check_order) {
    auto it = actual_messages.begin();
    for (size_t i = 0; i < expected_messages.size(); i++, ++it) {
      const PermissionMessage& actual_message = *it;
      if (expected_messages[i] != actual_message.message()) {
        // Message: Expected messages to be { "Foo" }, but got { "Bar", "Baz" }
        return testing::AssertionFailure()
               << "Expected messages to be:\n"
               << MessagesVectorToString(expected_messages) << "But got:\n"
               << MessagesToString(actual_messages);
      }

      if (!CheckThatSubmessagesMatch(expected_messages[i],
                                     expected_submessages[i],
                                     actual_message.submessages())) {
        return testing::AssertionFailure();
      }
    }
  } else {
    for (size_t i = 0; i < expected_messages.size(); i++) {
      testing::AssertionResult result = VerifyHasPermissionMessageImpl(
          expected_messages[i], expected_submessages[i], actual_messages);
      if (!result)
        return result;
    }
  }
  return testing::AssertionSuccess();
}

}  // namespace

testing::AssertionResult VerifyHasPermissionMessage(
    const PermissionsData* permissions_data,
    const std::u16string& expected_message) {
  return VerifyHasPermissionMessageImpl(
      expected_message, {}, permissions_data->GetPermissionMessages());
}

testing::AssertionResult VerifyHasPermissionMessage(
    const PermissionSet& permissions,
    Manifest::Type extension_type,
    const std::string& expected_message) {
  return VerifyHasPermissionMessageImpl(
      base::UTF8ToUTF16(expected_message), {},
      GetMessages(permissions, extension_type));
}

testing::AssertionResult VerifyNoPermissionMessages(
    const PermissionsData* permissions_data) {
  return VerifyPermissionMessages(permissions_data,
                                  std::vector<std::u16string>(), true);
}

testing::AssertionResult VerifyOnePermissionMessage(
    const PermissionsData* permissions_data,
    const std::string& expected_message) {
  return VerifyOnePermissionMessage(permissions_data,
                                    base::UTF8ToUTF16(expected_message));
}

testing::AssertionResult VerifyOnePermissionMessage(
    const PermissionsData* permissions_data,
    const std::u16string& expected_message) {
  return VerifyPermissionMessages(permissions_data, {expected_message}, true);
}

testing::AssertionResult VerifyOnePermissionMessage(
    const PermissionSet& permissions,
    Manifest::Type extension_type,
    const std::u16string& expected_message) {
  return VerifyPermissionMessagesWithSubmessagesImpl(
      {expected_message}, std::vector<std::vector<std::u16string>>(1),
      GetMessages(permissions, extension_type), true);
}

testing::AssertionResult VerifyOnePermissionMessageWithSubmessages(
    const PermissionsData* permissions_data,
    const std::string& expected_message,
    const std::vector<std::string>& expected_submessages) {
  return VerifyPermissionMessagesWithSubmessages(
      permissions_data, {expected_message}, {expected_submessages}, true);
}

testing::AssertionResult VerifyTwoPermissionMessages(
    const PermissionsData* permissions_data,
    const std::string& expected_message_1,
    const std::string& expected_message_2,
    bool check_order) {
  return VerifyPermissionMessages(permissions_data,
                                  {base::UTF8ToUTF16(expected_message_1),
                                   base::UTF8ToUTF16(expected_message_2)},
                                  check_order);
}

testing::AssertionResult VerifyPermissionMessages(
    const PermissionsData* permissions_data,
    const std::vector<std::string>& expected_messages,
    bool check_order) {
  return VerifyPermissionMessages(
      permissions_data, MakeVectorString16(expected_messages), check_order);
}

testing::AssertionResult VerifyPermissionMessages(
    const PermissionsData* permissions_data,
    const std::vector<std::u16string>& expected_messages,
    bool check_order) {
  return VerifyPermissionMessagesWithSubmessages(
      permissions_data, expected_messages,
      std::vector<std::vector<std::u16string>>(expected_messages.size()),
      check_order);
}

testing::AssertionResult VerifyPermissionMessagesWithSubmessages(
    const PermissionsData* permissions_data,
    const std::vector<std::string>& expected_messages,
    const std::vector<std::vector<std::string>>& expected_submessages,
    bool check_order) {
  return VerifyPermissionMessagesWithSubmessages(
      permissions_data, MakeVectorString16(expected_messages),
      MakeVectorVectorString16(expected_submessages), check_order);
}

testing::AssertionResult VerifyPermissionMessagesWithSubmessages(
    const PermissionsData* permissions_data,
    const std::vector<std::u16string>& expected_messages,
    const std::vector<std::vector<std::u16string>>& expected_submessages,
    bool check_order) {
  CHECK_EQ(expected_messages.size(), expected_submessages.size());
  return VerifyPermissionMessagesWithSubmessagesImpl(
      expected_messages, expected_submessages,
      permissions_data->GetPermissionMessages(), check_order);
}

}  // namespace extensions
