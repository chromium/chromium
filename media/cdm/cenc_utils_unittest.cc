// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cdm/cenc_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/check.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

const uint8_t kKey1Data[] = {
    0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
    0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03
};
const uint8_t kKey2Data[] = {
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
};
const uint8_t kKey3Data[] = {
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x05,
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x05,
};
const uint8_t kKey4Data[] = {
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x06,
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x06,
};
const uint8_t kCommonSystemSystemId[] = {
    0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,
    0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B
};

class CencUtilsTest : public testing::Test {
 public:
  CencUtilsTest()
      : key1_(kKey1Data, kKey1Data + std::size(kKey1Data)),
        key2_(kKey2Data, kKey2Data + std::size(kKey2Data)),
        key3_(kKey3Data, kKey3Data + std::size(kKey3Data)),
        key4_(kKey4Data, kKey4Data + std::size(kKey4Data)),
        common_system_system_id_(
            kCommonSystemSystemId,
            kCommonSystemSystemId + std::size(kCommonSystemSystemId)) {}

 protected:
  // Initialize the start of the 'pssh' box (up to key_count)
  void InitializePSSHBox(std::vector<uint8_t>* box,
                         uint8_t size,
                         uint8_t version) {
    DCHECK(box->size() == 0);

    box->reserve(size);
    // Add size.
    DCHECK(size < std::numeric_limits<uint8_t>::max());
    box->push_back(0);
    box->push_back(0);
    box->push_back(0);
    box->push_back(size);
    // Add 'pssh'.
    box->push_back('p');
    box->push_back('s');
    box->push_back('s');
    box->push_back('h');
    // Add version.
    box->push_back(version);
    // Add flags.
    box->push_back(0);
    box->push_back(0);
    box->push_back(0);
    // Add Common Encryption SystemID.
    box->insert(box->end(), common_system_system_id_.begin(),
                common_system_system_id_.end());
  }

  std::vector<uint8_t> MakePSSHBox(uint8_t version) {
    std::vector<uint8_t> box;
    uint8_t size = (version == 0) ? 32 : 36;
    InitializePSSHBox(&box, size, version);
    if (version > 0) {
      // Add key_count (= 0).
      box.push_back(0);
      box.push_back(0);
      box.push_back(0);
      box.push_back(0);
    }
    // Add data_size (= 0).
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    return box;
  }

  std::vector<uint8_t> MakePSSHBox(uint8_t version,
                                   const std::vector<uint8_t>& key1) {
    DCHECK(version > 0);
    DCHECK(key1.size() == 16);

    std::vector<uint8_t> box;
    uint8_t size = 52;
    InitializePSSHBox(&box, size, version);

    // Add key_count (= 1).
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    box.push_back(1);

    // Add key1.
    for (size_t i = 0; i < key1.size(); ++i)
      box.push_back(key1[i]);

    // Add data_size (= 0).
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    return box;
  }

  std::vector<uint8_t> MakePSSHBox(uint8_t version,
                                   const std::vector<uint8_t>& key1,
                                   const std::vector<uint8_t>& key2) {
    DCHECK(version > 0);
    DCHECK(key1.size() == 16);
    DCHECK(key2.size() == 16);

    std::vector<uint8_t> box;
    uint8_t size = 68;
    InitializePSSHBox(&box, size, version);

    // Add key_count (= 2).
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    box.push_back(2);

    // Add key1.
    for (size_t i = 0; i < key1.size(); ++i)
      box.push_back(key1[i]);

    // Add key2.
    for (size_t i = 0; i < key2.size(); ++i)
      box.push_back(key2[i]);

    // Add data_size (= 0).
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    return box;
  }

  void AppendData(std::vector<uint8_t>& pssh_box,
                  const std::vector<uint8_t>& data) {
    // This assumes that |pssh_box| has been created using the routines above,
    // and simply appends the data to the end of it. It updates the box size
    // and sets the data size.
    DCHECK(data.size() < 100);
    pssh_box[3] += static_cast<uint8_t>(data.size());
    pssh_box.pop_back();
    pssh_box.push_back(static_cast<uint8_t>(data.size()));
    pssh_box.insert(pssh_box.end(), data.begin(), data.end());
  }

