// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/wtf_serialization.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/variant_test_util.h"
#include "mojo/public/interfaces/bindings/tests/test_wtf_types.mojom-blink.h"
#include "mojo/public/interfaces/bindings/tests/test_wtf_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace mojo {
namespace test {
namespace {

const char kHelloWorld[] = "hello world";

// Replace the "o"s in "hello world" with "o"s with acute.
const char kUTF8HelloWorld[] = "hell\xC3\xB3 w\xC3\xB3rld";

class TestWTFImpl : public TestWTF {
 public:
  explicit TestWTFImpl(PendingReceiver<TestWTF> receiver)
      : receiver_(this, std::move(receiver)) {}

  // mojo::test::TestWTF implementation:
  void EchoString(const std::optional<std::string>& str,
                  EchoStringCallback callback) override {
    std::move(callback).Run(str);
  }

  void EchoStringArray(
      const std::optional<std::vector<std::optional<std::string>>>& arr,
      EchoStringArrayCallback callback) override {
    std::move(callback).Run(std::move(arr));
  }

  void EchoStringMap(
      const std::optional<
          base::flat_map<std::string, std::optional<std::string>>>& str_map,
      EchoStringMapCallback callback) override {
    std::move(callback).Run(std::move(str_map));
  }

 private:
  Receiver<TestWTF> receiver_;
};

class WTFTypesTest : public testing::Test {
 public:
  WTFTypesTest() {}

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

WTF::Vector<WTF::String> ConstructStringArray() {
  WTF::Vector<WTF::String> strs(4);
  // strs[0] is null.
  // strs[1] is empty.
  strs[1] = "";
  strs[2] = kHelloWorld;
  strs[3] = WTF::String::FromUTF8(kUTF8HelloWorld);

  return strs;
}

WTF::HashMap<WTF::String, WTF::String> ConstructStringMap() {
  WTF::HashMap<WTF::String, WTF::String> str_map;
  // A null string as value.
  str_map.insert("0", WTF::String());
  str_map.insert("1", kHelloWorld);
  str_map.insert("2", WTF::String::FromUTF8(kUTF8HelloWorld));

  return str_map;
}

void ExpectString(const WTF::String& expected_string,
                  base::OnceClosure closure,
                  const WTF::String& string) {
  EXPECT_EQ(expected_string, string);
  std::move(closure).Run();
}

void ExpectStringArray(std::optional<WTF::Vector<WTF::String>>* expected_arr,
                       base::OnceClosure closure,
                       const std::optional<WTF::Vector<WTF::String>>& arr) {
  EXPECT_EQ(*expected_arr, arr);
  std::move(closure).Run();
}

void ExpectStringMap(
    std::optional<WTF::HashMap<WTF::String, WTF::String>>* expected_map,
    base::OnceClosure closure,
    const std::optional<WTF::HashMap<WTF::String, WTF::String>>& map) {
  EXPECT_EQ(*expected_map, map);
  std::move(closure).Run();
}

}  // namespace

TEST_F(WTFTypesTest, Serialization_WTFVectorToWTFVector) {
  using MojomType = ArrayDataView<StringDataView>;

  WTF::Vector<WTF::String> strs = ConstructStringArray();
  auto cloned_strs = strs;

  mojo::Message message(0, 0, 0, 0, nullptr);
  mojo::internal::MessageFragment<
      typename mojo::internal::MojomTypeTraits<MojomType>::Data>
      fragment(message);
  constexpr const mojo::internal::ContainerValidateParams& validate_params =
      mojo::internal::GetArrayValidator<
          0, true, &mojo::internal::GetArrayValidator<0, false, nullptr>()>();
  mojo::internal::Serialize<MojomType>(cloned_strs, fragment, &validate_params);

  WTF::Vector<WTF::String> strs2;
  mojo::internal::Deserialize<MojomType>(fragment.data(), &strs2, &message);

  EXPECT_EQ(strs, strs2);
}

TEST_F(WTFTypesTest, Serialization_WTFVectorInlineCapacity) {
  using MojomType = ArrayDataView<StringDataView>;

  WTF::Vector<WTF::String, 1> strs(4);
  // strs[0] is null.
  // strs[1] is empty.
  strs[1] = "";
  strs[2] = kHelloWorld;
  strs[3] = WTF::String::FromUTF8(kUTF8HelloWorld);
  auto cloned_strs = strs;

  mojo::Message message(0, 0, 0, 0, nullptr);
  mojo::internal::MessageFragment<
      typename mojo::internal::MojomTypeTraits<MojomType>::Data>
      fragment(message);
  constexpr const mojo::internal::ContainerValidateParams& validate_params =
      mojo::internal::GetArrayValidator<
          0, true, &mojo::internal::GetArrayValidator<0, false, nullptr>()>();
  mojo::internal::Serialize<MojomType>(cloned_strs, fragment, &validate_params);

  WTF::Vector<WTF::String, 1> strs2;
  mojo::internal::Deserialize<MojomType>(fragment.data(), &strs2, &message);

  EXPECT_EQ(strs, strs2);
}

TEST_F(WTFTypesTest, Serialization_WTFVectorToStlVector) {
  using MojomType = ArrayDataView<StringDataView>;

  WTF::Vector<WTF::String> strs = ConstructStringArray();
  auto cloned_strs = strs;

  mojo::Message message(0, 0, 0, 0, nullptr);
  mojo::internal::MessageFragment<
      typename mojo::internal::MojomTypeTraits<MojomType>::Data>
      fragment(message);
  constexpr const mojo::internal::ContainerValidateParams& validate_params =
      mojo::internal::GetArrayValidator<
          0, true, &mojo::internal::GetArrayValidator<0, false, nullptr>()>();
  mojo::internal::Serialize<MojomType>(cloned_strs, fragment, &validate_params);

  std::vector<std::optional<std::string>> strs2;
  mojo::internal::Deserialize<MojomType>(fragment.data(), &strs2, &message);

  ASSERT_EQ(4u, strs2.size());
  EXPECT_FALSE(strs2[0]);
  EXPECT_EQ("", *strs2[1]);
  EXPECT_EQ(kHelloWorld, *strs2[2]);
  EXPECT_EQ(kUTF8HelloWorld, *strs2[3]);
}

TEST_F(WTFTypesTest, Serialization_PublicAPI) {
  blink::TestWTFStructPtr input(blink::TestWTFStruct::New(kHelloWorld, 42));

  blink::TestWTFStructPtr cloned_input = input.Clone();

  auto data = blink::TestWTFStruct::Serialize(&input);

  blink::TestWTFStructPtr output;
  ASSERT_TRUE(blink::TestWTFStruct::Deserialize(std::move(data), &output));
  EXPECT_TRUE(cloned_input.Equals(output));
}

TEST_F(WTFTypesTest, SendString) {
  Remote<blink::TestWTF> remote;
  TestWTFImpl impl(
      ConvertPendingReceiver<TestWTF>(remote.BindNewPipeAndPassReceiver()));

  WTF::Vector<WTF::String> strs = ConstructStringArray();

  for (size_t i = 0; i < strs.size(); ++i) {
    base::RunLoop loop;
    // Test that a WTF::String is unchanged after the following conversion:
    //   - serialized;
    //   - deserialized as std::optional<std::string>;
    //   - serialized;
    //   - deserialized as WTF::String.
    remote->EchoString(
        strs[i], base::BindOnce(&ExpectString, strs[i], loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(WTFTypesTest, SendStringArray) {
  Remote<blink::TestWTF> remote;
  TestWTFImpl impl(
      ConvertPendingReceiver<TestWTF>(remote.BindNewPipeAndPassReceiver()));

  std::optional<WTF::Vector<WTF::String>> arrs[3];
  // arrs[0] is empty.
  arrs[0].emplace();
  // arrs[1] is null.
  arrs[2] = ConstructStringArray();

  for (size_t i = 0; i < std::size(arrs); ++i) {
    base::RunLoop loop;
    // Test that a std::optional<WTF::Vector<WTF::String>> is unchanged after
    // the following conversion:
    //   - serialized;
    //   - deserialized as
    //     std::optional<std::vector<std::optional<std::string>>>;
    //   - serialized;
    //   - deserialized as std::optional<WTF::Vector<WTF::String>>.
    remote->EchoStringArray(
        arrs[i], base::BindOnce(&ExpectStringArray, base::Unretained(&arrs[i]),
                                loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(WTFTypesTest, SendStringMap) {
  Remote<blink::TestWTF> remote;
  TestWTFImpl impl(
      ConvertPendingReceiver<TestWTF>(remote.BindNewPipeAndPassReceiver()));

  std::optional<WTF::HashMap<WTF::String, WTF::String>> maps[3];
  // maps[0] is empty.
  maps[0].emplace();
  // maps[1] is null.
  maps[2] = ConstructStringMap();

  for (size_t i = 0; i < std::size(maps); ++i) {
    base::RunLoop loop;
    // Test that a std::optional<WTF::HashMap<WTF::String, WTF::String>> is
    // unchanged after the following conversion:
    //   - serialized;
    //   - deserialized as std::optional<
    //         base::flat_map<std::string, std::optional<std::string>>>;
    //   - serialized;
    //   - deserialized as std::optional<WTF::HashMap<WTF::String,
    //     WTF::String>>.
    remote->EchoStringMap(
        maps[i], base::BindOnce(&ExpectStringMap, base::Unretained(&maps[i]),
                                loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(WTFTypesTest, NestedStruct_CloneAndEquals) {
  auto a = blink::TestWTFStructWrapper::New();
  a->nested_struct = blink::TestWTFStruct::New("foo", 1);
  a->array_struct.push_back(blink::TestWTFStruct::New("bar", 2));
  a->array_struct.push_back(blink::TestWTFStruct::New("bar", 3));
  a->map_struct.insert(blink::TestWTFStruct::New("baz", 4),
                       blink::TestWTFStruct::New("baz", 5));
  auto b = a.Clone();
  EXPECT_EQ(a, b);
  EXPECT_EQ(2u, b->array_struct.size());
  EXPECT_EQ(1u, b->map_struct.size());
  EXPECT_NE(blink::TestWTFStructWrapper::New(), a);
  EXPECT_NE(blink::TestWTFStructWrapper::New(), b);
}

}  // namespace test
}  // namespace mojo
