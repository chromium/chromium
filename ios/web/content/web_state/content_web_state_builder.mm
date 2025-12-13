// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/web_state/content_web_state_builder.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "content/public/browser/navigation_controller.h"
#import "content/public/browser/navigation_entry.h"
#import "content/public/browser/restore_type.h"
#import "ios/web/content/content_browser_context.h"
#import "ios/web/content/web_state/content_web_state.h"
#import "ios/web/navigation/proto_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "net/http/http_util.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace web {

void ExtractContentSessionStorage(ContentWebState* web_state,
                                  content::NavigationController& controller,
                                  BrowserState* browser_state,
                                  proto::WebStateStorage storage) {
  content::BrowserContext* browser_context =
      ContentBrowserContext::FromBrowserState(browser_state);

  web_state->SetHasOpener(storage.has_opener());
  std::vector<std::unique_ptr<content::NavigationEntry>> items;
  int last_committed_item_index = -1;

  if (storage.has_navigation()) {
    const proto::NavigationStorage& navigation = storage.navigation();
    items.reserve(static_cast<size_t>(navigation.items_size()));
    last_committed_item_index = navigation.last_committed_item_index();

    for (const proto::NavigationItemStorage& item : navigation.items()) {
      GURL url = GURL(item.url());
      GURL virtual_url = GURL(item.virtual_url());
      if (virtual_url.is_empty()) {
        virtual_url = url;
      }

      std::unique_ptr<content::NavigationEntry> new_entry =
          content::NavigationController::CreateNavigationEntry(
              /* url= */ virtual_url,
              /* referrer= */ content::Referrer(),
              /* initiator_origin= */ std::nullopt,
              /* initiator_base_url= */ std::nullopt,
              /* transition= */ ui::PAGE_TRANSITION_RELOAD,
              /* is_renderer_initiated= */ false,
              /* extra_headers= */ std::string(), browser_context,
              /* blub_url_loader_factory= */ nullptr);

      new_entry->SetOriginalRequestURL(url);
      if (url.SchemeIsHTTPOrHTTPS()) {
        new_entry->SetURL(url);
        new_entry->SetVirtualURL(virtual_url);
      } else {
        new_entry->SetURL(virtual_url);
      }

      new_entry->SetTimestamp(TimeFromProto(item.timestamp()));
      new_entry->SetTitle(base::UTF8ToUTF16(item.title()));
      new_entry->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);

      if (item.has_http_request_headers()) {
        std::ostringstream buffer;
        for (const proto::HttpHeaderStorage& header :
             item.http_request_headers().headers()) {
          if (!net::HttpUtil::IsValidHeaderName(header.name()) ||
              !net::HttpUtil::IsValidHeaderValue(header.value())) {
            continue;
          }

          buffer << header.name() << ": " << header.value() << "\r\n";
        }
        const std::string extra_headers = buffer.str();
        if (!extra_headers.empty()) {
          new_entry->AddExtraHeaders(extra_headers);
        }
      }

      GURL rewritten_url = url;
      if (BrowserURLRewriter::GetInstance()->RewriteURLIfNecessary(
              &rewritten_url, browser_state)) {
        new_entry->SetURL(rewritten_url);
        new_entry->SetVirtualURL(url);
      }

      // Content doesn't allow about://newtab
      if (rewritten_url.possibly_invalid_spec() == "about://newtab/") {
        new_entry->SetURL(GURL("chrome://newtab"));
      }

      items.push_back(std::move(new_entry));
    }

    if (last_committed_item_index < 0 ||
        static_cast<size_t>(last_committed_item_index) >= items.size()) {
      last_committed_item_index = static_cast<int>(items.size()) - 1;
    }
  }

  controller.Restore(last_committed_item_index, content::RestoreType::kRestored,
                     &items);
}

void SerializeContentStorage(const ContentWebState* web_state,
                             const ContentNavigationManager* navigation_manager,
                             proto::WebStateStorage& storage) {
  const int navigation_items = navigation_manager->GetItemCount();
  int last_committed_item_index =
      navigation_manager->GetLastCommittedItemIndex();
  if (last_committed_item_index == -1) {
    // This can happen when a session is saved during restoration. Instead,
    // default to GetItemCount() - 1.
    last_committed_item_index = navigation_items - 1;
  }

  const int original_index = last_committed_item_index;
  proto::NavigationStorage& navigation_storage = *storage.mutable_navigation();
  for (int index = 0; index < navigation_items; ++index) {
    const NavigationItem* item = navigation_manager->GetItemAtIndex(index);
    if (item->GetURL().spec().size() > url::kMaxURLChars) {
      if (index < original_index) {
        --last_committed_item_index;
      }
      continue;
    }

    proto::NavigationItemStorage& item_storage =
        *navigation_storage.add_items();
    item_storage.set_virtual_url(item->GetVirtualURL().spec());
    item_storage.set_url(item->GetURL().spec());

    // Use default referrer if URL is longer than allowed. Navigation items with
    // these long URLs will not be serialized, so there is no point in keeping
    // referrer URL.
    const Referrer& referrer = item->GetReferrer();
    if (referrer.url.is_valid() &&
        referrer.url.spec().size() <= url::kMaxURLChars) {
      SerializeReferrerToProto(referrer, *item_storage.mutable_referrer());
    }

    SerializeTimeToProto(item->GetTimestamp(),
                         *item_storage.mutable_timestamp());
    item_storage.set_title(base::UTF16ToUTF8(item->GetTitle()));
    item_storage.set_user_agent(UserAgentTypeToProto(item->GetUserAgentType()));

    NavigationItem::HttpRequestHeaders* headers = item->GetHttpRequestHeaders();
    if (headers.count > 0) {
      SerializeHttpRequestHeadersToProto(
          headers, *item_storage.mutable_http_request_headers());
    }
  }

  navigation_storage.set_last_committed_item_index(last_committed_item_index);

  proto::WebStateMetadataStorage& metadata = *storage.mutable_metadata();
  metadata.set_navigation_item_count(navigation_storage.items_size());
  if (0 <= last_committed_item_index &&
      last_committed_item_index < navigation_storage.items_size()) {
    const proto::NavigationItemStorage& item_storage =
        navigation_storage.items(last_committed_item_index);

    proto::PageMetadataStorage& active_page = *metadata.mutable_active_page();
    active_page.set_page_title(item_storage.title());
    if (item_storage.virtual_url().empty()) {
      active_page.set_page_url(item_storage.url());
    } else {
      active_page.set_page_url(item_storage.virtual_url());
    }
  }

  SerializeTimeToProto(web_state->GetCreationTime(),
                       *metadata.mutable_creation_time());
  SerializeTimeToProto(web_state->GetLastActiveTime(),
                       *metadata.mutable_last_active_time());
}

}  // namespace web
