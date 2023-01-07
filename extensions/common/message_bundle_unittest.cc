// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/message_bundle.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

class MessageBundleTest : public testing::Test {
 protected:
  enum BadDictionary {
    INVALID_NAME,
    NAME_NOT_A_TREE,
    EMPTY_NAME_TREE,
    MISSING_MESSAGE,
    PLACEHOLDER_NOT_A_TREE,
    EMPTY_PLACEHOLDER_TREE,
    CONTENT_MISSING,
    MESSAGE_PLACEHOLDER_DOESNT_MATCH,
  };

  void CreateContentTree(const std::string& name,
                         const std::string& content,
                         base::DictionaryValue* dict) {
    auto content_tree = std::make_unique<base::DictionaryValue>();
    content_tree->SetStringKey(MessageBundle::kContentKey, content);
    dict->Set(name, std::move(content_tree));
  }

  void CreatePlaceholdersTree(base::DictionaryValue* dict) {
    auto placeholders_tree = std::make_unique<base::DictionaryValue>();
    CreateContentTree("a", "A", placeholders_tree.get());
    CreateContentTree("b", "B", placeholders_tree.get());
    CreateContentTree("c", "C", placeholders_tree.get());
    dict->Set(MessageBundle::kPlaceholdersKey, std::move(placeholders_tree));
  }

  void CreateMessageTree(const std::string& name,
                         const std::string& message,
                         bool create_placeholder_subtree,
                         base::Value::Dict* dict) {
    auto message_tree = std::make_unique<base::DictionaryValue>();
    if (create_placeholder_subtree)
      CreatePlaceholdersTree(message_tree.get());
    message_tree->SetStringKey(MessageBundle::kMessageKey, message);
    dict->Set(name, std::move(*message_tree).TakeDict());
  }

  std::unique_ptr<base::DictionaryValue> CreateGoodDictionary() {
    auto dict = std::make_unique<base::DictionaryValue>();
    CreateMessageTree("n1", "message1 $a$ $b$", true, &dict->GetDict());
    CreateMessageTree("n2", "message2 $c$", true, &dict->GetDict());
    CreateMessageTree("n3", "message3", false, &dict->GetDict());
    return dict;
  }

  std::unique_ptr<base::DictionaryValue> CreateBadDictionary(
      enum BadDictionary what_is_bad) {
    std::unique_ptr<base::DictionaryValue> dict = CreateGoodDictionary();
    // Now remove/break things.
    switch (what_is_bad) {
      case INVALID_NAME:
        CreateMessageTree("n 5", "nevermind", false, &dict->GetDict());
        break;
      case NAME_NOT_A_TREE:
        dict->SetStringKey("n4", "whatever");
        break;
      case EMPTY_NAME_TREE:
        dict->Set("n4", std::make_unique<base::DictionaryValue>());
        break;
      case MISSING_MESSAGE:
        RemoveDictionaryPath(dict.get(), {"n1", "message"});
        break;
      case PLACEHOLDER_NOT_A_TREE:
        dict->SetStringPath("n1.placeholders", "whatever");
        break;
      case EMPTY_PLACEHOLDER_TREE:
        dict->Set("n1.placeholders", std::make_unique<base::DictionaryValue>());
        break;
      case CONTENT_MISSING:
        RemoveDictionaryPath(dict.get(),
                             {"n1", "placeholders", "a", "content"});
        break;
      case MESSAGE_PLACEHOLDER_DOESNT_MATCH:
        base::DictionaryValue* value;
        RemoveDictionaryPath(dict.get(), {"n1", "placeholders", "a"});
        dict->GetDictionary("n1.placeholders", &value);
        CreateContentTree("x", "X", value);
        break;
    }

    return dict;
  }

  void RemoveDictionaryPath(base::Value* dict,
                            std::initializer_list<base::StringPiece> pieces) {
    ASSERT_TRUE(dict->is_dict());
    ASSERT_GE(pieces.size(), 2u);
    base::span<const base::StringPiece> path =
        base::make_span(pieces.begin(), pieces.size());
    dict = dict->FindPath(path.first(path.size() - 1));
    if (!dict)
      return;
    ASSERT_TRUE(dict->is_dict());
    dict->RemoveKey(path.back());
  }

