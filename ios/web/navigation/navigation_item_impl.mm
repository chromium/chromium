// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_item_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/web_client.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/text_elider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a new unique ID for use in NavigationItem during construction.  The
// returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
static int GetUniqueIDInConstructor() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

}  // namespace

namespace web {

// static
std::unique_ptr<NavigationItem> NavigationItem::Create() {
  return std::unique_ptr<NavigationItem>(new NavigationItemImpl());
}

NavigationItemImpl::NavigationItemImpl()
    : unique_id_(GetUniqueIDInConstructor()),
      transition_type_(ui::PAGE_TRANSITION_LINK),
      user_agent_type_(UserAgentType::MOBILE),
      is_created_from_push_state_(false),
      has_state_been_replaced_(false),
      is_created_from_hash_change_(false),
      should_skip_repost_form_confirmation_(false),
      should_skip_serialization_(false),
      navigation_initiation_type_(web::NavigationInitiationType::NONE),
      is_untrusted_(false) {}

NavigationItemImpl::~NavigationItemImpl() {
}

NavigationItemImpl::NavigationItemImpl(const NavigationItemImpl& item)
    : unique_id_(item.unique_id_),
      original_request_url_(item.original_request_url_),
      url_(item.url_),
      referrer_(item.referrer_),
      virtual_url_(item.virtual_url_),
      title_(item.title_),
      page_display_state_(item.page_display_state_),
      transition_type_(item.transition_type_),
      favicon_(item.favicon_),
      ssl_(item.ssl_),
      timestamp_(item.timestamp_),
      user_agent_type_(item.user_agent_type_),
      http_request_headers_([item.http_request_headers_ mutableCopy]),
      serialized_state_object_([item.serialized_state_object_ copy]),
      is_created_from_push_state_(item.is_created_from_push_state_),
      has_state_been_replaced_(item.has_state_been_replaced_),
      is_created_from_hash_change_(item.is_created_from_hash_change_),
      should_skip_repost_form_confirmation_(
          item.should_skip_repost_form_confirmation_),
      should_skip_serialization_(item.should_skip_serialization_),
      post_data_([item.post_data_ copy]),
      error_retry_state_machine_(item.error_retry_state_machine_),
      navigation_initiation_type_(item.navigation_initiation_type_),
      is_untrusted_(item.is_untrusted_),
      cached_display_title_(item.cached_display_title_) {}

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
  error_retry_state_machine_.SetURL(url);
  if (!wk_navigation_util::URLNeedsUserAgentType(url)) {
    SetUserAgentType(web::UserAgentType::NONE);
  } else if (GetUserAgentType() == web::UserAgentType::NONE) {
    SetUserAgentType(web::UserAgentType::MOBILE);
  }
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

void NavigationItemImpl::SetTitle(const base::string16& title) {
  if (title_ == title)
    return;
  title_ = title;
  cached_display_title_.clear();
}

const base::string16& NavigationItemImpl::GetTitle() const {
  return title_;
}

void NavigationItemImpl::SetPageDisplayState(
    const web::PageDisplayState& display_state) {
  page_display_state_ = display_state;
}

const PageDisplayState& NavigationItemImpl::GetPageDisplayState() const {
  return page_display_state_;
}

const base::string16& NavigationItemImpl::GetTitleForDisplay() const {
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

const FaviconStatus& NavigationItemImpl::GetFavicon() const {
  return favicon_;
}

FaviconStatus& NavigationItemImpl::GetFavicon() {
  return favicon_;
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
  DCHECK_EQ(!wk_navigation_util::URLNeedsUserAgentType(GetURL()),
            user_agent_type_ == UserAgentType::NONE);
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

NSDictionary* NavigationItemImpl::GetHttpRequestHeaders() const {
  return [http_request_headers_ copy];
}

void NavigationItemImpl::AddHttpRequestHeaders(
    NSDictionary* additional_headers) {
  if (!additional_headers)
    return;

  if (http_request_headers_)
    [http_request_headers_ addEntriesFromDictionary:additional_headers];
  else
    http_request_headers_ = [additional_headers mutableCopy];
}

void NavigationItemImpl::SetSerializedStateObject(
    NSString* serialized_state_object) {
  serialized_state_object_ = serialized_state_object;
}

NSString* NavigationItemImpl::GetSerializedStateObject() const {
  return serialized_state_object_;
}

void NavigationItemImpl::SetIsCreatedFromPushState(bool push_state) {
  is_created_from_push_state_ = push_state;
}

bool NavigationItemImpl::IsCreatedFromPushState() const {
  return is_created_from_push_state_;
}

void NavigationItemImpl::SetNavigationInitiationType(
    web::NavigationInitiationType navigation_initiation_type) {
  navigation_initiation_type_ = navigation_initiation_type;
}

web::NavigationInitiationType NavigationItemImpl::NavigationInitiationType()
    const {
  return navigation_initiation_type_;
}

void NavigationItemImpl::SetHasStateBeenReplaced(bool replace_state) {
  has_state_been_replaced_ = replace_state;
}

bool NavigationItemImpl::HasStateBeenReplaced() const {
  return has_state_been_replaced_;
}

void NavigationItemImpl::SetIsCreatedFromHashChange(bool hash_change) {
  is_created_from_hash_change_ = hash_change;
}

bool NavigationItemImpl::IsCreatedFromHashChange() const {
  return is_created_from_hash_change_;
}

void NavigationItemImpl::SetShouldSkipRepostFormConfirmation(bool skip) {
  should_skip_repost_form_confirmation_ = skip;
}

bool NavigationItemImpl::ShouldSkipRepostFormConfirmation() const {
  return should_skip_repost_form_confirmation_;
}

void NavigationItemImpl::SetShouldSkipSerialization(bool skip) {
  should_skip_serialization_ = skip;
}

bool NavigationItemImpl::ShouldSkipSerialization() const {
  return should_skip_serialization_;
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

ErrorRetryStateMachine& NavigationItemImpl::error_retry_state_machine() {
  return error_retry_state_machine_;
}

// static
base::string16 NavigationItemImpl::GetDisplayTitleForURL(const GURL& url) {
  if (url.is_empty())
    return base::string16();

  base::string16 title = url_formatter::FormatUrl(url);

  // For file:// URLs use the filename as the title, not the full path.
  if (url.SchemeIsFile()) {
    base::string16::size_type slashpos = title.rfind('/');
    if (slashpos != base::string16::npos && slashpos != (title.size() - 1))
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
          @"transition:%d "
           "displayState:%@ userAgentType:%s is_create_from_push_state: %@ "
           "has_state_been_replaced: %@ is_created_from_hash_change: %@ "
           "navigation_initiation_type: %d",
          url_.spec().c_str(), virtual_url_.spec().c_str(),
          original_request_url_.spec().c_str(), referrer_.url.spec().c_str(),
          base::UTF16ToUTF8(title_).c_str(), transition_type_,
          page_display_state_.GetDescription(),
          GetUserAgentTypeDescription(user_agent_type_).c_str(),
          is_created_from_push_state_ ? @"true" : @"false",
          has_state_been_replaced_ ? @"true" : @"false",
          is_created_from_hash_change_ ? @"true" : @"false",
          navigation_initiation_type_];
}
#endif

}  // namespace web
