// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/status.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "base/json/json_writer.h"
#include "media/base/media_serializers.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;

namespace media {

class UselessThingToBeSerialized {
 public:
  explicit UselessThingToBeSerialized(const char* name) : name_(name) {}
  const char* name_;
};

namespace internal {

template <>
struct MediaSerializer<UselessThingToBeSerialized> {
  static base::Value Serialize(const UselessThingToBeSerialized& t) {
    return base::Value(t.name_);
  }
};

}  // namespace internal

struct NoOkStatusTypeTraits {
  enum class Codes : StatusCodeType { kFoo = 0, kBar = 1, kBaz = 2 };
  static constexpr StatusGroupType Group() { return "NoDefaultNoOkType"; }
};

struct CustomDefaultValue {
  enum class Codes : StatusCodeType { kFoo = 0, kBar = 1, kBaz = 2 };
  static constexpr StatusGroupType Group() { return "CustomOkCode"; }
  static constexpr Codes OkEnumValue() { return Codes::kFoo; }
};

struct ZeroValueOkTypeTraits {
  enum class Codes : StatusCodeType { kOk = 0, kFoo = 1, kBar = 2, kBaz = 3 };
  static constexpr StatusGroupType Group() { return "ZeroValueOkTypeTraits"; }
};

struct NonZeroOkTypeTraits {
  enum class Codes : StatusCodeType {
    kOk = 100,
    kFoo = 0,
  };
  static constexpr StatusGroupType Group() { return "GroupWithNonZeroOkType"; }
};

struct TraitsWithCustomUKMSerializer {
  enum class Codes { kFoo, kBar };
  static constexpr StatusGroupType Group() { return "UKMSerializerCode"; }
  static uint32_t PackExtraData(const internal::StatusData& info) {
    auto maybe_key = info.data.GetDict().FindInt("might_exist_key");
    if (maybe_key.has_value())
      return *maybe_key * 77;
    return 0;
  }
};

struct TraitsWithDataPacking {
  enum class Codes { kOk, kFail };
  struct PackThis {
    int a;
    int b;
    std::string c;
  };
  static constexpr StatusGroupType Group() { return "GroupWithDataPacking"; }
  static void OnCreateFrom(TypedStatus<TraitsWithDataPacking>* status,
                           const PackThis& data) {
    status->WithData("DataA", data.a);
    status->WithData("DataB", data.b);
    status->WithData("DataC", data.c);
  }
};

struct TraitsWithDefaultNamedMessage {
  enum class Codes { kFail1, kFail2, kFail3 };
  static constexpr StatusGroupType Group() { return "GroupWithDefaultNames"; }
  static constexpr std::string_view ReadableCodeName(Codes code) {
    switch (code) {
      case Codes::kFail1:
        return "Failure1";
      case Codes::kFail2:
        return "Failure2";
      case Codes::kFail3:
        return "Failure3";
    }
  }
};

class StatusTest : public testing::Test {
 public:
  using NormalStatus = TypedStatus<ZeroValueOkTypeTraits>;

  NormalStatus DontFail() { return OkStatus(); }

  // Return a failure, with a line number.  Record the lower and upper line
  // number limits so that we can make sure that the error's line is bounded.
  constexpr static int lower_line_limit_ = __LINE__;
  NormalStatus FailEasily() {
    return NormalStatus(NormalStatus::Codes::kFoo, "Message");
  }
  constexpr static int upper_line_limit_ = __LINE__;

  NormalStatus FailRecursively(unsigned int count) {
    if (!count) {
      return FailEasily();
    }
    return FailRecursively(count - 1).AddHere();
  }

  template <typename T>
  NormalStatus FailWithData(const char* key, const T& t) {
    return NormalStatus(NormalStatus::Codes::kFoo, "Message", FROM_HERE)
        .WithData(key, t);
  }

  NormalStatus FailWithCause() {
    NormalStatus err = FailEasily();
    return FailEasily().AddCause(std::move(err));
  }

