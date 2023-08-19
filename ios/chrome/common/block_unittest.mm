// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <vector>

#import "base/memory/ref_counted.h"
#import "testing/platform_test.h"

// This test verifies assumptions about the murky world of interaction between
// C++ objects and blocks. Just to make sure.

namespace {

using BlockTest = PlatformTest;

class RefCountedObject : public base::RefCounted<RefCountedObject> {
 public:
  RefCountedObject() {}

  // Refcount is private in the superclass, fake it by counting how many times
  // release can be called until there is one count left, then retain the count
  // back.
  int refcount() {
    int count = 1;
    while (!HasOneRef()) {
      bool check = base::subtle::RefCountedBase::Release();
      EXPECT_FALSE(check);
      ++count;
    }
    for (int ii = 1; ii < count; ii++)
      base::subtle::RefCountedBase::AddRef();
    return count;
  }

 protected:
  friend base::RefCounted<RefCountedObject>;
  virtual ~RefCountedObject() {}
};

TEST_F(BlockTest, BlockAndCPlusPlus) {
  RefCountedObject* object = new RefCountedObject();
  object->AddRef();
  EXPECT_TRUE(object->HasOneRef());
  EXPECT_EQ(1, object->refcount());

  {
    scoped_refptr<RefCountedObject> object_test_ptr(object);
    EXPECT_EQ(2, object->refcount());
  }
  EXPECT_TRUE(object->HasOneRef());

  @autoreleasepool {
    void (^heap_block)(int) = nil;
    {
      scoped_refptr<RefCountedObject> object_ptr(object);
      EXPECT_EQ(2, object->refcount());
      void* object_void_ptr = (void*)object;

      void (^stack_block)(int) = ^(int expected) {
        EXPECT_EQ(object_void_ptr, object_ptr.get());
        EXPECT_EQ(expected, object_ptr.get()->refcount());
      };
      stack_block(4);
      heap_block = [stack_block copy];
      stack_block(4);
    }
    heap_block(2);
  }
  EXPECT_TRUE(object->HasOneRef());
  {
    scoped_refptr<RefCountedObject> object_test2_ptr(object);
    EXPECT_EQ(2, object->refcount());
  }
  EXPECT_TRUE(object->HasOneRef());
  object->Release();
}

TEST_F(BlockTest, BlockAndVectors) {
  void (^heap_block)(void) = nil;
  {
    std::vector<int> vector;
    vector.push_back(0);
    vector.push_back(1);
    vector.push_back(2);

    void (^stack_block)(void) = ^{
      EXPECT_EQ(3ul, vector.size());
      EXPECT_EQ(2, vector[2]);
    };
    stack_block();
    vector[2] = 42;
    vector.push_back(22);
    stack_block();
    heap_block = [stack_block copy];
    stack_block();
  }
  heap_block();
}

}  // namespace
