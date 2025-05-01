// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_l10n_util.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/message_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/l10n/l10n_util.h"

using extension_l10n_util::GzippedMessagesPermission;

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

namespace {

TEST(ExtensionL10nUtil, ValidateLocalesWithBadLocale) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().Append(kLocaleFolder);
  base::FilePath locale = src_path.AppendASCII("ms");
  ASSERT_TRUE(base::CreateDirectory(locale));

  base::FilePath messages_file = locale.Append(kMessagesFilename);
  std::string data = "{ \"name\":";
  ASSERT_TRUE(base::WriteFile(messages_file, data));

  auto manifest = base::Value::Dict().Set(keys::kDefaultLocale, "en");
  std::string error;
  EXPECT_FALSE(extension_l10n_util::ValidateExtensionLocales(temp.GetPath(),
                                                             manifest, &error));
  EXPECT_THAT(
      error,
      testing::HasSubstr(base::UTF16ToUTF8(messages_file.LossyDisplayName())));
}

TEST(ExtensionL10nUtil, ValidateLocalesWithErroneousLocalizations) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().Append(kLocaleFolder);
  ASSERT_TRUE(base::CreateDirectory(src_path));

  // Add valid default localization file.
  base::FilePath en_locale = src_path.AppendASCII("en");
  ASSERT_TRUE(base::CreateDirectory(en_locale));
  base::FilePath en_messages_file = en_locale.Append(kMessagesFilename);
  const std::string en_data = R"({ "name": { "message": "default" } })";
  ASSERT_TRUE(base::WriteFile(en_messages_file, en_data));

  // Add additional valid localization file.
  base::FilePath sr_locale = src_path.AppendASCII("sr");
  ASSERT_TRUE(base::CreateDirectory(sr_locale));
  base::FilePath sr_messages_file = sr_locale.Append(kMessagesFilename);
  const std::string sr_data = R"({ "name": { "message": "valid" } })";
  ASSERT_TRUE(base::WriteFile(sr_messages_file, sr_data));

  // Add additional localization file with undefined variable.
  base::FilePath de_locale = src_path.AppendASCII("de");
  ASSERT_TRUE(base::CreateDirectory(de_locale));
  base::FilePath de_messages_file = de_locale.Append(kMessagesFilename);
  const std::string de_data = R"({ "name": { "message": "with $VAR$" } })";
  ASSERT_TRUE(base::WriteFile(de_messages_file, de_data));

  // Add additional localization file with syntax error.
  base::FilePath es_locale = src_path.AppendASCII("es");
  ASSERT_TRUE(base::CreateDirectory(es_locale));
  base::FilePath es_messages_file = es_locale.Append(kMessagesFilename);
  const std::string es_data = R"({ "name": { "message": } })";
  ASSERT_TRUE(base::WriteFile(es_messages_file, es_data));

  // Add additional localization file with missing property.
  base::FilePath fr_locale = src_path.AppendASCII("fr");
  ASSERT_TRUE(base::CreateDirectory(fr_locale));
  base::FilePath fr_messages_file = fr_locale.Append(kMessagesFilename);
  const std::string fr_data = R"({ "name": { } })";
  ASSERT_TRUE(base::WriteFile(fr_messages_file, fr_data));

  const auto manifest = base::Value::Dict().Set(keys::kDefaultLocale, "en");
  std::string error;
  EXPECT_FALSE(extension_l10n_util::ValidateExtensionLocales(temp.GetPath(),
                                                             manifest, &error));
  EXPECT_FALSE(base::Contains(
      error, base::UTF16ToUTF8(sr_messages_file.LossyDisplayName())));
  EXPECT_THAT(error, testing::HasSubstr(ErrorUtils::FormatErrorMessage(
                         errors::kLocalesInvalidLocale,
                         base::UTF16ToUTF8(de_messages_file.LossyDisplayName()),
                         "Variable $VAR$ used but not defined.")));
  if (base::JSONReader::UsingRust()) {
    EXPECT_THAT(error,
                testing::HasSubstr(ErrorUtils::FormatErrorMessage(
                    errors::kLocalesInvalidLocale,
                    base::UTF16ToUTF8(es_messages_file.LossyDisplayName()),
                    "expected value at line 1 column 24")));
  } else {
    EXPECT_THAT(error,
                testing::HasSubstr(ErrorUtils::FormatErrorMessage(
                    errors::kLocalesInvalidLocale,
                    base::UTF16ToUTF8(es_messages_file.LossyDisplayName()),
                    "Line: 1, column: 24, Unexpected token.")));
  }
  EXPECT_THAT(error, testing::HasSubstr(ErrorUtils::FormatErrorMessage(
                         errors::kLocalesInvalidLocale,
                         base::UTF16ToUTF8(fr_messages_file.LossyDisplayName()),
                         "There is no \"message\" element for key name.")));
}

