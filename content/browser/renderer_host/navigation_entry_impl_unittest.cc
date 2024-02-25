// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_entry_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"

using base::ASCIIToUTF16;

namespace content {

namespace {

blink::PageState CreateTestPageState() {
  blink::ExplodedPageState exploded_state;
  std::string encoded_data;
  blink::EncodePageState(exploded_state, &encoded_data);
  return blink::PageState::CreateFromEncodedData(encoded_data);
}

}  // namespace

class NavigationEntryTest : public testing::Test {
 public:
  NavigationEntryTest() : instance_(nullptr) {}

  void SetUp() override {
    entry1_ = std::make_unique<NavigationEntryImpl>();

    const url::Origin kInitiatorOrigin =
        url::Origin::Create(GURL("https://initiator.example.com"));

    instance_ = SiteInstanceImpl::Create(&browser_context_);
    entry2_ = std::make_unique<NavigationEntryImpl>(
        instance_, GURL("test:url"),
        Referrer(GURL("from"), network::mojom::ReferrerPolicy::kDefault),
        kInitiatorOrigin, /* initiator_base_url= */ std::nullopt, u"title",
        ui::PAGE_TRANSITION_TYPED, false, nullptr /* blob_url_loader_factory */,
        false /* is_initial_entry */);
  }

  void TearDown() override {}

 private:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;

