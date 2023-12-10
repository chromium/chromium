// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/notification/notification_client.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringize_macros.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/client/notification/gstatic_json_fetcher.h"
#include "remoting/client/notification/notification_message.h"
#include "remoting/client/notification/version_range.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/locale_utils.h"
#endif

namespace remoting {

namespace {

#if BUILDFLAG(IS_IOS)
constexpr char kCurrentPlatform[] = "IOS";
#elif BUILDFLAG(IS_ANDROID)
constexpr char kCurrentPlatform[] = "ANDROID";
#else
constexpr char kCurrentPlatform[] = "UNKNOWN";
#endif

constexpr char kCurrentVersion[] = STRINGIZE(VERSION);

#if defined(NDEBUG)
constexpr bool kShouldIgnoreDevMessages = true;
#else
constexpr bool kShouldIgnoreDevMessages = false;
#endif

constexpr char kDefaultLocale[] = "en-US";
constexpr char kNotificationRootPath[] = "notification/";
constexpr char kNotificationRulesPath[] = "notification/rules.json";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("notification_client",
                                        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Fetches new notification messages to be shown on Chrome Remote "
            "Desktop app."
          trigger:
            "Opening the Chrome Remote Desktop app."
          data: "No user data."
          destination: OTHER
          destination_other:
            "The Chrome Remote Desktop app."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented."
        })");

template <typename T, typename IsTChecker, typename TGetter>
bool FindKeyAndGet(const base::Value::Dict& dict,
                   const std::string& key,
                   T* out,
                   IsTChecker is_t_checker,
                   TGetter t_getter,
                   const std::string& type_name) {
  const base::Value* value = dict.Find(key);
  if (!value) {
    LOG(WARNING) << "|" << key << "| not found in dictionary.";
    return false;
  }
  if (!(value->*is_t_checker)()) {
    LOG(ERROR) << "Value is not " << type_name;
    return false;
  }
  *out = (value->*t_getter)();
  return true;
}

bool FindKeyAndGet(const base::Value::Dict& dict,
                   const std::string& key,
                   std::string* out) {
  const std::string& (base::Value::*get_string_fp)() const =
      &base::Value::GetString;
  return FindKeyAndGet(dict, key, out, &base::Value::is_string, get_string_fp,
                       "string");
}

bool FindKeyAndGet(const base::Value::Dict& dict,
                   const std::string& key,
                   int* out) {
  return FindKeyAndGet(dict, key, out, &base::Value::is_int,
                       &base::Value::GetInt, "int");
}

bool FindKeyAndGet(const base::Value::Dict& dict,
                   const std::string& key,
                   bool* out) {
  return FindKeyAndGet(dict, key, out, &base::Value::is_bool,
                       &base::Value::GetBool, "bool");
}

bool ShouldShowNotificationForUser(const std::string& user_email,
                                   int percent_int) {
  DCHECK_GE(percent_int, 0);
  DCHECK_LE(percent_int, 100);

  // If the user is not logged in, we only want to show the notification if the
  // rollout percentage is 100. `hash("") % 100` could be anything between
  // [0, 99] and may therefore get selected for a lower percentage, so we add
  // special-casing for percent_int != 100.
  // For percent_int == 100, `hash(any_string) % 100` is always smaller than
  // 100, so no special-casing is needed.
  if (user_email.empty() && percent_int != 100) {
    return false;
  }

  return (base::PersistentHash(user_email) % 100) <
         static_cast<unsigned int>(percent_int);
}

class MessageAndLinkTextResults
    : public base::RefCounted<MessageAndLinkTextResults> {
 public:
  using Callback = base::OnceCallback<void(bool is_successful)>;
  MessageAndLinkTextResults(const std::string& locale,
                            Callback done,
                            std::string* out_message_translation,
                            std::string* out_link_translation)
      : locale_(locale),
        done_(std::move(done)),
        out_message_translation_(out_message_translation),
        out_link_translation_(out_link_translation) {}

  void OnMessageTranslationsFetched(std::optional<base::Value> translations) {
    is_message_translation_fetched_ = true;
    OnTranslationsFetched(std::move(translations), out_message_translation_);
  }

  void OnLinkTranslationsFetched(std::optional<base::Value> translations) {
    is_link_translation_fetched_ = true;
    OnTranslationsFetched(std::move(translations), out_link_translation_);
  }

 private:
  friend class base::RefCounted<MessageAndLinkTextResults>;

  ~MessageAndLinkTextResults() = default;

  void OnTranslationsFetched(std::optional<base::Value> translations,
                             std::string* string_to_update) {
    if (!done_) {
      LOG(WARNING) << "Received new translations after some translations have "
                   << "failed to fetch";
      return;
    }
    if (!translations) {
      LOG(ERROR) << "Failed to fetch translation file";
      NotifyFetchFailed();
      return;
    }

    const base::Value::Dict& dict = translations->GetDict();
    bool is_translation_found = false;
    is_translation_found = FindKeyAndGet(dict, locale_, string_to_update);
    if (!is_translation_found) {
      LOG(WARNING) << "Failed to find translation for locale " << locale_
                   << ". Looking for parent locales instead.";
      std::vector<std::string> parent_locales;
      l10n_util::GetParentLocales(locale_, &parent_locales);
      for (auto parent_locale : parent_locales) {
        // Locales returned by GetParentLocales() use "_" instead of "-", which
        // need to be fixed.
        std::replace(parent_locale.begin(), parent_locale.end(), '_', '-');
        if (FindKeyAndGet(dict, parent_locale, string_to_update)) {
          LOG(WARNING) << "Locale " << parent_locale
                       << " is being used instead.";
          is_translation_found = true;
          break;
        }
      }
    }
    if (!is_translation_found) {
      LOG(WARNING)
          << "No parent locale for " << locale_
          << " is found in translations. Falling back to default locale: "
          << kDefaultLocale;
      is_translation_found =
          FindKeyAndGet(dict, kDefaultLocale, string_to_update);
    }
    if (!is_translation_found) {
      LOG(ERROR) << "Failed to find translation for default locale";
      NotifyFetchFailed();
      return;
    }
    if (done_ && is_message_translation_fetched_ &&
        is_link_translation_fetched_) {
      RunDoneCallback(true);
    }
  }

  void NotifyFetchFailed() {
    DCHECK(done_);
    RunDoneCallback(false);
  }

  void RunDoneCallback(bool arg) {
    // Drop unowned resources before invoking callback destroying them.
    out_link_translation_ = nullptr;
    out_message_translation_ = nullptr;
    std::move(done_).Run(arg);
  }

  std::string locale_;
  Callback done_;
  raw_ptr<std::string> out_message_translation_;
  raw_ptr<std::string> out_link_translation_;
  bool is_message_translation_fetched_ = false;
  bool is_link_translation_fetched_ = false;
};

}  // namespace

