// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_FAKE_API_BINDINGS_H_
#define FUCHSIA_WEB_RUNNERS_CAST_FAKE_API_BINDINGS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/strings/string_piece.h"
#include "fuchsia_web/runners/cast/fidl/fidl/chromium/cast/cpp/fidl.h"

// Simple implementation of the ApiBindings service, for use by tests.
class FakeApiBindingsImpl : public chromium::cast::ApiBindings {
 public:
  FakeApiBindingsImpl();
  ~FakeApiBindingsImpl() override;

  FakeApiBindingsImpl(const FakeApiBindingsImpl&) = delete;
  FakeApiBindingsImpl& operator=(const FakeApiBindingsImpl&) = delete;

  // Returns the message port with the specified |name|. Runs the message loop
  // if a port of the specified name has not yet been received, and returns an
  // invalid handle if that times out before a |Connect()| call is received.
  fidl::InterfaceHandle<fuchsia::web::MessagePort> RunAndReturnConnectedPort(
      base::StringPiece name);

  // Sets the list of bindings which will be returned by GetAll().
  void set_bindings(std::vector<chromium::cast::ApiBinding> bindings) {
    bindings_ = std::move(bindings);
  }

 private:
  // chromium::cast::ApiBindings implementation.
  void GetAll(GetAllCallback callback) override;
  void Connect(
      std::string name,
      fidl::InterfaceHandle<fuchsia::web::MessagePort> message_port) override;

  // Bindings to return from GetAll().
  std::vector<chromium::cast::ApiBinding> bindings_;

  // Holds ports received via Connect(), for tests to take by calling
  // RunAndReturnConnectedPort(). Uses std::less<> as the comparator so that
  // StringPieces can be used for lookup without requiring a conversion.
  base::flat_map<std::string, fidl::InterfaceHandle<fuchsia::web::MessagePort>>
      ports_;

  // Used to wait for a specific port to be Connect()ed.
  base::StringPiece expected_port_name_;
  base::OnceClosure on_expected_port_received_;
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_FAKE_API_BINDINGS_H_
