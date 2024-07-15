// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals_kvv2_helper.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/debug/crash_logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/services/auction_worklet/trusted_signals_request_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8-context.h"

namespace auction_worklet {

namespace {
const char kHostName[] = "publisher.test";
const int kExperimentGroupId = 12345;
const char kTrustedBiddingSignalsSlotSizeParam[] = "slotSize=100,200";
const char kTrustedSignalsUrl[] = "https://url.test/";
const char kOriginFooUrl[] = "https://foo.test/";
const char kOriginBarUrl[] = "https://bar.test/";
}  // namespace

TEST(TrustedSignalsKVv2RequestHelperTest,
     TrustedBiddingSignalsRequestEncoding) {
  std::unique_ptr<TrustedBiddingSignalsKVv2RequestHelperBuilder>
      helper_builder =
          std::make_unique<TrustedBiddingSignalsKVv2RequestHelperBuilder>(
              kHostName, GURL(kTrustedSignalsUrl), kExperimentGroupId,
              kTrustedBiddingSignalsSlotSizeParam);

  helper_builder->AddTrustedSignalsRequest(
      std::string("groupA"), std::set<std::string>{"keyA", "keyAB"},
      url::Origin::Create(GURL(kOriginFooUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupB"), std::set<std::string>{"keyB", "keyAB"},
      url::Origin::Create(GURL(kOriginFooUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);
  // Another group in kOriginFooUrl, but with execution mode kCompatibilityMode,
  // for scenario of multiple partitions with different keys in one compression
  // group.
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupAB"), std::set<std::string>{"key"},
      url::Origin::Create(GURL(kOriginFooUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode);
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupC"), std::set<std::string>{"keyC", "keyCD"},
      url::Origin::Create(GURL(kOriginBarUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupD"), std::set<std::string>{"keyD", "keyCD"},
      url::Origin::Create(GURL(kOriginBarUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);
  // Test interest group name is merged into one partition with same joining
  // origin and kGroupedByOriginMode.
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupD"), std::set<std::string>{},
      url::Origin::Create(GURL(kOriginBarUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);
  // Test bidding keys are merged into one partition with same joining origin
  // and kGroupedByOriginMode.
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupD"), std::set<std::string>{"keyDD"},
      url::Origin::Create(GURL(kOriginBarUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);

  TrustedSignalsKVv2RequestHelper helper = helper_builder->Build();

  std::string post_body = helper.TakePostRequestBody();
  std::vector<uint8_t> body_bytes(post_body.begin(), post_body.end());

  // Use cbor.me to convert from
  // {
  //   "partitions": [
  //     {
  //       "id": 0,
  //       "metadata": {
  //         "hostname": "publisher.test",
  //         "slotSize": "100,200",
  //         "experimentGroupId": "12345"
  //       },
  //       "arguments": [
  //         {
  //           "data": [
  //             "groupA",
  //             "groupB"
  //           ],
  //           "tags": [
  //             "interestGroupNames"
  //           ]
  //         },
  //         {
  //           "data": [
  //             "keyA",
  //             "keyAB",
  //             "keyB"
  //           ],
  //           "tags": [
  //             "keys"
  //           ]
  //         }
  //       ],
  //       "compressionGroupId": 0
  //     },
  //     {
  //       "id": 1,
  //       "metadata": {
  //         "hostname": "publisher.test",
  //         "slotSize": "100,200",
  //         "experimentGroupId": "12345"
  //       },
  //       "arguments": [
  //         {
  //           "data": [
  //             "groupAB"
  //           ],
  //           "tags": [
  //             "interestGroupNames"
  //           ]
  //         },
  //         {
  //           "data": [
  //             "key"
  //           ],
  //           "tags": [
  //             "keys"
  //           ]
  //         }
  //       ],
  //       "compressionGroupId": 0
  //     },
  //     {
  //       "id": 0,
  //       "metadata": {
  //         "hostname": "publisher.test",
  //         "slotSize": "100,200",
  //         "experimentGroupId": "12345"
  //       },
  //       "arguments": [
  //         {
  //           "data": [
  //             "groupC",
  //             "groupD"
  //           ],
  //           "tags": [
  //             "interestGroupNames"
  //           ]
  //         },
  //         {
  //           "data": [
  //             "keyC",
  //             "keyCD",
  //             "keyD",
  //             "keyDD"
  //           ],
  //           "tags": [
  //             "keys"
  //           ]
  //         }
  //       ],
  //       "compressionGroupId": 1
  //     }
  //   ],
  //   "acceptCompression": [
  //     "none",
  //     "gzip"
  //   ]
  // }
  const std::string kExpectedBodyHex =
      "A26A706172746974696F6E7383A462696400686D65746164617461A368686F73746E616D"
      "656E7075626C69736865722E7465737468736C6F7453697A65673130302C323030716578"
      "706572696D656E7447726F7570496465313233343569617267756D656E747382A2646461"
      "7461826667726F7570416667726F75704264746167738172696E74657265737447726F75"
      "704E616D6573A2646461746183646B657941656B65794142646B65794264746167738164"
      "6B65797372636F6D7072657373696F6E47726F7570496400A462696401686D6574616461"
      "7461A368686F73746E616D656E7075626C69736865722E7465737468736C6F7453697A65"
      "673130302C323030716578706572696D656E7447726F7570496465313233343569617267"
      "756D656E747382A26464617461816767726F7570414264746167738172696E7465726573"
      "7447726F75704E616D6573A2646461746181636B6579647461677381646B65797372636F"
      "6D7072657373696F6E47726F7570496400A462696400686D65746164617461A368686F73"
      "746E616D656E7075626C69736865722E7465737468736C6F7453697A65673130302C3230"
      "30716578706572696D656E7447726F7570496465313233343569617267756D656E747382"
      "A26464617461826667726F7570436667726F75704464746167738172696E746572657374"
      "47726F75704E616D6573A2646461746184646B657943656B65794344646B657944656B65"
      "794444647461677381646B65797372636F6D7072657373696F6E47726F75704964017161"
      "6363657074436F6D7072657373696F6E82646E6F6E6564677A6970";
  // Prefix hex for `kExpectedBodyHex` which includes the compression format
  // code and the length.
  const std::string kExpectedPrefixHex = "000000025B";

  EXPECT_EQ(base::HexEncode(body_bytes), kExpectedPrefixHex + kExpectedBodyHex);
}

// TODO(crbug.com/337917489): When adding an identical IG, it should use the
// existing partition instead of creating a new one. After the implementation,
// the EXPECT_EQ() of second IG H should be failed.
TEST(TrustedSignalsKVv2RequestHelperTest, TrustedBiddingSignalsIsolationIndex) {
  // Add the following interest groups:
  // IG A[join_origin: foo.com, mode: group-by-origin]
  // IG B[join_origin: foo.com, mode: group-by-origin]
  // IG C[join_origin: foo.com, mode: compatibility]
  // IG D[join_origin: foo.com, mode: compatibility]
  // IG E[join_origin: bar.com, mode: compatibility]
  // IG F[join_origin: bar.com, mode: group-by-origin]
  // IG G[join_origin: bar.com, mode: compatibility]
  // IG H[join_origin: bar.com, mode: compatibility]
  // IG H, a dulplicate IG, aiming to test how request builder can handle a
  // identical IG.
  // will result the following groups: Compression: 0 -
  //    partition 0: A, B
  //    partition 1: C
  //    partition 2: D
  // Compression: 1 -
  //    partition 0: F
  //    partition 1: E
  //    partition 2: G
  //    partition 3: H
  //    partition 4: H

  std::unique_ptr<TrustedBiddingSignalsKVv2RequestHelperBuilder>
      helper_builder =
          std::make_unique<TrustedBiddingSignalsKVv2RequestHelperBuilder>(
              kHostName, GURL(kTrustedSignalsUrl), kExperimentGroupId,
              kTrustedBiddingSignalsSlotSizeParam);

  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 0),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupA"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginFooUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 0),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupB"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginFooUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 1),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupC"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginFooUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 2),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupD"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginFooUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 1),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupE"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginBarUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 0),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupF"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginBarUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 2),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupG"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginBarUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 3),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupH"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginBarUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 4),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupH"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginBarUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
}

}  // namespace auction_worklet
