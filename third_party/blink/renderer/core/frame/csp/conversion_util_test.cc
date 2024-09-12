// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/conversion_util.h"

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ContentSecurityPolicyConversionUtilTest, BackAndForthConversion) {
  using network::mojom::blink::ContentSecurityPolicy;
  using network::mojom::blink::ContentSecurityPolicyHeader;
  using network::mojom::blink::CSPDirectiveName;
  using network::mojom::blink::CSPTrustedTypes;

  auto basic_csp = ContentSecurityPolicy::New(
      network::mojom::blink::CSPSource::New("http", "www.example.org", 80, "",
                                            false, false),
      HashMap<CSPDirectiveName, String>(),
      HashMap<CSPDirectiveName, network::mojom::blink::CSPSourceListPtr>(),
      false, false, false, network::mojom::blink::WebSandboxFlags::kNone,
      ContentSecurityPolicyHeader::New(
          "my-csp", network::mojom::blink::ContentSecurityPolicyType::kEnforce,
          network::mojom::blink::ContentSecurityPolicySource::kHTTP),
      false, Vector<String>(),
      network::mojom::blink::CSPRequireTrustedTypesFor::None, nullptr,
      Vector<String>());

  using ModifyCSP = void(ContentSecurityPolicy&);
  ModifyCSP* test_cases[] = {
      [](ContentSecurityPolicy& csp) {},
      [](ContentSecurityPolicy& csp) {
        csp.raw_directives.insert(CSPDirectiveName::ScriptSrc, "'none'");
        csp.raw_directives.insert(
            CSPDirectiveName::DefaultSrc,
            " http://www.example.org:443/path 'self' invalid ");
      },
      [](ContentSecurityPolicy& csp) {
        csp.raw_directives.insert(CSPDirectiveName::ScriptSrc, "'none'");
        csp.raw_directives.insert(
            CSPDirectiveName::DefaultSrc,
            " http://www.example.org:443/path 'self' invalid ");
      },
      [](ContentSecurityPolicy& csp) { csp.upgrade_insecure_requests = true; },
      [](ContentSecurityPolicy& csp) { csp.treat_as_public_address = true; },
      [](ContentSecurityPolicy& csp) { csp.block_all_mixed_content = true; },
      [](ContentSecurityPolicy& csp) {
        csp.sandbox = network::mojom::blink::WebSandboxFlags::kPointerLock |
                      network::mojom::blink::WebSandboxFlags::kDownloads;
      },
      [](ContentSecurityPolicy& csp) {
        csp.header = ContentSecurityPolicyHeader::New(
            "my-csp", network::mojom::blink::ContentSecurityPolicyType::kReport,
            network::mojom::blink::ContentSecurityPolicySource::kMeta);
      },
      [](ContentSecurityPolicy& csp) { csp.use_reporting_api = true; },
      [](ContentSecurityPolicy& csp) {
        csp.report_endpoints = {"endpoint1", "endpoint2"};
      },
      [](ContentSecurityPolicy& csp) {
        csp.require_trusted_types_for =
            network::mojom::blink::CSPRequireTrustedTypesFor::Script;
      },
      [](ContentSecurityPolicy& csp) {
        csp.trusted_types = CSPTrustedTypes::New();
      },
      [](ContentSecurityPolicy& csp) {
        csp.trusted_types = CSPTrustedTypes::New(
            Vector<String>({"policy1", "policy2"}), false, false);
      },
      [](ContentSecurityPolicy& csp) {
        csp.trusted_types = CSPTrustedTypes::New(
            Vector<String>({"policy1", "policy2"}), true, false);
      },
      [](ContentSecurityPolicy& csp) {
        csp.trusted_types = CSPTrustedTypes::New(
            Vector<String>({"policy1", "policy2"}), false, true);
      },
      [](ContentSecurityPolicy& csp) {
        csp.parsing_errors = {"error1", "error2"};
      },
  };

  for (const auto& modify_csp : test_cases) {
    auto test_csp = basic_csp.Clone();
    (*modify_csp)(*test_csp);
    EXPECT_EQ(ConvertToMojoBlink(ConvertToPublic(test_csp.Clone())), test_csp);
  }
}

TEST(ContentSecurityPolicyConversionUtilTest,
     BackAndForthConversionForCSPSourceList) {
  using network::mojom::blink::ContentSecurityPolicy;
  using network::mojom::blink::CSPDirectiveName;
  using network::mojom::blink::CSPSource;
  using network::mojom::blink::CSPSourceList;

  auto basic_csp = ContentSecurityPolicy::New(
      CSPSource::New("http", "www.example.org", 80, "", false, false),
      HashMap<CSPDirectiveName, String>(),
      HashMap<CSPDirectiveName, network::mojom::blink::CSPSourceListPtr>(),
      false, false, false, network::mojom::blink::WebSandboxFlags::kNone,
      network::mojom::blink::ContentSecurityPolicyHeader::New(
          "my-csp", network::mojom::blink::ContentSecurityPolicyType::kEnforce,
          network::mojom::blink::ContentSecurityPolicySource::kHTTP),
      false, Vector<String>(),
      network::mojom::blink::CSPRequireTrustedTypesFor::None, nullptr,
      Vector<String>());

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
        source_list.hashes.emplace_back(
            network::mojom::blink::CSPHashSource::New(
                network::mojom::blink::CSPHashAlgorithm::SHA256,
                Vector<uint8_t>({'a', 'd'})));
        source_list.hashes.emplace_back(
            network::mojom::blink::CSPHashSource::New(
                network::mojom::blink::CSPHashAlgorithm::SHA384,
                Vector<uint8_t>({'c', 'd', 'e'})));
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
    auto script_src = CSPSourceList::New();
    (*modify_csp)(*script_src);
    test_csp->directives.insert(CSPDirectiveName::ScriptSrc,
                                std::move(script_src));
    EXPECT_EQ(ConvertToMojoBlink(ConvertToPublic(test_csp.Clone())), test_csp);
  }
}

}  // namespace blink
