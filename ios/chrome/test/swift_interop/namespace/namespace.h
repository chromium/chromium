// Copyright 2022 The Chromium Authors. All rights reserved.
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

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_NAMESPACE_NAMESPACE_H_