NotificationClient::NotificationClient(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : NotificationClient(
          std::make_unique<GstaticJsonFetcher>(network_task_runner),
          kCurrentPlatform,
          kCurrentVersion,
          base::SysInfo::OperatingSystemVersion(),
#if BUILDFLAG(IS_ANDROID)
          // GetApplicationLocale() returns empty string on Android since we
          // don't pack any .pak file into the apk, so we need to get the locale
          // string directly.
          base::android::GetDefaultLocaleString(),
#else
          l10n_util::GetApplicationLocale(""),
#endif
          kShouldIgnoreDevMessages) {
}

NotificationClient::~NotificationClient() = default;

NotificationClient::NotificationClient(std::unique_ptr<JsonFetcher> fetcher,
                                       const std::string& current_platform,
                                       const std::string& current_version,
                                       const std::string& current_os_version,
                                       const std::string& locale,
                                       bool should_ignore_dev_messages)
    : fetcher_(std::move(fetcher)),
      current_platform_(current_platform),
      current_version_(current_version),
      current_os_version_(current_os_version),
      locale_(locale),
      should_ignore_dev_messages_(should_ignore_dev_messages) {
  VLOG(1) << "Platform: " << current_platform_
          << ", version: " << current_version_ << ", locale: " << locale_;
}

void NotificationClient::GetNotification(const std::string& user_email,
                                         NotificationCallback callback) {
  fetcher_->FetchJsonFile(
      kNotificationRulesPath,
      base::BindOnce(&NotificationClient::OnRulesFetched,
                     base::Unretained(this), user_email, std::move(callback)),
      kTrafficAnnotation);
}

void NotificationClient::OnRulesFetched(const std::string& user_email,
                                        NotificationCallback callback,
                                        std::optional<base::Value> rules) {
  if (!rules) {
    LOG(ERROR) << "Rules not found";
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (!rules->is_list()) {
    LOG(ERROR) << "Rules should be list";
    std::move(callback).Run(std::nullopt);
    return;
  }

  for (const auto& rule : rules->GetList()) {
    std::string message_text_filename;
    std::string link_text_filename;
    auto message =
        ParseAndMatchRule(rule.GetDict(), user_email, &message_text_filename,
                          &link_text_filename);

    if (message) {
      FetchTranslatedTexts(message_text_filename, link_text_filename,
                           std::move(message), std::move(callback));
      return;
    }
  }
  // No matching rule is found.
  std::move(callback).Run(std::nullopt);
}

std::optional<NotificationMessage> NotificationClient::ParseAndMatchRule(
    const base::Value::Dict& rule,
    const std::string& user_email,
    std::string* out_message_text_filename,
    std::string* out_link_text_filename) {
  std::string target_platform;
  std::string version_spec_string;
  std::string message_id;
  std::string message_text_filename;
  std::string link_text_filename;
  std::string link_url;
  int percent;
  if (!FindKeyAndGet(rule, "target_platform", &target_platform) ||
      !FindKeyAndGet(rule, "version", &version_spec_string) ||
      !FindKeyAndGet(rule, "message_id", &message_id) ||
      !FindKeyAndGet(rule, "message_text", &message_text_filename) ||
      !FindKeyAndGet(rule, "link_text", &link_text_filename) ||
      !FindKeyAndGet(rule, "link_url", &link_url) ||
      !FindKeyAndGet(rule, "percent", &percent)) {
    return std::nullopt;
  }

  if (should_ignore_dev_messages_) {
    bool is_dev_mode;
    if (FindKeyAndGet(rule, "dev_mode", &is_dev_mode) && is_dev_mode) {
      return std::nullopt;
    }
  }

  if (target_platform != current_platform_) {
    VLOG(1) << "Notification ignored. Target platform: " << target_platform
            << "; current platform: " << current_platform_;
    return std::nullopt;
  }

  VersionRange version_range(version_spec_string);
  if (!version_range.IsValid()) {
    LOG(ERROR) << "Invalid version range: " << version_spec_string;
    return std::nullopt;
  }

  if (!version_range.ContainsVersion(current_version_)) {
    VLOG(1) << "Current version " << current_version_ << " not in range "
            << version_spec_string;
    return std::nullopt;
  }

  // OS version check is not performed if |os_version| is not specified.
  std::string os_version_spec_string;
  if (FindKeyAndGet(rule, "os_version", &os_version_spec_string)) {
    VersionRange os_version_range(os_version_spec_string);
    if (!os_version_range.IsValid()) {
      LOG(ERROR) << "Invalid OS version range: " << os_version_spec_string;
      return std::nullopt;
    }
    if (!os_version_range.ContainsVersion(current_os_version_)) {
      VLOG(1) << "Current OS version " << current_os_version_
              << " not in range " << os_version_spec_string;
      return std::nullopt;
    }
  }

  if (!ShouldShowNotificationForUser(user_email, percent)) {
    VLOG(1) << "User is not selected for notification";
    return std::nullopt;
  }

  auto message = std::make_optional<NotificationMessage>();
  message->message_id = message_id;
  message->link_url = link_url;
  message->allow_silence = false;
  FindKeyAndGet(rule, "allow_silence", &message->allow_silence);
  *out_message_text_filename = message_text_filename;
  *out_link_text_filename = link_text_filename;
  return message;
}

void NotificationClient::FetchTranslatedTexts(
    const std::string& message_text_filename,
    const std::string& link_text_filename,
    std::optional<NotificationMessage> partial_message,
    NotificationCallback done) {
  // Copy the message into a unique_ptr since moving std::optional does not
  // move the internal storage.
  auto message_copy = std::make_unique<NotificationMessage>(*partial_message);
  std::string* message_text_ptr = &message_copy->message_text;
  std::string* link_text_ptr = &message_copy->link_text;
  auto on_translated_texts_fetched = base::BindOnce(
      [](std::unique_ptr<NotificationMessage> message,
         NotificationCallback done, bool is_successful) {
        std::move(done).Run(
            is_successful ? std::make_optional<NotificationMessage>(*message)
                          : std::nullopt);
      },
      std::move(message_copy), std::move(done));
  auto results = base::MakeRefCounted<MessageAndLinkTextResults>(
      locale_, std::move(on_translated_texts_fetched), message_text_ptr,
      link_text_ptr);
  fetcher_->FetchJsonFile(
      kNotificationRootPath + message_text_filename,
      base::BindOnce(&MessageAndLinkTextResults::OnMessageTranslationsFetched,
                     results),
      kTrafficAnnotation);
  fetcher_->FetchJsonFile(
      kNotificationRootPath + link_text_filename,
      base::BindOnce(&MessageAndLinkTextResults::OnLinkTranslationsFetched,
                     results),
      kTrafficAnnotation);
}

}  // namespace remoting
