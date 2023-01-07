// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_OBJECT_PASSING_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_OBJECT_PASSING_H_

class Object {
 public:
  Object(int value) : value_(value) {}
  ~Object() {}

  int GetValue() const { return value_; }

 private:
  int value_;
};

class ObjectPassing {
 public:
  ObjectPassing(){};
  ~ObjectPassing(){};

  int AddReferences(const Object& one, const Object& two) const {
    return one.GetValue() + two.GetValue();
  }

  int AddPointers(Object* one, Object* two) const {
    return one->GetValue() + two->GetValue();
  }
};

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_OBJECT_PASSING_H_