  unsigned int ReservedMessagesCount() {
    // Update when adding new reserved messages.
    return 5U;
  }

  void CheckReservedMessages(MessageBundle* handler) {
    std::string ui_locale = extension_l10n_util::CurrentLocaleOrDefault();
    EXPECT_EQ(ui_locale,
              handler->GetL10nMessage(MessageBundle::kUILocaleKey));

    std::string text_dir = "ltr";
    if (base::i18n::GetTextDirectionForLocale(ui_locale.c_str()) ==
        base::i18n::RIGHT_TO_LEFT)
      text_dir = "rtl";

    EXPECT_EQ(text_dir, handler->GetL10nMessage(
        MessageBundle::kBidiDirectionKey));
  }

  bool AppendReservedMessages(const std::string& application_locale) {
    std::string error;
    return handler_->AppendReservedMessagesForLocale(
        application_locale, &error);
  }

  std::string CreateMessageBundle() {
    std::string error;
    handler_.reset(MessageBundle::Create(catalogs_, &error));

    return error;
  }

  void ClearDictionary() {
    handler_->dictionary_.clear();
  }

  std::unique_ptr<MessageBundle> handler_;
  MessageBundle::CatalogVector catalogs_;
};

TEST_F(MessageBundleTest, ReservedMessagesCount) {
  ASSERT_EQ(5U, ReservedMessagesCount());
}

TEST_F(MessageBundleTest, InitEmptyDictionaries) {
  CreateMessageBundle();
  EXPECT_TRUE(handler_.get() != nullptr);
  EXPECT_EQ(0U + ReservedMessagesCount(), handler_->size());
  CheckReservedMessages(handler_.get());
}

TEST_F(MessageBundleTest, InitGoodDefaultDict) {
  catalogs_.push_back(std::move(CreateGoodDictionary()->GetDict()));
  CreateMessageBundle();

  EXPECT_TRUE(handler_.get() != nullptr);
  EXPECT_EQ(3U + ReservedMessagesCount(), handler_->size());

  EXPECT_EQ("message1 A B", handler_->GetL10nMessage("n1"));
  EXPECT_EQ("message2 C", handler_->GetL10nMessage("n2"));
  EXPECT_EQ("message3", handler_->GetL10nMessage("n3"));
  CheckReservedMessages(handler_.get());
}

TEST_F(MessageBundleTest, InitAppDictConsultedFirst) {
  catalogs_.push_back(std::move(CreateGoodDictionary()->GetDict()));
  catalogs_.push_back(std::move(CreateGoodDictionary()->GetDict()));

  base::Value::Dict& app_dict = catalogs_[0];
  // Flip placeholders in message of n1 tree.
  app_dict.SetByDottedPath("n1.message", "message1 $b$ $a$");
  // Remove one message from app dict.
  app_dict.Remove("n2");
  // Replace n3 with N3.
  app_dict.Remove("n3");
  CreateMessageTree("N3", "message3_app_dict", false, &app_dict);

  CreateMessageBundle();

  EXPECT_TRUE(handler_.get() != nullptr);
  EXPECT_EQ(3U + ReservedMessagesCount(), handler_->size());

  EXPECT_EQ("message1 B A", handler_->GetL10nMessage("n1"));
  EXPECT_EQ("message2 C", handler_->GetL10nMessage("n2"));
  EXPECT_EQ("message3_app_dict", handler_->GetL10nMessage("n3"));
  CheckReservedMessages(handler_.get());
}