TEST(ExtensionL10nUtil, GetValidLocalesEmptyLocaleFolder) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().Append(kLocaleFolder);
  ASSERT_TRUE(base::CreateDirectory(src_path));

  std::string error;
  std::set<std::string> locales;
  EXPECT_FALSE(
      extension_l10n_util::GetValidLocales(src_path, &locales, &error));

  EXPECT_TRUE(locales.empty());
}

TEST(ExtensionL10nUtil, GetValidLocalesWithValidLocaleNoMessagesFile) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().Append(kLocaleFolder);
  ASSERT_TRUE(base::CreateDirectory(src_path));
  ASSERT_TRUE(base::CreateDirectory(src_path.AppendASCII("sr")));

  std::string error;
  std::set<std::string> locales;
  EXPECT_FALSE(
      extension_l10n_util::GetValidLocales(src_path, &locales, &error));

  EXPECT_TRUE(locales.empty());
}

TEST(ExtensionL10nUtil, GetValidLocalesWithUnsupportedLocale) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().Append(kLocaleFolder);
  ASSERT_TRUE(base::CreateDirectory(src_path));
  // Supported locale.
  base::FilePath locale_1 = src_path.AppendASCII("sr");
  ASSERT_TRUE(base::CreateDirectory(locale_1));
  ASSERT_TRUE(base::WriteFile(locale_1.Append(kMessagesFilename), ""));
  // Unsupported locale.
  base::FilePath locale_2 = src_path.AppendASCII("xxx_yyy");
  ASSERT_TRUE(base::CreateDirectory(locale_2));
  ASSERT_TRUE(base::WriteFile(locale_2.Append(kMessagesFilename), ""));

  std::string error;
  std::set<std::string> locales;
  EXPECT_TRUE(extension_l10n_util::GetValidLocales(src_path, &locales, &error));

  EXPECT_FALSE(locales.empty());
  EXPECT_TRUE(base::Contains(locales, "sr"));
  EXPECT_FALSE(base::Contains(locales, "xxx_yyy"));
}

