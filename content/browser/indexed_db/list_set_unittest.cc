// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/list_set.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::indexed_db {
namespace {

TEST(ListSetTest, ListSetIterator) {
  list_set<int> set;
  for (int i = 3; i > 0; --i)
    set.insert(i);

  list_set<int>::iterator it = set.begin();
  EXPECT_EQ(3, *it);
  ++it;
  EXPECT_EQ(2, *it);
  it++;
  EXPECT_EQ(1, *it);
  --it;
  EXPECT_EQ(2, *it);
  it--;
  EXPECT_EQ(3, *it);
  ++it;
  EXPECT_EQ(2, *it);
  it++;
  EXPECT_EQ(1, *it);
  ++it;
  EXPECT_EQ(set.end(), it);
}

TEST(ListSetTest, ListSetConstIterator) {
  list_set<int> set;
  for (int i = 5; i > 0; --i)
    set.insert(i);

  const list_set<int>& ref = set;

  list_set<int>::const_iterator it = ref.begin();
  for (int i = 5; i > 0; --i) {
    EXPECT_EQ(i, *it);
    ++it;
  }
  EXPECT_EQ(ref.end(), it);
}

TEST(ListSetTest, ListSetInsertFront) {
  list_set<int> set;
  for (int i = 5; i > 0; --i)
    set.insert(i);
  for (int i = 6; i <= 10; ++i)
    set.insert_front(i);

  const list_set<int>& ref = set;

  list_set<int>::const_iterator it = ref.begin();
  for (int i = 10; i > 0; --i) {
    EXPECT_EQ(i, *it);
    ++it;
  }
  EXPECT_EQ(ref.end(), it);
}

TEST(ListSetTest, ListSetPrimitive) {
  list_set<int> set;
  EXPECT_TRUE(set.empty());
  EXPECT_EQ(0u, set.size());
  {
    list_set<int>::iterator it = set.begin();
    EXPECT_EQ(set.end(), it);
  }

  for (int i = 5; i > 0; --i)
    set.insert(i);
  EXPECT_EQ(5u, set.size());
  EXPECT_FALSE(set.empty());

  set.erase(3);
  EXPECT_EQ(4u, set.size());

  EXPECT_EQ(1u, set.count(2));
  set.erase(2);
  EXPECT_EQ(0u, set.count(2));
  EXPECT_EQ(3u, set.size());

  {
    list_set<int>::iterator it = set.begin();
    EXPECT_EQ(5, *it);
    ++it;
    EXPECT_EQ(4, *it);
    ++it;
    EXPECT_EQ(1, *it);
    ++it;
    EXPECT_EQ(set.end(), it);
  }

  set.erase(1);
  set.erase(4);
  set.erase(5);

  EXPECT_EQ(0u, set.size());
  EXPECT_TRUE(set.empty());
  {
    list_set<int>::iterator it = set.begin();
    EXPECT_EQ(set.end(), it);
  }
}

namespace {

class WrappedInt {
 public:
  explicit WrappedInt(int value) : value_(value) {}

  WrappedInt(const WrappedInt&) = default;
  WrappedInt& operator=(const WrappedInt&) = default;

  int value() const { return value_; }
  bool operator<(const WrappedInt& rhs) const { return value_ < rhs.value_; }
  bool operator==(const WrappedInt& rhs) const { return value_ == rhs.value_; }

 private:
  int value_;
};

}  // namespace

TEST(ListSetTest, ListSetObject) {
  list_set<WrappedInt> set;
  EXPECT_EQ(0u, set.size());
  {
    list_set<WrappedInt>::iterator it = set.begin();
    EXPECT_EQ(set.end(), it);
  }

  set.insert(WrappedInt(0));
  set.insert(WrappedInt(1));
  set.insert(WrappedInt(2));

  EXPECT_EQ(3u, set.size());

  {
    list_set<WrappedInt>::iterator it = set.begin();
    EXPECT_EQ(0, it->value());
    ++it;
    EXPECT_EQ(1, it->value());
    ++it;
    EXPECT_EQ(2, it->value());
    ++it;
    EXPECT_EQ(set.end(), it);
  }

  set.erase(WrappedInt(0));
  set.erase(WrappedInt(1));
  set.erase(WrappedInt(2));

  EXPECT_EQ(0u, set.size());
  {
    list_set<WrappedInt>::iterator it = set.begin();
    EXPECT_EQ(set.end(), it);
  }
}

TEST(ListSetTest, ListSetPointer) {
  std::unique_ptr<WrappedInt> w0 = std::make_unique<WrappedInt>(0);
  std::unique_ptr<WrappedInt> w1 = std::make_unique<WrappedInt>(1);
  std::unique_ptr<WrappedInt> w2 = std::make_unique<WrappedInt>(2);

  list_set<WrappedInt*> set;
  EXPECT_EQ(0u, set.size());
  {
    list_set<WrappedInt*>::iterator it = set.begin();
    EXPECT_EQ(set.end(), it);
  }

  set.insert(w0.get());
  set.insert(w1.get());
  set.insert(w2.get());

  EXPECT_EQ(3u, set.size());

  {
    list_set<WrappedInt*>::iterator it = set.begin();
    EXPECT_EQ(0, (*it)->value());
    ++it;
    EXPECT_EQ(1, (*it)->value());
    ++it;
    EXPECT_EQ(2, (*it)->value());
    ++it;
    EXPECT_EQ(set.end(), it);
  }

  set.erase(w0.get());
  set.erase(w1.get());
  set.erase(w2.get());

  EXPECT_EQ(0u, set.size());
  {
    list_set<WrappedInt*>::iterator it = set.begin();
    EXPECT_EQ(set.end(), it);
  }
}

namespace {

class RefCountedInt : public base::RefCounted<RefCountedInt> {
 public:
  explicit RefCountedInt(int value) : value_(value) {}
  int value() { return value_; }

 private:
  friend class base::RefCounted<RefCountedInt>;

  ~RefCountedInt() = default;

  int value_;
};

}  // namespace

TEST(ListSetTest, ListSetRefCounted) {
  list_set<scoped_refptr<RefCountedInt>> set;
  EXPECT_EQ(0u, set.size());
  {
    list_set<scoped_refptr<RefCountedInt>>::iterator it = set.begin();
    EXPECT_EQ(set.end(), it);
  }

  auto r0 = base::MakeRefCounted<RefCountedInt>(0);
  auto r1 = base::MakeRefCounted<RefCountedInt>(1);
  auto r2 = base::MakeRefCounted<RefCountedInt>(2);

  set.insert(r0);
  set.insert(r1);
  set.insert(r2);

  EXPECT_EQ(3u, set.size());

  {
    list_set<scoped_refptr<RefCountedInt>>::iterator it = set.begin();
    EXPECT_EQ(0, (*it)->value());
    ++it;
    EXPECT_EQ(1, (*it)->value());
    ++it;
    EXPECT_EQ(2, (*it)->value());
    ++it;
    EXPECT_EQ(set.end(), it);
  }

  set.erase(r0);
  set.erase(r1);
  set.erase(r2);

  EXPECT_EQ(0u, set.size());
  {
    list_set<scoped_refptr<RefCountedInt>>::iterator it = set.begin();
    EXPECT_EQ(set.end(), it);
  }
}

}  // namespace
}  // namespace content::indexed_db
