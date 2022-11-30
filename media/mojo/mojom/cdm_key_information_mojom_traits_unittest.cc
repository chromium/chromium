// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/cdm_key_information_mojom_traits.h"

#include "media/base/cdm_key_information.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(CdmKeyInformationStructTraitsTest, ConvertCdmKeyInformation) {
  auto input = std::make_unique<CdmKeyInformation>(
      "key_id", CdmKeyInformation::KeyStatus::USABLE, 23);
  std::vector<uint8_t> data =
      media::mojom::CdmKeyInformation::Serialize(&input);

  std::unique_ptr<CdmKeyInformation> output;
  EXPECT_TRUE(
      media::mojom::CdmKeyInformation::Deserialize(std::move(data), &output));
  EXPECT_EQ(input->key_id, output->key_id);
  EXPECT_EQ(input->status, output->status);
  EXPECT_EQ(input->system_code, output->system_code);
}

}  // namespace media
