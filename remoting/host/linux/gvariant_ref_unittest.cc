// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gvariant_ref.h"

#include <glib.h>

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::gvariant {
namespace {
// A type whose mapping provides a From static method.
template <typename T, Type C>
concept ProvidesFrom = requires(T v) { GVariantRef<C>::template From<T>(v); };

// A type whose mapping provides a TryFrom static method.
template <typename T, Type C>
concept ProvidesTryFrom =
    requires(T v) { GVariantRef<C>::template TryFrom<T>(v); };

// A type whose mapping provides an Into static method for the provided Type.
template <typename T, Type C>
concept ProvidesInto = requires(GVariantRef<C> v) { v.template Into<T>(); };

// A type whose mapping provides a TryInto static method.
template <typename T, Type C>
concept ProvidesTryInto =
    requires(GVariantRef<C> v) { v.template TryInto<T>(); };

template <Type Parse, Type Return = Parse>
GVariantRef<Return> GVariantParse(const char* text) {
  GVariant* variant =
      g_variant_parse(Parse.gvariant_type(), text, nullptr, nullptr, nullptr);
  CHECK(variant);
  return GVariantRef<Return>::TakeUnchecked(variant);
}

}  // namespace

TEST(GVariantRefTest, EncodesBasicTypes) {
  EXPECT_EQ(GVariantParse<"b">("true"), GVariantRef<"b">::From(true));
  EXPECT_EQ(GVariantParse<"y">("5"), GVariantRef<"y">::From(std::uint8_t{5}));
  EXPECT_EQ(GVariantParse<"n">("-17"),
            GVariantRef<"n">::From(std::int16_t{-17}));
  EXPECT_EQ(GVariantParse<"q">("103"),
            GVariantRef<"q">::From(std::uint16_t{103}));
  EXPECT_EQ(GVariantParse<"i">("603"),
            GVariantRef<"i">::From(std::int32_t{603}));
  EXPECT_EQ(GVariantParse<"u">("57"),
            GVariantRef<"u">::From(std::uint32_t{57}));
  EXPECT_EQ(GVariantParse<"x">("-2037"),
            GVariantRef<"x">::From(std::int64_t{-2037}));
  EXPECT_EQ(GVariantParse<"t">("169"),
            GVariantRef<"t">::From(std::uint64_t{169}));
  EXPECT_EQ(GVariantParse<"d">("17.5"), GVariantRef<"d">::From(double{17.5}));

  static_assert(ProvidesFrom<std::int32_t, "i">);
  static_assert(!ProvidesFrom<std::int32_t, "u">);
  static_assert(ProvidesFrom<std::int32_t, "*">);
  static_assert(ProvidesFrom<std::int32_t, "?">);
  static_assert(!ProvidesTryFrom<std::int32_t, "u">);
}

TEST(GVariantRefTest, DecodesBasicTypes) {
  EXPECT_EQ(true, GVariantParse<"b">("true").Into<bool>());
  EXPECT_EQ(base::ok(std::uint8_t{124}),
            (GVariantParse<"y", "*">("124").TryInto<std::uint8_t>()));
  EXPECT_EQ(std::int16_t{93}, GVariantParse<"n">("93").Into<std::int16_t>());
  EXPECT_EQ(base::ok(std::uint16_t{567}),
            (GVariantParse<"q", "*">("567").TryInto<std::uint16_t>()));
  EXPECT_EQ(std::int32_t{-91643},
            GVariantParse<"i">("-91643").Into<std::int32_t>());
  EXPECT_EQ(base::ok(std::uint32_t{7832}),
            (GVariantParse<"u", "*">("7832").TryInto<std::uint32_t>()));
  EXPECT_EQ(std::int64_t{9582},
            GVariantParse<"x">("9582").Into<std::int64_t>());
  EXPECT_EQ(base::ok(std::uint64_t{3245}),
            (GVariantParse<"t", "*">("3245").TryInto<std::uint64_t>()));
  EXPECT_EQ(double{72.5}, GVariantParse<"d">("72.5").Into<double>());

  EXPECT_FALSE((GVariantParse<"i", "*">("-12").TryInto<double>().has_value()));

  static_assert(ProvidesInto<std::int32_t, "i">);
  static_assert(!ProvidesInto<std::int32_t, "u">);
  static_assert(ProvidesTryInto<std::int32_t, "*">);
  static_assert(ProvidesTryInto<std::int32_t, "?">);
  static_assert(!ProvidesTryInto<std::int32_t, "u">);
}

TEST(GVariantRefTest, Strings) {
  EXPECT_EQ(base::ok(GVariantParse<"s">("'Hello sailor!'")),
            GVariantRef<>::TryFrom(std::string("Hello sailor!")));
  EXPECT_EQ(GVariantParse<"s">("'Ahoy sailor!'"),
            GVariantRef<>::From(std::string_view("Ahoy sailor!")));
  EXPECT_EQ(GVariantParse<"s">("'So long sailor!'"),
            GVariantRef<>::From("So long sailor!"));

  EXPECT_EQ(
      base::ok(std::string("Welcome back!")),
      (GVariantParse<"s", "*">("'Welcome back!'").TryInto<std::string>()));
  EXPECT_EQ(std::string("Welcome back, again!"),
            GVariantParse<"s">("'Welcome back, again!'").Into<std::string>());

  EXPECT_FALSE(GVariantRef<"s">::TryFrom("Invalid\x85string").has_value());
  EXPECT_FALSE(
      (GVariantParse<"i", "*">("-5").TryInto<std::string>().has_value()));

  static_assert(!ProvidesTryInto<std::string_view, "*">);
  static_assert(!ProvidesTryInto<char*, "*">);
  static_assert(!ProvidesTryInto<const char*, "*">);
}

TEST(GVariantRefTest, Optional) {
  EXPECT_EQ(GVariantParse<"mi">("just 16"),
            GVariantRef<>::From(std::optional(std::int32_t{16})));
  EXPECT_EQ(GVariantParse<"md">("nothing"),
            GVariantRef<"md">::From(std::optional<double>()));
  EXPECT_EQ(GVariantParse<"ms">("just 'banana'"),
            GVariantRef<>::From(std::optional("banana")));
  EXPECT_EQ(GVariantParse<"ms">("nothing"),
            GVariantRef<"ms">::From(std::optional<std::string_view>()));
  // Indefinite type is okay if the option contains a concrete value.
  EXPECT_EQ(base::ok(GVariantParse<"mn">("just 539")),
            GVariantRef<>::TryFrom(
                std::optional(GVariantRef<>::From(std::int16_t{539}))));

  EXPECT_EQ(
      std::optional(std::string("apple")),
      GVariantParse<"ms">("just 'apple'").Into<std::optional<std::string>>());
  EXPECT_EQ(
      std::optional<std::uint32_t>(),
      GVariantParse<"mu">("nothing").Into<std::optional<std::uint32_t>>());
  EXPECT_EQ(base::ok(std::optional(std::string("peach"))),
            (GVariantParse<"ms", "*">("just 'peach'")
                 .TryInto<std::optional<std::string>>()));
  EXPECT_EQ(base::ok(std::optional<std::uint32_t>()),
            (GVariantParse<"mi", "*">("nothing")
                 .TryInto<std::optional<std::int32_t>>()));

  EXPECT_FALSE((GVariantParse<"i", "*">("15")
                    .TryInto<std::optional<std::int32_t>>()
                    .has_value()));
  EXPECT_FALSE((GVariantParse<"mi", "*">("27")
                    .TryInto<std::optional<std::uint8_t>>()
                    .has_value()));
  // Can't create empty invariant of indefinite type.
  EXPECT_FALSE(
      GVariantRef<>::TryFrom(std::optional<GVariantRef<>>()).has_value());

  // An indefinite type can't be used with From(), but can be used with other
  // operations supported by the inner type.
  static_assert(!ProvidesFrom<std::optional<GVariantRef<>>, "m*"> &&
                ProvidesTryFrom<std::optional<GVariantRef<>>, "m*"> &&
                ProvidesInto<std::optional<GVariantRef<>>, "m*"> &&
                ProvidesTryInto<std::optional<GVariantRef<>>, "m*">);
}

TEST(GVariantRefTest, Vector) {
  EXPECT_EQ(GVariantParse<"as">("[]"),
            GVariantRef<>::From(std::vector<const char*>()));
  EXPECT_EQ(
      GVariantParse<"an">("[5, -2, 45, -367, 97, -8]"),
      GVariantRef<>::From(std::vector<std::int16_t>{5, -2, 45, -367, 97, -8}));

  EXPECT_EQ(base::ok(std::vector<double>()),
            (GVariantParse<"ad", "*">("[]").TryInto<std::vector<double>>()));
  EXPECT_EQ(
      (std::vector<std::string>{"cabbage", "broccoli", "kale", "cauliflower"}),
      GVariantParse<"as">("['cabbage', 'broccoli', 'kale', 'cauliflower']")
          .Into<std::vector<std::string>>());

  EXPECT_FALSE((GVariantParse<"i", "*">("7")
                    .TryInto<std::vector<std::int32_t>>()
                    .has_value()));
  EXPECT_FALSE((GVariantParse<"ai", "*">("[]")
                    .TryInto<std::vector<std::uint8_t>>()
                    .has_value()));

  // An indefinite type can't be used with From(), but can be used with other
  // operations supported by the inner type.
  static_assert(!ProvidesFrom<std::vector<GVariantRef<>>, "a*"> &&
                ProvidesTryFrom<std::vector<GVariantRef<>>, "a*"> &&
                ProvidesInto<std::vector<GVariantRef<>>, "a*"> &&
                ProvidesTryInto<std::vector<GVariantRef<>>, "a*">);
}

TEST(GVariantRefTest, Map) {
  EXPECT_EQ(GVariantParse<"a{us}">("{}"),
            GVariantRef<>::From(std::map<std::uint32_t, const char*>()));
  EXPECT_EQ(
      GVariantParse<"a{sb}">(
          "{'a': true, 'b': false, 'c': false, 'd': false, 'e': true}"),
      GVariantRef<>::From(std::map<std::string_view, bool>{
          {"a", true}, {"b", false}, {"c", false}, {"d", false}, {"e", true}}));
  EXPECT_EQ(GVariantParse<"a{ib}">(
                "{1: true, 2: false, 3: false, 4: false, 5: true}"),
            GVariantRef<"a{ib}">::From(std::map<std::int32_t, bool>{
                {1, true}, {2, false}, {3, false}, {4, false}, {5, true}}));

  EXPECT_EQ(base::ok(std::map<bool, std::uint8_t>()),
            (GVariantParse<"a{by}", "*">("{}")
                 .TryInto<std::map<bool, std::uint8_t>>()));
  EXPECT_EQ(
      (std::map<std::int32_t, double>{{1, 1}, {2, 0.5}, {4, 0.25}, {8, 0.125}}),
      (GVariantParse<"a{id}">("{1: 1, 2: 0.5, 4: 0.25, 8: 0.125}")
           .Into<std::map<int32_t, double>>()));

  EXPECT_FALSE((GVariantParse<"i", "*">("-3")
                    .TryInto<std::map<std::uint32_t, std::uint8_t>>()
                    .has_value()));
  EXPECT_FALSE((GVariantParse<"a{ib}", "*">("{}")
                    .TryInto<std::map<std::uint32_t, std::uint8_t>>()
                    .has_value()));

  // An indefinite type can't be used with From(), but can be used with other
  // operations supported by the inner types.
  using IndefiniteValue = std::map<std::int64_t, GVariantRef<>>;
  using IndefiniteKey = std::map<GVariantRef<"?">, Boxed<GVariantRef<>>>;
  static_assert(!ProvidesFrom<IndefiniteValue, "a{x*}"> &&
                ProvidesTryFrom<IndefiniteValue, "a{x*}"> &&
                ProvidesInto<IndefiniteValue, "a{x*}"> &&
                ProvidesTryInto<IndefiniteValue, "a{x*}">);
  static_assert(!ProvidesFrom<IndefiniteKey, "a{?v}"> &&
                ProvidesTryFrom<IndefiniteKey, "a{?v}"> &&
                ProvidesInto<IndefiniteKey, "a{?v}"> &&
                ProvidesTryInto<IndefiniteKey, "a{?v}">);
}

TEST(GVariantRefTest, Pair) {
  EXPECT_EQ(
      GVariantParse<"{us}">("{31, 'xyzzy'}"),
      GVariantRef<>::From(std::pair<std::uint32_t, const char*>(31, "xyzzy")));
  EXPECT_EQ(GVariantParse<"{xt}">("{-64, 64}"),
            GVariantRef<"{xt}">::From(
                std::pair<std::int64_t, std::uint64_t>(-64, 64)));

  EXPECT_EQ(base::ok(std::pair<bool, std::uint8_t>(true, 9)),
            (GVariantParse<"{by}", "*">("{true, 9}")
                 .TryInto<std::pair<bool, std::uint8_t>>()));
  EXPECT_EQ((std::pair<double, std::string>(6.5, "poof")),
            (GVariantParse<"{ds}">("{6.5, 'poof'}")
                 .Into<std::pair<double, std::string>>()));

  EXPECT_FALSE((GVariantParse<"{uu}", "*">("{5, 7}")
                    .TryInto<std::pair<std::uint32_t, std::uint8_t>>()
                    .has_value()));
}

TEST(GVariantRefTest, Range) {
  EXPECT_EQ(GVariantParse<"ab">("[]"),
            GVariantRef<>::From(std::array<bool, 0>()));
  EXPECT_EQ(
      GVariantParse<"as">("['1', '2', '3', '4', '5']"),
      GVariantRef<>::From(std::set<std::string>{"5", "4", "3", "2", "1"}));

  // An indefinite type can't be used with From(), but can be used with
  // TryFrom().
  static_assert(!ProvidesFrom<std::array<GVariantRef<>, 4>, "a*"> &&
                ProvidesTryFrom<std::array<GVariantRef<>, 4>, "a*">);
}

TEST(GVariantRefTest, Tuple) {
  EXPECT_EQ(GVariantParse<"()">("()"), GVariantRef<>::From(std::tuple()));
  EXPECT_EQ(GVariantParse<"(ybmds)">("(63, true, 3.0, 'Hello!')"),
            GVariantRef<>::From(std::tuple(
                std::uint8_t{63}, true, std::optional(double{3.0}), "Hello!")));

  EXPECT_EQ(base::ok(std::tuple()),
            (GVariantParse<"()", "*">("()").TryInto<std::tuple<>>()));
  EXPECT_EQ(
      std::tuple(std::string("fancy"), false, double{-6.75}, std::int64_t{512}),
      (GVariantParse<"(sbdx)">("('fancy', false, -6.75, 512)")
           .Into<std::tuple<std::string, bool, double, std::int64_t>>()));

  EXPECT_FALSE((GVariantParse<"i", "*">("4")
                    .TryInto<std::tuple<std::int32_t>>()
                    .has_value()));
  EXPECT_FALSE(
      (GVariantParse<"(sisi)", "*">("('a', 1, 'b', 2)")
           .TryInto<std::tuple<std::string, std::int32_t, std::string>>()
           .has_value()));
  EXPECT_FALSE(
      (GVariantParse<"(sisi)", "*">("('a', 1, 'b', 2)")
           .TryInto<std::tuple<std::string, bool, std::string, std::int32_t>>()
           .has_value()));
}

TEST(GVariantRefTest, Variant) {
  using Strings = std::variant<const char*, std::string, std::string_view>;
  using BasicArrays =
      std::variant<std::vector<double>, std::vector<std::int32_t>,
                   std::vector<bool>>;
  using Mixed = std::variant<uint8_t, double, std::optional<std::string_view>,
                             GVariantRef<"?">>;

  EXPECT_EQ(GVariantParse<"s">("'asparagus'"),
            GVariantRef<>::From(Strings(std::in_place_index<0>, "asparagus")));
  EXPECT_EQ(GVariantParse<"s">("'broccoli'"),
            GVariantRef<>::From(Strings(std::in_place_index<1>, "broccoli")));
  EXPECT_EQ(GVariantParse<"s">("'carrot'"),
            GVariantRef<>::From(Strings(std::in_place_index<2>, "carrot")));

  EXPECT_EQ(GVariantParse<"ad">("[2.0, 3.5]"),
            GVariantRef<"a?">::From(
                BasicArrays(std::in_place_index<0>, std::vector{2.0, 3.5})));
  EXPECT_EQ(GVariantParse<"ai">("[28, 16]"),
            GVariantRef<"a?">::From(
                BasicArrays(std::in_place_index<1>, std::vector{28, 16})));
  EXPECT_EQ(GVariantParse<"ab">("[true, false]"),
            GVariantRef<"a?">::From(
                BasicArrays(std::in_place_index<2>, std::vector{true, false})));

  EXPECT_EQ(GVariantParse<"y">("6"),
            GVariantRef<>::From(Mixed(std::in_place_index<0>, 6)));
  EXPECT_EQ(GVariantParse<"d">("8.25"),
            GVariantRef<>::From(Mixed(std::in_place_index<1>, 8.25)));
  EXPECT_EQ(
      GVariantParse<"ms">("just 'delicata squash'"),
      GVariantRef<>::From(Mixed(std::in_place_index<2>, "delicata squash")));

  EXPECT_EQ(Strings(std::in_place_index<1>, "eggplant"),
            GVariantParse<"s">("'eggplant'").Into<Strings>());

  EXPECT_EQ(BasicArrays(std::in_place_index<2>, std::vector{true, false, true}),
            GVariantParse<"ab">("[true, false, true]").Into<BasicArrays>());

  EXPECT_EQ(base::ok(Mixed(std::in_place_index<0>, 12)),
            (GVariantParse<"y", "*">("12").TryInto<Mixed>()));
  EXPECT_EQ(Mixed(std::in_place_index<1>, 9.75),
            GVariantParse<"d">("9.75").Into<GVariantRef<"?">>().Into<Mixed>());
  EXPECT_EQ(
      Mixed(std::in_place_index<3>, GVariantRef<"?">::From(std::int32_t{93})),
      GVariantParse<"i">("93").Into<GVariantRef<"?">>().Into<Mixed>());

  // Doesn't match type string.
  EXPECT_FALSE((GVariantParse<"i", "*">("16").TryInto<Strings>().has_value()));
  // Matches indefinite type string, but not any of the variants
  EXPECT_FALSE(
      (GVariantParse<"a{sv}", "*">("{}").TryInto<Mixed>().has_value()));
  // Only matches a variant that doesn't provide TryInto().
  EXPECT_FALSE(
      (GVariantParse<"ms", "*">("just 'fennel'").TryInto<Mixed>().has_value()));

  static_assert(Mapping<Strings>::kType == Type("s"));
  static_assert(Mapping<BasicArrays>::kType == Type("a?"));
  static_assert(Mapping<Mixed>::kType == Type("*"));
  static_assert(ProvidesTryInto<Strings, "*">);
  static_assert(ProvidesInto<Strings, "s">);
  static_assert(!ProvidesTryInto<std::variant<std::string_view>, "*">);
  static_assert(ProvidesFrom<BasicArrays, "*">);
  static_assert(!ProvidesFrom<BasicArrays, "ai">);
  static_assert(ProvidesInto<BasicArrays, "ai">);
  static_assert(!ProvidesInto<BasicArrays, "*">);
}

TEST(GVariantRefTest, NestedGVariantRef) {
  EXPECT_EQ(base::ok(GVariantParse<"(sb)">("('X-387', true)")),
            GVariantRef<"(s?)">::TryFrom(std::tuple(
                GVariantRef<>::From("X-387"), GVariantRef<>::From(true))));
  EXPECT_EQ(GVariantParse<"i">("6"),
            GVariantRef<"?">::From(GVariantRef<"i">::From(std::int32_t{6})));

  auto tuple = GVariantParse<"(si)">("('Spiff', 8521)")
                   .Into<std::tuple<GVariantRef<"s">, GVariantRef<>>>();
  EXPECT_EQ(std::string("Spiff"), std::get<0>(tuple).Into<std::string>());
  EXPECT_EQ(base::ok(std::int32_t{8521}),
            std::get<1>(tuple).TryInto<std::int32_t>());
  EXPECT_EQ(base::ok(true),
            GVariantRef<"b">::From(true)
                .Into<GVariantRef<"?">>()
                .TryInto<GVariantRef<"b">>()
                .transform([](auto v) { return v.template Into<bool>(); }));

  EXPECT_FALSE(
      GVariantRef<"i">::TryFrom(GVariantRef<"?">::From(std::uint8_t{45}))
          .has_value());
  EXPECT_FALSE(
      GVariantRef<"?">::From(29.5).TryInto<GVariantRef<"u">>().has_value());

  static_assert(ProvidesInto<GVariantRef<"a*">, "av">);
  static_assert(ProvidesFrom<GVariantRef<"av">, "a*">);
  static_assert(!ProvidesInto<GVariantRef<"av">, "a*">);
  static_assert(!ProvidesFrom<GVariantRef<"a*">, "av">);
  static_assert(ProvidesTryInto<GVariantRef<"av">, "a*">);
  static_assert(ProvidesTryFrom<GVariantRef<"a*">, "av">);
  static_assert(!ProvidesTryInto<GVariantRef<"av">, "ar">);
  static_assert(!ProvidesTryFrom<GVariantRef<"ar">, "av">);
}

TEST(GVariantRefTest, Ignored) {
  EXPECT_TRUE((GVariantParse<"a{sv}", "*">("{'one': <uint32 1>, 'two': <2.0>}")
                   .TryInto<Ignored>()
                   .has_value()));

  // Any GVariantRef can be converted to an Ignored, but not the other way
  // around.
  static_assert(!ProvidesFrom<Ignored, "*"> && !ProvidesTryFrom<Ignored, "*"> &&
                ProvidesInto<Ignored, "*"> && ProvidesTryInto<Ignored, "*">);
}

TEST(GVariantRefTest, Boxed) {
  EXPECT_EQ(GVariantParse<"v">("<'beep'>"), GVariantRef<>::From(Boxed{"beep"}));

  EXPECT_EQ((Boxed<std::variant<std::uint16_t, std::string>>{"boop"}),
            (GVariantParse<"v">("<'boop'>")
                 .TryInto<Boxed<std::variant<std::uint16_t, std::string>>>()));
  EXPECT_EQ(Boxed{GVariantRef<>::From(std::int64_t{-204})},
            GVariantParse<"v">("<int64 -204>").Into<Boxed<GVariantRef<>>>());

  EXPECT_FALSE((GVariantParse<"i", "*">("-27")
                    .TryInto<Boxed<std::int32_t>>()
                    .has_value()));
  EXPECT_FALSE((GVariantParse<"v", "*">("<'buzz'>")
                    .TryInto<Boxed<std::int32_t>>()
                    .has_value()));

  // A boxed value is definite even if the inner type is not.
  static_assert(ProvidesFrom<std::vector<Boxed<GVariantRef<>>>, "*">);
}

TEST(GVariantRefTest, FilledMaybe) {
  EXPECT_EQ(GVariantParse<"ms">("just 'alpaca'"),
            GVariantRef<>::From(FilledMaybe{"alpaca"}));
  // Since the value is always guaranteed to be present, indefinite types (whose
  // actual type is only known at runtime) can be used with From.
  EXPECT_EQ(GVariantParse<"mx">("-362"),
            GVariantRef<>::From(
                FilledMaybe{GVariantRef<>::From(std::int64_t{-362})}));

  EXPECT_EQ(
      base::ok(FilledMaybe{true}),
      (GVariantParse<"mb", "*">("just true").TryInto<FilledMaybe<bool>>()));

  EXPECT_FALSE((GVariantParse<"u", "*">("45")
                    .TryInto<FilledMaybe<std::uint32_t>>()
                    .has_value()));
  EXPECT_FALSE((GVariantParse<"mu", "*">("nothing")
                    .TryInto<FilledMaybe<std::uint32_t>>()
                    .has_value()));
  EXPECT_FALSE((GVariantParse<"mi", "*">("just 45")
                    .TryInto<FilledMaybe<std::uint32_t>>()
                    .has_value()));
}

TEST(GVariantRefTest, EmptyArrayOf) {
  EXPECT_EQ(GVariantParse<"ai">("[]"),
            GVariantRef<>::From(EmptyArrayOf<"i">()));

  EXPECT_TRUE((
      GVariantParse<"ai", "*">("[]").TryInto<EmptyArrayOf<"i">>().has_value()));

  EXPECT_FALSE((
      GVariantParse<"au", "*">("[]").TryInto<EmptyArrayOf<"i">>().has_value()));
  EXPECT_FALSE((GVariantParse<"ai", "*">("[7]")
                    .TryInto<EmptyArrayOf<"i">>()
                    .has_value()));
}

TEST(GVariantRefTest, ObjectPath) {
  EXPECT_FALSE(ObjectPath::TryFrom("invalid path").has_value());

  auto path = ObjectPath::TryFrom("/valid/path");
  ASSERT_TRUE(path.has_value());

  EXPECT_EQ(GVariantParse<"o">("'/valid/path'"),
            GVariantRef<>::From(path.value()));

  EXPECT_EQ(path.value(),
            GVariantParse<"o">("'/valid/path'").Into<ObjectPath>());

  EXPECT_FALSE((GVariantParse<"s", "*">("'/valid/path'")
                    .TryInto<ObjectPath>()
                    .has_value()));
}

TEST(GVariantRefTest, TypeSignature) {
  EXPECT_FALSE(TypeSignature::TryFrom("invalid signature").has_value());

  auto signature = TypeSignature::TryFrom("goodsig");
  ASSERT_TRUE(signature.has_value());

  EXPECT_EQ(GVariantParse<"g">("'goodsig'"),
            GVariantRef<>::From(signature.value()));

  EXPECT_EQ(signature.value(),
            GVariantParse<"g">("'goodsig'").Into<TypeSignature>());

  EXPECT_FALSE((GVariantParse<"s", "*">("'goodsig'")
                    .TryInto<TypeSignature>()
                    .has_value()));
}

TEST(GVariantRefTest, FromReferences) {
  std::int32_t int32 = 57;
  std::string string = "check";
  std::vector<std::uint8_t> data{1, 2, 3, 4, 5};
  Boxed<std::vector<std::uint8_t>&> boxed_data{data};

  EXPECT_EQ(
      GVariantParse<"(isv)">("(57, 'check', <[byte 1, 2, 3, 4, 5]>)"),
      GVariantRef<>::From(std::forward_as_tuple(int32, string, boxed_data)));
}

TEST(GVariantRefTest, Destructure) {
  std::int32_t a, b;
  std::string c;
  std::int64_t d, f;
  std::uint64_t e, g;
  std::uint32_t h;
  GVariantRef<> i;
  GVariantParse<"(ii{s((xt)(xt))}uv)">(
      "(1, 2, {'3', ((4, 5), (6, 7))}, 8, <just byte 9>)")
      .Destructure(
          a, b,
          std::forward_as_tuple(
              c, std::forward_as_tuple(std::tie(d, e), std::tie(f, g))),
          h, std::tie(i));
  EXPECT_EQ(1, a);
  EXPECT_EQ(2, b);
  EXPECT_EQ("3", c);
  EXPECT_EQ(4, d);
  EXPECT_EQ(5u, e);
  EXPECT_EQ(6, f);
  EXPECT_EQ(7u, g);
  EXPECT_EQ(8u, h);
  EXPECT_EQ(GVariantRef<>::From(std::optional<std::uint8_t>(9)), i);
}

TEST(GVariantRefTest, TryDestructure) {
  std::int32_t a, b;
  std::string c;
  std::int64_t d, f;
  std::uint64_t e, g;
  std::uint32_t h;
  std::uint8_t i;
  ASSERT_TRUE((GVariantParse<"(ii{sa(xt)}umy)", "*">(
                   "(1, 2, {'3', [(4, 5), (6, 7)]}, 8, just 9)")
                   .TryDestructure(a, b,
                                   std::forward_as_tuple(
                                       c, std::forward_as_tuple(
                                              std::tie(d, e), std::tie(f, g))),
                                   h, std::tie(i))
                   .has_value()));
  EXPECT_EQ(1, a);
  EXPECT_EQ(2, b);
  EXPECT_EQ("3", c);
  EXPECT_EQ(4, d);
  EXPECT_EQ(5u, e);
  EXPECT_EQ(6, f);
  EXPECT_EQ(7u, g);
  EXPECT_EQ(8u, h);
  EXPECT_EQ(9, i);

  std::int16_t x;
  EXPECT_FALSE(
      (GVariantParse<"mn", "*">("nothing").TryDestructure(x).has_value()));

  std::uint16_t y;
  std::uint16_t z;
  EXPECT_FALSE(
      GVariantParse<"aq">("[1, 2, 3]").TryDestructure(y, z).has_value());
}

TEST(GVariantRefTest, Iteration) {
  auto variant = GVariantParse<"as">("['a', 'b', 'c', 'd', 'e']");
  std::vector<std::string> result;
  for (auto elem : variant) {
    result.push_back(elem.Into<std::string>());
  }
  EXPECT_EQ((std::vector<std::string>{"a", "b", "c", "d", "e"}), result);

  std::vector<std::string> rev_result;
  std::for_each(std::reverse_iterator(variant.end()),
                std::reverse_iterator(variant.begin()), [&](const auto& elem) {
                  rev_result.push_back(elem.template Into<std::string>());
                });
  EXPECT_EQ((std::vector<std::string>{"e", "d", "c", "b", "a"}), rev_result);

  EXPECT_EQ(std::partial_ordering::less, variant.begin() <=> variant.end());
  EXPECT_EQ(std::partial_ordering::equivalent,
            variant.begin() <=> variant.begin());
  EXPECT_EQ(5, variant.end() - variant.begin());
  EXPECT_EQ("c", (*(variant.begin() + 2)).string_view());
  EXPECT_EQ("c", (*(2 + variant.begin())).string_view());
  EXPECT_EQ("d", (*(variant.end() - 2)).string_view());

  auto inc = variant.begin();
  inc += 1;
  inc += 2;
  EXPECT_EQ("d", (*inc).string_view());

  auto dec = variant.end();
  dec -= 1;
  dec -= 2;
  EXPECT_EQ("c", (*dec).string_view());

  EXPECT_EQ("e", variant.begin()[4].string_view());
}

TEST(GVariantRefTest, TupleLike) {
  auto [a, b, c, d] = GVariantParse<"(iuxt)">("(1, 2, 3, 4)");

  EXPECT_EQ(1, a.Into<std::int32_t>());
  EXPECT_EQ(2u, b.Into<std::uint32_t>());
  EXPECT_EQ(3, c.Into<std::int64_t>());
  EXPECT_EQ(4u, d.Into<std::uint64_t>());
}

TEST(GVariantRefTest, Lookup) {
  auto vardict = GVariantParse<"a{sv}">("{'a': <true>, 'b': <int32 5>}");
  auto dict2 = GVariantParse<"a{ib}">("{1: false, 3: true, 5: true}");

  EXPECT_EQ(GVariantRef<>::From(Boxed{true}), vardict.LookUp("a"));
  EXPECT_EQ(GVariantRef<>::From(Boxed<std::int32_t>{5}), vardict.LookUp("b"));
  EXPECT_EQ(std::nullopt, vardict.LookUp("c"));

  EXPECT_EQ(GVariantRef<>::From(false), dict2.LookUp(std::int32_t{1}));
  EXPECT_EQ(std::nullopt, dict2.LookUp(std::int32_t{2}));
  EXPECT_EQ(GVariantRef<>::From(true), dict2.LookUp(std::int32_t{3}));
  EXPECT_EQ(std::nullopt, dict2.LookUp(std::int32_t{4}));
  EXPECT_EQ(GVariantRef<>::From(true), dict2.LookUp(std::int32_t{5}));
}

}  // namespace remoting::gvariant
