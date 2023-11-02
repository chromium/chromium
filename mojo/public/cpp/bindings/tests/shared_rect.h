// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_SHARED_RECT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_SHARED_RECT_H_


namespace mojo {
namespace test {

// An implementation of a hypothetical Rect type specifically for consumers in
// both Chromium and Blink.
class SharedRect {
 public:
  SharedRect() {}
  SharedRect(int x, int y, int width, int height)
      : x_(x), y_(y), width_(width), height_(height) {}

  int x() const { return x_; }
  void set_x(int x) { x_ = x; }

  int y() const { return y_; }
  void set_y(int y) { y_ = y; }

  int width() const { return width_; }
  void set_width(int width) { width_ = width; }

  int height() const { return height_; }
  void set_height(int height) { height_ = height; }

 private:
  int x_ = 0;
  int y_ = 0;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_SHARED_RECT_H_
