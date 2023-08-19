// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_item_impl.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {
namespace {

const char kItemURLString[] = "http://init.test";
static NSString* const kHTTPHeaderKey1 = @"key1";
static NSString* const kHTTPHeaderKey2 = @"key2";
static NSString* const kHTTPHeaderValue1 = @"value1";
static NSString* const kHTTPHeaderValue2 = @"value2";

class NavigationItemTest : public PlatformTest {
 protected:
  void SetUp() override {
    item_.reset(new web::NavigationItemImpl());
    item_->SetOriginalRequestURL(GURL(kItemURLString));
    item_->SetURL(GURL(kItemURLString));
    item_->SetTransitionType(ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    item_->SetTimestamp(base::Time::Now());
    item_->AddHttpRequestHeaders(@{kHTTPHeaderKey1 : kHTTPHeaderValue1});
    item_->SetPostData([@"Test data" dataUsingEncoding:NSUTF8StringEncoding]);
  }

  // The NavigationItemImpl instance being tested.
  std::unique_ptr<NavigationItemImpl> item_;
};

// TODO(rohitrao): Add and adapt tests from NavigationEntryImpl.
TEST_F(NavigationItemTest, Dummy) {
  const GURL url("http://init.test");
  item_->SetURL(url);
  EXPECT_TRUE(item_->GetURL().is_valid());
}

#ifndef NDEBUG
// Tests that the debug description is as expected.
TEST_F(NavigationItemTest, Description) {
  item_->SetTitle(u"Title");
  NSString* description = item_->GetDescription();
  EXPECT_TRUE([description containsString:@"url:http://init.test/"]);
  EXPECT_TRUE([description containsString:@"originalurl:http://init.test/"]);
  EXPECT_TRUE([description containsString:@"title:Title"]);
  EXPECT_TRUE([description containsString:@"transition:2"]);
  EXPECT_TRUE([description containsString:@"userAgent:NONE"]);
  EXPECT_TRUE(
      [description containsString:@"is_created_from_hash_change: false"]);
  EXPECT_TRUE([description containsString:@"navigation_initiation_type: 0"]);
  EXPECT_TRUE([description containsString:@"https_upgrade_type: None"]);
}
#endif

// Tests that cloning NavigationItemImpls create copies of data members that are
// objects.
TEST_F(NavigationItemTest, Clone) {
  // Create objects to be copied.
  NSString* postData0 = @"postData0";
  NSMutableData* mutablePostData =
      [[postData0 dataUsingEncoding:NSUTF8StringEncoding] mutableCopy];
  item_->SetPostData(mutablePostData);
  NSString* state0 = @"state0";
  NSMutableString* mutableState = [state0 mutableCopy];
  item_->SetSerializedStateObject(mutableState);

  // Clone.
  std::unique_ptr<web::NavigationItemImpl> clone = item_->Clone();

  // Modify the objects.
  NSString* postData1 = @"postData1";
  [mutablePostData setData:[postData1 dataUsingEncoding:NSUTF8StringEncoding]];
  NSString* state1 = @"state1";
  [mutableState setString:state1];

  // Check that changes occurred in `item_`, but not in `copy`.
  EXPECT_NSEQ([postData1 dataUsingEncoding:NSUTF8StringEncoding],
              item_->GetPostData());
  EXPECT_NSEQ(state1, item_->GetSerializedStateObject());
  EXPECT_NSEQ([postData0 dataUsingEncoding:NSUTF8StringEncoding],
              clone -> GetPostData());
  EXPECT_NSEQ(state0, clone->GetSerializedStateObject());

  // Ensure that HTTP headers are still mutable after the copying.
  clone->AddHttpRequestHeaders(@{});
}

// Tests whether `NavigationItem::AddHttpRequestHeaders()` adds the passed
// headers to the item's request http headers.
TEST_F(NavigationItemTest, AddHttpRequestHeaders) {
  EXPECT_NSEQ(@{kHTTPHeaderKey1 : kHTTPHeaderValue1},
              item_->GetHttpRequestHeaders());

  item_->AddHttpRequestHeaders(@{kHTTPHeaderKey1 : kHTTPHeaderValue2});
  EXPECT_NSEQ(@{kHTTPHeaderKey1 : kHTTPHeaderValue2},
              item_->GetHttpRequestHeaders());

  item_->AddHttpRequestHeaders(@{kHTTPHeaderKey2 : kHTTPHeaderValue1});
  NSDictionary* expected = @{
    kHTTPHeaderKey1 : kHTTPHeaderValue2,
    kHTTPHeaderKey2 : kHTTPHeaderValue1
  };
  EXPECT_NSEQ(expected, item_->GetHttpRequestHeaders());
}

// Tests whether `NavigationItem::AddHttpRequestHeaders()` removes the header
// value associated with the passed key from the item's request http headers.
TEST_F(NavigationItemTest, RemoveHttpRequestHeaderForKey) {
  NSDictionary* httpHeaders = @{
    kHTTPHeaderKey1 : kHTTPHeaderValue1,
    kHTTPHeaderKey2 : kHTTPHeaderValue2
  };
  item_->AddHttpRequestHeaders(httpHeaders);
  EXPECT_NSEQ(httpHeaders, item_->GetHttpRequestHeaders());

  item_->RemoveHttpRequestHeaderForKey(kHTTPHeaderKey1);
  EXPECT_NSEQ(@{kHTTPHeaderKey2 : kHTTPHeaderValue2},
              item_->GetHttpRequestHeaders());

  item_->RemoveHttpRequestHeaderForKey(kHTTPHeaderKey2);
  EXPECT_FALSE(item_->GetHttpRequestHeaders());
}

// Tests the getter, setter, and cloning for the original request URL.
TEST_F(NavigationItemTest, OriginalURL) {
  GURL original_url = GURL(kItemURLString);
  EXPECT_EQ(original_url, item_->GetOriginalRequestURL());
  std::unique_ptr<web::NavigationItemImpl> clone = item_->Clone();
  GURL new_url = GURL("http://new_url.test");
  item_->SetOriginalRequestURL(new_url);
  EXPECT_EQ(new_url, item_->GetOriginalRequestURL());
  EXPECT_EQ(original_url, clone->GetOriginalRequestURL());
}

// Tests the behavior of GetVirtualURL().
TEST_F(NavigationItemTest, VirtualURLTest) {
  // Ensure that GetVirtualURL() returns GetURL() when not set to a custom
  // value.
  GURL original_url = item_->GetURL();
  EXPECT_EQ(original_url, item_->GetVirtualURL());
  // Set the virtual URL and check that the correct value is reported and that
  // GetURL() still reports the original URL.
  GURL new_virtual_url = GURL("http://new_url.test");
  item_->SetVirtualURL(new_virtual_url);
  EXPECT_EQ(new_virtual_url, item_->GetVirtualURL());
  EXPECT_EQ(original_url, item_->GetURL());
}

// Tests setting title longer than kMaxTitleLength.
TEST_F(NavigationItemTest, ExtraLongTitle) {
  item_->SetTitle(base::UTF8ToUTF16(std::string(kMaxTitleLength + 1, 'i')));
  EXPECT_EQ(kMaxTitleLength, item_->GetTitle().size());
}

// Tests NavigationItemImpl::GetDisplayTitleForURL method.
TEST_F(NavigationItemTest, GetDisplayTitleForURL) {
  std::u16string title;

  title = NavigationItemImpl::GetDisplayTitleForURL(GURL("http://foo.org/"));
  EXPECT_EQ("foo.org", base::UTF16ToUTF8(title));

  title = NavigationItemImpl::GetDisplayTitleForURL(GURL("file://foo.org/"));
  EXPECT_EQ("file://foo.org/", base::UTF16ToUTF8(title));

  title = NavigationItemImpl::GetDisplayTitleForURL(GURL("file://foo/1.gz"));
  EXPECT_EQ("1.gz", base::UTF16ToUTF8(title));
}

// Tests NavigationItemImpl::GetTitleForDisplay method
TEST_F(NavigationItemTest, GetTitleForDisplay) {
  item_->SetURL(GURL("file://foo/test.pdf"));
  item_->SetVirtualURL(GURL("testappspecific://foo/"));
  EXPECT_EQ("test.pdf", base::UTF16ToUTF8(item_->GetTitleForDisplay()));

  item_->SetURL(GURL("testappspecific://foo/test.pdf"));
  item_->SetVirtualURL(GURL("testappspecific://foo/test.pdf"));
  EXPECT_EQ("testappspecific://foo/test.pdf",
            base::UTF16ToUTF8(item_->GetTitleForDisplay()));
}

// Tests that RestoreStateFromItem correctly restore the state.
TEST_F(NavigationItemTest, RestoreState) {
  NavigationItemImpl other_item;
  other_item.SetUserAgentType(UserAgentType::DESKTOP);
  other_item.SetURL(GURL("www.otherurl.com"));
  other_item.SetVirtualURL(GURL("www.virtual.com"));

  ASSERT_NE(other_item.GetURL(), item_->GetURL());

  // With a different URL, only the UserAgent should be restored.
  item_->RestoreStateFromItem(&other_item);
  EXPECT_EQ(other_item.GetUserAgentType(), item_->GetUserAgentType());
  EXPECT_NE(other_item.GetVirtualURL(), item_->GetVirtualURL());

  NavigationItemImpl other_item2;
  other_item2.SetUserAgentType(UserAgentType::DESKTOP);
  other_item2.SetURL(item_->GetURL());
  other_item2.SetVirtualURL(GURL("www.virtual.com"));

  // Same URL, everything is restored.
  item_->RestoreStateFromItem(&other_item2);
  EXPECT_EQ(other_item2.GetUserAgentType(), item_->GetUserAgentType());
  EXPECT_EQ(other_item2.GetVirtualURL(), item_->GetVirtualURL());
}

// Tests that NavigationItemImpl round trip correctly when serialized to proto.
TEST_F(NavigationItemTest, NavigationItemImplRoundTrip) {
  NavigationItemImpl original;
  original.SetURL(GURL("http://url.test"));
  original.SetVirtualURL(GURL("http://virtual.test"));
  original.SetReferrer(
      Referrer(GURL("http://referrer.url"), ReferrerPolicyDefault));
  original.SetTimestamp(base::Time::Now());
  original.SetTitle(u"Title");
  original.SetUserAgentType(UserAgentType::DESKTOP);
  original.AddHttpRequestHeaders(@{@"HeaderKey" : @"HeaderValue"});
  original.SetTransitionType(ui::PAGE_TRANSITION_TYPED);

  proto::NavigationItemStorage storage;
  original.SerializeToProto(storage);

  NavigationItemImpl decoded(storage);

  EXPECT_EQ(original.GetURL(), decoded.GetURL());
  EXPECT_EQ(original.GetVirtualURL(), decoded.GetVirtualURL());
  EXPECT_EQ(original.GetReferrer(), decoded.GetReferrer());
  EXPECT_EQ(original.GetTimestamp(), decoded.GetTimestamp());
  EXPECT_EQ(original.GetUserAgentType(), decoded.GetUserAgentType());
  EXPECT_NSEQ(original.GetHttpRequestHeaders(),
              decoded.GetHttpRequestHeaders());

  // The page transition type should be ui::PAGE_TRANSITION_RELOAD.
  EXPECT_FALSE(ui::PageTransitionCoreTypeIs(original.GetTransitionType(),
                                            decoded.GetTransitionType()));
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_RELOAD,
                                           decoded.GetTransitionType()));
}

