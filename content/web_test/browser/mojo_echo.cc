// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/mojo_echo.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
void MojoEcho::Bind(mojo::PendingReceiver<mojom::MojoEcho> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<MojoEcho>(),
                              std::move(receiver));
}

MojoEcho::MojoEcho() = default;

MojoEcho::~MojoEcho() = default;

void MojoEcho::EchoBoolFromUnion(mojom::TestUnionPtr test_union,
                                 EchoBoolFromUnionCallback callback) {
  std::move(callback).Run(test_union->get_bool_value());
}

void MojoEcho::EchoInt32FromUnion(mojom::TestUnionPtr test_union,
                                  EchoInt32FromUnionCallback callback) {
  std::move(callback).Run(test_union->get_int32_value());
}

void MojoEcho::EchoStringFromUnion(mojom::TestUnionPtr test_union,
                                   EchoStringFromUnionCallback callback) {
  std::move(callback).Run(test_union->get_string_value());
}

void MojoEcho::EchoBoolAsUnion(bool value, EchoBoolAsUnionCallback callback) {
  std::move(callback).Run(mojom::TestUnion::NewBoolValue(value));
}

void MojoEcho::EchoInt32AsUnion(int32_t value,
                                EchoInt32AsUnionCallback callback) {
  std::move(callback).Run(mojom::TestUnion::NewInt32Value(value));
}

void MojoEcho::EchoStringAsUnion(const std::string& value,
                                 EchoStringAsUnionCallback callback) {
  std::move(callback).Run(mojom::TestUnion::NewStringValue(value));
}

void MojoEcho::EchoNullFromOptionalUnion(
    mojom::TestUnionPtr test_union,
    EchoNullFromOptionalUnionCallback callback) {
  DCHECK(!test_union);
  std::move(callback).Run();
}

void MojoEcho::EchoBoolFromOptionalUnion(
    mojom::TestUnionPtr test_union,
    EchoBoolFromOptionalUnionCallback callback) {
  std::move(callback).Run(test_union->get_bool_value());
}

void MojoEcho::EchoInt32FromOptionalUnion(
    mojom::TestUnionPtr test_union,
    EchoInt32FromOptionalUnionCallback callback) {
  std::move(callback).Run(test_union->get_int32_value());
}

void MojoEcho::EchoStringFromOptionalUnion(
    mojom::TestUnionPtr test_union,
    EchoStringFromOptionalUnionCallback callback) {
  std::move(callback).Run(test_union->get_string_value());
}

void MojoEcho::EchoNullAsOptionalUnion(
    EchoNullAsOptionalUnionCallback callback) {
  std::move(callback).Run(nullptr);
}

void MojoEcho::EchoBoolAsOptionalUnion(
    bool value,
    EchoBoolAsOptionalUnionCallback callback) {
  std::move(callback).Run(mojom::TestUnion::NewBoolValue(value));
}

void MojoEcho::EchoInt32AsOptionalUnion(
    int32_t value,
    EchoInt32AsOptionalUnionCallback callback) {
  std::move(callback).Run(mojom::TestUnion::NewInt32Value(value));
}

void MojoEcho::EchoStringAsOptionalUnion(
    const std::string& value,
    EchoStringAsOptionalUnionCallback callback) {
  std::move(callback).Run(mojom::TestUnion::NewStringValue(value));
}

void MojoEcho::EchoInt8FromNestedUnion(
    mojom::NestedUnionPtr test_union,
    EchoInt8FromNestedUnionCallback callback) {
  std::move(callback).Run(test_union->get_int8_value());
}

void MojoEcho::EchoBoolFromNestedUnion(
    mojom::NestedUnionPtr test_union,
    EchoBoolFromNestedUnionCallback callback) {
  std::move(callback).Run(test_union->get_union_value()->get_bool_value());
}

void MojoEcho::EchoStringFromNestedUnion(
    mojom::NestedUnionPtr test_union,
    EchoStringFromNestedUnionCallback callback) {
  std::move(callback).Run(test_union->get_union_value()->get_string_value());
}

void MojoEcho::EchoInt8AsNestedUnion(int8_t value,
                                     EchoInt8AsNestedUnionCallback callback) {
  std::move(callback).Run(mojom::NestedUnion::NewInt8Value(value));
}

void MojoEcho::EchoBoolAsNestedUnion(bool value,
                                     EchoBoolAsNestedUnionCallback callback) {
  std::move(callback).Run(
      mojom::NestedUnion::NewUnionValue(mojom::TestUnion::NewBoolValue(value)));
}

void MojoEcho::EchoStringAsNestedUnion(
    const std::string& value,
    EchoStringAsNestedUnionCallback callback) {
  std::move(callback).Run(mojom::NestedUnion::NewUnionValue(
      mojom::TestUnion::NewStringValue(value)));
}

void MojoEcho::EchoNullFromOptionalNestedUnion(
    mojom::NestedUnionPtr test_union,
    EchoNullFromOptionalNestedUnionCallback callback) {
  DCHECK(!test_union);
  std::move(callback).Run();
}

void MojoEcho::EchoInt8FromOptionalNestedUnion(
    mojom::NestedUnionPtr test_union,
    EchoInt8FromOptionalNestedUnionCallback callback) {
  std::move(callback).Run(test_union->get_int8_value());
}

void MojoEcho::EchoBoolFromOptionalNestedUnion(
    mojom::NestedUnionPtr test_union,
    EchoBoolFromOptionalNestedUnionCallback callback) {
  std::move(callback).Run(test_union->get_union_value()->get_bool_value());
}

void MojoEcho::EchoStringFromOptionalNestedUnion(
    mojom::NestedUnionPtr test_union,
    EchoStringFromOptionalNestedUnionCallback callback) {
  std::move(callback).Run(test_union->get_union_value()->get_string_value());
}

void MojoEcho::EchoNullAsOptionalNestedUnion(
    EchoNullAsOptionalNestedUnionCallback callback) {
  std::move(callback).Run(nullptr);
}

void MojoEcho::EchoInt8AsOptionalNestedUnion(
    int8_t value,
    EchoInt8AsOptionalNestedUnionCallback callback) {
  std::move(callback).Run(mojom::NestedUnion::NewInt8Value(value));
}

void MojoEcho::EchoBoolAsOptionalNestedUnion(
    bool value,
    EchoBoolAsOptionalNestedUnionCallback callback) {
  std::move(callback).Run(
      mojom::NestedUnion::NewUnionValue(mojom::TestUnion::NewBoolValue(value)));
}

void MojoEcho::EchoStringAsOptionalNestedUnion(
    const std::string& value,
    EchoStringAsOptionalNestedUnionCallback callback) {
  std::move(callback).Run(mojom::NestedUnion::NewUnionValue(
      mojom::TestUnion::NewStringValue(value)));
}

void MojoEcho::EchoBoolArray(const std::vector<bool>& values,
                             EchoBoolArrayCallback callback) {
  std::move(callback).Run(values);
}

}  // namespace content