TEST(ExtensionL10nUtil, GetValidLocalesWithValidLocalesAndMessagesFile) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir =
      install_dir.AppendASCII("extension_with_locales").Append(kLocaleFolder);

  std::string error;
  std::set<std::string> locales;
  EXPECT_TRUE(
      extension_l10n_util::GetValidLocales(install_dir, &locales, &error));
  EXPECT_EQ(3U, locales.size());
  EXPECT_TRUE(base::Contains(locales, "sr"));
  EXPECT_TRUE(base::Contains(locales, "en"));
  EXPECT_TRUE(base::Contains(locales, "en_US"));
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsValidFallback) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("en-US");
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir =
      install_dir.AppendASCII("extension_with_locales").Append(kLocaleFolder);

  std::string error;
  std::unique_ptr<MessageBundle> bundle(
      extension_l10n_util::LoadMessageCatalogs(
          install_dir, "sr", GzippedMessagesPermission::kDisallow, &error));
  ASSERT_TRUE(bundle);
  EXPECT_TRUE(error.empty());
  EXPECT_EQ("Color", bundle->GetL10nMessage("color"));
  EXPECT_EQ("Not in the US or GB.", bundle->GetL10nMessage("not_in_US_or_GB"));
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsLowercaseLocales) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("en-US");
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extension_with_lowercase_locales")
                    .Append(kLocaleFolder);

  std::string error;
  std::unique_ptr<MessageBundle> bundle(
      extension_l10n_util::LoadMessageCatalogs(
          install_dir, "en-US", GzippedMessagesPermission::kDisallow, &error));
  ASSERT_TRUE(bundle);
  EXPECT_TRUE(error.empty());
  const base::FilePath locale_uppercase_path = install_dir.AppendASCII("en_US");
  const base::FilePath locale_lowercase_path = install_dir.AppendASCII("en_us");
  if (base::PathExists(locale_uppercase_path) &&
      base::PathExists(locale_lowercase_path)) {
    // Path system is case-insensitive.
    EXPECT_EQ("color lowercase", bundle->GetL10nMessage("color"));
  } else {
    EXPECT_EQ("", bundle->GetL10nMessage("color"));
  }
  std::set<std::string> all_locales;
  extension_l10n_util::GetAllLocales(&all_locales);
  EXPECT_FALSE(extension_l10n_util::ShouldSkipValidation(
      install_dir, locale_uppercase_path, all_locales));
  EXPECT_FALSE(extension_l10n_util::ShouldSkipValidation(
      install_dir, locale_lowercase_path, all_locales));
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsMissingFiles) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("sr");
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().Append(kLocaleFolder);
  ASSERT_TRUE(base::CreateDirectory(src_path));
  ASSERT_TRUE(base::CreateDirectory(src_path.AppendASCII("en")));
  ASSERT_TRUE(base::CreateDirectory(src_path.AppendASCII("sr")));

  std::string error;
  EXPECT_FALSE(extension_l10n_util::LoadMessageCatalogs(
      src_path, "en", GzippedMessagesPermission::kDisallow, &error));
  EXPECT_FALSE(error.empty());
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsBadJSONFormat) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("sr");
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().Append(kLocaleFolder);
  ASSERT_TRUE(base::CreateDirectory(src_path));

  base::FilePath locale = src_path.AppendASCII("sr");
  ASSERT_TRUE(base::CreateDirectory(locale));

  std::string data = "{ \"name\":";
  base::FilePath messages_file = locale.Append(kMessagesFilename);
  ASSERT_TRUE(base::WriteFile(messages_file, data));

  std::string error;
  EXPECT_FALSE(extension_l10n_util::LoadMessageCatalogs(
      src_path, "en_US", GzippedMessagesPermission::kDisallow, &error));
  if (base::JSONReader::UsingRust()) {
    EXPECT_NE(std::string::npos,
              error.find(ErrorUtils::FormatErrorMessage(
                  errors::kLocalesInvalidLocale,
                  base::UTF16ToUTF8(messages_file.LossyDisplayName()),
                  "EOF while parsing a value at line 1 column 9")));
  } else {
    EXPECT_NE(std::string::npos,
              error.find(ErrorUtils::FormatErrorMessage(
                  errors::kLocalesInvalidLocale,
                  base::UTF16ToUTF8(messages_file.LossyDisplayName()),
                  "Line: 1, column: 10,")));
  }
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsDuplicateKeys) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("sr");
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().Append(kLocaleFolder);
  ASSERT_TRUE(base::CreateDirectory(src_path));

  base::FilePath locale = src_path.AppendASCII("en");
  ASSERT_TRUE(base::CreateDirectory(locale));

  std::string data =
      "{ \"name\": { \"message\": \"something\" }, "
      "\"name\": { \"message\": \"something else\" } }";
  ASSERT_TRUE(base::WriteFile(locale.Append(kMessagesFilename), data));

  std::string error;
  // JSON parser hides duplicates. We are going to get only one key/value
  // pair at the end.
  std::unique_ptr<MessageBundle> message_bundle(
      extension_l10n_util::LoadMessageCatalogs(
          src_path, "en", GzippedMessagesPermission::kDisallow, &error));
  EXPECT_TRUE(message_bundle.get());
  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsWithUndefinedVariable) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("sr");
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().Append(kLocaleFolder);
  ASSERT_TRUE(base::CreateDirectory(src_path));

  base::FilePath locale = src_path.AppendASCII("sr");
  ASSERT_TRUE(base::CreateDirectory(locale));

  std::string data = R"({ "name": { "message": "with $VAR$" } })";
  base::FilePath messages_file = locale.Append(kMessagesFilename);
  ASSERT_TRUE(base::WriteFile(messages_file, data));

  std::string error;
  EXPECT_FALSE(extension_l10n_util::LoadMessageCatalogs(
      src_path, "sr", GzippedMessagesPermission::kDisallow, &error));
  EXPECT_THAT(error,
              testing::HasSubstr("Variable $VAR$ used but not defined."));
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsCompressed) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("sr");
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.GetPath().Append(kLocaleFolder);
  ASSERT_TRUE(base::CreateDirectory(src_path));

  base::FilePath locale = src_path.AppendASCII("en");
  ASSERT_TRUE(base::CreateDirectory(locale));

  // Create a compressed messages.json.gz file.
  std::string data = "{ \"name\": { \"message\": \"something\" } }";
  std::string compressed_data;
  ASSERT_TRUE(compression::GzipCompress(data, &compressed_data));
  ASSERT_TRUE(base::WriteFile(
      locale.Append(kMessagesFilename).AddExtension(FILE_PATH_LITERAL(".gz")),
      compressed_data));

  // Test that LoadMessageCatalogs fails with gzip_permission = kDisallow.
  std::string error;
  std::unique_ptr<MessageBundle> message_bundle(
      extension_l10n_util::LoadMessageCatalogs(
          src_path, "en", GzippedMessagesPermission::kDisallow, &error));
  EXPECT_FALSE(message_bundle.get());
  EXPECT_FALSE(error.empty());

  // Test that LoadMessageCatalogs succeeds with gzip_permission =
  // kAllowForTrustedSource.
  error.clear();
  message_bundle.reset(extension_l10n_util::LoadMessageCatalogs(
      src_path, "en", GzippedMessagesPermission::kAllowForTrustedSource,
      &error));
  EXPECT_TRUE(message_bundle.get());
  EXPECT_TRUE(error.empty());
  EXPECT_EQ("something", message_bundle->GetL10nMessage("name"));
}

