// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"

#include <random>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
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
  wtf_size_t test_data_size = 10000;
  Vector<char> test_data(test_data_size);
  std::generate(test_data.begin(), test_data.end(), rng);
  scoped_refptr<base::RefCountedData<Vector<char>>> input_data =
      base::MakeRefCounted<base::RefCountedData<Vector<char>>>(
          Vector<char>(test_data));

  // Verify expectations.
  ASSERT_EQ(input_data->data.size(), test_data_size);
  ASSERT_EQ(test_data.size(), test_data_size);

  // Create IDBKey binary key type mojom message.
  std::unique_ptr<IDBKey> input = IDBKey::CreateBinary(input_data);
  mojo::Message mojo_message = mojom::blink::IDBKey::SerializeAsMessage(&input);

  // Deserialize the mojo message.
  std::unique_ptr<IDBKey> output;
  ASSERT_TRUE(mojom::blink::IDBKey::DeserializeFromMessage(
      std::move(mojo_message), &output));
  scoped_refptr<base::RefCountedData<Vector<char>>> output_data =
      output->Binary();

  // Verify expectations.
  ASSERT_EQ(output_data->data.size(), test_data_size);
  ASSERT_EQ(test_data, output_data->data);
}

TEST(IDBMojomTraitsTest, IDBValue) {
  // Generate test data.
  std::mt19937 rng(5);
  wtf_size_t test_data_size = 10000;
  Vector<char> test_data(test_data_size);
  std::generate(test_data.begin(), test_data.end(), rng);
  Vector<char> input_data = Vector<char>(test_data);

  // Verify expectations.
  ASSERT_EQ(input_data.size(), test_data_size);
  ASSERT_EQ(test_data.size(), test_data_size);

  // Create IDBValue mojom message.
  auto input =
      std::make_unique<IDBValue>(std::move(input_data), Vector<WebBlobInfo>());
  mojo::Message mojo_message =
      mojom::blink::IDBValue::SerializeAsMessage(&input);

  // Deserialize the mojo message.
  std::unique_ptr<IDBValue> output;
  ASSERT_TRUE(mojom::blink::IDBValue::DeserializeFromMessage(
      std::move(mojo_message), &output));
  const std::optional<Vector<char>>& output_data = output->Data();

  // Verify expectations.
  ASSERT_TRUE(output_data);
  ASSERT_EQ(output_data->size(), test_data_size);
  ASSERT_EQ(test_data, *output_data);
}

}  // namespace blink
