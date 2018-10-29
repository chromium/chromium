// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/clear_site_data_handler.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_response_headers.h"

namespace content {

namespace {

// Datatypes.
const char kDatatypeWildcard[] = "\"*\"";
const char kDatatypeCookies[] = "\"cookies\"";
const char kDatatypeStorage[] = "\"storage\"";
const char kDatatypeCache[] = "\"cache\"";

// Pretty-printed log output.
const char kConsoleMessageTemplate[] = "Clear-Site-Data header on '%s': %s";
const char kConsoleMessageCleared[] = "Cleared data types: %s.";
const char kConsoleMessageDatatypeSeparator[] = ", ";

bool AreExperimentalFeaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
}

// Represents the parameters as a single number to be recorded in a histogram.
int ParametersMask(bool clear_cookies, bool clear_storage, bool clear_cache) {
  return static_cast<int>(clear_cookies) * (1 << 0) +
         static_cast<int>(clear_storage) * (1 << 1) +
         static_cast<int>(clear_cache) * (1 << 2);
}

// Helper class to find the BrowserContext associated with the request and
// requests the actual clearing of data for |origin|. The data types to be
// deleted are determined by |clear_cookies|, |clear_storage|, and
// |clear_cache|. |web_contents_getter| identifies the WebContents from which
// the request originated.
// TODO(crbug.com/876931): |SiteDataClearer| could be merged into
// |ClearSiteDataHandler| to make things cleaner.
class SiteDataClearer : public BrowsingDataRemover::Observer {
 public:
  static void Run(
      const base::RepeatingCallback<WebContents*()>& web_contents_getter,
      const url::Origin& origin,
      bool clear_cookies,
      bool clear_storage,
      bool clear_cache,
      base::OnceClosure callback) {
    WebContents* web_contents = web_contents_getter.Run();
    if (!web_contents)
      return;

    (new SiteDataClearer(web_contents, origin, clear_cookies, clear_storage,
                         clear_cache, std::move(callback)))
        ->RunAndDestroySelfWhenDone();
  }

 private:
  SiteDataClearer(const WebContents* web_contents,
                  const url::Origin& origin,
                  bool clear_cookies,
                  bool clear_storage,
                  bool clear_cache,
                  base::OnceClosure callback)
      : origin_(origin),
        clear_cookies_(clear_cookies),
        clear_storage_(clear_storage),
        clear_cache_(clear_cache),
        callback_(std::move(callback)),
        pending_task_count_(0),
        remover_(nullptr),
        scoped_observer_(this) {
    remover_ = BrowserContext::GetBrowsingDataRemover(
        web_contents->GetBrowserContext());
    DCHECK(remover_);
    scoped_observer_.Add(remover_);
  }

  ~SiteDataClearer() override {}