// Tests that NavigationItemImpl round trip correctly when serialized to proto
// even when the URL is not an HTTP/HTTPS url.
TEST_F(NavigationItemTest, NavigationItemImplRoundTripNonHTTPURL) {
  NavigationItemImpl original;
  original.SetURL(GURL("file:///path/to/file.pdf"));
  original.SetVirtualURL(GURL("http://virtual.test"));

  proto::NavigationItemStorage storage;
  original.SerializeToProto(storage);

  NavigationItemImpl decoded(storage);

  EXPECT_NE(original.GetURL(), decoded.GetURL());
  EXPECT_EQ(original.GetVirtualURL(), decoded.GetURL());
  EXPECT_EQ(original.GetVirtualURL(), decoded.GetVirtualURL());
}

// Tests that NavigationItemImpl round trip correctly when serialized to proto
// even when the URL is not an HTTP/HTTPS url, in absence of virtual URL.
TEST_F(NavigationItemTest, NavigationItemImplRoundTripNonHTTPURLNoVirtualURL) {
  NavigationItemImpl original;
  original.SetURL(GURL("testwebui://invalid/"));

  proto::NavigationItemStorage storage;
  original.SerializeToProto(storage);

  NavigationItemImpl decoded(storage);

  EXPECT_EQ(original.GetURL(), decoded.GetURL());
  EXPECT_EQ(original.GetVirtualURL(), decoded.GetVirtualURL());
}