 protected:
  // Destructors for SiteInstances must run before |task_environment_| shuts
  // down.
  std::unique_ptr<NavigationEntryImpl> entry1_;
  std::unique_ptr<NavigationEntryImpl> entry2_;
  // SiteInstances are deleted when their NavigationEntries are gone.
  scoped_refptr<SiteInstanceImpl> instance_;
};

// Test unique ID accessors
TEST_F(NavigationEntryTest, NavigationEntryUniqueIDs) {
  // Two entries should have different IDs by default
  EXPECT_NE(entry1_->GetUniqueID(), entry2_->GetUniqueID());

  // Can set an entry to have the same ID as another
  entry2_->set_unique_id(entry1_->GetUniqueID());
  EXPECT_EQ(entry1_->GetUniqueID(), entry2_->GetUniqueID());
}

// Test URL accessors
TEST_F(NavigationEntryTest, NavigationEntryURLs) {
  // Start with no virtual_url (even if a url is set)
  EXPECT_FALSE(entry1_->has_virtual_url());
  EXPECT_FALSE(entry2_->has_virtual_url());

  EXPECT_EQ(GURL(), entry1_->GetURL());
  EXPECT_EQ(GURL(), entry1_->GetVirtualURL());
  EXPECT_TRUE(entry1_->GetTitleForDisplay().empty());

  // Setting URL affects virtual_url and GetTitleForDisplay
  entry1_->SetURL(GURL("http://www.google.com"));
  EXPECT_EQ(GURL("http://www.google.com"), entry1_->GetURL());
  EXPECT_EQ(GURL("http://www.google.com"), entry1_->GetVirtualURL());
  EXPECT_EQ(u"google.com", entry1_->GetTitleForDisplay());

  // https:// should be omitted from displayed titles as well
  entry1_->SetURL(GURL("https://www.chromium.org/robots.txt"));
  EXPECT_EQ(GURL("https://www.chromium.org/robots.txt"), entry1_->GetURL());
  EXPECT_EQ(GURL("https://www.chromium.org/robots.txt"),
            entry1_->GetVirtualURL());
  EXPECT_EQ(u"chromium.org/robots.txt", entry1_->GetTitleForDisplay());

  // Setting URL with RTL characters causes it to be wrapped in an LTR
  // embedding.
  entry1_->SetURL(GURL("http://www.xn--rgba6eo.com"));
  EXPECT_EQ(
      u"\x202a"
      u"\x062c\x0648\x062c\x0644"
      u".com\x202c",
      entry1_->GetTitleForDisplay());

  // file:/// URLs should only show the filename.
  entry1_->SetURL(GURL("file:///foo/bar baz.txt"));
  EXPECT_EQ(u"bar baz.txt", entry1_->GetTitleForDisplay());

  // file:/// URLs should *not* be wrapped in an LTR embedding.
  entry1_->SetURL(GURL("file:///foo/%D8%A7%D8%A8 %D8%AC%D8%AF.txt"));
  EXPECT_EQ(
      u"\x0627\x0628"
      u" \x062c\x062f"
      u".txt",
      entry1_->GetTitleForDisplay());

  // For file:/// URLs, make sure that slashes after the filename are ignored.
  // Regression test for https://crbug.com/503003.
  entry1_->SetURL(GURL("file:///foo/bar baz.txt#foo/bar"));
  EXPECT_EQ(u"bar baz.txt#foo/bar", entry1_->GetTitleForDisplay());
  entry1_->SetURL(GURL("file:///foo/bar baz.txt?x=foo/bar"));
  EXPECT_EQ(u"bar baz.txt?x=foo/bar", entry1_->GetTitleForDisplay());
  entry1_->SetURL(GURL("file:///foo/bar baz.txt#baz/boo?x=foo/bar"));
  EXPECT_EQ(u"bar baz.txt#baz/boo?x=foo/bar", entry1_->GetTitleForDisplay());
  entry1_->SetURL(GURL("file:///foo/bar baz.txt?x=foo/bar#baz/boo"));
  EXPECT_EQ(u"bar baz.txt?x=foo/bar#baz/boo", entry1_->GetTitleForDisplay());
  entry1_->SetURL(GURL("file:///foo/bar baz.txt#foo/bar#baz/boo"));
  EXPECT_EQ(u"bar baz.txt#foo/bar#baz/boo", entry1_->GetTitleForDisplay());
  entry1_->SetURL(GURL("file:///foo/bar baz.txt?x=foo/bar?y=baz/boo"));
  EXPECT_EQ(u"bar baz.txt?x=foo/bar?y=baz/boo", entry1_->GetTitleForDisplay());

  // For chrome-untrusted:// URLs, title is blank.
  entry1_->SetURL(GURL("chrome-untrusted://terminal/html/terminal.html"));
  EXPECT_EQ(std::u16string(), entry1_->GetTitleForDisplay());

  // Title affects GetTitleForDisplay
  entry1_->SetTitle(u"Google");
  EXPECT_EQ(u"Google", entry1_->GetTitleForDisplay());

  // Setting virtual_url doesn't affect URL
  entry2_->SetVirtualURL(GURL("display:url"));
  EXPECT_TRUE(entry2_->has_virtual_url());
  EXPECT_EQ(GURL("test:url"), entry2_->GetURL());
  EXPECT_EQ(GURL("display:url"), entry2_->GetVirtualURL());

  // Having a title set in constructor overrides virtual URL
  EXPECT_EQ(u"title", entry2_->GetTitleForDisplay());

  // User typed URL is independent of the others
  EXPECT_EQ(GURL(), entry1_->GetUserTypedURL());
  EXPECT_EQ(GURL(), entry2_->GetUserTypedURL());
  entry2_->set_user_typed_url(GURL("typedurl"));
  EXPECT_EQ(GURL("typedurl"), entry2_->GetUserTypedURL());
}

// Test Favicon inner class construction.
TEST_F(NavigationEntryTest, NavigationEntryFavicons) {
  EXPECT_EQ(GURL(), entry1_->GetFavicon().url);
  EXPECT_FALSE(entry1_->GetFavicon().valid);
}

// Test SSLStatus inner class
TEST_F(NavigationEntryTest, NavigationEntrySSLStatus) {
  // Default (unknown)
  EXPECT_FALSE(entry1_->GetSSL().initialized);
  EXPECT_FALSE(entry2_->GetSSL().initialized);
  EXPECT_FALSE(!!entry1_->GetSSL().certificate);
  EXPECT_EQ(0U, entry1_->GetSSL().cert_status);
  int content_status = entry1_->GetSSL().content_status;
  EXPECT_FALSE(!!(content_status & SSLStatus::DISPLAYED_INSECURE_CONTENT));
  EXPECT_FALSE(!!(content_status & SSLStatus::RAN_INSECURE_CONTENT));
}

// Test other basic accessors
TEST_F(NavigationEntryTest, NavigationEntryAccessors) {
  // SiteInstance
  EXPECT_EQ(nullptr, entry1_->site_instance());
  EXPECT_EQ(instance_, entry2_->site_instance());

  // Page type
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry1_->GetPageType());
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry2_->GetPageType());
  entry2_->set_page_type(PAGE_TYPE_ERROR);
  EXPECT_EQ(PAGE_TYPE_ERROR, entry2_->GetPageType());

  // Referrer
  EXPECT_EQ(GURL(), entry1_->GetReferrer().url);
  EXPECT_EQ(GURL("from"), entry2_->GetReferrer().url);
  entry2_->SetReferrer(
      Referrer(GURL("from2"), network::mojom::ReferrerPolicy::kDefault));
  EXPECT_EQ(GURL("from2"), entry2_->GetReferrer().url);

  // Title
  EXPECT_EQ(std::u16string(), entry1_->GetTitle());
  EXPECT_EQ(u"title", entry2_->GetTitle());
  entry2_->SetTitle(u"title2");
  EXPECT_EQ(u"title2", entry2_->GetTitle());

  // Transition type
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      entry1_->GetTransitionType(), ui::PAGE_TRANSITION_LINK));
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      entry2_->GetTransitionType(), ui::PAGE_TRANSITION_TYPED));
  entry2_->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      entry2_->GetTransitionType(), ui::PAGE_TRANSITION_RELOAD));

  // Is renderer initiated
  EXPECT_FALSE(entry1_->is_renderer_initiated());
  EXPECT_FALSE(entry2_->is_renderer_initiated());
  entry2_->set_is_renderer_initiated(true);
  EXPECT_TRUE(entry2_->is_renderer_initiated());

  // Post Data
  EXPECT_FALSE(entry1_->GetHasPostData());
  EXPECT_FALSE(entry2_->GetHasPostData());
  entry2_->SetHasPostData(true);
  EXPECT_TRUE(entry2_->GetHasPostData());

  // Restored
  EXPECT_EQ(RestoreType::kNotRestored, entry1_->restore_type());
  EXPECT_FALSE(entry1_->IsRestored());
  EXPECT_EQ(RestoreType::kNotRestored, entry2_->restore_type());
  EXPECT_FALSE(entry2_->IsRestored());
  entry2_->set_restore_type(RestoreType::kRestored);
  EXPECT_EQ(RestoreType::kRestored, entry2_->restore_type());
  EXPECT_TRUE(entry2_->IsRestored());

  // Original URL
  EXPECT_EQ(GURL(), entry1_->GetOriginalRequestURL());
  EXPECT_EQ(GURL(), entry2_->GetOriginalRequestURL());
  entry2_->SetOriginalRequestURL(GURL("original_url"));
  EXPECT_EQ(GURL("original_url"), entry2_->GetOriginalRequestURL());

  // User agent override
  EXPECT_FALSE(entry1_->GetIsOverridingUserAgent());
  EXPECT_FALSE(entry2_->GetIsOverridingUserAgent());
  entry2_->SetIsOverridingUserAgent(true);
  EXPECT_TRUE(entry2_->GetIsOverridingUserAgent());

  // Post data
  EXPECT_FALSE(entry1_->GetPostData());
  EXPECT_FALSE(entry2_->GetPostData());
  const int length = 11;
  const char* raw_data = "post\n\n\0data";
  scoped_refptr<network::ResourceRequestBody> post_data =
      network::ResourceRequestBody::CreateFromBytes(raw_data, length);
  entry2_->SetPostData(post_data);
  EXPECT_EQ(post_data, entry2_->GetPostData());

  // Initiator origin.
  EXPECT_FALSE(
      entry1_->root_node()->frame_entry->initiator_origin().has_value());
  ASSERT_TRUE(
      entry2_->root_node()->frame_entry->initiator_origin().has_value());
  EXPECT_EQ(url::Origin::Create(GURL("https://initiator.example.com")),
            entry2_->root_node()->frame_entry->initiator_origin().value());

  // State.
  //
  // Note that calling SetPageState may also set some other FNE members
  // (referrer, initiator, etc.).  This is why it is important to test
  // SetPageState/GetPageState last.
  blink::PageState test_page_state = CreateTestPageState();
  NavigationEntryRestoreContextImpl context;
  entry2_->SetPageState(test_page_state, &context);
  EXPECT_EQ(test_page_state.ToEncodedData(),
            entry2_->GetPageState().ToEncodedData());
}

