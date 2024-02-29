// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_MOJO_ECHO_H_
#define CONTENT_WEB_TEST_BROWSER_MOJO_ECHO_H_

#include "content/web_test/common/mojo_echo.mojom.h"

namespace content {

class MojoEcho : public mojom::MojoEcho {
 public:
  static void Bind(mojo::PendingReceiver<mojom::MojoEcho> receiver);

  MojoEcho();
  ~MojoEcho() override;

  // mojom::MojoEcho
  void EchoBoolFromUnion(mojom::TestUnionPtr test_union,
                         EchoBoolFromUnionCallback callback) override;
  void EchoInt32FromUnion(mojom::TestUnionPtr test_union,
                          EchoInt32FromUnionCallback callback) override;
  void EchoStringFromUnion(mojom::TestUnionPtr test_union,
                           EchoStringFromUnionCallback callback) override;
  void EchoBoolAsUnion(bool value, EchoBoolAsUnionCallback callback) override;
  void EchoInt32AsUnion(int32_t value,
                        EchoInt32AsUnionCallback callback) override;
  void EchoStringAsUnion(const std::string& value,
                         EchoStringAsUnionCallback callback) override;
  void EchoNullFromOptionalUnion(
      mojom::TestUnionPtr test_union,
      EchoNullFromOptionalUnionCallback callback) override;
  void EchoBoolFromOptionalUnion(
      mojom::TestUnionPtr test_union,
      EchoBoolFromOptionalUnionCallback callback) override;
  void EchoInt32FromOptionalUnion(
      mojom::TestUnionPtr test_union,
      EchoInt32FromOptionalUnionCallback callback) override;
  void EchoStringFromOptionalUnion(
      mojom::TestUnionPtr test_union,
      EchoStringFromOptionalUnionCallback callback) override;
  void EchoNullAsOptionalUnion(
      EchoNullAsOptionalUnionCallback callback) override;
  void EchoBoolAsOptionalUnion(
      bool value,
      EchoBoolAsOptionalUnionCallback callback) override;
  void EchoInt32AsOptionalUnion(
      int32_t value,
      EchoInt32AsOptionalUnionCallback callback) override;
  void EchoStringAsOptionalUnion(
      const std::string& value,
      EchoStringAsOptionalUnionCallback callback) override;
  void EchoInt8FromNestedUnion(
      mojom::NestedUnionPtr test_union,
      EchoInt8FromNestedUnionCallback callback) override;
  void EchoBoolFromNestedUnion(
      mojom::NestedUnionPtr test_union,
      EchoBoolFromNestedUnionCallback callback) override;
  void EchoStringFromNestedUnion(
      mojom::NestedUnionPtr test_union,
      EchoStringFromNestedUnionCallback callback) override;
  void EchoInt8AsNestedUnion(int8_t value,
                             EchoInt8AsNestedUnionCallback callback) override;
  void EchoBoolAsNestedUnion(bool value,
                             EchoBoolAsNestedUnionCallback callback) override;
  void EchoStringAsNestedUnion(
      const std::string& value,
      EchoStringAsNestedUnionCallback callback) override;
  void EchoNullFromOptionalNestedUnion(
      mojom::NestedUnionPtr test_union,
      EchoNullFromOptionalNestedUnionCallback callback) override;
  void EchoInt8FromOptionalNestedUnion(
      mojom::NestedUnionPtr test_union,
      EchoInt8FromOptionalNestedUnionCallback callback) override;
  void EchoBoolFromOptionalNestedUnion(
      mojom::NestedUnionPtr test_union,
      EchoBoolFromOptionalNestedUnionCallback callback) override;
  void EchoStringFromOptionalNestedUnion(
      mojom::NestedUnionPtr test_union,
      EchoStringFromOptionalNestedUnionCallback callback) override;
  void EchoNullAsOptionalNestedUnion(
      EchoNullAsOptionalNestedUnionCallback callback) override;
  void EchoInt8AsOptionalNestedUnion(
      int8_t value,
      EchoInt8AsOptionalNestedUnionCallback callback) override;
  void EchoBoolAsOptionalNestedUnion(
      bool value,
      EchoBoolAsOptionalNestedUnionCallback callback) override;
  void EchoStringAsOptionalNestedUnion(
      const std::string& value,
      EchoStringAsOptionalNestedUnionCallback callback) override;

  void EchoBoolArray(const std::vector<bool>& values,
                     EchoBoolArrayCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_MOJO_ECHO_H_