  void RunAndDestroySelfWhenDone() {
    // Cookies and channel IDs are scoped to
    // a) eTLD+1 of |origin|'s host if |origin|'s host is a registrable domain
    //    or a subdomain thereof
    // b) |origin|'s host exactly if it is an IP address or an internal hostname
    //    (e.g. "localhost" or "fileserver").
    // TODO(msramek): What about plugin data?
    if (clear_cookies_) {
      std::string domain = GetDomainAndRegistry(
          origin_.host(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

      if (domain.empty())
        domain = origin_.host();  // IP address or internal hostname.

      std::unique_ptr<BrowsingDataFilterBuilder> domain_filter_builder(
          BrowsingDataFilterBuilder::Create(
              BrowsingDataFilterBuilder::WHITELIST));
      domain_filter_builder->AddRegisterableDomain(domain);

      pending_task_count_++;
      remover_->RemoveWithFilterAndReply(
          base::Time(), base::Time::Max(),
          BrowsingDataRemover::DATA_TYPE_COOKIES |
              BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS |
              BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS,
          BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
              BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
          std::move(domain_filter_builder), this);
    }

    // Delete origin-scoped data.
    int remove_mask = 0;
    if (clear_storage_)
      remove_mask |= BrowsingDataRemover::DATA_TYPE_DOM_STORAGE;
    if (clear_cache_)
      remove_mask |= BrowsingDataRemover::DATA_TYPE_CACHE;

    if (remove_mask) {
      std::unique_ptr<BrowsingDataFilterBuilder> origin_filter_builder(
          BrowsingDataFilterBuilder::Create(
              BrowsingDataFilterBuilder::WHITELIST));
      origin_filter_builder->AddOrigin(origin_);

      pending_task_count_++;
      remover_->RemoveWithFilterAndReply(
          base::Time(), base::Time::Max(), remove_mask,
          BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
              BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
          std::move(origin_filter_builder), this);
    }

    DCHECK_GT(pending_task_count_, 0);
  }

  // BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone() override {
    DCHECK(pending_task_count_);
    if (--pending_task_count_)
      return;

    std::move(callback_).Run();
    delete this;
  }

  url::Origin origin_;
  bool clear_cookies_;
  bool clear_storage_;
  bool clear_cache_;
  base::OnceClosure callback_;
  int pending_task_count_;
  BrowsingDataRemover* remover_;
  ScopedObserver<BrowsingDataRemover, BrowsingDataRemover::Observer>
      scoped_observer_;
};

// Outputs a single |formatted_message| on the UI thread.
void OutputFormattedMessage(WebContents* web_contents,
                            ConsoleMessageLevel level,
                            const std::string& formatted_text) {
  if (web_contents)
    web_contents->GetMainFrame()->AddMessageToConsole(level, formatted_text);
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
    ConsoleMessageLevel level) {
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
    base::RepeatingCallback<WebContents*()> web_contents_getter,
    const GURL& url,
    const std::string& header_value,
    int load_flags,
    base::OnceClosure callback) {
  ClearSiteDataHandler handler(std::move(web_contents_getter), url,
                               header_value, load_flags, std::move(callback),
                               std::make_unique<ConsoleMessagesDelegate>());
  handler.HandleHeaderAndOutputConsoleMessages();
}

// static
bool ClearSiteDataHandler::ParseHeaderForTesting(
    const std::string& header,
    bool* clear_cookies,
    bool* clear_storage,
    bool* clear_cache,
    ConsoleMessagesDelegate* delegate,
    const GURL& current_url) {
  return ClearSiteDataHandler::ParseHeader(header, clear_cookies, clear_storage,
                                           clear_cache, delegate, current_url);
}

ClearSiteDataHandler::ClearSiteDataHandler(
    base::RepeatingCallback<WebContents*()> web_contents_getter,
    const GURL& url,
    const std::string& header_value,
    int load_flags,
    base::OnceClosure callback,
    std::unique_ptr<ConsoleMessagesDelegate> delegate)
    : web_contents_getter_(std::move(web_contents_getter)),
      url_(url),
      header_value_(header_value),
      load_flags_(load_flags),
      callback_(std::move(callback)),
      delegate_(std::move(delegate)) {
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
  if (!IsOriginSecure(url_)) {
    delegate_->AddMessage(url_, "Not supported for insecure origins.",
                          CONSOLE_MESSAGE_LEVEL_ERROR);
    return false;
  }

  url::Origin origin = url::Origin::Create(url_);
  if (origin.opaque()) {
    delegate_->AddMessage(url_, "Not supported for unique origins.",
                          CONSOLE_MESSAGE_LEVEL_ERROR);
    return false;
  }

  // The LOAD_DO_NOT_SAVE_COOKIES flag prohibits the request from doing any
  // modification to cookies. Clear-Site-Data applies this restriction to other
  // data types as well.
  // TODO(msramek): Consider showing a blocked icon via
  // TabSpecificContentSettings and reporting the action in the "Blocked"
  // section of the cookies dialog in OIB.
  if (load_flags_ & net::LOAD_DO_NOT_SAVE_COOKIES) {
    delegate_->AddMessage(
        url_,
        "The request's credentials mode prohibits modifying cookies "
        "and other local data.",
        CONSOLE_MESSAGE_LEVEL_ERROR);
    return false;
  }

  bool clear_cookies;
  bool clear_storage;
  bool clear_cache;

  if (!ClearSiteDataHandler::ParseHeader(header_value_, &clear_cookies,
                                         &clear_storage, &clear_cache,
                                         delegate_.get(), url_)) {
    return false;
  }

  // Record the call parameters.
  UMA_HISTOGRAM_ENUMERATION(
      "Navigation.ClearSiteData.Parameters",
      ParametersMask(clear_cookies, clear_storage, clear_cache), (1 << 3));

  ExecuteClearingTask(
      origin, clear_cookies, clear_storage, clear_cache,
      base::BindOnce(&ClearSiteDataHandler::TaskFinished,
                     base::TimeTicks::Now(), std::move(delegate_),
                     web_contents_getter_, std::move(callback_)));

  return true;
}

// static
bool ClearSiteDataHandler::ParseHeader(const std::string& header,
                                       bool* clear_cookies,
                                       bool* clear_storage,
                                       bool* clear_cache,
                                       ConsoleMessagesDelegate* delegate,
                                       const GURL& current_url) {
  if (!base::IsStringASCII(header)) {
    delegate->AddMessage(current_url, "Must only contain ASCII characters.",
                         CONSOLE_MESSAGE_LEVEL_ERROR);
    return false;
  }

  *clear_cookies = false;
  *clear_storage = false;
  *clear_cache = false;

  std::vector<std::string> input_types = base::SplitString(
      header, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::string output_types;

  for (unsigned i = 0; i < input_types.size(); i++) {
    bool* data_type = nullptr;

    if (AreExperimentalFeaturesEnabled() &&
        input_types[i] == kDatatypeWildcard) {
      input_types.push_back(kDatatypeCookies);
      input_types.push_back(kDatatypeStorage);
      input_types.push_back(kDatatypeCache);
      continue;
    } else if (input_types[i] == kDatatypeCookies) {
      data_type = clear_cookies;
    } else if (input_types[i] == kDatatypeStorage) {
      data_type = clear_storage;
    } else if (input_types[i] == kDatatypeCache) {
      data_type = clear_cache;
    } else {
      delegate->AddMessage(
          current_url,
          base::StringPrintf("Unrecognized type: %s.", input_types[i].c_str()),
          CONSOLE_MESSAGE_LEVEL_ERROR);
      continue;
    }

    DCHECK(data_type);

    if (*data_type)
      continue;

    *data_type = true;
    if (!output_types.empty())
      output_types += kConsoleMessageDatatypeSeparator;
    output_types += input_types[i];
  }

  if (!*clear_cookies && !*clear_storage && !*clear_cache) {
    delegate->AddMessage(current_url, "No recognized types specified.",
                         CONSOLE_MESSAGE_LEVEL_ERROR);
    return false;
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
  delegate->AddMessage(current_url, console_output, CONSOLE_MESSAGE_LEVEL_INFO);

  return true;
}

void ClearSiteDataHandler::ExecuteClearingTask(const url::Origin& origin,
                                               bool clear_cookies,
                                               bool clear_storage,
                                               bool clear_cache,
                                               base::OnceClosure callback) {
  SiteDataClearer::Run(web_contents_getter_, origin, clear_cookies,
                       clear_storage, clear_cache, std::move(callback));
}

// static
void ClearSiteDataHandler::TaskFinished(
    base::TimeTicks clearing_started,
    std::unique_ptr<ConsoleMessagesDelegate> delegate,
    base::RepeatingCallback<WebContents*()> web_contents_getter,
    base::OnceClosure callback) {
  DCHECK(!clearing_started.is_null());

  UMA_HISTOGRAM_CUSTOM_TIMES("Navigation.ClearSiteData.Duration",
                             base::TimeTicks::Now() - clearing_started,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromSeconds(1), 50);

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