TEST_F(MessageBundleTest, InitBadAppDict) {
  catalogs_.push_back(std::move(CreateBadDictionary(INVALID_NAME)->GetDict()));
  catalogs_.push_back(std::move(CreateGoodDictionary()->GetDict()));

  std::string error = CreateMessageBundle();

  EXPECT_TRUE(handler_.get() == nullptr);
  EXPECT_EQ("Name of a key \"n 5\" is invalid. Only ASCII [a-z], "
            "[A-Z], [0-9] and \"_\" are allowed.", error);

  catalogs_[0] = std::move(CreateBadDictionary(NAME_NOT_A_TREE)->GetDict());
  handler_.reset(MessageBundle::Create(catalogs_, &error));
  EXPECT_TRUE(handler_.get() == nullptr);
  EXPECT_EQ("Not a valid tree for key n4.", error);

  catalogs_[0] = std::move(CreateBadDictionary(EMPTY_NAME_TREE)->GetDict());
  handler_.reset(MessageBundle::Create(catalogs_, &error));
  EXPECT_TRUE(handler_.get() == nullptr);
  EXPECT_EQ("There is no \"message\" element for key n4.", error);

  catalogs_[0] = std::move(CreateBadDictionary(MISSING_MESSAGE)->GetDict());
  handler_.reset(MessageBundle::Create(catalogs_, &error));
  EXPECT_TRUE(handler_.get() == nullptr);
  EXPECT_EQ("There is no \"message\" element for key n1.", error);

  catalogs_[0] =
      std::move(CreateBadDictionary(PLACEHOLDER_NOT_A_TREE)->GetDict());
  handler_.reset(MessageBundle::Create(catalogs_, &error));
  EXPECT_TRUE(handler_.get() == nullptr);
  EXPECT_EQ("Not a valid \"placeholders\" element for key n1.", error);

  catalogs_[0] =
      std::move(CreateBadDictionary(EMPTY_PLACEHOLDER_TREE)->GetDict());
  handler_.reset(MessageBundle::Create(catalogs_, &error));
  EXPECT_TRUE(handler_.get() == nullptr);
  EXPECT_EQ("Variable $a$ used but not defined.", error);

  catalogs_[0] = std::move(CreateBadDictionary(CONTENT_MISSING)->GetDict());
  handler_.reset(MessageBundle::Create(catalogs_, &error));
  EXPECT_TRUE(handler_.get() == nullptr);
  EXPECT_EQ("Invalid \"content\" element for key n1.", error);

  catalogs_[0] = std::move(
      CreateBadDictionary(MESSAGE_PLACEHOLDER_DOESNT_MATCH)->GetDict());
  handler_.reset(MessageBundle::Create(catalogs_, &error));
  EXPECT_TRUE(handler_.get() == nullptr);
  EXPECT_EQ("Variable $a$ used but not defined.", error);
}

TEST_F(MessageBundleTest, ReservedMessagesOverrideDeveloperMessages) {
  catalogs_.push_back(std::move(CreateGoodDictionary()->GetDict()));

  base::Value::Dict& dict = catalogs_[0];
  CreateMessageTree(MessageBundle::kUILocaleKey, "x", false, &dict);

  std::string error = CreateMessageBundle();

  EXPECT_TRUE(handler_.get() == nullptr);
  std::string expected_error = ErrorUtils::FormatErrorMessage(
      errors::kReservedMessageFound, MessageBundle::kUILocaleKey);
  EXPECT_EQ(expected_error, error);
}

TEST_F(MessageBundleTest, AppendReservedMessagesForLTR) {
  CreateMessageBundle();

  ASSERT_TRUE(handler_.get() != nullptr);
  ClearDictionary();
  ASSERT_TRUE(AppendReservedMessages("en_US"));

  EXPECT_EQ("en_US",
            handler_->GetL10nMessage(MessageBundle::kUILocaleKey));
  EXPECT_EQ("ltr", handler_->GetL10nMessage(
      MessageBundle::kBidiDirectionKey));
  EXPECT_EQ("rtl", handler_->GetL10nMessage(
      MessageBundle::kBidiReversedDirectionKey));
  EXPECT_EQ("left", handler_->GetL10nMessage(
      MessageBundle::kBidiStartEdgeKey));
  EXPECT_EQ("right", handler_->GetL10nMessage(
      MessageBundle::kBidiEndEdgeKey));
}

TEST_F(MessageBundleTest, AppendReservedMessagesForRTL) {
  CreateMessageBundle();

  ASSERT_TRUE(handler_.get() != nullptr);
  ClearDictionary();
  ASSERT_TRUE(AppendReservedMessages("he"));

  EXPECT_EQ("he",
            handler_->GetL10nMessage(MessageBundle::kUILocaleKey));
  EXPECT_EQ("rtl", handler_->GetL10nMessage(
      MessageBundle::kBidiDirectionKey));
  EXPECT_EQ("ltr", handler_->GetL10nMessage(
      MessageBundle::kBidiReversedDirectionKey));
  EXPECT_EQ("right", handler_->GetL10nMessage(
      MessageBundle::kBidiStartEdgeKey));
  EXPECT_EQ("left", handler_->GetL10nMessage(
      MessageBundle::kBidiEndEdgeKey));
}

