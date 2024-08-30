// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/test_content_client.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const int kDummyStringId = 123;
const int kDummyDefaultResourceId = 456;
const int kDummyResourceId = 789;
const int kDummyJSResourceId = 790;

const char16_t kDummyString[] = u"foo";
const char kDummyDefaultResource[] = "<html>foo</html>";
const char kDummyResource[] = "<html>blah</html>";
const char kDummyJSResource[] = "export const bar = 5;";

class TestClient : public TestContentClient {
 public:
  ~TestClient() override {}

  std::u16string GetLocalizedString(int message_id) override {
    if (message_id == kDummyStringId)
      return kDummyString;
    return std::u16string();
  }

  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override {
    base::RefCountedStaticMemory* bytes = nullptr;
    if (resource_id == kDummyDefaultResourceId) {
      bytes = new base::RefCountedStaticMemory(
          base::byte_span_with_nul_from_cstring(kDummyDefaultResource));
    } else if (resource_id == kDummyResourceId) {
      bytes = new base::RefCountedStaticMemory(
          base::byte_span_with_nul_from_cstring(kDummyResource));
    } else if (resource_id == kDummyJSResourceId) {
      bytes = new base::RefCountedStaticMemory(
          base::byte_span_with_nul_from_cstring(kDummyJSResource));
    }
    return bytes;
  }
};

}  // namespace

class WebUIDataSourceTest : public testing::Test {
 public:
  WebUIDataSourceTest() {}
  ~WebUIDataSourceTest() override {}
  WebUIDataSourceImpl* source() { return source_.get(); }

  void StartDataRequest(const std::string& path,
                        URLDataSource::GotDataCallback callback) {
    source_->StartDataRequest(GURL("https://any-host/" + path),
                              WebContents::Getter(), std::move(callback));
  }

  std::string GetMimeTypeForPath(const std::string& path) const {
    return source_->GetMimeType(GURL("https://any-host/" + path));
  }

  void HandleRequest(const std::string& path,
                     WebUIDataSourceImpl::GotDataCallback) {
    request_path_ = path;
  }

  void RequestFilterQueryStringCallback(
      scoped_refptr<base::RefCountedMemory> data);

 protected:
  std::string request_path_;
  TestClient client_;

 private:
  void SetUp() override {
    SetContentClient(&client_);
    WebUIDataSourceImpl* source = new WebUIDataSourceImpl("host");
    source->disable_load_time_data_defaults_for_testing();
    source_ = base::WrapRefCounted(source);
  }

  BrowserTaskEnvironment task_environment_;
  scoped_refptr<WebUIDataSourceImpl> source_;
};

void EmptyStringsCallback(bool from_js_module,
                          scoped_refptr<base::RefCountedMemory> data) {
  std::string result(base::as_string_view(*data));
  EXPECT_NE(result.find("loadTimeData.data = {"), std::string::npos);
  EXPECT_NE(result.find("};"), std::string::npos);
  bool has_import = result.find("import {loadTimeData}") != std::string::npos;
  EXPECT_EQ(from_js_module, has_import);
}

TEST_F(WebUIDataSourceTest, EmptyStrings) {
  source()->UseStringsJs();
  StartDataRequest("strings.js", base::BindOnce(&EmptyStringsCallback, false));
}

TEST_F(WebUIDataSourceTest, EmptyModuleStrings) {
  source()->UseStringsJs();
  StartDataRequest("strings.m.js", base::BindOnce(&EmptyStringsCallback, true));
}

void SomeValuesCallback(scoped_refptr<base::RefCountedMemory> data) {
  std::string result(base::as_string_view(*data));
  EXPECT_NE(result.find("\"flag\":true"), std::string::npos);
  EXPECT_NE(result.find("\"counter\":10"), std::string::npos);
  EXPECT_NE(result.find("\"debt\":-456"), std::string::npos);
  EXPECT_NE(result.find("\"threshold\":0.55"), std::string::npos);
  EXPECT_NE(result.find("\"planet\":\"pluto\""), std::string::npos);
  EXPECT_NE(result.find("\"button\":\"foo\""), std::string::npos);
}

TEST_F(WebUIDataSourceTest, SomeValues) {
  source()->UseStringsJs();
  source()->AddBoolean("flag", true);
  source()->AddInteger("counter", 10);
  source()->AddInteger("debt", -456);
  source()->AddDouble("threshold", 0.55);
  source()->AddString("planet", u"pluto");
  source()->AddLocalizedString("button", kDummyStringId);
  StartDataRequest("strings.js", base::BindOnce(&SomeValuesCallback));
}

void DefaultResourceFoobarCallback(scoped_refptr<base::RefCountedMemory> data) {
  std::string result(base::as_string_view(*data));
  EXPECT_NE(result.find(kDummyDefaultResource), std::string::npos);
}

