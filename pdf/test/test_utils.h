// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_TEST_UTILS_H_
#define PDF_TEST_TEST_UTILS_H_

namespace pp {
class FloatRect;
class Point;
class Rect;
class Size;
}  // namespace pp

namespace chrome_pdf {

void ComparePoint(const pp::Point& expected_point,
                  const pp::Point& given_point);
void CompareRect(const pp::Rect& expected_rect, const pp::Rect& given_rect);
void CompareRect(const pp::FloatRect& expected_rect,
                 const pp::FloatRect& given_rect);
void CompareSize(const pp::Size& expected_size, const pp::Size& given_size);

}  // namespace chrome_pdf

#endif  // PDF_TEST_TEST_UTILS_H_