// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/deduping_factory.h"

#include <memory>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTypeName[] = "Foo";
const char kTypeName2[] = "Foo2";

// This serves as an example how to use the DedupingFactory.
class BaseClass : public base::RefCounted<BaseClass> {
 public:
  // The type is introduced so that we can compare derived classes even though
  // Equals takes a parameter of type BaseClass. Each derived class gets an
  // entry in Type.
  enum Type { FOO };

  explicit BaseClass(Type type) : type_(type) {}

  Type type() const { return type_; }

  // For BaseClassT template:
  virtual bool Equals(const BaseClass* other) const = 0;

 protected:
  friend class base::RefCounted<BaseClass>;
  virtual ~BaseClass() {}

 private:
  const Type type_;
};

class Foo : public BaseClass {
 public:
  explicit Foo(int parameter) : BaseClass(FOO), parameter_(parameter) {}
  Foo(const Foo&) = delete;
  Foo& operator=(const Foo&) = delete;
  bool Equals(const BaseClass* other) const override {
    return other->type() == type() &&
           static_cast<const Foo*>(other)->parameter_ == parameter_;
  }
  int parameter() const {
    return parameter_;
  }

 private:
  friend class base::RefCounted<BaseClass>;
  ~Foo() override {}

  // Note that this class must be immutable.
  const int parameter_;
};

scoped_refptr<const BaseClass> CreateFoo(const std::string& /*instance_type*/,
                                         const base::Value::Dict& value,
                                         std::string* error,
                                         bool* bad_message) {
  std::optional<int> parameter = value.FindInt("parameter");
  if (!parameter) {
    *error = "No parameter";
    *bad_message = true;
    return nullptr;
  }
  return scoped_refptr<const BaseClass>(new Foo(*parameter));
}

base::Value::Dict CreateDictWithParameter(int parameter) {
  base::Value::Dict dict;
  dict.Set("parameter", parameter);
  return dict;
}

}  // namespace

namespace extensions {

using FactoryT = DedupingFactory<BaseClass, const base::Value::Dict&>;

TEST(DedupingFactoryTest, InstantiationParameterized) {
  FactoryT factory(2);
  factory.RegisterFactoryMethod(kTypeName, FactoryT::IS_PARAMETERIZED,
                                &CreateFoo);

  base::Value::Dict d1 = CreateDictWithParameter(1);
  base::Value::Dict d2 = CreateDictWithParameter(2);
  base::Value::Dict d3 = CreateDictWithParameter(3);
  base::Value::Dict d4 = CreateDictWithParameter(4);

  std::string error;
  bool bad_message = false;

  // Fill factory with 2 different types.
  scoped_refptr<const BaseClass> c1(
      factory.Instantiate(kTypeName, d1, &error, &bad_message));
  scoped_refptr<const BaseClass> c2(
      factory.Instantiate(kTypeName, d2, &error, &bad_message));
  ASSERT_TRUE(c1.get());
  ASSERT_TRUE(c2.get());
  EXPECT_EQ(1, static_cast<const Foo*>(c1.get())->parameter());
  EXPECT_EQ(2, static_cast<const Foo*>(c2.get())->parameter());

  // This one produces an overflow, now the cache contains [2, 3]
  scoped_refptr<const BaseClass> c3(
      factory.Instantiate(kTypeName, d3, &error, &bad_message));
  ASSERT_TRUE(c3.get());
  EXPECT_EQ(3, static_cast<const Foo*>(c3.get())->parameter());

  // Reuse 2, this should give the same instance as c2.
  scoped_refptr<const BaseClass> c2_b(
      factory.Instantiate(kTypeName, d2, &error, &bad_message));
  EXPECT_EQ(2, static_cast<const Foo*>(c2_b.get())->parameter());
  EXPECT_EQ(c2, c2_b);

  // Also check that the reuse of 2 moved it to the end, so that the cache is
  // now [3, 2] and 3 is discarded before 2.
  // This discards 3, so the cache becomes [2, 1]
  scoped_refptr<const BaseClass> c1_b(
      factory.Instantiate(kTypeName, d1, &error, &bad_message));

  scoped_refptr<const BaseClass> c2_c(
      factory.Instantiate(kTypeName, d2, &error, &bad_message));
  EXPECT_EQ(2, static_cast<const Foo*>(c2_c.get())->parameter());
  EXPECT_EQ(c2, c2_c);
}

TEST(DedupingFactoryTest, InstantiationNonParameterized) {
  FactoryT factory(2);
  factory.RegisterFactoryMethod(kTypeName, FactoryT::IS_NOT_PARAMETERIZED,
                                &CreateFoo);

  base::Value::Dict d1 = CreateDictWithParameter(1);
  base::Value::Dict d2 = CreateDictWithParameter(2);

  std::string error;
  bool bad_message = false;

  // We create two instances with different dictionaries but because the type is
  // declared to be not parameterized, we should get the same instance.
  scoped_refptr<const BaseClass> c1(
      factory.Instantiate(kTypeName, d1, &error, &bad_message));
  scoped_refptr<const BaseClass> c2(
      factory.Instantiate(kTypeName, d2, &error, &bad_message));
  ASSERT_TRUE(c1.get());
  ASSERT_TRUE(c2.get());
  EXPECT_EQ(1, static_cast<const Foo*>(c1.get())->parameter());
  EXPECT_EQ(1, static_cast<const Foo*>(c2.get())->parameter());
  EXPECT_EQ(c1, c2);
}

TEST(DedupingFactoryTest, TypeNames) {
  FactoryT factory(2);
  factory.RegisterFactoryMethod(kTypeName, FactoryT::IS_PARAMETERIZED,
                                &CreateFoo);
  factory.RegisterFactoryMethod(kTypeName2, FactoryT::IS_PARAMETERIZED,
                                &CreateFoo);

  base::Value::Dict d1 = CreateDictWithParameter(1);

  std::string error;
  bool bad_message = false;

  scoped_refptr<const BaseClass> c1_a(
      factory.Instantiate(kTypeName, d1, &error, &bad_message));
  scoped_refptr<const BaseClass> c1_b(
      factory.Instantiate(kTypeName2, d1, &error, &bad_message));

  ASSERT_TRUE(c1_a.get());
  ASSERT_TRUE(c1_b.get());
  EXPECT_NE(c1_a, c1_b);
}

TEST(DedupingFactoryTest, Clear) {
  FactoryT factory(2);
  factory.RegisterFactoryMethod(kTypeName, FactoryT::IS_PARAMETERIZED,
                                &CreateFoo);

  base::Value::Dict d1 = CreateDictWithParameter(1);

  std::string error;
  bool bad_message = false;

  scoped_refptr<const BaseClass> c1_a(
      factory.Instantiate(kTypeName, d1, &error, &bad_message));

  factory.ClearPrototypes();

  scoped_refptr<const BaseClass> c1_b(
      factory.Instantiate(kTypeName, d1, &error, &bad_message));

  ASSERT_TRUE(c1_a.get());
  ASSERT_TRUE(c1_b.get());
  EXPECT_NE(c1_a, c1_b);
}

}  // namespace extensions