// Caller owns the returned object.
MessageBundle* CreateManifestBundle() {
  base::Value::Dict catalog;

  base::Value::Dict name_tree;
  name_tree.Set("message", "name");
  catalog.Set("name", std::move(name_tree));

  base::Value::Dict short_name_tree;
  short_name_tree.Set("message", "short_name");
  catalog.Set("short_name", std::move(short_name_tree));

  base::Value::Dict description_tree;
  description_tree.Set("message", "description");
  catalog.Set("description", std::move(description_tree));

  base::Value::Dict action_title_tree;
  action_title_tree.Set("message", "action title");
  catalog.Set("title", std::move(action_title_tree));

  base::Value::Dict omnibox_keyword_tree;
  omnibox_keyword_tree.Set("message", "omnibox keyword");
  catalog.Set("omnibox_keyword", std::move(omnibox_keyword_tree));

  base::Value::Dict file_handler_title_tree;
  file_handler_title_tree.Set("message", "file handler title");
  catalog.Set("file_handler_title", std::move(file_handler_title_tree));

  base::Value::Dict launch_local_path_tree;
  launch_local_path_tree.Set("message", "main.html");
  catalog.Set("launch_local_path", std::move(launch_local_path_tree));

  base::Value::Dict launch_web_url_tree;
  launch_web_url_tree.Set("message", "http://www.google.com/");
  catalog.Set("launch_web_url", std::move(launch_web_url_tree));

  base::Value::Dict first_command_description_tree;
  first_command_description_tree.Set("message", "first command");
  catalog.Set("first_command_description",
              std::move(first_command_description_tree));

  base::Value::Dict second_command_description_tree;
  second_command_description_tree.Set("message", "second command");
  catalog.Set("second_command_description",
              std::move(second_command_description_tree));

  base::Value::Dict url_country_tree;
  url_country_tree.Set("message", "de");
  catalog.Set("country", std::move(url_country_tree));

  MessageBundle::CatalogVector catalogs;
  catalogs.push_back(std::move(catalog));

  std::string error;
  MessageBundle* bundle = MessageBundle::Create(catalogs, &error);
  EXPECT_TRUE(bundle);
  EXPECT_TRUE(error.empty());

  return bundle;
}

TEST(ExtensionL10nUtil, LocalizeEmptyManifest) {
  base::Value::Dict manifest;
  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_FALSE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));
  EXPECT_EQ(std::string(errors::kInvalidName), error);
}