  const std::vector<uint8_t>& Key1() { return key1_; }
  const std::vector<uint8_t>& Key2() { return key2_; }
  const std::vector<uint8_t>& Key3() { return key3_; }
  const std::vector<uint8_t>& Key4() { return key4_; }
  const std::vector<uint8_t>& CommonSystemSystemId() {
    return common_system_system_id_;
  }

 private:
  std::vector<uint8_t> key1_;
  std::vector<uint8_t> key2_;
  std::vector<uint8_t> key3_;
  std::vector<uint8_t> key4_;
  std::vector<uint8_t> common_system_system_id_;
};

TEST_F(CencUtilsTest, EmptyPSSH) {
  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(std::vector<uint8_t>()));
  EXPECT_FALSE(GetKeyIdsForCommonSystemId(std::vector<uint8_t>(), &key_ids));
}

TEST_F(CencUtilsTest, PSSHVersion0) {
  std::vector<uint8_t> box = MakePSSHBox(0);
  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box));
  EXPECT_FALSE(GetKeyIdsForCommonSystemId(box, &key_ids));
}

TEST_F(CencUtilsTest, PSSHVersion1WithNoKeys) {
  std::vector<uint8_t> box = MakePSSHBox(1);
  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box));
  EXPECT_FALSE(GetKeyIdsForCommonSystemId(box, &key_ids));
}

TEST_F(CencUtilsTest, PSSHVersion1WithOneKey) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1());
  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box, &key_ids));
  EXPECT_EQ(1u, key_ids.size());
  EXPECT_EQ(key_ids[0], Key1());
}

TEST_F(CencUtilsTest, PSSHVersion1WithTwoKeys) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1(), Key2());
  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box, &key_ids));
  EXPECT_EQ(2u, key_ids.size());
  EXPECT_EQ(key_ids[0], Key1());
  EXPECT_EQ(key_ids[1], Key2());
}

TEST_F(CencUtilsTest, PSSHVersion0Plus1) {
  std::vector<uint8_t> box0 = MakePSSHBox(0);
  std::vector<uint8_t> box1 = MakePSSHBox(1, Key1());

  // Concatenate box1 onto end of box0.
  box0.insert(box0.end(), box1.begin(), box1.end());
  EXPECT_TRUE(ValidatePsshInput(box0));

  // No key IDs returned as only the first 'pssh' box is processed.
  KeyIdList key_ids;
  EXPECT_FALSE(GetKeyIdsForCommonSystemId(box0, &key_ids));
}

TEST_F(CencUtilsTest, PSSHVersion1Plus0) {
  std::vector<uint8_t> box0 = MakePSSHBox(0);
  std::vector<uint8_t> box1 = MakePSSHBox(1, Key1());

  // Concatenate box0 onto end of box1.
  box1.insert(box1.end(), box0.begin(), box0.end());

  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box1));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box1, &key_ids));
  EXPECT_EQ(1u, key_ids.size());
  EXPECT_EQ(key_ids[0], Key1());
}

TEST_F(CencUtilsTest, MultiplePSSHVersion1) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1(), Key2());
  std::vector<uint8_t> box1 = MakePSSHBox(1, Key3());
  std::vector<uint8_t> box2 = MakePSSHBox(1, Key4());

  // Concatenate box1 and box2 onto end of box.
  box.insert(box.end(), box1.begin(), box1.end());
  box.insert(box.end(), box2.begin(), box2.end());

  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box, &key_ids));
  EXPECT_EQ(2u, key_ids.size());
  EXPECT_EQ(key_ids[0], Key1());
  EXPECT_EQ(key_ids[1], Key2());
}