// Test basic Clone behavior.
TEST_F(NavigationEntryTest, NavigationEntryClone) {
  // Set some additional values.
  entry2_->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);

  std::unique_ptr<NavigationEntryImpl> clone(entry2_->Clone());

  // Values from FrameNavigationEntry.
  EXPECT_EQ(entry2_->site_instance(), clone->site_instance());
  EXPECT_TRUE(clone->root_node()->frame_entry->initiator_origin().has_value());
  EXPECT_EQ(entry2_->root_node()->frame_entry->initiator_origin(),
            clone->root_node()->frame_entry->initiator_origin());

  // Value from constructor.
  EXPECT_EQ(entry2_->GetTitle(), clone->GetTitle());

  // Value set after constructor.
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      clone->GetTransitionType(), entry2_->GetTransitionType()));
}

// Test timestamps.
TEST_F(NavigationEntryTest, NavigationEntryTimestamps) {
  EXPECT_EQ(base::Time(), entry1_->GetTimestamp());
  const base::Time now = base::Time::Now();
  entry1_->SetTimestamp(now);
  EXPECT_EQ(now, entry1_->GetTimestamp());
}

TEST_F(NavigationEntryTest, SetPageStateWithCorruptedSequenceNumbers) {
  // Create a page state for multiple frames with identical sequence numbers,
  // which ought never happen.
  blink::ExplodedPageState exploded_state;
  blink::ExplodedFrameState child_state;
  exploded_state.top.item_sequence_number = 1234;
  exploded_state.top.document_sequence_number = 5678;
  child_state.target = u"unique_name";
  child_state.item_sequence_number = 1234;
  child_state.document_sequence_number = 5678;
  exploded_state.top.children.push_back(child_state);
  std::string encoded_data;
  blink::EncodePageState(exploded_state, &encoded_data);
  blink::PageState page_state =
      blink::PageState::CreateFromEncodedData(encoded_data);

  NavigationEntryRestoreContextImpl context;
  entry1_->SetPageState(page_state, &context);

  ASSERT_EQ(1u, entry1_->root_node()->children.size());
  EXPECT_NE(entry1_->root_node()->frame_entry.get(),
            entry1_->root_node()->children[0]->frame_entry.get());
}