TEST(ExtensionL10nUtil, LocalizeManifestWithoutNameMsgAndEmptyDescription) {
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "no __MSG");
  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  const std::string* result = manifest.FindString(keys::kName);
  ASSERT_TRUE(result);
  EXPECT_EQ("no __MSG", *result);

  EXPECT_FALSE(manifest.Find(keys::kDescription));

  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, LocalizeManifestWithNameMsgAndEmptyDescription) {
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "__MSG_name__");
  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  const std::string* result = manifest.FindString(keys::kName);
  ASSERT_TRUE(result);
  EXPECT_EQ("name", *result);

  EXPECT_FALSE(manifest.Find(keys::kDescription));

  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, LocalizeManifestWithLocalLaunchURL) {
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "name");
  manifest.SetByDottedPath(keys::kLaunchLocalPath, "__MSG_launch_local_path__");
  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  const std::string* result =
      manifest.FindStringByDottedPath(keys::kLaunchLocalPath);
  ASSERT_TRUE(result);
  EXPECT_EQ("main.html", *result);

  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, LocalizeManifestWithHostedLaunchURL) {
  base::Value::Dict manifest;
  manifest.SetByDottedPath(keys::kName, "name");
  manifest.SetByDottedPath(keys::kLaunchWebURL, "__MSG_launch_web_url__");
  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  const std::string* result =
      manifest.FindStringByDottedPath(keys::kLaunchWebURL);
  ASSERT_TRUE(result);
  EXPECT_EQ("http://www.google.com/", *result);

  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, LocalizeManifestWithBadNameMsg) {
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "__MSG_name_is_bad__");
  manifest.Set(keys::kDescription, "__MSG_description__");
  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_FALSE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  const std::string* result = manifest.FindString(keys::kName);
  ASSERT_TRUE(result);
  EXPECT_EQ("__MSG_name_is_bad__", *result);

  result = manifest.FindString(keys::kDescription);
  ASSERT_TRUE(result);
  EXPECT_EQ("__MSG_description__", *result);

  EXPECT_EQ("Variable __MSG_name_is_bad__ used but not defined.", error);
}

TEST(ExtensionL10nUtil, LocalizeManifestWithNameDescriptionDefaultTitleMsgs) {
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "__MSG_name__");
  manifest.Set(keys::kDescription, "__MSG_description__");
  std::string action_title(keys::kBrowserAction);
  action_title.append(".");
  action_title.append(keys::kActionDefaultTitle);
  manifest.SetByDottedPath(action_title, "__MSG_title__");

  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  const std::string* result = manifest.FindString(keys::kName);
  ASSERT_TRUE(result);
  EXPECT_EQ("name", *result);

  result = manifest.FindString(keys::kDescription);
  ASSERT_TRUE(result);
  EXPECT_EQ("description", *result);

  result = manifest.FindStringByDottedPath(action_title);
  ASSERT_TRUE(result);
  EXPECT_EQ("action title", *result);

  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, LocalizeManifestWithNameDescriptionOmniboxMsgs) {
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "__MSG_name__");
  manifest.Set(keys::kDescription, "__MSG_description__");
  manifest.SetByDottedPath(keys::kOmniboxKeyword, "__MSG_omnibox_keyword__");

  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  const std::string* result = manifest.FindString(keys::kName);
  ASSERT_TRUE(result);
  EXPECT_EQ("name", *result);

  result = manifest.FindString(keys::kDescription);
  ASSERT_TRUE(result);
  EXPECT_EQ("description", *result);

  result = manifest.FindStringByDottedPath(keys::kOmniboxKeyword);
  ASSERT_TRUE(result);
  EXPECT_EQ("omnibox keyword", *result);

  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, LocalizeManifestWithNameDescriptionFileHandlerTitle) {
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "__MSG_name__");
  manifest.Set(keys::kDescription, "__MSG_description__");

  base::Value::Dict handler;
  handler.Set(keys::kActionDefaultTitle, "__MSG_file_handler_title__");
  base::Value::List handlers;
  handlers.Append(std::move(handler));
  manifest.Set(keys::kFileBrowserHandlers, std::move(handlers));

  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  const std::string* result = manifest.FindString(keys::kName);
  ASSERT_TRUE(result);
  EXPECT_EQ("name", *result);

  result = manifest.FindString(keys::kDescription);
  ASSERT_TRUE(result);
  EXPECT_EQ("description", *result);

  base::Value::List* handlers_raw =
      manifest.FindList(keys::kFileBrowserHandlers);
  ASSERT_TRUE(handlers_raw);
  ASSERT_EQ(handlers_raw->size(), 1u);
  base::Value::Dict* handler_raw = (*handlers_raw)[0].GetIfDict();
  result = handler_raw->FindString(keys::kActionDefaultTitle);
  ASSERT_TRUE(result);
  EXPECT_EQ("file handler title", *result);

  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, LocalizeManifestWithNameDescriptionCommandDescription) {
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "__MSG_name__");
  manifest.Set(keys::kDescription, "__MSG_description__");
  base::Value::Dict commands;

  base::Value::Dict first_command;
  first_command.Set(keys::kDescription, "__MSG_first_command_description__");
  commands.Set("first_command", std::move(first_command));

  base::Value::Dict second_command;
  second_command.Set(keys::kDescription, "__MSG_second_command_description__");
  commands.Set("second_command", std::move(second_command));
  manifest.Set(keys::kCommands, std::move(commands));

  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  const std::string* result = manifest.FindString(keys::kName);
  ASSERT_TRUE(result);
  EXPECT_EQ("name", *result);

  result = manifest.FindString(keys::kDescription);
  ASSERT_TRUE(result);
  EXPECT_EQ("description", *result);

  result =
      manifest.FindStringByDottedPath("commands.first_command.description");
  ASSERT_TRUE(result);
  EXPECT_EQ("first command", *result);

  result =
      manifest.FindStringByDottedPath("commands.second_command.description");
  ASSERT_TRUE(result);
  EXPECT_EQ("second command", *result);

  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, LocalizeManifestWithShortName) {
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "extension name");
  manifest.Set(keys::kShortName, "__MSG_short_name__");

  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));
  EXPECT_TRUE(error.empty());

  const std::string* result = manifest.FindString(keys::kShortName);
  ASSERT_TRUE(result);
  EXPECT_EQ("short_name", *result);
}

