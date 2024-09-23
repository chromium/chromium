// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/navigation/content_navigation_item.h"

#import "base/notreached.h"
#import "content/public/browser/favicon_status.h"
#import "content/public/browser/navigation_entry.h"
#import "content/public/browser/navigation_handle.h"
#import "content/public/browser/ssl_status.h"
#import "content/public/browser/web_contents.h"
#import "net/http/http_request_headers.h"

namespace web {

const char kNavigationItemDataKey[] = "navigation_item";

class NavigationItemHolder : public base::SupportsUserData::Data {
 public:
  static NavigationItem* GetOrCreate(content::NavigationEntry* entry) {
    NavigationItemHolder* holder = static_cast<NavigationItemHolder*>(
        entry->GetUserData(kNavigationItemDataKey));
    if (!holder) {
      std::unique_ptr<NavigationItemHolder> data =
          std::make_unique<NavigationItemHolder>();
      data->navigation_item_ =
          base::WrapUnique(new ContentNavigationItem(entry));
      entry->SetUserData(kNavigationItemDataKey, std::move(data));
      holder = static_cast<NavigationItemHolder*>(
          entry->GetUserData(kNavigationItemDataKey));
    }
    CHECK(holder);
    return holder->navigation_item_.get();
  }

  NavigationItemHolder() = default;

