// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_OUTLINED_CLASS_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_OUTLINED_CLASS_H_

class Outlined {
 private:
  int sum_;

 public:
  Outlined();
  Outlined(int initial_value);

  int OutlinedAddition(int delta);
};

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_OUTLINED_CLASS_H_