TEST_F(NavigationEntryTest, SetPageStateWithDefaultSequenceNumbers) {
  blink::PageState page_state1 =
      blink::PageState::CreateFromURL(GURL("http://foo.com"));
  blink::PageState page_state2 =
      blink::PageState::CreateFromURL(GURL("http://bar.com"));

  NavigationEntryRestoreContextImpl context;
  entry1_->SetPageState(page_state1, &context);
  entry2_->SetPageState(page_state2, &context);

  // Because no sequence numbers were set on the PageState objects, they will
  // default to 0.
  EXPECT_EQ(entry1_->root_node()->frame_entry->item_sequence_number(), 0);
  EXPECT_EQ(entry2_->root_node()->frame_entry->item_sequence_number(), 0);
  EXPECT_EQ(entry1_->root_node()->frame_entry->document_sequence_number(), 0);
  EXPECT_EQ(entry2_->root_node()->frame_entry->document_sequence_number(), 0);

  // However, because the item sequence number was the "default" value,
  // NavigationEntryRestoreContext should not have de-duplicated the root
  // FrameNavigationEntries, even though they "match".
  EXPECT_NE(entry1_->root_node()->frame_entry.get(),
            entry2_->root_node()->frame_entry.get());
}

#if BUILDFLAG(IS_ANDROID)
// Failing test, see crbug/1050906.
// Test that content URIs correctly show the file display name as the title.
TEST_F(NavigationEntryTest, DISABLED_NavigationEntryContentUri) {
  base::FilePath image_path;
  EXPECT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &image_path));
  image_path = image_path.Append(FILE_PATH_LITERAL("content"));
  image_path = image_path.Append(FILE_PATH_LITERAL("test"));
  image_path = image_path.Append(FILE_PATH_LITERAL("data"));
  image_path = image_path.Append(FILE_PATH_LITERAL("blank.jpg"));
  EXPECT_TRUE(base::PathExists(image_path));

  base::FilePath content_uri = base::InsertImageIntoMediaStore(image_path);

  entry1_->SetURL(GURL(content_uri.value()));
  EXPECT_EQ(u"blank.jpg", entry1_->GetTitleForDisplay());
}
#endif

}  // namespace content