TEST(ExtensionL10nUtil, LocalizeManifestWithBadShortName) {
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "extension name");
  manifest.Set(keys::kShortName, "__MSG_short_name_bad__");

  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_FALSE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));
  EXPECT_FALSE(error.empty());

  const std::string* result = manifest.FindString(keys::kShortName);
  ASSERT_TRUE(result);
  EXPECT_EQ("__MSG_short_name_bad__", *result);
}

TEST(ExtensionL10nUtil, LocalizeManifestWithSearchProviderMsgs) {
  base::Value::Dict manifest;
  manifest.Set(keys::kName, "__MSG_name__");
  manifest.Set(keys::kDescription, "__MSG_description__");

  base::Value::Dict search_provider;
  search_provider.Set("name", "__MSG_country__");
  search_provider.Set("keyword", "__MSG_omnibox_keyword__");
  search_provider.Set("search_url", "http://www.foo.__MSG_country__");
  search_provider.Set("favicon_url", "http://www.foo.__MSG_country__");
  search_provider.Set("suggest_url", "http://www.foo.__MSG_country__");
  manifest.SetByDottedPath(keys::kOverrideSearchProvider,
                           std::move(search_provider));

  manifest.SetByDottedPath(keys::kOverrideHomepage,
                           "http://www.foo.__MSG_country__");

  base::Value::List startup_pages;
  startup_pages.Append("http://www.foo.__MSG_country__");
  manifest.SetByDottedPath(keys::kOverrideStartupPage,
                           std::move(startup_pages));

  std::string error;
  std::unique_ptr<MessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  const std::string* result = manifest.FindString(keys::kName);
  ASSERT_TRUE(result);
  EXPECT_EQ("name", *result);

  result = manifest.FindString(keys::kDescription);
  ASSERT_TRUE(result);
  EXPECT_EQ("description", *result);

  std::string key_prefix(keys::kOverrideSearchProvider);
  key_prefix += '.';
  result = manifest.FindStringByDottedPath(key_prefix + "name");
  ASSERT_TRUE(result);
  EXPECT_EQ("de", *result);

  result = manifest.FindStringByDottedPath(key_prefix + "keyword");
  ASSERT_TRUE(result);
  EXPECT_EQ("omnibox keyword", *result);

  result = manifest.FindStringByDottedPath(key_prefix + "search_url");
  ASSERT_TRUE(result);
  EXPECT_EQ("http://www.foo.de", *result);

  result = manifest.FindStringByDottedPath(key_prefix + "favicon_url");
  ASSERT_TRUE(result);
  EXPECT_EQ("http://www.foo.de", *result);

  result = manifest.FindStringByDottedPath(key_prefix + "suggest_url");
  ASSERT_TRUE(result);
  EXPECT_EQ("http://www.foo.de", *result);

  result = manifest.FindStringByDottedPath(keys::kOverrideHomepage);
  ASSERT_TRUE(result);
  EXPECT_EQ("http://www.foo.de", *result);

  base::Value::List* startup_pages_raw =
      manifest.FindListByDottedPath(keys::kOverrideStartupPage);
  ASSERT_TRUE(startup_pages_raw);
  ASSERT_FALSE(startup_pages_raw->empty());
  ASSERT_TRUE((*startup_pages_raw)[0].is_string());
  EXPECT_EQ("http://www.foo.de", (*startup_pages_raw)[0].GetString());

  EXPECT_TRUE(error.empty());
}

