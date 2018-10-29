/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_BOX_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_BOX_TEST_HELPERS_H_

#include <gtest/gtest.h>
namespace blink {
class FloatBox;

bool ApproximatelyEqual(const float&, const float&);

namespace float_box_test {
bool ApproximatelyEqual(const float&, const float&);
bool ApproximatelyEqual(const FloatBox&, const FloatBox&);
testing::AssertionResult AssertAlmostEqual(const char*,
                                           const char*,
                                           const FloatBox&,
                                           const FloatBox&);
testing::AssertionResult AssertContains(const char*,
                                        const char*,
                                        const FloatBox&,
                                        const FloatBox&);
}  // namespace float_box_test
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_BOX_TEST_HELPERS_H_
