// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_NAMESPACE_NAMESPACE_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_NAMESPACE_NAMESPACE_H_

class Goat {
 public:
  int GetValue() const { return 7; }
};

enum class Foo { cat, dog, goat };

namespace space {

class Goat {
 public:
  void DoNothing() const {}
  int GetValue() const { return 42; }
};

enum class Animal { cat, dog, goat };
enum Vehicle { car, boat, bike };
enum { kPen, kPencil };

}  // namespace space

namespace outer {

namespace inner {

class NestedGoat {
 public:
  int GetValue() const { return 50; }
  space::Animal GetAnimal() { return space::Animal::goat; }
};

}  // namespace inner

}  // namespace outer

// Swift interop will compile -- yet crash the test -- if there is a
// enum class with the same name both in and out of a namespace and the
// namespaced enum is referenced by XCTest methods.

enum class SameNameEnum { watermelon, apple, orange };

namespace sameName {

enum class SameNameEnum { watermelon, apple, orange };

}  // namespace sameName

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_NAMESPACE_NAMESPACE_H_
