// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_entry_impl.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/page_state_serialization.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace content {

namespace {

// A test class for testing SSLStatus user data.
class TestSSLStatusData : public SSLStatus::UserData {
 public:
  TestSSLStatusData() {}
  ~TestSSLStatusData() override {}

  void set_user_data_flag(bool user_data_flag) {
    user_data_flag_ = user_data_flag;
  }
  bool user_data_flag() { return user_data_flag_; }

  // SSLStatus implementation:
  std::unique_ptr<SSLStatus::UserData> Clone() override {
    std::unique_ptr<TestSSLStatusData> cloned =
        std::make_unique<TestSSLStatusData>();
    cloned->set_user_data_flag(user_data_flag_);
    return std::move(cloned);
  }

 private:
  bool user_data_flag_ = false;
  DISALLOW_COPY_AND_ASSIGN(TestSSLStatusData);
};

PageState CreateTestPageState() {
  ExplodedPageState exploded_state;
  std::string encoded_data;
  EncodePageState(exploded_state, &encoded_data);
  return PageState::CreateFromEncodedData(encoded_data);
}

}  // namespace

class NavigationEntryTest : public testing::Test {
 public:
  NavigationEntryTest() : instance_(nullptr) {}

  void SetUp() override {
    entry1_.reset(new NavigationEntryImpl);

    const url::Origin kInitiatorOrigin =
        url::Origin::Create(GURL("https://initiator.example.com"));

    instance_ = SiteInstanceImpl::Create(&browser_context_);
    entry2_.reset(new NavigationEntryImpl(
        instance_, GURL("test:url"),
        Referrer(GURL("from"), network::mojom::ReferrerPolicy::kDefault),
        kInitiatorOrigin, ASCIIToUTF16("title"), ui::PAGE_TRANSITION_TYPED,
        false, nullptr /* blob_url_loader_factory */));
  }

  void TearDown() override {}

 protected:
  std::unique_ptr<NavigationEntryImpl> entry1_;
  std::unique_ptr<NavigationEntryImpl> entry2_;
  // SiteInstances are deleted when their NavigationEntries are gone.
  scoped_refptr<SiteInstanceImpl> instance_;

 private:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
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
  EXPECT_EQ(ASCIIToUTF16("www.google.com"), entry1_->GetTitleForDisplay());

  // Setting URL with RTL characters causes it to be wrapped in an LTR
  // embedding.
  entry1_->SetURL(GURL("http://www.xn--rgba6eo.com"));
  EXPECT_EQ(base::WideToUTF16(L"\x202a"
                              L"www.\x062c\x0648\x062c\x0644"
                              L".com\x202c"),
            entry1_->GetTitleForDisplay());

  // file:/// URLs should only show the filename.
  entry1_->SetURL(GURL("file:///foo/bar baz.txt"));
  EXPECT_EQ(ASCIIToUTF16("bar baz.txt"), entry1_->GetTitleForDisplay());

  // file:/// URLs should *not* be wrapped in an LTR embedding.
  entry1_->SetURL(GURL("file:///foo/%D8%A7%D8%A8 %D8%AC%D8%AF.txt"));
  EXPECT_EQ(base::WideToUTF16(L"\x0627\x0628"
                              L" \x062c\x062f"
                              L".txt"),
            entry1_->GetTitleForDisplay());

  // For file:/// URLs, make sure that slashes after the filename are ignored.
  // Regression test for https://crbug.com/503003.
  entry1_->SetURL(GURL("file:///foo/bar baz.txt#foo/bar"));
  EXPECT_EQ(ASCIIToUTF16("bar baz.txt#foo/bar"), entry1_->GetTitleForDisplay());
  entry1_->SetURL(GURL("file:///foo/bar baz.txt?x=foo/bar"));
  EXPECT_EQ(ASCIIToUTF16("bar baz.txt?x=foo/bar"),
            entry1_->GetTitleForDisplay());
  entry1_->SetURL(GURL("file:///foo/bar baz.txt#baz/boo?x=foo/bar"));
  EXPECT_EQ(ASCIIToUTF16("bar baz.txt#baz/boo?x=foo/bar"),
            entry1_->GetTitleForDisplay());
  entry1_->SetURL(GURL("file:///foo/bar baz.txt?x=foo/bar#baz/boo"));
  EXPECT_EQ(ASCIIToUTF16("bar baz.txt?x=foo/bar#baz/boo"),
            entry1_->GetTitleForDisplay());
  entry1_->SetURL(GURL("file:///foo/bar baz.txt#foo/bar#baz/boo"));
  EXPECT_EQ(ASCIIToUTF16("bar baz.txt#foo/bar#baz/boo"),
            entry1_->GetTitleForDisplay());
  entry1_->SetURL(GURL("file:///foo/bar baz.txt?x=foo/bar?y=baz/boo"));
  EXPECT_EQ(ASCIIToUTF16("bar baz.txt?x=foo/bar?y=baz/boo"),
            entry1_->GetTitleForDisplay());

  // For chrome-untrusted:// URLs, title is blank.
  entry1_->SetURL(GURL("chrome-untrusted://terminal/html/terminal.html"));
  EXPECT_EQ(base::string16(), entry1_->GetTitleForDisplay());

