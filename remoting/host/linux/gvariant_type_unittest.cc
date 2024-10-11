// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gvariant_type.h"

#include <iterator>
#include <vector>

#include "remoting/host/linux/gvariant_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::gvariant {

TEST(GVariantTypeTest, IsValid) {
  EXPECT_FALSE(Type("").IsValid());
  EXPECT_TRUE(Type("i").IsValid());
  EXPECT_TRUE(Type("r").IsValid());
  EXPECT_FALSE(Type("c").IsValid());
  EXPECT_FALSE(Type("ii").IsValid());
  EXPECT_TRUE(Type("(ii)").IsValid());
  EXPECT_FALSE(Type("(ii").IsValid());
  EXPECT_FALSE(Type("(ii)i").IsValid());
  EXPECT_FALSE(Type("(ii}").IsValid());
  EXPECT_FALSE(Type("a").IsValid());
  EXPECT_TRUE(Type("ai").IsValid());
  EXPECT_FALSE(Type("aii").IsValid());
  EXPECT_TRUE(Type("aammmas").IsValid());
  EXPECT_FALSE(Type("aammmass").IsValid());
  EXPECT_TRUE(Type("aammma(ss)").IsValid());
  EXPECT_TRUE(Type("{sv}").IsValid());
  EXPECT_TRUE(Type("{?*}").IsValid());
  EXPECT_FALSE(Type("{vs}").IsValid());
  EXPECT_FALSE(Type("{**}").IsValid());
  EXPECT_FALSE(Type("{s}").IsValid());
  EXPECT_FALSE(Type("{sss}").IsValid());
  EXPECT_TRUE(Type("{sam(i)}").IsValid());
  EXPECT_FALSE(Type("{sv)").IsValid());
  EXPECT_TRUE(Type("(iidua(aa{smav})b)").IsValid());
  EXPECT_TRUE(Type("(????a(aa{?ma?})b)").IsValid());
  EXPECT_FALSE(Type("(????a(aa{*ma?})b)").IsValid());
  EXPECT_TRUE(Type("(iidua(aar)b)").IsValid());
}

TEST(GVariantTypeTest, IsSubtypeOf) {
  EXPECT_TRUE(
      Type("(iidua(aa{smav})b)").IsSubtypeOf(Type("(iidua(aa{smav})b)")));
  EXPECT_TRUE(
      Type("(iidua(aa{smav})b)").IsSubtypeOf(Type("(????a(aa{?mav})b)")));
  EXPECT_FALSE(
      Type("(iidua(aa{smav})b)").IsSubtypeOf(Type("(iidua(aa{sma?})b)")));
  EXPECT_FALSE(Type("(iidua(aa{smav})b)").IsSubtypeOf(Type("(iidua?b)")));
  EXPECT_TRUE(
      Type("(iidua(aa{smav})b)").IsSubtypeOf(Type("(iidua(aa{sma*})b)")));
  EXPECT_TRUE(Type("(iidua(aa{smav})b)").IsSubtypeOf(Type("(iidua(aa*)b)")));
  EXPECT_TRUE(Type("(iidua(aa{smav})b)").IsSubtypeOf(Type("(iidua(*)b)")));
  EXPECT_FALSE(Type("(iidua(aa{smav})b)").IsSubtypeOf(Type("(iidu*)")));
  EXPECT_FALSE(Type("(iidua(aa{smav}))").IsSubtypeOf(Type("(iidu*b)")));
  EXPECT_TRUE(Type("(iidua(aa{smav})b)").IsSubtypeOf(Type("(iiduarb)")));
  EXPECT_FALSE(Type("(iidua(aa{smav})b)").IsSubtypeOf(Type("(iidua(aar)b)")));
  EXPECT_FALSE(Type("(iidua(aa{smav})b)").IsSubtypeOf(Type("(iidurb)")));
}

TEST(GVariantTypeTest, HasCommonTypeWith) {
  EXPECT_FALSE(Type("i").HasCommonTypeWith(Type("u")));
  EXPECT_TRUE(Type("(?ou)").HasCommonTypeWith(Type("(io?)")));
  EXPECT_TRUE(Type("((*)i(*))").HasCommonTypeWith(Type("(ri(a{sv}))")));
  EXPECT_FALSE(Type("((*)i(*))").HasCommonTypeWith(Type("(ri(a{sv}i))")));
  EXPECT_FALSE(Type("((*)i(*))").HasCommonTypeWith(Type("(ri(a{sv})i)")));
  EXPECT_FALSE(Type("((*)i(*))").HasCommonTypeWith(Type("(ru(a{sv}))")));
}

TEST(GVariantTypeTest, CommonSuperTypeOf) {
  EXPECT_EQ(Type("s"), (TypeBase::CommonSuperTypeOf<"s", "s", "s">()));
  EXPECT_EQ(Type("(uu?)"),
            (TypeBase::CommonSuperTypeOf<"(uuu)", "(uuu)", "(uui)">()));
  EXPECT_EQ(Type("r"),
            (TypeBase::CommonSuperTypeOf<"(uuu)", "(uu)", "(uuu)">()));
  EXPECT_EQ(Type("r"), (TypeBase::CommonSuperTypeOf<"(uuu)", "r", "(uuu)">()));
  EXPECT_EQ(Type("a{?*}"),
            (TypeBase::CommonSuperTypeOf<"a{sb}", "a{sv}", "a{iv}">()));
  EXPECT_EQ(Type("m(r?*)"),
            (TypeBase::CommonSuperTypeOf<"m(()ba{sv})", "m((ssii)su)",
                                         "m((aiay)x{ob})">()));
}

TEST(GVariantTypeTest, ContainedType) {
  EXPECT_EQ(Type("s"), TypeBase::ContainedType<"(sss)">());
  EXPECT_EQ(Type("{sv}"), TypeBase::ContainedType<"a{sv}">());
  EXPECT_EQ(Type("{s*}"), TypeBase::ContainedType<"({sar}{sv}{si})">());
  EXPECT_EQ(Type("r"), TypeBase::ContainedType<"((sar)(sv)())">());
  EXPECT_EQ(Type("?"), TypeBase::ContainedType<"{si}">());
  EXPECT_EQ(Type("(ssii)"), TypeBase::ContainedType<"m(ssii)">());
  EXPECT_EQ(Type("*"), TypeBase::ContainedType<"v">());
  EXPECT_EQ(Type("v"), TypeBase::ContainedType<"(vvv)">());
}

}  // namespace remoting::gvariant