void DefaultResourceStringsCallback(
    scoped_refptr<base::RefCountedMemory> data) {
  std::string result(base::as_string_view(*data));
  EXPECT_NE(result.find(kDummyDefaultResource), std::string::npos);
}

TEST_F(WebUIDataSourceTest, DefaultResource) {
  source()->SetDefaultResource(kDummyDefaultResourceId);
  StartDataRequest("foobar", base::BindOnce(&DefaultResourceFoobarCallback));
  StartDataRequest("strings.js",
                   base::BindOnce(&DefaultResourceStringsCallback));
}

void NamedResourceFoobarCallback(scoped_refptr<base::RefCountedMemory> data) {
  std::string result(base::as_string_view(*data));
  EXPECT_NE(result.find(kDummyResource), std::string::npos);
}

void NamedResourceStringsCallback(scoped_refptr<base::RefCountedMemory> data) {
  std::string result(base::as_string_view(*data));
  EXPECT_NE(result.find(kDummyDefaultResource), std::string::npos);
}

TEST_F(WebUIDataSourceTest, NamedResource) {
  source()->SetDefaultResource(kDummyDefaultResourceId);
  source()->AddResourcePath("foobar", kDummyResourceId);
  StartDataRequest("foobar", base::BindOnce(&NamedResourceFoobarCallback));
  StartDataRequest("strings.js", base::BindOnce(&NamedResourceStringsCallback));
}

void NamedResourceWithQueryStringCallback(
    scoped_refptr<base::RefCountedMemory> data) {
  std::string result(base::as_string_view(*data));
  EXPECT_NE(result.find(kDummyResource), std::string::npos);
}

TEST_F(WebUIDataSourceTest, NamedResourceWithQueryString) {
  source()->SetDefaultResource(kDummyDefaultResourceId);
  source()->AddResourcePath("foobar", kDummyResourceId);
  StartDataRequest("foobar?query?string",
                   base::BindOnce(&NamedResourceWithQueryStringCallback));
}

void NamedResourceWithUrlFragmentCallback(
    scoped_refptr<base::RefCountedMemory> data) {
  EXPECT_NE(data, nullptr);
  std::string result(base::as_string_view(*data));
  EXPECT_NE(result.find(kDummyResource), std::string::npos);
}

TEST_F(WebUIDataSourceTest, NamedResourceWithUrlFragment) {
  source()->AddResourcePath("foobar", kDummyResourceId);
  StartDataRequest("foobar#fragment",
                   base::BindOnce(&NamedResourceWithUrlFragmentCallback));
}

void WebUIDataSourceTest::RequestFilterQueryStringCallback(
    scoped_refptr<base::RefCountedMemory> data) {
  std::string result(base::as_string_view(*data));
  // Check that the query string is passed to the request filter (and not
  // trimmed).
  EXPECT_EQ("foobar?query?string", request_path_);
}

TEST_F(WebUIDataSourceTest, RequestFilterQueryString) {
  request_path_ = std::string();
  source()->SetRequestFilter(
      base::BindRepeating([](const std::string& path) { return true; }),
      base::BindRepeating(&WebUIDataSourceTest::HandleRequest,
                          base::Unretained(this)));
  source()->SetDefaultResource(kDummyDefaultResourceId);
  source()->AddResourcePath("foobar", kDummyResourceId);
  StartDataRequest(
      "foobar?query?string",
      base::BindOnce(&WebUIDataSourceTest::RequestFilterQueryStringCallback,
                     base::Unretained(this)));
}

