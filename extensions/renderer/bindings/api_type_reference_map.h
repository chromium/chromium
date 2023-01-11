// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_TYPE_REFERENCE_MAP_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_TYPE_REFERENCE_MAP_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"

namespace extensions {
class APISignature;
class ArgumentSpec;

// A map storing type specifications and method signatures for API definitions.
class APITypeReferenceMap {
 public:
  // A callback used to initialize an unknown type, so that these can be
  // created lazily.
  using InitializeTypeCallback =
      base::RepeatingCallback<void(const std::string& name)>;

  explicit APITypeReferenceMap(InitializeTypeCallback initialize_type);

  APITypeReferenceMap(const APITypeReferenceMap&) = delete;
  APITypeReferenceMap& operator=(const APITypeReferenceMap&) = delete;

  ~APITypeReferenceMap();

  // Adds the |spec| to the map under the given |name|.
  void AddSpec(const std::string& name, std::unique_ptr<ArgumentSpec> spec);

  // Returns the spec for the given |name|.
  const ArgumentSpec* GetSpec(const std::string& name) const;

  // Adds the |signature| to the map under the given |name|. |name| is expected
  // to be fully qualified with the API and method name, e.g. tabs.create.
  void AddAPIMethodSignature(const std::string& name,
                             std::unique_ptr<APISignature> signature);

  // Returns the signature for the given |name|. |name| is expected
  // to be fully qualified with the API and method name, e.g. tabs.create.
  const APISignature* GetAPIMethodSignature(const std::string& name) const;

  // Adds the |signature| to the map under the given |name|. |name| is expected
  // to be fully qualified with API, type, and method (e.g.
  // storage.StorageArea.get).
  void AddTypeMethodSignature(const std::string& name,
                              std::unique_ptr<APISignature> signature);

  // Returns the signature for the given |name|. |name| is expected
  // to be fully qualified with API, type, and method (e.g.
  // storage.StorageArea.get).
  const APISignature* GetTypeMethodSignature(const std::string& name) const;

  // Returns true if the map has a signature for the given |name|. Unlike
  // GetTypeMethodSignature(), this will not try to fetch the type by loading
  // an API.
  bool HasTypeMethodSignature(const std::string& name) const;

  // Adds a custom signature for bindings to use.
  void AddCustomSignature(const std::string& name,
                          std::unique_ptr<APISignature> signature);

  // Looks up a custom signature that was previously added.
  const APISignature* GetCustomSignature(const std::string& name) const;

  // Adds a signature for an API event with the given `event_name`.
  void AddEventSignature(const std::string& event_name,
                         std::unique_ptr<APISignature> signature);

  // Retrieves a signature for an API event with the given `event_name`.
  const APISignature* GetEventSignature(const std::string& event_name) const;

  // Returns the associated APISignature for the given |name|. Logic differs
  // slightly from a normal GetAPIMethodSignature as we don't want to initialize
  // a new type if the signature is not found.
  const APISignature* GetAsyncResponseSignature(const std::string& name) const;

  bool empty() const { return type_refs_.empty(); }
  size_t size() const { return type_refs_.size(); }

 private:
  InitializeTypeCallback initialize_type_;

  std::map<std::string, std::unique_ptr<ArgumentSpec>> type_refs_;

  using SignatureMap = std::map<std::string, std::unique_ptr<APISignature>>;
  SignatureMap api_methods_;
  SignatureMap type_methods_;
  SignatureMap custom_signatures_;
  SignatureMap event_signatures_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_TYPE_REFERENCE_MAP_H_
