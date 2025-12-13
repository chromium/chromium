// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_TEST_SIGNED_WEB_BUNDLE_BUILDER_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_TEST_SIGNED_WEB_BUNDLE_BUILDER_H_

#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "url/gurl.h"

namespace web_app {

struct TestSignedWebBundle {
  TestSignedWebBundle(std::vector<uint8_t> data,
                      const web_package::SignedWebBundleId& id);

  TestSignedWebBundle(const TestSignedWebBundle&);
  TestSignedWebBundle(TestSignedWebBundle&&);

  ~TestSignedWebBundle();

  std::vector<uint8_t> data;
  web_package::SignedWebBundleId id;
};

class TestSignedWebBundleBuilder {
 public:
  explicit TestSignedWebBundleBuilder(
      web_package::test::KeyPair key_pair =
          web_package::test::Ed25519KeyPair::CreateRandom(),
      web_package::test::WebBundleSigner::ErrorsForTesting errors_for_testing =
          {/*integrity_block_errors=*/{},
           /*signatures_errors=*/{}});

  explicit TestSignedWebBundleBuilder(
      web_package::test::KeyPairs key_pairs,
      const web_package::SignedWebBundleId& web_bundle_id,
      web_package::test::WebBundleSigner::ErrorsForTesting errors_for_testing =
          {/*integrity_block_errors=*/{},
           /*signatures_errors=*/{}});
  ~TestSignedWebBundleBuilder();

  static constexpr std::string_view kTestManifestUrl =
      "/.well-known/manifest.webmanifest";
  static constexpr std::string_view kTestIconUrl = "/256x256-green.png";
  static constexpr std::string_view kTestHtmlUrl = "/index.html";

  // TODO(crbug.com/40264793): Use a struct instead when designated
  // initializers are supported.
  class BuildOptions {
   public:
    BuildOptions();
    BuildOptions(const BuildOptions&);
    BuildOptions(BuildOptions&&);
    ~BuildOptions();

    BuildOptions& AddKeyPair(web_package::test::KeyPair key_pair) {
      key_pairs_.push_back(std::move(key_pair));
      return *this;
    }

    BuildOptions& SetWebBundleId(web_package::SignedWebBundleId web_bundle_id) {
      web_bundle_id_ = std::move(web_bundle_id);
      return *this;
    }

    BuildOptions& SetVersion(IwaVersion version) {
      version_ = std::move(version);
      return *this;
    }

    BuildOptions& SetPrimaryUrl(GURL primary_url) {
      primary_url_ = std::move(primary_url);
      return *this;
    }

    BuildOptions& SetAppName(const std::string& app_name) {
      app_name_ = app_name;
      return *this;
    }

    BuildOptions& SetBaseUrl(GURL base_url) {
      base_url_ = std::move(base_url);
      return *this;
    }

    BuildOptions& SetIndexHTMLContent(std::string_view index_html_content) {
      index_html_content_ = index_html_content;
      return *this;
    }

    BuildOptions& SetErrorsForTesting(
        web_package::test::WebBundleSigner::ErrorsForTesting
            errors_for_testing) {
      errors_for_testing_ = errors_for_testing;
      return *this;
    }

    web_package::test::KeyPairs key_pairs_;
    std::optional<web_package::SignedWebBundleId> web_bundle_id_;
    IwaVersion version_;
    std::string app_name_;
    std::optional<GURL> primary_url_;
    std::optional<GURL> base_url_;
    std::optional<std::string_view> index_html_content_;
    web_package::test::WebBundleSigner::ErrorsForTesting errors_for_testing_;
  };

  // Adds a manifest type payload to the bundle.
  void AddManifest(std::string_view manifest_string);

  // Adds a image/PNG type payload to the bundle.
  void AddPngImage(std::string_view url, std::string_view image_string);

  // Adds a text/html type payload to the bundle.
  void AddHtml(std::string_view url, std::string_view html_content);

  // Adds an application/javascript type payload to the bundle.
  void AddJavaScript(std::string_view url, std::string_view script_content);

  // For each file, deduces the mime type from the extension and adds a payload
  // with this type to the bundle. Only files with the extension ".webmanifest"
  // are added with the manifest mime type. For files in sub directories, the
  // url mirrors the relative path.
  void AddFilesFromFolder(const base::FilePath& folder);

  // Adds the primary url to the bundle. DO NOT use this for IWAs - primary URLs
  // are not supported in IWAs.
  void AddPrimaryUrl(GURL url);

  TestSignedWebBundle Build();
  static TestSignedWebBundle BuildDefault(
      BuildOptions build_options = BuildOptions());

 private:
  web_package::test::KeyPairs key_pairs_;

  // This field is always set if there's more than one entry in `key_pairs_`.
  std::optional<web_package::SignedWebBundleId> web_bundle_id_;
  web_package::test::WebBundleSigner::ErrorsForTesting errors_for_testing_;
  web_package::WebBundleBuilder builder_;
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_TEST_SIGNED_WEB_BUNDLE_BUILDER_H_