TEST_F(CencUtilsTest, PsshBoxSmallerThanSize) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1(), Key2());
  KeyIdList key_ids;

  // Tries every buffer size less than the indicated 'pssh' box size.
  for (size_t i = 1; i < box.size(); ++i) {
    // Truncate the box to be less than the specified box size.
    std::vector<uint8_t> truncated(&box[0], &box[0] + i);
    EXPECT_FALSE(ValidatePsshInput(truncated)) << "Failed for length " << i;
    EXPECT_FALSE(GetKeyIdsForCommonSystemId(truncated, &key_ids));
  }
}

TEST_F(CencUtilsTest, PsshBoxLargerThanSize) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1(), Key2());
  KeyIdList key_ids;

  // Add 20 additional bytes to |box|.
  size_t original_size = box.size();
  for (size_t i = 0; i < 20; ++i)
    box.push_back(static_cast<uint8_t>(i));

  // Tries every size greater than |original_size|.
  for (size_t i = original_size + 1; i < box.size(); ++i) {
    // Modify size of box passed to be less than current size.
    std::vector<uint8_t> truncated(&box[0], &box[0] + i);
    EXPECT_FALSE(ValidatePsshInput(truncated)) << "Failed for length " << i;
    EXPECT_FALSE(GetKeyIdsForCommonSystemId(truncated, &key_ids));
  }
}

TEST_F(CencUtilsTest, UnrecognizedSystemID) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1(), Key2());

  // Modify the System ID.
  ++box[20];

  KeyIdList key_ids;
  EXPECT_FALSE(GetKeyIdsForCommonSystemId(box, &key_ids));
}

TEST_F(CencUtilsTest, InvalidFlags) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1(), Key2());

  // Modify flags.
  box[10] = 3;

  KeyIdList key_ids;
  EXPECT_FALSE(GetKeyIdsForCommonSystemId(box, &key_ids));
}

TEST_F(CencUtilsTest, LongSize) {
  const uint8_t data[] = {
      0x00, 0x00, 0x00, 0x01,                          // size = 1
      0x70, 0x73, 0x73, 0x68,                          // 'pssh'
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4c,  // longsize
      0x01,                                            // version
      0x00, 0x00, 0x00,                                // flags
      0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // SystemID
      0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
      0x00, 0x00, 0x00, 0x02,                          // key count
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,  // key1
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,  // key2
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
      0x00, 0x00, 0x00, 0x00  // datasize
  };

  KeyIdList key_ids;
  EXPECT_TRUE(
      ValidatePsshInput(std::vector<uint8_t>(data, data + std::size(data))));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(
      std::vector<uint8_t>(data, data + std::size(data)), &key_ids));
  EXPECT_EQ(2u, key_ids.size());
}

TEST_F(CencUtilsTest, SizeIsZero) {
  const uint8_t data[] = {
      0x00, 0x00, 0x00, 0x00,                          // size = 0
      0x70, 0x73, 0x73, 0x68,                          // 'pssh'
      0x01,                                            // version
      0x00, 0x00, 0x00,                                // flags
      0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // SystemID
      0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
      0x00, 0x00, 0x00, 0x02,                          // key count
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,  // key1
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,  // key2
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
      0x00, 0x00, 0x00, 0x00  // datasize
  };

  KeyIdList key_ids;
  EXPECT_TRUE(
      ValidatePsshInput(std::vector<uint8_t>(data, data + std::size(data))));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(
      std::vector<uint8_t>(data, data + std::size(data)), &key_ids));
  EXPECT_EQ(2u, key_ids.size());
}

TEST_F(CencUtilsTest, HugeSize) {
  const uint8_t data[] = {
      0x00, 0x00, 0x00, 0x01,                          // size = 1
      0x70, 0x73, 0x73, 0x68,                          // 'pssh'
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // longsize = big
      0x01,                                            // version
      0x00, 0x00, 0x00,                                // flags
      0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // SystemID
      0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
      0x00, 0x00, 0x00, 0x02,                          // key count
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,  // key1
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,  // key2
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
      0x00, 0x00, 0x00, 0x00  // datasize
  };

  KeyIdList key_ids;
  // These calls fail as the box size is huge (0xffffffffffffffff) and there
  // is not enough bytes in |data|.
  EXPECT_FALSE(
      ValidatePsshInput(std::vector<uint8_t>(data, data + std::size(data))));
  EXPECT_FALSE(GetKeyIdsForCommonSystemId(
      std::vector<uint8_t>(data, data + std::size(data)), &key_ids));
}

