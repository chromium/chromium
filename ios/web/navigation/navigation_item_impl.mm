// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_item_impl.h"

#import <stddef.h>

#import <memory>
#import <utility>

#import "base/check_op.h"
#import "base/strings/utf_string_conversions.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/proto_util.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/web_client.h"
#import "ui/base/page_transition_types.h"
#import "ui/gfx/text_elider.h"

namespace web {
namespace {

// Returns a new unique ID for use in NavigationItem during construction.  The
// returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
static int GetUniqueIDInConstructor() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

// Returns whether `referrer` needs to be serialized.
bool ShouldSerializeReferrer(const Referrer& referrer) {
  return referrer.url.is_valid() &&
         referrer.url.spec().size() < url::kMaxURLChars;
}

}  // namespace

using HttpRequestHeaders = NavigationItem::HttpRequestHeaders;

// Value 512 was picked as a tradeoff between saving memory from excessively
// long titles, while preserving the entire title as often as possible for
// features like Sync, where titles can be shared to other platforms that
// have UI surfaces supporting longer titles than iOS.
const size_t kMaxTitleLength = 512;

// static
std::unique_ptr<NavigationItem> NavigationItem::Create() {
  return std::unique_ptr<NavigationItem>(new NavigationItemImpl());
}

NavigationItemImpl::NavigationItemImpl()
    : unique_id_(GetUniqueIDInConstructor()) {}

NavigationItemImpl::~NavigationItemImpl() {
}

NavigationItemImpl::NavigationItemImpl(
    const proto::NavigationItemStorage& storage)
    : unique_id_(GetUniqueIDInConstructor()),
      referrer_(ReferrerFromProto(storage.referrer())),
      title_(base::UTF8ToUTF16(storage.title())),
      // Use reload transition type to avoid incorrect increase for other
      // transition types (such as typed).
      transition_type_(ui::PAGE_TRANSITION_RELOAD),
      timestamp_(TimeFromProto(storage.timestamp())),
      user_agent_type_(UserAgentTypeFromProto(storage.user_agent())),
      http_request_headers_(
          HttpRequestHeadersFromProto(storage.http_request_headers())) {
  // While the virtual URL is persisted, the original request URL and the
  // non-virtual URL needs to be set upon NavigationItem creation. Since
  // GetVirtualURL() returns `url_` for the non-overridden case, this will
  // also update the virtual URL reported by this object.
  url_ = original_request_url_ = GURL(storage.url());

  // In the cases where the URL to be restored is not an HTTP URL, it is
  // very likely that we can't restore the page (e.g. for files, it could
  // be an external PDF that has been deleted), don't restore it to avoid
  // issues.
  const GURL virtual_url(storage.virtual_url());
  if (url_.SchemeIsHTTPOrHTTPS()) {
    if (virtual_url.is_valid() && virtual_url != url_) {
      virtual_url_ = virtual_url;
    }
  } else {
    if (virtual_url.is_valid()) {
      url_ = virtual_url;
    }
  }
}

void NavigationItemImpl::SerializeToProto(
    proto::NavigationItemStorage& storage) const {
  if (url_.is_valid()) {
    storage.set_url(url_.spec());
  }
  if (url_ != virtual_url_ && virtual_url_.is_valid()) {
    storage.set_virtual_url(virtual_url_.spec());
  }
  if (!title_.empty()) {
    storage.set_title(base::UTF16ToUTF8(title_));
  }
  SerializeTimeToProto(timestamp_, *storage.mutable_timestamp());
  storage.set_user_agent(UserAgentTypeToProto(user_agent_type_));
  if (ShouldSerializeReferrer(referrer_)) {
    SerializeReferrerToProto(referrer_, *storage.mutable_referrer());
  }
  if (http_request_headers_.count) {
    SerializeHttpRequestHeadersToProto(http_request_headers_,
                                       *storage.mutable_http_request_headers());
  }
}

std::unique_ptr<NavigationItemImpl> NavigationItemImpl::Clone() {
  return base::WrapUnique(new NavigationItemImpl(*this));
}

int NavigationItemImpl::GetUniqueID() const {
  return unique_id_;
}

void NavigationItemImpl::SetOriginalRequestURL(const GURL& url) {
  original_request_url_ = url;
}

const GURL& NavigationItemImpl::GetOriginalRequestURL() const {
  return original_request_url_;
}

void NavigationItemImpl::SetURL(const GURL& url) {
  url_ = url;
  cached_display_title_.clear();
}

const GURL& NavigationItemImpl::GetURL() const {
  return url_;
}

void NavigationItemImpl::SetReferrer(const web::Referrer& referrer) {
  referrer_ = referrer;
}

const web::Referrer& NavigationItemImpl::GetReferrer() const {
  return referrer_;
}

void NavigationItemImpl::SetVirtualURL(const GURL& url) {
  virtual_url_ = (url == url_) ? GURL() : url;
  cached_display_title_.clear();
}

const GURL& NavigationItemImpl::GetVirtualURL() const {
  return virtual_url_.is_empty() ? url_ : virtual_url_;
}

void NavigationItemImpl::SetTitle(const std::u16string& title) {
  if (title_ == title)
    return;

  if (title.size() > kMaxTitleLength) {
    title_ = gfx::TruncateString(title, kMaxTitleLength, gfx::CHARACTER_BREAK);
  } else {
    title_ = title;
  }
  cached_display_title_.clear();
}

const std::u16string& NavigationItemImpl::GetTitle() const {
  return title_;
}

const std::u16string& NavigationItemImpl::GetTitleForDisplay() const {
  // Most pages have real titles. Don't even bother caching anything if this is
  // the case.
  if (!title_.empty())
    return title_;

  // More complicated cases will use the URLs as the title. This result we will
  // cache since it's more complicated to compute.
  if (!cached_display_title_.empty())
    return cached_display_title_;

  // File urls have different display rules, so use one if it is present.
  cached_display_title_ = NavigationItemImpl::GetDisplayTitleForURL(
      GetURL().SchemeIsFile() ? GetURL() : GetVirtualURL());
  return cached_display_title_;
}

void NavigationItemImpl::SetTransitionType(ui::PageTransition transition_type) {
  transition_type_ = transition_type;
}

ui::PageTransition NavigationItemImpl::GetTransitionType() const {
  return transition_type_;
}

const FaviconStatus& NavigationItemImpl::GetFaviconStatus() const {
  return favicon_status_;
}

void NavigationItemImpl::SetFaviconStatus(const FaviconStatus& favicon_status) {
  favicon_status_ = favicon_status;
}

const SSLStatus& NavigationItemImpl::GetSSL() const {
  return ssl_;
}

SSLStatus& NavigationItemImpl::GetSSL() {
  return ssl_;
}

void NavigationItemImpl::SetTimestamp(base::Time timestamp) {
  timestamp_ = timestamp;
}

base::Time NavigationItemImpl::GetTimestamp() const {
  return timestamp_;
}

void NavigationItemImpl::SetUserAgentType(UserAgentType type) {
  user_agent_type_ = type;
}

void NavigationItemImpl::SetUntrusted() {
  is_untrusted_ = true;
}

bool NavigationItemImpl::IsUntrusted() {
  return is_untrusted_;
}

UserAgentType NavigationItemImpl::GetUserAgentType() const {
  return user_agent_type_;
}

bool NavigationItemImpl::HasPostData() const {
  return post_data_ != nil;
}

HttpRequestHeaders* NavigationItemImpl::GetHttpRequestHeaders() const {
  return [http_request_headers_ copy];
}

void NavigationItemImpl::AddHttpRequestHeaders(
    HttpRequestHeaders* additional_headers) {
  if (!additional_headers)
    return;

  if (http_request_headers_)
    [http_request_headers_ addEntriesFromDictionary:additional_headers];
  else
    http_request_headers_ = [additional_headers mutableCopy];
}

void NavigationItemImpl::SetHttpsUpgradeType(
    HttpsUpgradeType https_upgrade_type) {
  https_upgrade_type_ = https_upgrade_type;
}

HttpsUpgradeType NavigationItemImpl::GetHttpsUpgradeType() const {
  return https_upgrade_type_;
}

void NavigationItemImpl::SetSerializedStateObject(
    NSString* serialized_state_object) {
  serialized_state_object_ = serialized_state_object;
}

NSString* NavigationItemImpl::GetSerializedStateObject() const {
  return serialized_state_object_;
}

void NavigationItemImpl::SetNavigationInitiationType(
    web::NavigationInitiationType navigation_initiation_type) {
  navigation_initiation_type_ = navigation_initiation_type;
}

web::NavigationInitiationType NavigationItemImpl::NavigationInitiationType()
    const {
  return navigation_initiation_type_;
}

void NavigationItemImpl::SetIsCreatedFromHashChange(bool hash_change) {
  is_created_from_hash_change_ = hash_change;
}

bool NavigationItemImpl::IsCreatedFromHashChange() const {
  return is_created_from_hash_change_;
}

void NavigationItemImpl::SetShouldSkipSerialization(bool skip) {
  should_skip_serialization_ = skip;
}

bool NavigationItemImpl::ShouldSkipSerialization() const {
  return should_skip_serialization_ || url_.spec().size() > url::kMaxURLChars;
}

void NavigationItemImpl::SetPostData(NSData* post_data) {
  post_data_ = post_data;
}

NSData* NavigationItemImpl::GetPostData() const {
  return post_data_;
}

void NavigationItemImpl::RemoveHttpRequestHeaderForKey(NSString* key) {
  DCHECK(key);
  [http_request_headers_ removeObjectForKey:key];
  if (![http_request_headers_ count])
    http_request_headers_ = nil;
}

void NavigationItemImpl::ResetHttpRequestHeaders() {
  http_request_headers_ = nil;
}

void NavigationItemImpl::ResetForCommit() {
  // Navigation initiation type is only valid for pending navigations, thus
  // always reset to NONE after the item is committed.
  SetNavigationInitiationType(web::NavigationInitiationType::NONE);
}

void NavigationItemImpl::RestoreStateFromItem(NavigationItem* other) {
  // Restore the UserAgent type in any case, as if the URLs are different it
  // might mean that `this` is a next navigation. The page display state and the
  // virtual URL only make sense if it is the same item. The other headers might
  // not make sense after creating a new navigation to the page.
  if (other->GetUserAgentType() != UserAgentType::NONE) {
    SetUserAgentType(other->GetUserAgentType());
  }
  if (url_ == other->GetURL()) {
    SetVirtualURL(other->GetVirtualURL());
  }
}

// static
std::u16string NavigationItemImpl::GetDisplayTitleForURL(const GURL& url) {
  if (url.is_empty())
    return std::u16string();

  std::u16string title = url_formatter::FormatUrl(url);

  // For file:// URLs use the filename as the title, not the full path.
  if (url.SchemeIsFile()) {
    std::u16string::size_type slashpos = title.rfind('/');
    if (slashpos != std::u16string::npos && slashpos != (title.size() - 1))
      title = title.substr(slashpos + 1);
  }

  const size_t kMaxTitleChars = 4 * 1024;
  gfx::ElideString(title, kMaxTitleChars, &title);
  return title;
}

#ifndef NDEBUG
NSString* NavigationItemImpl::GetDescription() const {
  return [NSString
      stringWithFormat:
          @"url:%s virtual_url_:%s originalurl:%s referrer: %s title:%s "
          @"transition:%d userAgent:%s "
           "is_created_from_hash_change: %@ "
           "navigation_initiation_type: %d "
           "https_upgrade_type: %s",
          url_.spec().c_str(), virtual_url_.spec().c_str(),
          original_request_url_.spec().c_str(), referrer_.url.spec().c_str(),
          base::UTF16ToUTF8(title_).c_str(), transition_type_,
          GetUserAgentTypeDescription(user_agent_type_).c_str(),
          is_created_from_hash_change_ ? @"true" : @"false",
          static_cast<int>(navigation_initiation_type_),
          GetHttpsUpgradeTypeDescription(https_upgrade_type_).c_str()];
}
#endif

NavigationItemImpl::NavigationItemImpl(const NavigationItemImpl& item)
    : unique_id_(item.unique_id_),
      original_request_url_(item.original_request_url_),
      url_(item.url_),
      referrer_(item.referrer_),
      virtual_url_(item.virtual_url_),
      title_(item.title_),
      transition_type_(item.transition_type_),
      favicon_status_(item.favicon_status_),
      ssl_(item.ssl_),
      timestamp_(item.timestamp_),
      user_agent_type_(item.user_agent_type_),
      http_request_headers_([item.http_request_headers_ mutableCopy]),
      serialized_state_object_([item.serialized_state_object_ copy]),
      is_created_from_hash_change_(item.is_created_from_hash_change_),
      should_skip_serialization_(item.should_skip_serialization_),
      post_data_([item.post_data_ copy]),
      navigation_initiation_type_(item.navigation_initiation_type_),
      is_untrusted_(item.is_untrusted_),
      cached_display_title_(item.cached_display_title_),
      https_upgrade_type_(item.https_upgrade_type_) {
  CloneDataFrom(item);
}

}  // namespace web
