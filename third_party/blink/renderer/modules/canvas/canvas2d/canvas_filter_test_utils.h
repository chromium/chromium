// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FILTER_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FILTER_TEST_UTILS_H_

#include <string>

namespace blink {
class V8UnionObjectOrObjectArray;
class V8TestingScope;
}  // namespace blink

namespace blink_testing {

blink::V8UnionObjectOrObjectArray* ParseFilter(blink::V8TestingScope& scope,
                                               const std::string& value);

}  // namespace blink_testing

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FILTER_TEST_UTILS_H_
