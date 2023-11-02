// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/name.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace ports {
namespace test {

TEST(NameTest, Defaults) {
  PortName default_port_name;
  EXPECT_EQ(kInvalidPortName, default_port_name);

  NodeName default_node_name;
  EXPECT_EQ(kInvalidNodeName, default_node_name);
}

TEST(NameTest, PortNameChecks) {
  PortName port_name_a(50, 100);
  PortName port_name_b(50, 100);
  PortName port_name_c(100, 50);

  EXPECT_EQ(port_name_a, port_name_b);
  EXPECT_NE(port_name_a, port_name_c);
  EXPECT_NE(port_name_b, port_name_c);

  EXPECT_LT(port_name_a, port_name_c);
  EXPECT_LT(port_name_b, port_name_c);
  EXPECT_FALSE(port_name_a < port_name_b);
  EXPECT_FALSE(port_name_b < port_name_a);

  std::hash<PortName> port_hash_fn;

  size_t hash_a = port_hash_fn(port_name_a);
  size_t hash_b = port_hash_fn(port_name_b);
  size_t hash_c = port_hash_fn(port_name_c);

  EXPECT_EQ(hash_a, hash_b);
  EXPECT_NE(hash_a, hash_c);
  EXPECT_NE(hash_b, hash_c);
}

TEST(NameTest, NodeNameChecks) {
  NodeName node_name_a(50, 100);
  NodeName node_name_b(50, 100);
  NodeName node_name_c(100, 50);

  EXPECT_EQ(node_name_a, node_name_b);
  EXPECT_NE(node_name_a, node_name_c);
  EXPECT_NE(node_name_b, node_name_c);

  EXPECT_LT(node_name_a, node_name_c);
  EXPECT_LT(node_name_b, node_name_c);
  EXPECT_FALSE(node_name_a < node_name_b);
  EXPECT_FALSE(node_name_b < node_name_a);

  std::hash<NodeName> node_hash_fn;

  size_t hash_a = node_hash_fn(node_name_a);
  size_t hash_b = node_hash_fn(node_name_b);
  size_t hash_c = node_hash_fn(node_name_c);

  EXPECT_EQ(hash_a, hash_b);
  EXPECT_NE(hash_a, hash_c);
  EXPECT_NE(hash_b, hash_c);
}

}  // namespace test
}  // namespace ports
}  // namespace core
}  // namespace mojo
