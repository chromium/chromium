/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/link_rel_attribute.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

// TODO(dcheng): This is a bit gross. Refactor this to not take so many bools...
static inline void TestLinkRelAttribute(const String& value,
                                        bool is_style_sheet,
                                        mojom::blink::FaviconIconType icon_type,
                                        bool is_alternate,
                                        bool is_dns_prefetch,
                                        bool is_link_prerender,
                                        bool is_preconnect = false,
                                        bool is_canonical = false,
                                        bool is_compression_dictionary = false,
                                        bool is_payment = false) {
  SCOPED_TRACE(value.Utf8());
  LinkRelAttribute link_rel_attribute(value);
  ASSERT_EQ(is_style_sheet, link_rel_attribute.IsStyleSheet());
  ASSERT_EQ(icon_type, link_rel_attribute.GetIconType());
  ASSERT_EQ(is_alternate, link_rel_attribute.IsAlternate());
  ASSERT_EQ(is_dns_prefetch, link_rel_attribute.IsDNSPrefetch());
  ASSERT_EQ(is_link_prerender, link_rel_attribute.IsLinkPrerender());
  ASSERT_EQ(is_preconnect, link_rel_attribute.IsPreconnect());
  ASSERT_EQ(is_canonical, link_rel_attribute.IsCanonical());
  ASSERT_EQ(is_compression_dictionary,
            link_rel_attribute.IsCompressionDictionary());
  ASSERT_EQ(is_payment, link_rel_attribute.IsPayment());
}

TEST(LinkRelAttributeTest, Constructor) {
  test::TaskEnvironment task_environment;
  TestLinkRelAttribute("stylesheet", true,
                       mojom::blink::FaviconIconType::kInvalid, false, false,
                       false);
  TestLinkRelAttribute("sTyLeShEeT", true,
                       mojom::blink::FaviconIconType::kInvalid, false, false,
                       false);

  TestLinkRelAttribute("icon", false, mojom::blink::FaviconIconType::kFavicon,
                       false, false, false);
  TestLinkRelAttribute("iCoN", false, mojom::blink::FaviconIconType::kFavicon,
                       false, false, false);
  TestLinkRelAttribute("shortcut icon", false,
                       mojom::blink::FaviconIconType::kFavicon, false, false,
                       false);
  TestLinkRelAttribute("sHoRtCuT iCoN", false,
                       mojom::blink::FaviconIconType::kFavicon, false, false,
                       false);

  TestLinkRelAttribute("dns-prefetch", false,
                       mojom::blink::FaviconIconType::kInvalid, false, true,
                       false);
  TestLinkRelAttribute("dNs-pReFeTcH", false,
                       mojom::blink::FaviconIconType::kInvalid, false, true,
                       false);
  TestLinkRelAttribute("alternate dNs-pReFeTcH", false,
                       mojom::blink::FaviconIconType::kInvalid, true, true,
                       false);

  TestLinkRelAttribute("apple-touch-icon", false,
                       mojom::blink::FaviconIconType::kTouchIcon, false, false,
                       false);
  TestLinkRelAttribute("aPpLe-tOuCh-IcOn", false,
                       mojom::blink::FaviconIconType::kTouchIcon, false, false,
                       false);
  TestLinkRelAttribute("apple-touch-icon-precomposed", false,
                       mojom::blink::FaviconIconType::kTouchPrecomposedIcon,
                       false, false, false);
  TestLinkRelAttribute("aPpLe-tOuCh-IcOn-pReCoMpOsEd", false,
                       mojom::blink::FaviconIconType::kTouchPrecomposedIcon,
                       false, false, false);

  TestLinkRelAttribute("alternate stylesheet", true,
                       mojom::blink::FaviconIconType::kInvalid, true, false,
                       false);
  TestLinkRelAttribute("stylesheet alternate", true,
                       mojom::blink::FaviconIconType::kInvalid, true, false,
                       false);
  TestLinkRelAttribute("aLtErNaTe sTyLeShEeT", true,
                       mojom::blink::FaviconIconType::kInvalid, true, false,
                       false);
  TestLinkRelAttribute("sTyLeShEeT aLtErNaTe", true,
                       mojom::blink::FaviconIconType::kInvalid, true, false,
                       false);

  TestLinkRelAttribute("stylesheet icon prerender aLtErNaTe", true,
                       mojom::blink::FaviconIconType::kFavicon, true, false,
                       true);
  TestLinkRelAttribute("alternate icon stylesheet", true,
                       mojom::blink::FaviconIconType::kFavicon, true, false,
                       false);

  TestLinkRelAttribute("alternate import", false,
                       mojom::blink::FaviconIconType::kInvalid, true, false,
                       false);
  TestLinkRelAttribute("stylesheet import", true,
                       mojom::blink::FaviconIconType::kInvalid, false, false,
                       false);

  TestLinkRelAttribute("preconnect", false,
                       mojom::blink::FaviconIconType::kInvalid, false, false,
                       false, /*is_preconnect=*/true);
  TestLinkRelAttribute("pReCoNnEcT", false,
                       mojom::blink::FaviconIconType::kInvalid, false, false,
                       false, /*is_preconnect=*/true);

  TestLinkRelAttribute("canonical", false,
                       mojom::blink::FaviconIconType::kInvalid, false, false,
                       false, /*is_preconnect=*/false, /*is_canonical=*/true);
  TestLinkRelAttribute("caNONiCAL", false,
                       mojom::blink::FaviconIconType::kInvalid, false, false,
                       false, /*is_preconnect=*/false, /*is_canonical=*/true);

  TestLinkRelAttribute("compression-dictionary", false,
                       mojom::blink::FaviconIconType::kInvalid, false, false,
                       false, /*is_preconnect=*/false, /*is_canonical=*/false,
                       /*is_compression_dictionary=*/true);
  TestLinkRelAttribute("COMpRessiOn-diCtIonAry", false,
                       mojom::blink::FaviconIconType::kInvalid, false, false,
                       false, /*is_preconnect=*/false, /*is_canonical=*/false,
                       /*is_compression_dictionary=*/true);
  TestLinkRelAttribute("dictionary", false,
                       mojom::blink::FaviconIconType::kInvalid, false, false,
                       false, /*is_preconnect=*/false, /*is_canonical=*/false,
                       /*is_compression_dictionary=*/false);
  TestLinkRelAttribute(
      "payment", false, mojom::blink::FaviconIconType::kInvalid, false, false,
      false, /*is_preconnect=*/false, /*is_canonical=*/false,
      /*is_compression_dictionary=*/false, /*is_payment=*/true);
  TestLinkRelAttribute(
      "pAymENt", false, mojom::blink::FaviconIconType::kInvalid, false, false,
      false, /*is_preconnect=*/false, /*is_canonical=*/false,
      /*is_compression_dictionary=*/false, /*is_payment=*/true);
}

}  // namespace blink