// Tests that NavigationItemImpl serialization skips invalid referrer.
TEST_F(NavigationItemTest, SerializationSkipsInvalidReferrer) {
  NavigationItemImpl original;
  original.SetReferrer(Referrer(GURL("invalid"), ReferrerPolicyDefault));

  proto::NavigationItemStorage storage;
  original.SerializeToProto(storage);

  EXPECT_FALSE(storage.has_referrer());
}

// Tests that NavigationItemImpl serialization skips HTTP headers if empty.
TEST_F(NavigationItemTest, SerializationSkipsEmptyHTTPHeaders) {
  NavigationItemImpl original;

  proto::NavigationItemStorage storage;
  original.SerializeToProto(storage);

  EXPECT_FALSE(storage.has_http_request_headers());
}

// Tests that NavigationItemImpl serialization optimizes the serialization
// of URL and virtual URL when both are equals.
TEST_F(NavigationItemTest, SerializationOptimizesURLStorage) {
  NavigationItemImpl original;
  // Set virtual URL before URL because SetVirtualURL() also optimize the
  // storage of the URLs in NavigationItemImpl by clearing virtual_url_
  // if the value set is equal to url_.
  original.SetVirtualURL(GURL("http://url.test"));
  original.SetURL(GURL("http://url.test"));

  proto::NavigationItemStorage storage;
  original.SerializeToProto(storage);

  EXPECT_FALSE(storage.url().empty());
  EXPECT_TRUE(storage.virtual_url().empty());
}

