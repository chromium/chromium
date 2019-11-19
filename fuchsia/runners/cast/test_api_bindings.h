// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_TEST_API_BINDINGS_H_
#define FUCHSIA_RUNNERS_CAST_TEST_API_BINDINGS_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"

// Simple implementation of the ApiBindings service, for use by tests.
class TestApiBindings : public chromium::cast::ApiBindings {
 public:
  TestApiBindings();
  ~TestApiBindings() override;

  // Spins a RunLoop until a port named |port_name| is received.
  fidl::InterfaceHandle<::fuchsia::web::MessagePort>
  RunUntilMessagePortReceived(base::StringPiece port_name);

  // Sets the list of bindings which will be returned by GetAll().
  void set_bindings(std::vector<chromium::cast::ApiBinding> bindings) {
    bindings_ = std::move(bindings);
  }

 private:
  // chromium::cast::ApiBindingsManager implementation.
  void GetAll(GetAllCallback callback) override;
  void Connect(
      std::string channel_id,
      fidl::InterfaceHandle<::fuchsia::web::MessagePort> message_port) override;

  std::map<std::string, fidl::InterfaceHandle<::fuchsia::web::MessagePort>>
      ports_;
  std::vector<chromium::cast::ApiBinding> bindings_;
  base::OnceClosure port_received_closure_;

  DISALLOW_COPY_AND_ASSIGN(TestApiBindings);
};

#endif  // FUCHSIA_RUNNERS_CAST_TEST_API_BINDINGS_H_