TEST_F(WebUIDataSourceTest, MimeType) {
  const char* css = "text/css";
  const char* html = "text/html";
  const char* js = "application/javascript";
  const char* jpg = "image/jpeg";
  const char* json = "application/json";
  const char* mp4 = "video/mp4";
  const char* pdf = "application/pdf";
  const char* png = "image/png";
  const char* svg = "image/svg+xml";
  const char* wasm = "application/wasm";
  const char* woff2 = "application/font-woff2";

  EXPECT_EQ(GetMimeTypeForPath(std::string()), html);
  EXPECT_EQ(GetMimeTypeForPath("foo"), html);
  EXPECT_EQ(GetMimeTypeForPath("foo.html"), html);
  EXPECT_EQ(GetMimeTypeForPath(".js"), js);
  EXPECT_EQ(GetMimeTypeForPath("foo.js"), js);
  EXPECT_EQ(GetMimeTypeForPath("js"), html);
  EXPECT_EQ(GetMimeTypeForPath("foojs"), html);
  EXPECT_EQ(GetMimeTypeForPath("foo.jsp"), html);
  EXPECT_EQ(GetMimeTypeForPath("foocss"), html);
  EXPECT_EQ(GetMimeTypeForPath("foo.css"), css);
  EXPECT_EQ(GetMimeTypeForPath(".css.foo"), html);
  EXPECT_EQ(GetMimeTypeForPath("foopng"), html);
  EXPECT_EQ(GetMimeTypeForPath("foo.png"), png);
  EXPECT_EQ(GetMimeTypeForPath(".png.foo"), html);
  EXPECT_EQ(GetMimeTypeForPath("foo.svg"), svg);
  EXPECT_EQ(GetMimeTypeForPath("foo.js.wasm"), wasm);
  EXPECT_EQ(GetMimeTypeForPath("foo.out.wasm"), wasm);
  EXPECT_EQ(GetMimeTypeForPath(".woff2"), woff2);
  EXPECT_EQ(GetMimeTypeForPath("foo.json"), json);
  EXPECT_EQ(GetMimeTypeForPath("foo.pdf"), pdf);
  EXPECT_EQ(GetMimeTypeForPath("foo.jpg"), jpg);
  EXPECT_EQ(GetMimeTypeForPath("foo.mp4"), mp4);

  // With query strings.
  EXPECT_EQ(GetMimeTypeForPath("foo?abc?abc"), html);
  EXPECT_EQ(GetMimeTypeForPath("foo.html?abc?abc"), html);
  EXPECT_EQ(GetMimeTypeForPath("foo.css?abc?abc"), css);
  EXPECT_EQ(GetMimeTypeForPath("foo.js?abc?abc"), js);
  EXPECT_EQ(GetMimeTypeForPath("foo.svg?abc?abc"), svg);

  // With URL fragments.
  EXPECT_EQ(GetMimeTypeForPath("foo#abc#abc"), html);
  EXPECT_EQ(GetMimeTypeForPath("foo.html#abc#abc"), html);
  EXPECT_EQ(GetMimeTypeForPath("foo.css#abc#abc"), css);
  EXPECT_EQ(GetMimeTypeForPath("foo.js#abc#abc"), js);
  EXPECT_EQ(GetMimeTypeForPath("foo.svg#abc#abc"), svg);

  // With query strings and URL fragments.
  EXPECT_EQ(GetMimeTypeForPath("foo?abc#abc"), html);
  EXPECT_EQ(GetMimeTypeForPath("foo.html?abc#abc"), html);
  EXPECT_EQ(GetMimeTypeForPath("foo.css?abc#abc"), css);
  EXPECT_EQ(GetMimeTypeForPath("foo.js?abc#abc"), js);
  EXPECT_EQ(GetMimeTypeForPath("foo.svg?abc#abc"), svg);
}

TEST_F(WebUIDataSourceTest, ShouldServeMimeTypeAsContentTypeHeader) {
  EXPECT_TRUE(source()->source()->ShouldServeMimeTypeAsContentTypeHeader());
}

void InvalidResourceCallback(scoped_refptr<base::RefCountedMemory> data) {
  EXPECT_EQ(nullptr, data);
}

void NamedResourceBarJSCallback(scoped_refptr<base::RefCountedMemory> data) {
  std::string result(base::as_string_view(*data));
  EXPECT_NE(result.find(kDummyJSResource), std::string::npos);
}

TEST_F(WebUIDataSourceTest, NoSetDefaultResource) {
  // Set an empty path resource instead of a default.
  source()->AddResourcePath("", kDummyDefaultResourceId);
  source()->AddResourcePath("foobar.html", kDummyResourceId);
  source()->AddResourcePath("bar.js", kDummyJSResourceId);

  // Empty paths return the resource for the empty path.
  StartDataRequest("", base::BindOnce(&DefaultResourceFoobarCallback));
  StartDataRequest("/", base::BindOnce(&DefaultResourceFoobarCallback));
  // Un-mapped path that does not look like a file request also returns the
  // resource associated with the empty path.
  StartDataRequest("subpage", base::BindOnce(&DefaultResourceFoobarCallback));
  // Paths that are valid filenames succeed and return the file contents.
  StartDataRequest("foobar.html", base::BindOnce(&NamedResourceFoobarCallback));
  StartDataRequest("bar.js", base::BindOnce(&NamedResourceBarJSCallback));
  // Invalid file requests fail
  StartDataRequest("does_not_exist.html",
                   base::BindOnce(&InvalidResourceCallback));
  StartDataRequest("does_not_exist.js",
                   base::BindOnce(&InvalidResourceCallback));
  StartDataRequest("does_not_exist.ts",
                   base::BindOnce(&InvalidResourceCallback));

  // strings.m.js fails until UseStringsJs is called.
  StartDataRequest("strings.m.js", base::BindOnce(&InvalidResourceCallback));
  source()->UseStringsJs();
  StartDataRequest("strings.m.js", base::BindOnce(&EmptyStringsCallback, true));
}

