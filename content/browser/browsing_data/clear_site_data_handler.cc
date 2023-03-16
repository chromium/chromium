// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/clear_site_data_handler.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/buckets/bucket_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

namespace {

// Datatypes.
const char kDatatypeWildcard[] = "\"*\"";
const char kDatatypeCookies[] = "\"cookies\"";
const char kDatatypeStorage[] = "\"storage\"";
const char kDatatypeStorageBucketPrefix[] = "\"storage:";
const char kDatatypeStorageBucketSuffix[] = "\"";
const char kDatatypeCache[] = "\"cache\"";

// Pretty-printed log output.
const char kConsoleMessageTemplate[] = "Clear-Site-Data header on '%s': %s";
const char kConsoleMessageCleared[] = "Cleared data types: %s.";
const char kConsoleMessageDatatypeSeparator[] = ", ";

bool AreExperimentalFeaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
}

// Outputs a single |formatted_message| on the UI thread.
void OutputFormattedMessage(WebContents* web_contents,
                            blink::mojom::ConsoleMessageLevel level,
                            const std::string& formatted_text) {
  if (web_contents)
    web_contents->GetPrimaryMainFrame()->AddMessageToConsole(level,
                                                             formatted_text);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ConsoleMessagesDelegate

ClearSiteDataHandler::ConsoleMessagesDelegate::ConsoleMessagesDelegate()
    : output_formatted_message_function_(
          base::BindRepeating(&OutputFormattedMessage)) {}

ClearSiteDataHandler::ConsoleMessagesDelegate::~ConsoleMessagesDelegate() {}

void ClearSiteDataHandler::ConsoleMessagesDelegate::AddMessage(
    const GURL& url,
    const std::string& text,
    blink::mojom::ConsoleMessageLevel level) {
  messages_.push_back({url, text, level});
}

void ClearSiteDataHandler::ConsoleMessagesDelegate::OutputMessages(
    const base::RepeatingCallback<WebContents*()>& web_contents_getter) {
  if (messages_.empty())
    return;

  WebContents* web_contents = web_contents_getter.Run();

  for (const auto& message : messages_) {
    // Prefix each message with |kConsoleMessageTemplate|.
    output_formatted_message_function_.Run(
        web_contents, message.level,
        base::StringPrintf(kConsoleMessageTemplate, message.url.spec().c_str(),
                           message.text.c_str()));
  }

  messages_.clear();
}

void ClearSiteDataHandler::ConsoleMessagesDelegate::
    SetOutputFormattedMessageFunctionForTesting(
        const OutputFormattedMessageFunction& function) {
  output_formatted_message_function_ = function;
}

////////////////////////////////////////////////////////////////////////////////
// ClearSiteDataHandler

// static
void ClearSiteDataHandler::HandleHeader(
    base::RepeatingCallback<BrowserContext*()> browser_context_getter,
    base::RepeatingCallback<WebContents*()> web_contents_getter,
    const GURL& url,
    const std::string& header_value,
    int load_flags,
    const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
    const absl::optional<blink::StorageKey>& storage_key,
    bool partitioned_state_allowed_only,
    base::OnceClosure callback) {
  ClearSiteDataHandler handler(browser_context_getter, web_contents_getter, url,
                               header_value, load_flags, cookie_partition_key,
                               storage_key, partitioned_state_allowed_only,
                               std::move(callback),
                               std::make_unique<ConsoleMessagesDelegate>());
  handler.HandleHeaderAndOutputConsoleMessages();
}

// static
bool ClearSiteDataHandler::ParseHeaderForTesting(
    const std::string& header,
    bool* clear_cookies,
    bool* clear_storage,
    bool* clear_cache,
    std::set<std::string>* storage_buckets_to_remove,
    ConsoleMessagesDelegate* delegate,
    const GURL& current_url) {
  return ClearSiteDataHandler::ParseHeader(
      header, clear_cookies, clear_storage, clear_cache,
      storage_buckets_to_remove, delegate, current_url);
}

ClearSiteDataHandler::ClearSiteDataHandler(
    base::RepeatingCallback<BrowserContext*()> browser_context_getter,
    base::RepeatingCallback<WebContents*()> web_contents_getter,
    const GURL& url,
    const std::string& header_value,
    int load_flags,
    const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
    const absl::optional<blink::StorageKey>& storage_key,
    bool partitioned_state_allowed_only,
    base::OnceClosure callback,
    std::unique_ptr<ConsoleMessagesDelegate> delegate)
    : browser_context_getter_(browser_context_getter),
      web_contents_getter_(web_contents_getter),
      url_(url),
      header_value_(header_value),
      load_flags_(load_flags),
      cookie_partition_key_(cookie_partition_key),
      storage_key_(storage_key),
      partitioned_state_allowed_only_(partitioned_state_allowed_only),
      callback_(std::move(callback)),
      delegate_(std::move(delegate)) {
  DCHECK(browser_context_getter_);
  DCHECK(web_contents_getter_);
  DCHECK(delegate_);
}

ClearSiteDataHandler::~ClearSiteDataHandler() = default;

bool ClearSiteDataHandler::HandleHeaderAndOutputConsoleMessages() {
  bool deferred = Run();

  // If the redirect is deferred, wait until it is resumed.
  // TODO(crbug.com/876931): Delay output until next frame for navigations.
  if (!deferred) {
    OutputConsoleMessages();
    RunCallbackNotDeferred();
  }

  return deferred;
}

bool ClearSiteDataHandler::Run() {
  // Only accept the header on secure non-unique origins.
  if (!network::IsUrlPotentiallyTrustworthy(url_)) {
    delegate_->AddMessage(url_, "Not supported for insecure origins.",
                          blink::mojom::ConsoleMessageLevel::kError);
    return false;
  }

  url::Origin origin = url::Origin::Create(url_);
  if (origin.opaque()) {
    delegate_->AddMessage(url_, "Not supported for unique origins.",
                          blink::mojom::ConsoleMessageLevel::kError);
    return false;
  }

  // The LOAD_DO_NOT_SAVE_COOKIES flag prohibits the request from doing any
  // modification to cookies. Clear-Site-Data applies this restriction to other
  // data types as well.
  // TODO(msramek): Consider showing a blocked icon via
  // PageSpecificContentSettings and reporting the action in the "Blocked"
  // section of the cookies dialog in OIB.
  if (load_flags_ & net::LOAD_DO_NOT_SAVE_COOKIES) {
    delegate_->AddMessage(
        url_,
        "The request's credentials mode prohibits modifying cookies "
        "and other local data.",
        blink::mojom::ConsoleMessageLevel::kError);
    return false;
  }

  bool clear_cookies;
  bool clear_storage;
  bool clear_cache;
  std::set<std::string> storage_buckets_to_remove;

  if (!ClearSiteDataHandler::ParseHeader(
          header_value_, &clear_cookies, &clear_storage, &clear_cache,
          &storage_buckets_to_remove, delegate_.get(), url_)) {
    return false;
  }

  ExecuteClearingTask(
      origin, clear_cookies, clear_storage, clear_cache,
      storage_buckets_to_remove,
      base::BindOnce(&ClearSiteDataHandler::TaskFinished,
                     base::TimeTicks::Now(), std::move(delegate_),
                     web_contents_getter_, std::move(callback_)));

  return true;
}

// static
bool ClearSiteDataHandler::ParseHeader(
    const std::string& header,
    bool* clear_cookies,
    bool* clear_storage,
    bool* clear_cache,
    std::set<std::string>* storage_buckets_to_remove,
    ConsoleMessagesDelegate* delegate,
    const GURL& current_url) {
  if (!base::IsStringASCII(header)) {
    delegate->AddMessage(current_url, "Must only contain ASCII characters.",
                         blink::mojom::ConsoleMessageLevel::kError);
    return false;
  }

  *clear_cookies = false;
  *clear_storage = false;
  *clear_cache = false;

  std::vector<std::string> input_types = base::SplitString(
      header, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::string output_types;

  if (AreExperimentalFeaturesEnabled() &&
      std::find(input_types.begin(), input_types.end(), kDatatypeWildcard) !=
          input_types.end()) {
    input_types.push_back(kDatatypeCookies);
    input_types.push_back(kDatatypeStorage);
    input_types.push_back(kDatatypeCache);
  }

  for (auto& input_type : input_types) {
    // Match here if the beginning is '"storage:' and ends with '"'.
    if (base::FeatureList::IsEnabled(blink::features::kStorageBuckets) &&
        base::StartsWith(input_type, kDatatypeStorageBucketPrefix) &&
        base::EndsWith(input_type, kDatatypeStorageBucketSuffix)) {
      const int prefix_len = strlen(kDatatypeStorageBucketPrefix);

      const std::string bucket_name = input_type.substr(
          prefix_len, input_type.length() -
                          (prefix_len + strlen(kDatatypeStorageBucketSuffix)));

      if (IsValidBucketName(bucket_name))
        storage_buckets_to_remove->insert(bucket_name);

      // Exit the loop and continue since for buckets, there are no booleans
      // and the logic later would cause a crash.
      continue;
    }

    bool* data_type = nullptr;
    if (input_type == kDatatypeCookies) {
      data_type = clear_cookies;
    } else if (input_type == kDatatypeStorage) {
      data_type = clear_storage;
    } else if (input_type == kDatatypeCache) {
      data_type = clear_cache;
    } else {
      delegate->AddMessage(
          current_url,
          base::StringPrintf("Unrecognized type: %s.", input_type.c_str()),
          blink::mojom::ConsoleMessageLevel::kError);
      continue;
    }

    DCHECK(data_type);

    if (*data_type)
      continue;

    *data_type = true;
    if (!output_types.empty())
      output_types += kConsoleMessageDatatypeSeparator;
    output_types += input_type;
  }

  if (!*clear_cookies && !*clear_storage && !*clear_cache &&
      storage_buckets_to_remove->empty()) {
    delegate->AddMessage(current_url, "No recognized types specified.",
                         blink::mojom::ConsoleMessageLevel::kError);
    return false;
  }

  if (*clear_storage && !storage_buckets_to_remove->empty()) {
    // `clear_storage` and `clear_storage_buckets` cannot both be true. When
    // that happens, `clear_storage` stays true and we empty `storage_buckets
    // _to_remove`
    delegate->AddMessage(current_url,
                         "All the buckets related to this url will be "
                         "cleared. When passing type 'storage', any "
                         "additional buckets specifiers are ignored.",
                         blink::mojom::ConsoleMessageLevel::kWarning);
    storage_buckets_to_remove->clear();
  }

  // Pretty-print which types are to be cleared.
  // TODO(crbug.com/798760): Remove the disclaimer about cookies.
  std::string console_output =
      base::StringPrintf(kConsoleMessageCleared, output_types.c_str());
  if (*clear_cookies) {
    console_output +=
        " Clearing channel IDs and HTTP authentication cache is currently not"
        " supported, as it breaks active network connections.";
  }
  delegate->AddMessage(current_url, console_output,
                       blink::mojom::ConsoleMessageLevel::kInfo);

  return true;
}

void ClearSiteDataHandler::ExecuteClearingTask(
    const url::Origin& origin,
    bool clear_cookies,
    bool clear_storage,
    bool clear_cache,
    const std::set<std::string>& storage_buckets_to_remove,
    base::OnceClosure callback) {
  ClearSiteData(browser_context_getter_, origin, clear_cookies, clear_storage,
                clear_cache, storage_buckets_to_remove,
                true /*avoid_closing_connections*/, cookie_partition_key_,
                storage_key_, partitioned_state_allowed_only_,
                std::move(callback));
}

// static
void ClearSiteDataHandler::TaskFinished(
    base::TimeTicks clearing_started,
    std::unique_ptr<ConsoleMessagesDelegate> delegate,
    base::RepeatingCallback<WebContents*()> web_contents_getter,
    base::OnceClosure callback) {
  DCHECK(!clearing_started.is_null());

  // TODO(crbug.com/876931): Delay output until next frame for navigations.
  delegate->OutputMessages(web_contents_getter);

  std::move(callback).Run();
}

void ClearSiteDataHandler::OutputConsoleMessages() {
  delegate_->OutputMessages(web_contents_getter_);
}

void ClearSiteDataHandler::RunCallbackNotDeferred() {
  std::move(callback_).Run();
}

}  // namespace content
