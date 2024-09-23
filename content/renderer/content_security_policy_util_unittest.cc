// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/content_security_policy_util.h"

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ContentSecurityPolicyUtilTest, BackAndForthConversion) {
  using network::mojom::ContentSecurityPolicy;
  using network::mojom::ContentSecurityPolicyHeader;
  using network::mojom::CSPDirectiveName;

  auto basic_csp = ContentSecurityPolicy::New(
      network::mojom::CSPSource::New("http", "www.example.org", 80, "", false,
                                     false),
      base::flat_map<CSPDirectiveName, std::string>(),
      base::flat_map<CSPDirectiveName, network::mojom::CSPSourceListPtr>(),
      false, false, false, network::mojom::WebSandboxFlags::kNone,
      ContentSecurityPolicyHeader::New(
          "my-csp", network::mojom::ContentSecurityPolicyType::kEnforce,
          network::mojom::ContentSecurityPolicySource::kHTTP),
      false, std::vector<std::string>(),
      network::mojom::CSPRequireTrustedTypesFor::None, nullptr,
      std::vector<std::string>());

  using ModifyCSP = void(ContentSecurityPolicy&);
  ModifyCSP* test_cases[] = {
      [](ContentSecurityPolicy& csp) {},
      [](ContentSecurityPolicy& csp) {
        csp.raw_directives[CSPDirectiveName::ScriptSrc] = "'none'";
        csp.raw_directives[CSPDirectiveName::DefaultSrc] =
            " http://www.example.org:443/path 'self' invalid ";
      },
      [](ContentSecurityPolicy& csp) {
        csp.raw_directives[CSPDirectiveName::ScriptSrc] = "'none'";
        csp.raw_directives[CSPDirectiveName::DefaultSrc] =
            " http://www.example.org:443/path 'self' invalid ";
      },
      [](ContentSecurityPolicy& csp) { csp.upgrade_insecure_requests = true; },
      [](ContentSecurityPolicy& csp) { csp.treat_as_public_address = true; },
      [](ContentSecurityPolicy& csp) { csp.block_all_mixed_content = true; },
      [](ContentSecurityPolicy& csp) {
        csp.sandbox = network::mojom::WebSandboxFlags::kPointerLock |
                      network::mojom::WebSandboxFlags::kDownloads;
      },
      [](ContentSecurityPolicy& csp) {
        csp.header = ContentSecurityPolicyHeader::New(
            "my-csp", network::mojom::ContentSecurityPolicyType::kReport,
            network::mojom::ContentSecurityPolicySource::kMeta);
      },
      [](ContentSecurityPolicy& csp) { csp.use_reporting_api = true; },
      [](ContentSecurityPolicy& csp) {
        csp.report_endpoints = {"endpoint1", "endpoint2"};
      },
      [](ContentSecurityPolicy& csp) {
        csp.require_trusted_types_for =
            network::mojom::CSPRequireTrustedTypesFor::Script;
      },
      [](ContentSecurityPolicy& csp) {
        csp.trusted_types = network::mojom::CSPTrustedTypes::New();
      },
      [](ContentSecurityPolicy& csp) {
        csp.trusted_types = network::mojom::CSPTrustedTypes::New(
            std::vector<std::string>({"policy1", "policy2"}), false, false);
      },
      [](ContentSecurityPolicy& csp) {
        csp.trusted_types = network::mojom::CSPTrustedTypes::New(
            std::vector<std::string>({"policy1", "policy2"}), true, false);
      },
      [](ContentSecurityPolicy& csp) {
        csp.trusted_types = network::mojom::CSPTrustedTypes::New(
            std::vector<std::string>({"policy1", "policy2"}), false, true);
      },
      [](ContentSecurityPolicy& csp) {
        csp.parsing_errors = {"error1", "error2"};
      },
  };

  for (const auto& modify_csp : test_cases) {
    auto test_csp = basic_csp.Clone();
    (*modify_csp)(*test_csp);
    EXPECT_EQ(BuildContentSecurityPolicy(
                  ToWebContentSecurityPolicy(test_csp.Clone())),
              test_csp);
  }
}

TEST(ContentSecurityPolicyUtilTest, BackAndForthConversionForCSPSourceList) {
  using network::mojom::ContentSecurityPolicy;
  using network::mojom::CSPDirectiveName;
  using network::mojom::CSPSource;
  using network::mojom::CSPSourceList;

  auto basic_csp = network::mojom::ContentSecurityPolicy::New(
      CSPSource::New("http", "www.example.org", 80, "", false, false),
      base::flat_map<CSPDirectiveName, std::string>(),
      base::flat_map<CSPDirectiveName, network::mojom::CSPSourceListPtr>(),
      false, false, false, network::mojom::WebSandboxFlags::kNone,
      network::mojom::ContentSecurityPolicyHeader::New(
          "my-csp", network::mojom::ContentSecurityPolicyType::kEnforce,
          network::mojom::ContentSecurityPolicySource::kHTTP),
      false, std::vector<std::string>(),
      network::mojom::CSPRequireTrustedTypesFor::None, nullptr,
      std::vector<std::string>());

  basic_csp->directives[CSPDirectiveName::ScriptSrc] = CSPSourceList::New();

  using ModifyCSP = void(CSPSourceList&);
  ModifyCSP* test_cases[] = {
      [](CSPSourceList& source_list) {},
      [](CSPSourceList& source_list) {
        source_list.sources.emplace_back(
            CSPSource::New("http", "www.example.org", 80, "", false, false));
        source_list.sources.emplace_back(CSPSource::New(
            "http", "www.example.org", -1, "/path", false, false));
        source_list.sources.emplace_back(
            CSPSource::New("http", "www.example.org", 80, "", true, false));
        source_list.sources.emplace_back(
            CSPSource::New("http", "www.example.org", 8080, "", false, true));
      },
      [](CSPSourceList& source_list) {
        source_list.nonces.emplace_back("nonce-abc");
        source_list.nonces.emplace_back("nonce-cde");
      },
      [](CSPSourceList& source_list) {
        source_list.hashes.emplace_back(network::mojom::CSPHashSource::New(
            network::mojom::CSPHashAlgorithm::SHA256,
            std::vector<uint8_t>({'a', 'd'})));
        source_list.hashes.emplace_back(network::mojom::CSPHashSource::New(
            network::mojom::CSPHashAlgorithm::SHA384,
            std::vector<uint8_t>({'c', 'd', 'e'})));
      },
      [](CSPSourceList& source_list) { source_list.allow_self = true; },
      [](CSPSourceList& source_list) { source_list.allow_star = true; },
      [](CSPSourceList& source_list) { source_list.allow_inline = true; },
      [](CSPSourceList& source_list) { source_list.allow_eval = true; },
      [](CSPSourceList& source_list) { source_list.allow_wasm_eval = true; },
      [](CSPSourceList& source_list) {
        source_list.allow_wasm_unsafe_eval = true;
      },
      [](CSPSourceList& source_list) { source_list.allow_dynamic = true; },
      [](CSPSourceList& source_list) {
        source_list.allow_unsafe_hashes = true;
      },
      [](CSPSourceList& source_list) { source_list.report_sample = true; },
  };

  for (const auto& modify_csp : test_cases) {
    auto test_csp = basic_csp.Clone();
    test_csp->directives[CSPDirectiveName::ScriptSrc] = CSPSourceList::New();
    (*modify_csp)(*test_csp->directives[CSPDirectiveName::ScriptSrc]);
    EXPECT_EQ(BuildContentSecurityPolicy(
                  ToWebContentSecurityPolicy(test_csp.Clone())),
              test_csp);
  }
}

}  // namespace content