TEST_F(CencUtilsTest, GetPsshData_Version0) {
  const uint8_t data_bytes[] = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> pssh_data;

  std::vector<uint8_t> box = MakePSSHBox(0);
  EXPECT_TRUE(GetPsshData(box, CommonSystemSystemId(), &pssh_data));
  EXPECT_EQ(0u, pssh_data.size());

  std::vector<uint8_t> data(data_bytes, data_bytes + std::size(data_bytes));
  AppendData(box, data);
  EXPECT_TRUE(GetPsshData(box, CommonSystemSystemId(), &pssh_data));
  EXPECT_EQ(data, pssh_data);
}

TEST_F(CencUtilsTest, GetPsshData_Version1NoKeys) {
  const uint8_t data_bytes[] = {0x05, 0x06, 0x07, 0x08};
  std::vector<uint8_t> pssh_data;

  std::vector<uint8_t> box = MakePSSHBox(1);
  EXPECT_TRUE(GetPsshData(box, CommonSystemSystemId(), &pssh_data));
  EXPECT_EQ(0u, pssh_data.size());

  std::vector<uint8_t> data(data_bytes, data_bytes + std::size(data_bytes));
  AppendData(box, data);
  EXPECT_TRUE(GetPsshData(box, CommonSystemSystemId(), &pssh_data));
  EXPECT_EQ(data, pssh_data);
}

TEST_F(CencUtilsTest, GetPsshData_Version1WithKeys) {
  const uint8_t data_bytes[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  std::vector<uint8_t> pssh_data;

  std::vector<uint8_t> box = MakePSSHBox(1, Key1());
  EXPECT_TRUE(GetPsshData(box, CommonSystemSystemId(), &pssh_data));
  EXPECT_EQ(0u, pssh_data.size());

  std::vector<uint8_t> data(data_bytes, data_bytes + std::size(data_bytes));
  AppendData(box, data);
  EXPECT_TRUE(GetPsshData(box, CommonSystemSystemId(), &pssh_data));
  EXPECT_EQ(data, pssh_data);
}

TEST_F(CencUtilsTest, GetPsshData_Version2) {
  std::vector<uint8_t> pssh_data;

  std::vector<uint8_t> box = MakePSSHBox(1, Key1());
  EXPECT_TRUE(GetPsshData(box, CommonSystemSystemId(), &pssh_data));

  // Change the version manually, since we don't know what v2 will contain.
  box[8] = 2;
  EXPECT_FALSE(GetPsshData(box, CommonSystemSystemId(), &pssh_data));
}

TEST_F(CencUtilsTest, GetPsshData_Version2ThenVersion1) {
  std::vector<uint8_t> pssh_data;

  std::vector<uint8_t> box_v1 = MakePSSHBox(1, Key1());
  std::vector<uint8_t> box_v2 = MakePSSHBox(2, Key2(), Key3());

  // Concatenate the boxes together (v2 first).
  std::vector<uint8_t> boxes;
  boxes.insert(boxes.end(), box_v2.begin(), box_v2.end());
  boxes.insert(boxes.end(), box_v1.begin(), box_v1.end());
  EXPECT_TRUE(GetPsshData(boxes, CommonSystemSystemId(), &pssh_data));

  // GetKeyIdsForCommonSystemId() should return the single key from the v1
  // 'pssh' box.
  KeyIdList key_ids;
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(boxes, &key_ids));
  EXPECT_EQ(1u, key_ids.size());
  EXPECT_EQ(key_ids[0], Key1());
}

