// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"

#include <random>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "mojo/public/cpp/bindings/string_traits_wtf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/mojo/string16_mojom_traits.h"

namespace blink {

TEST(IDBMojomTraitsTest, IDBKeyBinary) {
  // Generate test data.
  std::mt19937 rng(5);
  size_t test_data_size = 10000;
  Vector<char> test_data(test_data_size);
  std::generate(test_data.begin(), test_data.end(), rng);
  scoped_refptr<SharedBuffer> input_data =
      SharedBuffer::Create(test_data.data(), test_data.size());
  Vector<uint8_t> input_vector = input_data->CopyAs<Vector<uint8_t>>();

  // Verify expectations.
  ASSERT_EQ(input_data->size(), test_data_size);
  ASSERT_EQ(input_vector.size(), test_data_size);

  // Create IDBKey binary key type mojom message.
  std::unique_ptr<IDBKey> input = IDBKey::CreateBinary(input_data);
  mojo::Message mojo_message = mojom::blink::IDBKey::SerializeAsMessage(&input);

  // Deserialize the mojo message.
  std::unique_ptr<IDBKey> output;
  ASSERT_TRUE(mojom::blink::IDBKey::DeserializeFromMessage(
      std::move(mojo_message), &output));
  scoped_refptr<SharedBuffer> output_data = output->Binary();
  Vector<uint8_t> output_vector = output_data->CopyAs<Vector<uint8_t>>();

  // Verify expectations.
  ASSERT_EQ(output_data->size(), test_data_size);
  ASSERT_EQ(output_vector.size(), test_data_size);
  ASSERT_EQ(input_vector, output_vector);
}

TEST(IDBMojomTraitsTest, IDBValue) {
  // Generate test data.
  std::mt19937 rng(5);
  size_t test_data_size = 10000;
  Vector<char> test_data(test_data_size);
  std::generate(test_data.begin(), test_data.end(), rng);
  scoped_refptr<SharedBuffer> input_data =
      SharedBuffer::Create(test_data.data(), test_data.size());
  Vector<uint8_t> input_vector = input_data->CopyAs<Vector<uint8_t>>();

  // Verify expectations.
  ASSERT_EQ(input_data->size(), test_data_size);
  ASSERT_EQ(input_vector.size(), test_data_size);

  // Create IDBValue mojom message.
  auto input =
      std::make_unique<IDBValue>(std::move(input_data), Vector<WebBlobInfo>());
  mojo::Message mojo_message =
      mojom::blink::IDBValue::SerializeAsMessage(&input);

  // Deserialize the mojo message.
  std::unique_ptr<IDBValue> output;
  ASSERT_TRUE(mojom::blink::IDBValue::DeserializeFromMessage(
      std::move(mojo_message), &output));
  scoped_refptr<SharedBuffer> output_data = output->Data();
  Vector<uint8_t> output_vector = output_data->CopyAs<Vector<uint8_t>>();

  // Verify expectations.
  ASSERT_EQ(output_data->size(), test_data_size);
  ASSERT_EQ(output_vector.size(), test_data_size);
  ASSERT_EQ(input_vector, output_vector);
}

}  // namespace blink