  NormalStatus DoSomethingGiveItBack(NormalStatus me) {
    me.WithData("data", "Hey you! psst! Help me outta here! I'm trapped!");
    return me;
  }
};

TEST_F(StatusTest, DifferentModesOfConstruction) {
  // Can construct any type with OkStatus
  NormalStatus ok = OkStatus();
  ASSERT_TRUE(ok.is_ok());

  // Can construct implicitly from a code
  NormalStatus ok2 = NormalStatus::Codes::kOk;
  ASSERT_TRUE(ok.is_ok());

  // Can construct implicitly from a {code, message} braced initializer.
  NormalStatus foo = {NormalStatus::Codes::kFoo, "msg"};
  ASSERT_EQ(foo.code(), NormalStatus::Codes::kFoo);
  ASSERT_EQ(foo.message(), "msg");

  // Can construct explicitly from a code and message
  NormalStatus foo2 = NormalStatus(NormalStatus::Codes::kFoo, "msg2");
  ASSERT_EQ(foo2.code(), NormalStatus::Codes::kFoo);
  ASSERT_EQ(foo2.message(), "msg2");

  using PackingStatus = TypedStatus<TraitsWithDataPacking>;
  TraitsWithDataPacking::PackThis data = {7, 3, "apple pie"};

  // Can construct implicitly from a {code, data} for a type with OnCreateFrom
  // in it's traits
  PackingStatus packed = {PackingStatus::Codes::kFail, data};
  ASSERT_EQ(packed.code(), PackingStatus::Codes::kFail);
  ASSERT_EQ(packed.message(), "");
  // Keep serialized around, accessing |data| from it inline causes it
  // to be destructed and |unpacked| to be used after being freed.
  auto serialized = MediaSerializeForTesting(packed).TakeDict();
  auto* unpacked = serialized.FindDict("data");
  ASSERT_NE(unpacked, nullptr);
  ASSERT_EQ(unpacked->size(), 3ul);
  ASSERT_EQ(unpacked->FindInt("DataA"), 7);
  ASSERT_EQ(unpacked->FindInt("DataB"), 3);
  ASSERT_EQ(*unpacked->FindString("DataC"), "apple pie");

  // Can construct implicitly from a {code, "message", data} for a type with
  // OnCreateFrom in it's traits
  PackingStatus packed2 = {PackingStatus::Codes::kFail, "*explosion*", data};
  ASSERT_EQ(packed2.code(), PackingStatus::Codes::kFail);
  ASSERT_EQ(packed2.message(), "*explosion*");
  serialized = MediaSerializeForTesting(packed).TakeDict();
  unpacked = serialized.FindDict("data");
  ASSERT_NE(unpacked, nullptr);
  ASSERT_EQ(unpacked->size(), 3ul);
  ASSERT_EQ(unpacked->FindInt("DataA"), 7);
  ASSERT_EQ(unpacked->FindInt("DataB"), 3);
  ASSERT_EQ(*unpacked->FindString("DataC"), "apple pie");

  NormalStatus root = NormalStatus::Codes::kFoo;
  PackingStatus derived = {PackingStatus::Codes::kFail, std::move(root)};
  serialized = MediaSerializeForTesting(derived).TakeDict();
  unpacked = serialized.FindDict("cause");
  ASSERT_NE(unpacked, nullptr);
  ASSERT_EQ(unpacked->size(), 5ul);
  ASSERT_EQ(unpacked->FindInt("code").value_or(0),
            static_cast<int>(NormalStatus::Codes::kFoo));

  root = NormalStatus::Codes::kFoo;
  derived = {PackingStatus::Codes::kFail, "blah", std::move(root)};
  serialized = MediaSerializeForTesting(derived).TakeDict();
  unpacked = serialized.FindDict("cause");
  ASSERT_EQ(*serialized.FindString("message"), "blah");
  ASSERT_NE(unpacked, nullptr);
  ASSERT_EQ(unpacked->size(), 5ul);
  ASSERT_EQ(unpacked->FindInt("code").value_or(0),
            static_cast<int>(NormalStatus::Codes::kFoo));
}


TEST_F(StatusTest, StaticOKMethodGivesCorrectSerialization) {
  NormalStatus ok = DontFail();
  base::Value actual = MediaSerializeForTesting(ok);
  ASSERT_EQ(actual.GetString(), "Ok");
}

TEST_F(StatusTest, SingleLayerError) {
  NormalStatus failed = FailEasily();
  base::Value actual = MediaSerializeForTesting(failed);
  const base::DictValue& actual_dict = actual.GetDict();
  ASSERT_EQ(actual_dict.size(), 5ul);
  ASSERT_EQ(*actual_dict.FindString("message"), "Message");
  ASSERT_EQ(actual_dict.FindList("stack")->size(), 1ul);
  ASSERT_EQ(actual_dict.Find("cause"), nullptr);
  ASSERT_EQ(actual_dict.FindDict("data")->size(), 0ul);

  const auto& stack = *actual_dict.FindList("stack");
  ASSERT_EQ(stack[0].GetDict().size(), 2ul);  // line and file

  // This is a bit fragile, since it's dependent on the file layout.  Just check
  // that it's somewhere in the `FailEasily`` function.
  int line = stack[0].GetDict().FindInt("line").value_or(-1);
  ASSERT_GT(line, lower_line_limit_);
  ASSERT_LT(line, upper_line_limit_);
  ASSERT_THAT(*stack[0].GetDict().FindString("file"),
              HasSubstr("status_unittest.cc"));
}

TEST_F(StatusTest, MultipleErrorLayer) {
  NormalStatus failed = FailRecursively(3);
  base::Value actual = MediaSerializeForTesting(failed);
  const base::DictValue& actual_dict = actual.GetDict();
  ASSERT_EQ(actual_dict.size(), 5ul);
  ASSERT_EQ(*actual_dict.FindString("message"), "Message");
  ASSERT_EQ(actual_dict.FindList("stack")->size(), 4ul);
  ASSERT_EQ(actual_dict.Find("cause"), nullptr);
  ASSERT_EQ(actual_dict.FindDict("data")->size(), 0ul);

  const auto& stack = *actual_dict.FindList("stack");
  ;
  ASSERT_EQ(stack[0].GetDict().size(), 2ul);  // line and file
}

TEST_F(StatusTest, CanHaveData) {
  NormalStatus failed = FailWithData("example", "data");
  base::Value actual = MediaSerializeForTesting(failed);
  const base::DictValue& actual_dict = actual.GetDict();
  ASSERT_EQ(actual_dict.size(), 5ul);
  ASSERT_EQ(*actual_dict.FindString("message"), "Message");
  ASSERT_EQ(actual_dict.FindList("stack")->size(), 1ul);
  ASSERT_EQ(actual_dict.Find("cause"), nullptr);
  ASSERT_EQ(actual_dict.FindDict("data")->size(), 1ul);

  const auto& stack = *actual_dict.FindList("stack");
  ;
  ASSERT_EQ(stack[0].GetDict().size(), 2ul);  // line and file

  ASSERT_EQ(*actual_dict.FindDict("data")->FindString("example"), "data");
}

TEST_F(StatusTest, CanUseCustomSerializer) {
  NormalStatus failed =
      FailWithData("example", UselessThingToBeSerialized("F"));
  base::Value actual = MediaSerializeForTesting(failed);
  const base::DictValue& actual_dict = actual.GetDict();
  ASSERT_EQ(actual_dict.size(), 5ul);
  ASSERT_EQ(*actual_dict.FindString("message"), "Message");
  ASSERT_EQ(actual_dict.FindList("stack")->size(), 1ul);
  ASSERT_EQ(actual_dict.Find("cause"), nullptr);
  ASSERT_EQ(actual_dict.FindDict("data")->size(), 1ul);

  const auto& stack = *actual_dict.FindList("stack");
  ;
  ASSERT_EQ(stack[0].GetDict().size(), 2ul);  // line and file

  ASSERT_EQ(*actual_dict.FindDict("data")->FindString("example"), "F");
}

TEST_F(StatusTest, CausedByHasVector) {
  NormalStatus causal = FailWithCause();
  base::Value actual = MediaSerializeForTesting(causal);
  const base::DictValue& actual_dict = actual.GetDict();
  ASSERT_EQ(actual_dict.size(), 6ul);
  ASSERT_EQ(*actual_dict.FindString("message"), "Message");
  ASSERT_EQ(actual_dict.FindList("stack")->size(), 1ul);
  ASSERT_EQ(actual_dict.FindDict("data")->size(), 0ul);
  ASSERT_NE(actual_dict.Find("cause"), nullptr);

  const base::DictValue* nested = actual_dict.FindDict("cause");
  ASSERT_NE(nested, nullptr);
  ASSERT_EQ(nested->size(), 5ul);
  ASSERT_EQ(*nested->FindString("message"), "Message");
  ASSERT_EQ(nested->FindList("stack")->size(), 1ul);
  ASSERT_EQ(nested->Find("cause"), nullptr);
  ASSERT_EQ(nested->FindDict("data")->size(), 0ul);
}

TEST_F(StatusTest, CausedByCanAssignCopy) {
  NormalStatus causal = FailWithCause();
  NormalStatus copy_causal = causal;
  base::Value causal_serialized = MediaSerializeForTesting(causal);
  base::Value copy_causal_serialized = MediaSerializeForTesting(copy_causal);

  base::DictValue* original = causal_serialized.GetDict().FindDict("cause");
  ASSERT_EQ(original->size(), 5ul);
  ASSERT_EQ(*original->FindString("message"), "Message");
  ASSERT_EQ(original->FindList("stack")->size(), 1ul);
  ASSERT_EQ(original->Find("cause"), nullptr);
  ASSERT_EQ(original->FindDict("data")->size(), 0ul);

  base::DictValue* copied = copy_causal_serialized.GetDict().FindDict("cause");
  ASSERT_EQ(copied->size(), 5ul);
  ASSERT_EQ(*copied->FindString("message"), "Message");
  ASSERT_EQ(copied->FindList("stack")->size(), 1ul);
  ASSERT_EQ(copied->Find("cause"), nullptr);
  ASSERT_EQ(copied->FindDict("data")->size(), 0ul);
}

TEST_F(StatusTest, CanCopyEasily) {
  NormalStatus failed = FailEasily();
  NormalStatus withData = DoSomethingGiveItBack(failed);

  base::Value actual = MediaSerializeForTesting(failed);
  const base::DictValue& actual_dict = actual.GetDict();
  ASSERT_EQ(actual_dict.size(), 5ul);
  ASSERT_EQ(*actual_dict.FindString("message"), "Message");
  ASSERT_EQ(actual_dict.FindList("stack")->size(), 1ul);
  ASSERT_EQ(actual_dict.Find("cause"), nullptr);
  ASSERT_EQ(actual_dict.FindDict("data")->size(), 0ul);

  actual = MediaSerializeForTesting(withData);
  ASSERT_EQ(actual_dict.size(), 5ul);
  ASSERT_EQ(*actual_dict.FindString("message"), "Message");
  ASSERT_EQ(actual_dict.FindList("stack")->size(), 1ul);
  ASSERT_EQ(actual_dict.Find("cause"), nullptr);
  ASSERT_EQ(actual_dict.FindDict("data")->size(), 1ul);
}

TEST_F(StatusTest, Okayness) {
  EXPECT_FALSE(
      TypedStatus<ZeroValueOkTypeTraits>(ZeroValueOkTypeTraits::Codes::kFoo)
          .is_ok());
  EXPECT_TRUE(
      TypedStatus<ZeroValueOkTypeTraits>(ZeroValueOkTypeTraits::Codes::kOk)
          .is_ok());

  EXPECT_FALSE(
      TypedStatus<NonZeroOkTypeTraits>(NonZeroOkTypeTraits::Codes::kFoo)
          .is_ok());
  EXPECT_TRUE(TypedStatus<NonZeroOkTypeTraits>(NonZeroOkTypeTraits::Codes::kOk)
                  .is_ok());
}

TEST_F(StatusTest, MustHaveOkOrHelperMethod) {
  auto nook = internal::StatusTraitsHelper<NoOkStatusTypeTraits>::OkEnumValue();
  ASSERT_FALSE(nook.has_value());

  auto kok = internal::StatusTraitsHelper<ZeroValueOkTypeTraits>::OkEnumValue();
  ASSERT_TRUE(kok.has_value());
  ASSERT_EQ(*kok, ZeroValueOkTypeTraits::Codes::kOk);

  auto custom = internal::StatusTraitsHelper<CustomDefaultValue>::OkEnumValue();
  ASSERT_TRUE(custom.has_value());
  ASSERT_EQ(*custom, CustomDefaultValue::Codes::kFoo);
}

TEST_F(StatusTest, OkStatusInitializesToOk) {
  // Construction from the return value of OkStatus() should be `kOk`, for any
  // status traits that has `kOk`.  We only test explicit construction, though
  // this is probably used as an implicit construction in practice when it's
  // a return value.
  EXPECT_EQ(TypedStatus<ZeroValueOkTypeTraits>(OkStatus()).code(),
            ZeroValueOkTypeTraits::Codes::kOk);
  EXPECT_EQ(TypedStatus<NonZeroOkTypeTraits>(OkStatus()).code(),
            NonZeroOkTypeTraits::Codes::kOk);
}

}  // namespace media
