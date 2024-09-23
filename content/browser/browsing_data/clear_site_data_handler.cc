// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/clear_site_data_handler.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/buckets/bucket_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/url_request/clear_site_data.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

namespace {

// Pretty-printed log output.
const char kConsoleMessageTemplate[] = "Clear-Site-Data header on '%s': %s";
const char kConsoleMessageCleared[] = "Cleared data types: %s.";
const char kConsoleMessageDatatypeSeparator[] = ", ";

enum LoggableEventMask {
  CLEAR_SITE_DATA_NO_RECOGNIZABLE_TYPES = 0,
  CLEAR_SITE_DATA_COOKIES = 1 << 0,
  CLEAR_SITE_DATA_STORAGE = 1 << 1,
  CLEAR_SITE_DATA_CACHE = 1 << 2,
  CLEAR_SITE_DATA_BUCKETS = 1 << 3,
  CLEAR_SITE_DATA_CLIENT_HINTS = 1 << 4,
  CLEAR_SITE_DATA_MAX_VALUE = 1 << 5,
};

void LogEvent(int event) {
  UMA_HISTOGRAM_ENUMERATION("Storage.ClearSiteDataHeader.Parameters", event,
                            static_cast<int>(CLEAR_SITE_DATA_MAX_VALUE));
}

// Represents the parameters as a single number to be recorded in a histogram.
int ParametersMask(const ClearSiteDataTypeSet clear_site_data_types,
                   bool has_buckets) {
  int mask = CLEAR_SITE_DATA_NO_RECOGNIZABLE_TYPES;
  if (clear_site_data_types.Has(ClearSiteDataType::kCookies)) {
    mask = mask | CLEAR_SITE_DATA_COOKIES;
  }
  if (clear_site_data_types.Has(ClearSiteDataType::kStorage)) {
    mask = mask | CLEAR_SITE_DATA_STORAGE;
  }
  if (clear_site_data_types.Has(ClearSiteDataType::kCache)) {
    mask = mask | CLEAR_SITE_DATA_CACHE;
  }
  if (has_buckets) {
    mask = mask | CLEAR_SITE_DATA_BUCKETS;
  }
  if (clear_site_data_types.Has(ClearSiteDataType::kClientHints)) {
    mask = mask | CLEAR_SITE_DATA_CLIENT_HINTS;
  }
  return mask;
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
    base::WeakPtr<WebContents> web_contents) {
  if (messages_.empty())
    return;

  for (const auto& message : messages_) {
    // Prefix each message with |kConsoleMessageTemplate|.
    output_formatted_message_function_.Run(
        web_contents.get(), message.level,
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
    base::WeakPtr<BrowserContext> browser_context,
    base::WeakPtr<WebContents> web_contents,
    const StoragePartitionConfig& storage_partition_config,
    const GURL& url,
    const std::string& header_value,
    int load_flags,
    const std::optional<net::CookiePartitionKey> cookie_partition_key,
    const std::optional<blink::StorageKey> storage_key,
    bool partitioned_state_allowed_only,
    base::OnceClosure callback) {
  ClearSiteDataHandler handler(
      browser_context, web_contents, storage_partition_config, url,
      header_value, load_flags, cookie_partition_key, storage_key,
      partitioned_state_allowed_only, std::move(callback),
      std::make_unique<ConsoleMessagesDelegate>());
  handler.HandleHeaderAndOutputConsoleMessages();
}

// static
bool ClearSiteDataHandler::ParseHeaderForTesting(
    const std::string& header,
    ClearSiteDataTypeSet* clear_site_data_types,
    std::set<std::string>* storage_buckets_to_remove,
    ConsoleMessagesDelegate* delegate,
    const GURL& current_url) {
  return ClearSiteDataHandler::ParseHeader(header, clear_site_data_types,
                                           storage_buckets_to_remove, delegate,
                                           current_url);
}

ClearSiteDataHandler::ClearSiteDataHandler(
    base::WeakPtr<BrowserContext> browser_context,
    base::WeakPtr<WebContents> web_contents,
    const StoragePartitionConfig& storage_partition_config,
    const GURL& url,
    const std::string& header_value,
    int load_flags,
    const std::optional<net::CookiePartitionKey> cookie_partition_key,
    const std::optional<blink::StorageKey> storage_key,
    bool partitioned_state_allowed_only,
    base::OnceClosure callback,
    std::unique_ptr<ConsoleMessagesDelegate> delegate)
    : browser_context_(browser_context),
      web_contents_(web_contents),
      storage_partition_config_(storage_partition_config),
      url_(url),
      header_value_(header_value),
      load_flags_(load_flags),
      cookie_partition_key_(cookie_partition_key),
      storage_key_(storage_key),
      partitioned_state_allowed_only_(partitioned_state_allowed_only),
      callback_(std::move(callback)),
      delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

ClearSiteDataHandler::~ClearSiteDataHandler() = default;

bool ClearSiteDataHandler::HandleHeaderAndOutputConsoleMessages() {
  bool deferred = Run();

  // If the redirect is deferred, wait until it is resumed.
  // TODO(crbug.com/41409604): Delay output until next frame for navigations.
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

  ClearSiteDataTypeSet clear_site_data_types;
  std::set<std::string> storage_buckets_to_remove;

  if (!ClearSiteDataHandler::ParseHeader(header_value_, &clear_site_data_types,
                                         &storage_buckets_to_remove,
                                         delegate_.get(), url_)) {
    return false;
  }

  ExecuteClearingTask(
      origin, clear_site_data_types, storage_buckets_to_remove,
      base::BindOnce(&ClearSiteDataHandler::TaskFinished,
                     base::TimeTicks::Now(), std::move(delegate_),
                     web_contents_, std::move(callback_)));

  return true;
}

// static
bool ClearSiteDataHandler::ParseHeader(
    const std::string& header,
    ClearSiteDataTypeSet* clear_site_data_types,
    std::set<std::string>* storage_buckets_to_remove,
    ConsoleMessagesDelegate* delegate,
    const GURL& current_url) {
  DCHECK(clear_site_data_types);
  DCHECK(storage_buckets_to_remove);
  DCHECK(delegate);

  if (!base::IsStringASCII(header)) {
    delegate->AddMessage(current_url, "Must only contain ASCII characters.",
                         blink::mojom::ConsoleMessageLevel::kError);
    LogEvent(CLEAR_SITE_DATA_NO_RECOGNIZABLE_TYPES);
    return false;
  }

  clear_site_data_types->Clear();

  std::vector<std::string> input_types =
      net::ClearSiteDataHeaderContents(header);
  std::string output_types;

  if (base::Contains(input_types, net::kDatatypeWildcard)) {
    input_types.push_back(net::kDatatypeCookies);
    input_types.push_back(net::kDatatypeStorage);
    input_types.push_back(net::kDatatypeCache);
    input_types.push_back(net::kDatatypeClientHints);
  }

  for (auto& input_type : input_types) {
    // Match here if the beginning is '"storage:' and ends with '"'.
    if (base::FeatureList::IsEnabled(blink::features::kStorageBuckets) &&
        base::StartsWith(input_type, net::kDatatypeStorageBucketPrefix) &&
        base::EndsWith(input_type, net::kDatatypeStorageBucketSuffix)) {
      const int prefix_len = strlen(net::kDatatypeStorageBucketPrefix);

      const std::string bucket_name = input_type.substr(
          prefix_len,
          input_type.length() -
              (prefix_len + strlen(net::kDatatypeStorageBucketSuffix)));

      if (IsValidBucketName(bucket_name))
        storage_buckets_to_remove->insert(bucket_name);

      // Exit the loop and continue since for buckets, there are no booleans
      // and the logic later would cause a crash.
      continue;
    }

    ClearSiteDataType data_type = ClearSiteDataType::kUndefined;
    if (input_type == net::kDatatypeCookies) {
      data_type = ClearSiteDataType::kCookies;
    } else if (input_type == net::kDatatypeStorage) {
      data_type = ClearSiteDataType::kStorage;
    } else if (input_type == net::kDatatypeCache) {
      data_type = ClearSiteDataType::kCache;
    } else if (input_type == net::kDatatypeClientHints) {
      data_type = ClearSiteDataType::kClientHints;
    } else if (input_type == net::kDatatypeWildcard) {
      continue;
    } else {
      delegate->AddMessage(
          current_url,
          base::StringPrintf("Unrecognized type: %s.", input_type.c_str()),
          blink::mojom::ConsoleMessageLevel::kError);
      continue;
    }

    DCHECK_NE(data_type, ClearSiteDataType::kUndefined);

    if (clear_site_data_types->Has(data_type)) {
      continue;
    }

    clear_site_data_types->Put(data_type);
    if (!output_types.empty())
      output_types += kConsoleMessageDatatypeSeparator;
    output_types += input_type;
  }

  if (clear_site_data_types->empty() && storage_buckets_to_remove->empty()) {
    delegate->AddMessage(current_url, "No recognized types specified.",
                         blink::mojom::ConsoleMessageLevel::kError);
    LogEvent(CLEAR_SITE_DATA_NO_RECOGNIZABLE_TYPES);
    return false;
  }

  if (clear_site_data_types->Has(ClearSiteDataType::kStorage) &&
      !storage_buckets_to_remove->empty()) {
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
  // TODO(crbug.com/41363015): Remove the disclaimer about cookies.
  std::string console_output =
      base::StringPrintf(kConsoleMessageCleared, output_types.c_str());
  if (clear_site_data_types->Has(ClearSiteDataType::kCookies)) {
    console_output +=
        " Clearing channel IDs and HTTP authentication cache is currently not"
        " supported, as it breaks active network connections.";
  }
  delegate->AddMessage(current_url, console_output,
                       blink::mojom::ConsoleMessageLevel::kInfo);

  // Note that presence of headers is also logged in WebRequest.ResponseHeader
  LogEvent(ParametersMask(*clear_site_data_types,
                          !storage_buckets_to_remove->empty()));

  return true;
}

void ClearSiteDataHandler::ExecuteClearingTask(
    const url::Origin& origin,
    const ClearSiteDataTypeSet clear_site_data_types,
    const std::set<std::string>& storage_buckets_to_remove,
    base::OnceClosure callback) {
  ClearSiteData(browser_context_, storage_partition_config_, origin,
                clear_site_data_types, storage_buckets_to_remove,
                /*avoid_closing_connections=*/true, cookie_partition_key_,
                storage_key_, partitioned_state_allowed_only_,
                std::move(callback));
}

// static
void ClearSiteDataHandler::TaskFinished(
    base::TimeTicks clearing_started,
    std::unique_ptr<ConsoleMessagesDelegate> delegate,
    base::WeakPtr<WebContents> web_contents,
    base::OnceClosure callback) {
  DCHECK(!clearing_started.is_null());

  // TODO(crbug.com/41409604): Delay output until next frame for navigations.
  delegate->OutputMessages(web_contents);

  std::move(callback).Run();
}

void ClearSiteDataHandler::OutputConsoleMessages() {
  delegate_->OutputMessages(web_contents_);
}

void ClearSiteDataHandler::RunCallbackNotDeferred() {
  std::move(callback_).Run();
}

}  // namespace content
