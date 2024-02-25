// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/navigation/navigation_util.h"

#import "base/strings/utf_string_conversions.h"
#import "ios/web/navigation/proto_util.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/proto/storage.pb.h"

namespace web {
namespace {

// Serialized the information for the request described by `params`, `title`
// and `creation_time` to protobuf message in `storage`.
void SerializeToNavigationItemStorage(
    const NavigationManager::WebLoadParams& params,
    const std::u16string& title,
    base::Time creation_time,
    UserAgentType user_agent,
    proto::NavigationItemStorage& storage) {
  if (params.url.is_valid()) {
    storage.set_url(params.url.spec());
  }
  if (params.url != params.virtual_url && params.virtual_url.is_valid()) {
    storage.set_virtual_url(params.virtual_url.spec());
  }
  if (!title.empty()) {
    storage.set_title(base::UTF16ToUTF8(title));
  }
  SerializeTimeToProto(creation_time, *storage.mutable_timestamp());
  storage.set_user_agent(UserAgentTypeToProto(user_agent));

  if (params.referrer.url.is_valid()) {
    SerializeReferrerToProto(params.referrer, *storage.mutable_referrer());
  }
  if (params.extra_headers.count) {
    SerializeHttpRequestHeadersToProto(params.extra_headers,
                                       *storage.mutable_http_request_headers());
  }
}

}  // namespace

proto::WebStateStorage CreateWebStateStorage(
    const NavigationManager::WebLoadParams& params,
    const std::u16string& title,
    bool created_with_opener,
    UserAgentType user_agent,
    base::Time creation_time) {
  DCHECK(!params.post_data);
  proto::WebStateStorage storage;

  // Create the NavigationItemStorage.
  SerializeToNavigationItemStorage(params, title, creation_time, user_agent,
                                   *storage.mutable_navigation()->add_items());

  storage.set_has_opener(created_with_opener);
  storage.set_user_agent(UserAgentTypeToProto(user_agent));

  proto::WebStateMetadataStorage& metadata = *storage.mutable_metadata();
  SerializeTimeToProto(creation_time, *metadata.mutable_creation_time());
  SerializeTimeToProto(creation_time, *metadata.mutable_last_active_time());
  metadata.set_navigation_item_count(storage.navigation().items_size());

  const int last_committed_item_index =
      storage.navigation().last_committed_item_index();

  DCHECK_GE(last_committed_item_index, 0);
  DCHECK_LT(last_committed_item_index, metadata.navigation_item_count());
  const proto::NavigationItemStorage& item =
      storage.navigation().items(last_committed_item_index);

  proto::PageMetadataStorage& page_metadata = *metadata.mutable_active_page();
  page_metadata.set_page_title(item.title());

  // Use the virtual URL if set, otherwise defaults to the real URL.
  const std::string& virtual_url = item.virtual_url();
  page_metadata.set_page_url(virtual_url.empty() ? item.url() : virtual_url);

  return storage;
}

}  // namespace web