// Tests correct decoding of the URL and virtual URL when using http: scheme.
TEST_F(NavigationItemTest, DecodeHTTPScheme) {
  web::proto::NavigationItemStorage storage;
  storage.set_url("http://url.test");
  storage.set_virtual_url("http://virtual.test");
  ASSERT_NE(storage.url(), storage.virtual_url());

  NavigationItemImpl navigation_item(storage);
  EXPECT_EQ(GURL(storage.url()), navigation_item.GetURL());
  EXPECT_EQ(GURL(storage.virtual_url()), navigation_item.GetVirtualURL());
}

// Tests correct decoding of the URL and virtual URL when using file: scheme.
TEST_F(NavigationItemTest, DecodeFileScheme) {
  web::proto::NavigationItemStorage storage;
  storage.set_url("file://myfile.test");
  storage.set_virtual_url("http://virtual.test");
  ASSERT_NE(storage.url(), storage.virtual_url());

  NavigationItemImpl navigation_item(storage);
  EXPECT_EQ(GURL(storage.virtual_url()), navigation_item.GetURL());
  EXPECT_EQ(GURL(storage.virtual_url()), navigation_item.GetVirtualURL());
}

// Tests correct decoding of the URL and virtual URL when using blob: scheme.
TEST_F(NavigationItemTest, DecodeBlobScheme) {
  web::proto::NavigationItemStorage storage;
  storage.set_url("blob:myfile.test");
  storage.set_virtual_url("http://virtual.test");
  ASSERT_NE(storage.url(), storage.virtual_url());

  NavigationItemImpl navigation_item(storage);
  EXPECT_EQ(GURL(storage.virtual_url()), navigation_item.GetURL());
  EXPECT_EQ(GURL(storage.virtual_url()), navigation_item.GetVirtualURL());
}

}  // namespace
}  // namespace web
