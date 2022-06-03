// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "content/public/common/user_agent.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/pepper/url_request_info_util.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/url_request_info_resource.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/test_globals.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/thunk/thunk.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view.h"

// This test is a end-to-end test from the resource to the WebKit request
// object. The actual resource implementation is so simple, it makes sense to
// test it by making sure the conversion routines actually work at the same
// time.

using blink::WebString;
using blink::WebView;
using blink::WebURL;
using blink::WebURLRequest;
using ppapi::proxy::URLRequestInfoResource;
using ppapi::URLRequestInfoData;

namespace content {

class URLRequestInfoTest : public RenderViewTest {
 public:
  // Note: using -1 as the instance value allows code in
  // url_request_info_util.cc to detect that this is a test instance.
  URLRequestInfoTest() : pp_instance_(-1) {}

  void SetUp() override {
    RenderViewTest::SetUp();
    test_globals_.GetResourceTracker()->DidCreateInstance(pp_instance_);

    // This resource doesn't do IPC, so a null connection is fine.
    info_ = new URLRequestInfoResource(
        ppapi::proxy::Connection(), pp_instance_, URLRequestInfoData());
  }

  void TearDown() override {
    test_globals_.GetResourceTracker()->DidDeleteInstance(pp_instance_);
    RenderViewTest::TearDown();
  }

  WebString GetURL() {
    WebURLRequest web_request;
    URLRequestInfoData data = info_->GetData();
    if (!CreateWebURLRequest(pp_instance_, &data, GetMainFrame(), &web_request))
      return WebString();
    return web_request.Url().GetString();
  }

  WebString GetMethod() {
    WebURLRequest web_request;
    URLRequestInfoData data = info_->GetData();
    if (!CreateWebURLRequest(pp_instance_, &data, GetMainFrame(), &web_request))
      return WebString();
    return web_request.HttpMethod();
  }

  blink::mojom::RequestContextType GetContext() {
    WebURLRequest web_request;
    URLRequestInfoData data = info_->GetData();
    if (!CreateWebURLRequest(pp_instance_, &data, GetMainFrame(), &web_request))
      return blink::mojom::RequestContextType::UNSPECIFIED;
    return web_request.GetRequestContext();
  }

  network::mojom::RequestDestination GetDestination() {
    WebURLRequest web_request;
    URLRequestInfoData data = info_->GetData();
    if (!CreateWebURLRequest(pp_instance_, &data, GetMainFrame(), &web_request))
      return network::mojom::RequestDestination::kEmpty;
    return web_request.GetRequestDestination();
  }

  network::mojom::RequestMode GetMode() {
    WebURLRequest web_request;
    URLRequestInfoData data = info_->GetData();
    if (!CreateWebURLRequest(pp_instance_, &data, GetMainFrame(), &web_request))
      return network::mojom::RequestMode::kNavigate;
    return web_request.GetMode();
  }

  WebString GetHeaderValue(const char* field) {
    WebURLRequest web_request;
    URLRequestInfoData data = info_->GetData();
    if (!CreateWebURLRequest(pp_instance_, &data, GetMainFrame(), &web_request))
      return WebString();
    return web_request.HttpHeaderField(WebString::FromUTF8(field));
  }

  bool SetBooleanProperty(PP_URLRequestProperty prop, bool b) {
    return info_->SetBooleanProperty(prop, b);
  }
  bool SetStringProperty(PP_URLRequestProperty prop, const std::string& s) {
    return info_->SetStringProperty(prop, s);
  }

  PP_Instance pp_instance_;

  // Disables locking for the duration of the test.
  ppapi::ProxyLock::LockingDisablerForTest disable_locking_;

  // Needs to be alive for resource tracking to work.
  ppapi::TestGlobals test_globals_;

