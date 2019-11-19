// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_INTERACTION_PROVIDER_H_
#define EXTENSIONS_RENDERER_BINDINGS_INTERACTION_PROVIDER_H_

#include <memory>

#include "v8/include/v8.h"

namespace extensions {

// Provides user interaction related utilities.
class InteractionProvider {
 public:
  // A token for an interaction. This can be used for deferred creation of an
  // interaction.
  class Token {
   public:
    virtual ~Token() {}
  };
  // The scope for an interaction.
  // A context is assumed to have active interaction while this is present.
  class Scope {
   public:
    virtual ~Scope() {}
  };

  virtual ~InteractionProvider() {}

  // Returns a token representing the current state of interaction,
  // possibly for use in later point in time to create a |Scope|.
  virtual std::unique_ptr<Token> GetCurrentToken(
      v8::Local<v8::Context> v8_context) const = 0;

  // Creates a scoped interaction from a |token|, possibly retrieved earlier.
  virtual std::unique_ptr<Scope> CreateScopedInteraction(
      v8::Local<v8::Context> v8_context,
      std::unique_ptr<Token> token) const = 0;

  // Returns true if |v8_context| has an active interaction.
  virtual bool HasActiveInteraction(
      v8::Local<v8::Context> v8_context) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_INTERACTION_PROVIDER_H_