TEST_F(CencUtilsTest, GetPsshData_Version1ThenVersion2) {
  std::vector<uint8_t> pssh_data;

  std::vector<uint8_t> box_v1 = MakePSSHBox(1, Key3());
  std::vector<uint8_t> box_v2 = MakePSSHBox(2, Key4());

  // Concatenate the boxes together (v1 first).
  std::vector<uint8_t> boxes;
  boxes.insert(boxes.end(), box_v1.begin(), box_v1.end());
  boxes.insert(boxes.end(), box_v2.begin(), box_v2.end());
  EXPECT_TRUE(GetPsshData(boxes, CommonSystemSystemId(), &pssh_data));

  // GetKeyIdsForCommonSystemId() should return the single key from the v1
  // 'pssh' box.
  KeyIdList key_ids;
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(boxes, &key_ids));
  EXPECT_EQ(1u, key_ids.size());
  EXPECT_EQ(key_ids[0], Key3());
}

TEST_F(CencUtilsTest, GetPsshData_DifferentSystemID) {
  std::vector<uint8_t> unknown_system_id(kKey1Data,
                                         kKey1Data + std::size(kKey1Data));
  std::vector<uint8_t> pssh_data;

  std::vector<uint8_t> box = MakePSSHBox(1, Key1());
  EXPECT_TRUE(GetPsshData(box, CommonSystemSystemId(), &pssh_data));
  EXPECT_FALSE(GetPsshData(box, unknown_system_id, &pssh_data));
}

TEST_F(CencUtilsTest, GetPsshData_MissingData) {
  const uint8_t data_bytes[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  std::vector<uint8_t> pssh_data;

  std::vector<uint8_t> box = MakePSSHBox(1, Key1());
  std::vector<uint8_t> data(data_bytes, data_bytes + std::size(data_bytes));
  AppendData(box, data);
  EXPECT_TRUE(GetPsshData(box, CommonSystemSystemId(), &pssh_data));

  // Remove some data from the end, so now the size is incorrect.
  box.pop_back();
  box.pop_back();
  EXPECT_FALSE(GetPsshData(box, CommonSystemSystemId(), &pssh_data));
}

TEST_F(CencUtilsTest, GetPsshData_MultiplePssh) {
  const uint8_t data1_bytes[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  const uint8_t data2_bytes[] = {0xa1, 0xa2, 0xa3, 0xa4};
  std::vector<uint8_t> pssh_data;

  std::vector<uint8_t> box1 = MakePSSHBox(1, Key1());
  std::vector<uint8_t> data1(data1_bytes, data1_bytes + std::size(data1_bytes));
  AppendData(box1, data1);

  std::vector<uint8_t> box2 = MakePSSHBox(0);
  std::vector<uint8_t> data2(data2_bytes, data2_bytes + std::size(data2_bytes));
  AppendData(box2, data2);

  box1.insert(box1.end(), box2.begin(), box2.end());
  EXPECT_TRUE(GetPsshData(box1, CommonSystemSystemId(), &pssh_data));
  EXPECT_EQ(data1, pssh_data);
  EXPECT_NE(data2, pssh_data);
}

TEST_F(CencUtilsTest, NonPsshData) {
  // Create a non-'pssh' box.
  const uint8_t data[] = {
    0x00, 0x00, 0x00, 0x08,   // size = 8
    'p',  's',  's',  'g'
  };
  std::vector<uint8_t> non_pssh_box(data, data + std::size(data));
  EXPECT_FALSE(ValidatePsshInput(non_pssh_box));

  // Make a valid 'pssh' box.
  std::vector<uint8_t> pssh_box = MakePSSHBox(1, Key1());
  EXPECT_TRUE(ValidatePsshInput(pssh_box));

  // Concatenate the boxes together (|pssh_box| first).
  std::vector<uint8_t> boxes;
  boxes.insert(boxes.end(), pssh_box.begin(), pssh_box.end());
  boxes.insert(boxes.end(), non_pssh_box.begin(), non_pssh_box.end());
  EXPECT_FALSE(ValidatePsshInput(boxes));

  // Repeat with |non_pssh_box| first.
  boxes.clear();
  boxes.insert(boxes.end(), non_pssh_box.begin(), non_pssh_box.end());
  boxes.insert(boxes.end(), pssh_box.begin(), pssh_box.end());
  EXPECT_FALSE(ValidatePsshInput(boxes));
}

}  // namespace media