 private:
  std::unique_ptr<NavigationItem> navigation_item_;
};

// TODO(crbug.com/40257932): rather than converting here, we could instead use
// network::mojom::ReferrerPolicy directly and remove web::ReferrerPolicy.
ReferrerPolicy FromContentReferrerPolicy(
    network::mojom::ReferrerPolicy policy) {
  switch (policy) {
    case network::mojom::ReferrerPolicy::kAlways:
      return ReferrerPolicyAlways;
    case network::mojom::ReferrerPolicy::kDefault:
      return ReferrerPolicyDefault;
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return ReferrerPolicyNoReferrerWhenDowngrade;
    case network::mojom::ReferrerPolicy::kNever:
      return ReferrerPolicyNever;
    case network::mojom::ReferrerPolicy::kOrigin:
      return ReferrerPolicyOrigin;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return ReferrerPolicyOriginWhenCrossOrigin;
    case network::mojom::ReferrerPolicy::kSameOrigin:
      return ReferrerPolicySameOrigin;
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return ReferrerPolicyStrictOrigin;
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      return ReferrerPolicyStrictOriginWhenCrossOrigin;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return ReferrerPolicyDefault;
}

NavigationItem* ContentNavigationItem::GetOrCreate(
    content::NavigationEntry* entry) {
  if (!entry) {
    return nullptr;
  }
  return NavigationItemHolder::GetOrCreate(entry);
}

ContentNavigationItem::~ContentNavigationItem() = default;

int ContentNavigationItem::GetUniqueID() const {
  return entry_->GetUniqueID();
}

void ContentNavigationItem::SetOriginalRequestURL(const GURL& url) {
  NOTREACHED_IN_MIGRATION();
}

const GURL& ContentNavigationItem::GetOriginalRequestURL() const {
  return entry_->GetOriginalRequestURL();
}

void ContentNavigationItem::SetURL(const GURL& url) {
  NOTREACHED_IN_MIGRATION();
}

const GURL& ContentNavigationItem::GetURL() const {
  return entry_->GetURL();
}

void ContentNavigationItem::SetReferrer(const Referrer& referrer) {
  NOTREACHED_IN_MIGRATION();
}

const Referrer& ContentNavigationItem::GetReferrer() const {
  const auto& content_referrer = entry_->GetReferrer();
  referrer_ = Referrer(content_referrer.url,
                       FromContentReferrerPolicy(content_referrer.policy));
  return referrer_;
}

void ContentNavigationItem::SetVirtualURL(const GURL& url) {
  NOTREACHED_IN_MIGRATION();
}

const GURL& ContentNavigationItem::GetVirtualURL() const {
  return entry_->GetVirtualURL();
}

void ContentNavigationItem::SetTitle(const std::u16string& title) {
  NOTREACHED_IN_MIGRATION();
}

const std::u16string& ContentNavigationItem::GetTitle() const {
  return entry_->GetTitle();
}

const std::u16string& ContentNavigationItem::GetTitleForDisplay() const {
  return entry_->GetTitleForDisplay();
}

void ContentNavigationItem::SetTransitionType(
    ui::PageTransition transition_type) {
  NOTREACHED_IN_MIGRATION();
}

ui::PageTransition ContentNavigationItem::GetTransitionType() const {
  return entry_->GetTransitionType();
}

void ContentNavigationItem::SetFaviconStatus(
    const FaviconStatus& favicon_status) {
  NOTREACHED_IN_MIGRATION();
}

const FaviconStatus& ContentNavigationItem::GetFaviconStatus() const {
  auto& favicon = entry_->GetFavicon();
  favicon_status_ = {favicon.valid, favicon.url, favicon.image};
  return favicon_status_;
}

const SSLStatus& ContentNavigationItem::GetSSL() const {
  return const_cast<ContentNavigationItem*>(this)->GetSSL();
}

SSLStatus& ContentNavigationItem::GetSSL() {
  // TODO(crbug.com/40257932): update web::SSLStatus to include some of the
  // newer fields/values that are included in content::SSLStatus.
  const auto& content_ssl_status = entry_->GetSSL();
  constexpr int WEB_SUPPORTED_STATUS_MASK =
      content::SSLStatus::DISPLAYED_INSECURE_CONTENT;
  ssl_status_.content_status =
      content_ssl_status.content_status & WEB_SUPPORTED_STATUS_MASK;
  if (content_ssl_status.content_status == content::SSLStatus::NORMAL_CONTENT) {
    ssl_status_.security_style = web::SECURITY_STYLE_AUTHENTICATED;
  } else {
    ssl_status_.security_style = web::SECURITY_STYLE_UNAUTHENTICATED;
  }
  ssl_status_.certificate = content_ssl_status.certificate;
  ssl_status_.cert_status = content_ssl_status.cert_status;
  return ssl_status_;
}

void ContentNavigationItem::SetTimestamp(base::Time timestamp) {
  NOTREACHED_IN_MIGRATION();
}

base::Time ContentNavigationItem::GetTimestamp() const {
  return entry_->GetTimestamp();
}

void ContentNavigationItem::SetUserAgentType(UserAgentType type) {
  user_agent_type_ = type;
}

UserAgentType ContentNavigationItem::GetUserAgentType() const {
  return user_agent_type_;
}

bool ContentNavigationItem::HasPostData() const {
  return entry_->GetHasPostData();
}

void ContentNavigationItem::AddHttpRequestHeaders(
    HttpRequestHeaders* additional_headers) {
  NOTREACHED_IN_MIGRATION();
}

NavigationItem::HttpRequestHeaders*
ContentNavigationItem::GetHttpRequestHeaders() const {
  auto headers_string = entry_->GetExtraHeaders();
  if (!headers_string.empty()) {
    net::HttpRequestHeaders headers;
    headers.AddHeadersFromString(headers_string);
    NSMutableDictionary* headers_dict = [[NSMutableDictionary alloc] init];
    for (const auto& kv : headers.GetHeaderVector()) {
      NSString* key = [NSString stringWithUTF8String:kv.key.c_str()];
      NSString* value = [NSString stringWithUTF8String:kv.value.c_str()];
      [headers_dict setObject:value forKey:key];
    }
    headers_ = headers_dict;
  }
  return headers_;
}

void ContentNavigationItem::SetHttpsUpgradeType(
    HttpsUpgradeType https_upgrade_type) {
  NOTREACHED_IN_MIGRATION();
}

HttpsUpgradeType ContentNavigationItem::GetHttpsUpgradeType() const {
  // TODO(crbug.com/40257932): Determine an analog.
  return HttpsUpgradeType::kNone;
}

ContentNavigationItem::ContentNavigationItem(content::NavigationEntry* entry)
    : entry_(entry) {}

}  // namespace web
