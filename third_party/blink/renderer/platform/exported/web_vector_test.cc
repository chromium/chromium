// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_vector.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(WebVectorTest, Iterators) {
  Vector<int> input;
  for (int i = 0; i < 5; ++i)
    input.push_back(i);

  WebVector<int> web_vector(input);
  const WebVector<int>& const_web_vector = web_vector;
  Vector<int> output;

  ASSERT_EQ(input.size(), web_vector.size());

  // Use begin()/end() iterators directly.
  for (WebVector<int>::iterator it = web_vector.begin(); it != web_vector.end();
       ++it)
    output.push_back(*it);
  ASSERT_EQ(input.size(), output.size());
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i], output[i]);

  // Use begin()/end() const_iterators directly.
  output.clear();
  for (WebVector<int>::const_iterator it = const_web_vector.begin();
       it != const_web_vector.end(); ++it)
    output.push_back(*it);
  ASSERT_EQ(input.size(), output.size());
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i], output[i]);

  // Use range-based for loop.
  output.clear();
  for (int x : web_vector)
    output.push_back(x);
  ASSERT_EQ(input.size(), output.size());
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i], output[i]);
}

TEST(WebVectorTest, Empty) {
  WebVector<int> vector;
  ASSERT_TRUE(vector.empty());
  int value = 1;
  vector.Assign(&value, 1);
  ASSERT_EQ(1u, vector.size());
  ASSERT_FALSE(vector.empty());
}

TEST(WebVectorTest, Swap) {
  const int kFirstData[] = {1, 2, 3, 4, 5};
  const int kSecondData[] = {6, 5, 8};
  const size_t kFirstDataLength = base::size(kFirstData);
  const size_t kSecondDataLength = base::size(kSecondData);

  WebVector<int> first(kFirstData, kFirstDataLength);
  WebVector<int> second(kSecondData, kSecondDataLength);
  ASSERT_EQ(kFirstDataLength, first.size());
  ASSERT_EQ(kSecondDataLength, second.size());
  first.Swap(second);
  ASSERT_EQ(kSecondDataLength, first.size());
  ASSERT_EQ(kFirstDataLength, second.size());
  for (size_t i = 0; i < first.size(); ++i)
    EXPECT_EQ(kSecondData[i], first[i]);
  for (size_t i = 0; i < second.size(); ++i)
    EXPECT_EQ(kFirstData[i], second[i]);
}

TEST(WebVectorTest, CreateFromPointer) {
  const int kValues[] = {1, 2, 3, 4, 5};

  WebVector<int> vector(kValues, 3);
  ASSERT_EQ(3u, vector.size());
  ASSERT_EQ(1, vector[0]);
  ASSERT_EQ(2, vector[1]);
  ASSERT_EQ(3, vector[2]);
}

TEST(WebVectorTest, CreateFromWtfVector) {
  Vector<int> input;
  for (int i = 0; i < 5; ++i)
    input.push_back(i);

  WebVector<int> vector(input);
  ASSERT_EQ(input.size(), vector.size());
  for (size_t i = 0; i < vector.size(); ++i)
    EXPECT_EQ(input[i], vector[i]);

  WebVector<int> copy(input);
  ASSERT_EQ(input.size(), copy.size());
  for (size_t i = 0; i < copy.size(); ++i)
    EXPECT_EQ(input[i], copy[i]);

  WebVector<int> assigned;
  assigned = copy;
  ASSERT_EQ(input.size(), assigned.size());
  for (size_t i = 0; i < assigned.size(); ++i)
    EXPECT_EQ(input[i], assigned[i]);
}

TEST(WebVectorTest, CreateFromStdVector) {
  std::vector<int> input;
  for (int i = 0; i < 5; ++i)
    input.push_back(i);

  WebVector<int> vector(input);
  ASSERT_EQ(input.size(), vector.size());
  for (size_t i = 0; i < vector.size(); ++i)
    EXPECT_EQ(input[i], vector[i]);

  WebVector<int> assigned;
  assigned = input;
  ASSERT_EQ(input.size(), assigned.size());
  for (size_t i = 0; i < assigned.size(); ++i)
    EXPECT_EQ(input[i], assigned[i]);
}

TEST(WebVectorTest, Reserve) {
  WebVector<int> vector;
  vector.reserve(10);

  EXPECT_EQ(10U, vector.capacity());
}

TEST(WebVectorTest, EmplaceBackArgumentForwarding) {
  WebVector<WebString> vector;
  vector.reserve(1);
  WebUChar buffer[] = {'H', 'e', 'l', 'l', 'o', ' ', 'b', 'l', 'i', 'n', 'k'};
  vector.emplace_back(buffer, base::size(buffer));
  ASSERT_EQ(1U, vector.size());
  EXPECT_EQ(WebString(buffer, base::size(buffer)), vector[0]);
}

TEST(WebVectorTest, EmplaceBackElementPlacement) {
  WebVector<int> vector;
  vector.reserve(10);
  for (int i = 0; i < 10; ++i)
    vector.emplace_back(i);
  ASSERT_EQ(10U, vector.size());
  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(i, vector[i]);
}

TEST(WebVectorTest, ResizeToSameSize) {
  WebVector<int> vector;
  vector.reserve(10);
  for (int i = 0; i < 10; ++i)
    vector.emplace_back(i);
  vector.resize(10);
  ASSERT_EQ(10U, vector.size());
  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(i, vector[i]);
}

TEST(WebVectorTest, ResizeShrink) {
  WebVector<int> vector;
  vector.reserve(10);
  for (int i = 0; i < 10; ++i)
    vector.emplace_back(i);
  vector.resize(5);
  ASSERT_EQ(5U, vector.size());
  for (int i = 0; i < 5; ++i)
    EXPECT_EQ(i, vector[i]);
}

namespace {

// Used to ensure that WebVector supports types without a default constructor.
struct NoDefaultConstructor {
  NoDefaultConstructor(int data) : data(data) {}

  int data;
};

}  // anonymous namespace

TEST(WebVectorTest, NoDefaultConstructor) {
  WebVector<NoDefaultConstructor> vector;
  vector.reserve(1);
  vector.emplace_back(42);
  ASSERT_EQ(1U, vector.size());
  EXPECT_EQ(42, vector[0].data);
}

}  // namespace blink