  scoped_refptr<URLRequestInfoResource> info_;
};

TEST_F(URLRequestInfoTest, GetInterface) {
  const PPB_URLRequestInfo* request_info =
      ppapi::thunk::GetPPB_URLRequestInfo_1_0_Thunk();
  EXPECT_TRUE(request_info);
  EXPECT_TRUE(request_info->Create);
  EXPECT_TRUE(request_info->IsURLRequestInfo);
  EXPECT_TRUE(request_info->SetProperty);
  EXPECT_TRUE(request_info->AppendDataToBody);
  EXPECT_TRUE(request_info->AppendFileToBody);
}

TEST_F(URLRequestInfoTest, AsURLRequestInfo) {
  EXPECT_EQ(info_.get(), info_->AsPPB_URLRequestInfo_API());
}

TEST_F(URLRequestInfoTest, StreamToFile) {
  SetStringProperty(PP_URLREQUESTPROPERTY_URL, "http://www.google.com");

  EXPECT_FALSE(SetBooleanProperty(PP_URLREQUESTPROPERTY_STREAMTOFILE, true));
  EXPECT_FALSE(SetBooleanProperty(PP_URLREQUESTPROPERTY_STREAMTOFILE, false));
}

TEST_F(URLRequestInfoTest, FollowRedirects) {
  EXPECT_TRUE(info_->GetData().follow_redirects);

  EXPECT_TRUE(SetBooleanProperty(PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS, false));
  EXPECT_FALSE(info_->GetData().follow_redirects);

  EXPECT_TRUE(SetBooleanProperty(PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS, true));
  EXPECT_TRUE(info_->GetData().follow_redirects);
}

TEST_F(URLRequestInfoTest, RecordDownloadProgress) {
  EXPECT_FALSE(info_->GetData().record_download_progress);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS, true));
  EXPECT_TRUE(info_->GetData().record_download_progress);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS, false));
  EXPECT_FALSE(info_->GetData().record_download_progress);
}

TEST_F(URLRequestInfoTest, RecordUploadProgress) {
  EXPECT_FALSE(info_->GetData().record_upload_progress);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS, true));
  EXPECT_TRUE(info_->GetData().record_upload_progress);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS, false));
  EXPECT_FALSE(info_->GetData().record_upload_progress);
}

TEST_F(URLRequestInfoTest, AllowCrossOriginRequests) {
  EXPECT_FALSE(info_->GetData().allow_cross_origin_requests);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS, true));
  EXPECT_TRUE(info_->GetData().allow_cross_origin_requests);

  EXPECT_TRUE(SetBooleanProperty(PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS,
                                 false));
  EXPECT_FALSE(info_->GetData().allow_cross_origin_requests);
}

TEST_F(URLRequestInfoTest, AllowCredentials) {
  EXPECT_FALSE(info_->GetData().allow_credentials);

  EXPECT_TRUE(SetBooleanProperty(PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS, true));
  EXPECT_TRUE(info_->GetData().allow_credentials);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS, false));
  EXPECT_FALSE(info_->GetData().allow_credentials);
}

TEST_F(URLRequestInfoTest, SetURL) {
  const char* url = "http://www.google.com/";
  EXPECT_TRUE(SetStringProperty(PP_URLREQUESTPROPERTY_URL, url));
  EXPECT_STREQ(url, GetURL().Utf8().data());
}

TEST_F(URLRequestInfoTest, JavascriptURL) {
  const char* url = "javascript:foo = bar";
  EXPECT_FALSE(URLRequestRequiresUniversalAccess(info_->GetData()));
  SetStringProperty(PP_URLREQUESTPROPERTY_URL, url);
  EXPECT_TRUE(URLRequestRequiresUniversalAccess(info_->GetData()));
}

TEST_F(URLRequestInfoTest, SetMethod) {
  // Test default method is "GET".
  EXPECT_STREQ("GET", GetMethod().Utf8().data());
  EXPECT_TRUE(SetStringProperty(PP_URLREQUESTPROPERTY_METHOD, "POST"));
  EXPECT_STREQ("POST", GetMethod().Utf8().data());
}

TEST_F(URLRequestInfoTest, SetHeaders) {
  // Test default header field.
  EXPECT_STREQ("", GetHeaderValue("foo").Utf8().data());
  // Test that we can set a header field.
  EXPECT_TRUE(SetStringProperty(PP_URLREQUESTPROPERTY_HEADERS, "foo: bar"));
  EXPECT_STREQ("bar", GetHeaderValue("foo").Utf8().data());
  // Test that we can set multiple header fields using \n delimiter.
  EXPECT_TRUE(
      SetStringProperty(PP_URLREQUESTPROPERTY_HEADERS, "foo: bar\nbar: baz"));
  EXPECT_STREQ("bar", GetHeaderValue("foo").Utf8().data());
  EXPECT_STREQ("baz", GetHeaderValue("bar").Utf8().data());
}

TEST_F(URLRequestInfoTest, RequestContextAndDestination) {
  // Test context and destination for PLUGIN.
  EXPECT_EQ(blink::mojom::RequestContextType::PLUGIN, GetContext());
  EXPECT_EQ(network::mojom::RequestDestination::kEmbed, GetDestination());
  EXPECT_EQ(network::mojom::RequestMode::kNoCors, GetMode());
}

// TODO(bbudge) Unit tests for AppendDataToBody, AppendFileToBody.

}  // namespace content