  // Title affects GetTitleForDisplay
  entry1_->SetTitle(ASCIIToUTF16("Google"));
  EXPECT_EQ(ASCIIToUTF16("Google"), entry1_->GetTitleForDisplay());

  // Setting virtual_url doesn't affect URL
  entry2_->SetVirtualURL(GURL("display:url"));
  EXPECT_TRUE(entry2_->has_virtual_url());
  EXPECT_EQ(GURL("test:url"), entry2_->GetURL());
  EXPECT_EQ(GURL("display:url"), entry2_->GetVirtualURL());

  // Having a title set in constructor overrides virtual URL
  EXPECT_EQ(ASCIIToUTF16("title"), entry2_->GetTitleForDisplay());

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

// Tests that SSLStatus user data can be added, retrieved, and copied.
TEST_F(NavigationEntryTest, SSLStatusUserData) {
  // Set up an SSLStatus with some user data on it.
  SSLStatus ssl;
  ssl.user_data = std::make_unique<TestSSLStatusData>();
  TestSSLStatusData* ssl_data =
      static_cast<TestSSLStatusData*>(ssl.user_data.get());
  ASSERT_TRUE(ssl_data);
  ssl_data->set_user_data_flag(true);

  // Clone the SSLStatus and test that the user data has been cloned.
  SSLStatus cloned(ssl);
  TestSSLStatusData* cloned_ssl_data =
      static_cast<TestSSLStatusData*>(cloned.user_data.get());
  ASSERT_TRUE(cloned_ssl_data);
  EXPECT_TRUE(cloned_ssl_data->user_data_flag());
  EXPECT_NE(cloned_ssl_data, ssl_data);
}

// Test other basic accessors
TEST_F(NavigationEntryTest, NavigationEntryAccessors) {
  // SiteInstance
  EXPECT_TRUE(entry1_->site_instance() == nullptr);
  EXPECT_EQ(instance_, entry2_->site_instance());
  entry1_->set_site_instance(instance_);
  EXPECT_EQ(instance_, entry1_->site_instance());

  // Page type
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry1_->GetPageType());
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry2_->GetPageType());
  entry2_->set_page_type(PAGE_TYPE_INTERSTITIAL);
  EXPECT_EQ(PAGE_TYPE_INTERSTITIAL, entry2_->GetPageType());

  // Referrer
  EXPECT_EQ(GURL(), entry1_->GetReferrer().url);
  EXPECT_EQ(GURL("from"), entry2_->GetReferrer().url);
  entry2_->SetReferrer(
      Referrer(GURL("from2"), network::mojom::ReferrerPolicy::kDefault));
  EXPECT_EQ(GURL("from2"), entry2_->GetReferrer().url);

  // Title
  EXPECT_EQ(base::string16(), entry1_->GetTitle());
  EXPECT_EQ(ASCIIToUTF16("title"), entry2_->GetTitle());
  entry2_->SetTitle(ASCIIToUTF16("title2"));
  EXPECT_EQ(ASCIIToUTF16("title2"), entry2_->GetTitle());

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
  EXPECT_EQ(RestoreType::NONE, entry1_->restore_type());
  EXPECT_FALSE(entry1_->IsRestored());
  EXPECT_EQ(RestoreType::NONE, entry2_->restore_type());
  EXPECT_FALSE(entry2_->IsRestored());
  entry2_->set_restore_type(RestoreType::LAST_SESSION_EXITED_CLEANLY);
  EXPECT_EQ(RestoreType::LAST_SESSION_EXITED_CLEANLY, entry2_->restore_type());
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
  PageState test_page_state = CreateTestPageState();
  entry2_->SetPageState(test_page_state);
  EXPECT_EQ(test_page_state.ToEncodedData(),
            entry2_->GetPageState().ToEncodedData());
}

// Test basic Clone behavior.
TEST_F(NavigationEntryTest, NavigationEntryClone) {
  // Set some additional values.
  entry2_->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
  entry2_->set_should_replace_entry(true);

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

  // Value not copied due to ResetForCommit.
  EXPECT_NE(entry2_->should_replace_entry(), clone->should_replace_entry());
}

// Test timestamps.
TEST_F(NavigationEntryTest, NavigationEntryTimestamps) {
  EXPECT_EQ(base::Time(), entry1_->GetTimestamp());
  const base::Time now = base::Time::Now();
  entry1_->SetTimestamp(now);
  EXPECT_EQ(now, entry1_->GetTimestamp());
}

#if defined(OS_ANDROID)
// Failing test, see crbug/1050906.
// Test that content URIs correctly show the file display name as the title.
TEST_F(NavigationEntryTest, DISABLED_NavigationEntryContentUri) {
  base::FilePath image_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &image_path));
  image_path = image_path.Append(FILE_PATH_LITERAL("content"));
  image_path = image_path.Append(FILE_PATH_LITERAL("test"));
  image_path = image_path.Append(FILE_PATH_LITERAL("data"));
  image_path = image_path.Append(FILE_PATH_LITERAL("blank.jpg"));
  EXPECT_TRUE(base::PathExists(image_path));

  base::FilePath content_uri = base::InsertImageIntoMediaStore(image_path);

  entry1_->SetURL(GURL(content_uri.value()));
  EXPECT_EQ(ASCIIToUTF16("blank.jpg"), entry1_->GetTitleForDisplay());
}
#endif

}  // namespace content