// Tests that we don't relocalize with default and current locales missing.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestEmptyManifest) {
  base::Value::Dict manifest;
  EXPECT_FALSE(extension_l10n_util::ShouldRelocalizeManifest(manifest));
}

// Tests that we relocalize without a current locale.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestWithDefaultLocale) {
  base::Value::Dict manifest;
  manifest.Set(keys::kDefaultLocale, "en_US");
  EXPECT_TRUE(extension_l10n_util::ShouldRelocalizeManifest(manifest));
}

// Tests that we don't relocalize without a default locale.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestWithCurrentLocale) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("en-US");
  base::Value::Dict manifest;
  manifest.Set(keys::kCurrentLocale, "en_US");
  EXPECT_FALSE(extension_l10n_util::ShouldRelocalizeManifest(manifest));
}

// Tests that we don't relocalize with same current_locale as system locale.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestSameCurrentLocale) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("en-US");
  base::Value::Dict manifest;
  manifest.Set(keys::kDefaultLocale, "en_US");
  manifest.Set(keys::kCurrentLocale, "en_US");
  EXPECT_FALSE(extension_l10n_util::ShouldRelocalizeManifest(manifest));
}

// Tests that we relocalize with a different current_locale.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestDifferentCurrentLocale) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("en-US");
  base::Value::Dict manifest;
  manifest.Set(keys::kDefaultLocale, "en_US");
  manifest.Set(keys::kCurrentLocale, "sr");
  EXPECT_TRUE(extension_l10n_util::ShouldRelocalizeManifest(manifest));
}

// Tests that we don't relocalize with the same current_locale as preferred
// locale.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestSameCurrentLocaleAsPreferred) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("en-GB", "en-CA");
  base::Value::Dict manifest;
  manifest.Set(keys::kDefaultLocale, "en_US");
  manifest.Set(keys::kCurrentLocale, "en_CA");

  // Preferred and current locale are both en_CA.
  EXPECT_FALSE(extension_l10n_util::ShouldRelocalizeManifest(manifest));
}

// Tests that we relocalize with a different current_locale from the preferred
// locale.
TEST(ExtensionL10nUtil,
     ShouldRelocalizeManifestDifferentCurrentLocaleThanPreferred) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("en-GB", "en-CA");
  base::Value::Dict manifest;
  manifest.Set(keys::kDefaultLocale, "en_US");
  manifest.Set(keys::kCurrentLocale, "en_GB");

  // Requires relocalization as the preferred (en_CA) differs from current
  // (en_GB).
  EXPECT_TRUE(extension_l10n_util::ShouldRelocalizeManifest(manifest));
}

TEST(ExtensionL10nUtil, GetAllFallbackLocales) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("en-US");
  std::vector<std::string> fallback_locales;
  extension_l10n_util::GetAllFallbackLocales("all", &fallback_locales);
  ASSERT_EQ(3U, fallback_locales.size());

  EXPECT_EQ("en_US", fallback_locales[0]);
  EXPECT_EQ("en", fallback_locales[1]);
  EXPECT_EQ("all", fallback_locales[2]);
}

TEST(ExtensionL10nUtil, GetAllFallbackLocalesWithPreferredLocale) {
  extension_l10n_util::ScopedLocaleForTest scoped_locale("en-GB", "en-CA");
  std::vector<std::string> fallback_locales;
  extension_l10n_util::GetAllFallbackLocales("all", &fallback_locales);
  ASSERT_EQ(4U, fallback_locales.size());

  EXPECT_EQ("en_CA", fallback_locales[0]);
  EXPECT_EQ("en_GB", fallback_locales[1]);
  EXPECT_EQ("en", fallback_locales[2]);
  EXPECT_EQ("all", fallback_locales[3]);
}

}  // namespace
}  // namespace extensions
