// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_POLYMORPHISM_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_POLYMORPHISM_H_

class Shape {
 public:
  Shape(int width, int height) : width_(width), height_(height) {}
  virtual ~Shape() {}

  virtual int Area() = 0;
  virtual int NumberOfSides() { return 0; }

  // Virtual or not, this cannot be called on inherited classes unless it
  // is redefined in their declaration.
  // https://github.com/apple/swift/issues/55192
  bool HasSides() { return true; }

 protected:
  int width_;
  int height_;
};

class Rectangle : public Shape {
 public:
  Rectangle(int width, int height) : Shape(width, height) {}
  virtual ~Rectangle() {}

  virtual int Area() { return width_ * height_; }
  virtual int NumberOfSides() { return 4; }
};

class Square : public Rectangle {
 public:
  Square(int size) : Rectangle(size, size) {}
  virtual ~Square() {}

  // Even though these should not be necessary, they are. Without them, calling
  // these methods on a Square object results in a compiler error.
  // https://github.com/apple/swift/issues/55192
  virtual int Area() { return width_ * height_; }
  virtual int NumberOfSides() { return 4; }
};

class Triangle : public Shape {
 public:
  Triangle(int width, int height) : Shape(width, height) {}
  virtual ~Triangle() {}

  virtual int Area() { return (width_ * height_) / 2; }
  virtual int NumberOfSides() { return 3; }
};

// For testing runtime polymorphism.
Shape* MakeShape(int width, int height) {
  return new Triangle(width, height);
}

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_POLYMORPHISM_H_