TEST_F(MessageBundleTest, IsValidNameCheckValidCharacters) {
  EXPECT_TRUE(MessageBundle::IsValidName(std::string("a__BV_9")));
  EXPECT_TRUE(MessageBundle::IsValidName(std::string("@@a__BV_9")));
  EXPECT_FALSE(MessageBundle::IsValidName(std::string("$a__BV_9$")));
  EXPECT_FALSE(MessageBundle::IsValidName(std::string("a-BV-9")));
  EXPECT_FALSE(MessageBundle::IsValidName(std::string("a#BV!9")));
  EXPECT_FALSE(MessageBundle::IsValidName(std::string("a<b")));
}

struct ReplaceVariables {
  const char* original;
  const char* result;
  const char* error;
  const char* begin_delimiter;
  const char* end_delimiter;
  bool pass;
};

TEST(MessageBundle, ReplaceMessagesInText) {
  const char* kMessageBegin = MessageBundle::kMessageBegin;
  const char* kMessageEnd = MessageBundle::kMessageEnd;
  const char* kPlaceholderBegin = MessageBundle::kPlaceholderBegin;
  const char* kPlaceholderEnd = MessageBundle::kPlaceholderEnd;

  static ReplaceVariables test_cases[] = {
    // Message replacement.
    { "This is __MSG_siMPle__ message", "This is simple message",
      "", kMessageBegin, kMessageEnd, true },
    { "This is __MSG_", "This is __MSG_",
      "", kMessageBegin, kMessageEnd, true },
    { "This is __MSG__simple__ message", "This is __MSG__simple__ message",
      "Variable __MSG__simple__ used but not defined.",
      kMessageBegin, kMessageEnd, false },
    { "__MSG_LoNg__", "A pretty long replacement",
      "", kMessageBegin, kMessageEnd, true },
    { "A __MSG_SimpLE__MSG_ a", "A simpleMSG_ a",
      "", kMessageBegin, kMessageEnd, true },
    { "A __MSG_simple__MSG_long__", "A simpleMSG_long__",
      "", kMessageBegin, kMessageEnd, true },
    { "A __MSG_simple____MSG_long__", "A simpleA pretty long replacement",
      "", kMessageBegin, kMessageEnd, true },
    { "__MSG_d1g1ts_are_ok__", "I are d1g1t",
      "", kMessageBegin, kMessageEnd, true },
    // Placeholder replacement.
    { "This is $sImpLe$ message", "This is simple message",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "This is $", "This is $",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "This is $$sIMPle$ message", "This is $simple message",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "$LONG_V$", "A pretty long replacement",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "A $simple$$ a", "A simple$ a",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "A $simple$long_v$", "A simplelong_v$",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "A $simple$$long_v$", "A simpleA pretty long replacement",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "This is $bad name$", "This is $bad name$",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "This is $missing$", "This is $missing$",
       "Variable $missing$ used but not defined.",
       kPlaceholderBegin, kPlaceholderEnd, false },
  };

  MessageBundle::SubstitutionMap messages;
  messages.insert(std::make_pair("simple", "simple"));
  messages.insert(std::make_pair("long", "A pretty long replacement"));
  messages.insert(std::make_pair("long_v", "A pretty long replacement"));
  messages.insert(std::make_pair("bad name", "Doesn't matter"));
  messages.insert(std::make_pair("d1g1ts_are_ok", "I are d1g1t"));

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    std::string text = test_cases[i].original;
    std::string error;
    EXPECT_EQ(test_cases[i].pass,
              MessageBundle::ReplaceVariables(messages,
                                              test_cases[i].begin_delimiter,
                                              test_cases[i].end_delimiter,
                                              &text,
                                              &error));
    EXPECT_EQ(test_cases[i].result, text);
  }
}

}  // namespace extensions