TEST_F(WebUIDataSourceTest, SetCspValues) {
  URLDataSource* url_data_source = source()->source();

  // Default values.
  EXPECT_EQ("child-src 'none';",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::ChildSrc));
  EXPECT_EQ("", url_data_source->GetContentSecurityPolicy(
                    network::mojom::CSPDirectiveName::ConnectSrc));
  EXPECT_EQ("", url_data_source->GetContentSecurityPolicy(
                    network::mojom::CSPDirectiveName::DefaultSrc));
  EXPECT_EQ("", url_data_source->GetContentSecurityPolicy(
                    network::mojom::CSPDirectiveName::FrameSrc));
  EXPECT_EQ("", url_data_source->GetContentSecurityPolicy(
                    network::mojom::CSPDirectiveName::ImgSrc));
  EXPECT_EQ("", url_data_source->GetContentSecurityPolicy(
                    network::mojom::CSPDirectiveName::MediaSrc));
  EXPECT_EQ("object-src 'none';",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::ObjectSrc));
  EXPECT_EQ("script-src chrome://resources 'self';",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::ScriptSrc));
  EXPECT_EQ("", url_data_source->GetContentSecurityPolicy(
                    network::mojom::CSPDirectiveName::StyleSrc));
  EXPECT_EQ("require-trusted-types-for 'script';",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::RequireTrustedTypesFor));
  EXPECT_EQ("trusted-types;",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::TrustedTypes));

  // Override each directive and test it updates the underlying URLDataSource.
  source()->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc, "child-src 'self';");
  EXPECT_EQ("child-src 'self';",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::ChildSrc));

  source()->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      "connect-src 'self' 'unsafe-inline';");
  EXPECT_EQ("connect-src 'self' 'unsafe-inline';",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::ConnectSrc));

  source()->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "default-src 'self';");
  EXPECT_EQ("default-src 'self';",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::DefaultSrc));

  source()->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src 'self';");
  EXPECT_EQ("frame-src 'self';",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::FrameSrc));

  source()->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src 'self' blob:;");
  EXPECT_EQ("img-src 'self' blob:;",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::ImgSrc));

  source()->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc, "media-src 'self' blob:;");
  EXPECT_EQ("media-src 'self' blob:;",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::MediaSrc));

  source()->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ObjectSrc, "object-src 'self' data:;");
  EXPECT_EQ("object-src 'self' data:;",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::ObjectSrc));

  source()->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-inline';");
  EXPECT_EQ("script-src chrome://resources 'self' 'unsafe-inline';",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::ScriptSrc));

  source()->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline';");
  EXPECT_EQ("style-src 'self' 'unsafe-inline';",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::StyleSrc));

  source()->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor,
      "require-trusted-types-for 'wasm';");
  EXPECT_EQ("require-trusted-types-for 'wasm';",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::RequireTrustedTypesFor));
  source()->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes, "trusted-types test;");
  EXPECT_EQ("trusted-types test;",
            url_data_source->GetContentSecurityPolicy(
                network::mojom::CSPDirectiveName::TrustedTypes));
  source()->DisableTrustedTypesCSP();
  EXPECT_EQ("", url_data_source->GetContentSecurityPolicy(
                    network::mojom::CSPDirectiveName::RequireTrustedTypesFor));
  EXPECT_EQ("", url_data_source->GetContentSecurityPolicy(
                    network::mojom::CSPDirectiveName::TrustedTypes));
}

TEST_F(WebUIDataSourceTest, SetCrossOriginPolicyValues) {
  URLDataSource* url_data_source = source()->source();

  // Default values.
  EXPECT_EQ("", url_data_source->GetCrossOriginOpenerPolicy());
  EXPECT_EQ("", url_data_source->GetCrossOriginEmbedderPolicy());
  EXPECT_EQ("", url_data_source->GetCrossOriginResourcePolicy());

  // Overridden values.
  source()->OverrideCrossOriginOpenerPolicy("same-origin");
  EXPECT_EQ("same-origin", url_data_source->GetCrossOriginOpenerPolicy());
  source()->OverrideCrossOriginEmbedderPolicy("require-corp");
  EXPECT_EQ("require-corp", url_data_source->GetCrossOriginEmbedderPolicy());
  source()->OverrideCrossOriginResourcePolicy("cross-origin");
  EXPECT_EQ("cross-origin", url_data_source->GetCrossOriginResourcePolicy());

  // Remove/change the values.
  source()->OverrideCrossOriginOpenerPolicy("same-site");
  EXPECT_EQ("same-site", url_data_source->GetCrossOriginOpenerPolicy());
  source()->OverrideCrossOriginEmbedderPolicy("");
  EXPECT_EQ("", url_data_source->GetCrossOriginEmbedderPolicy());
  source()->OverrideCrossOriginResourcePolicy("same-origin");
  EXPECT_EQ("same-origin", url_data_source->GetCrossOriginResourcePolicy());
}

}  // namespace content
