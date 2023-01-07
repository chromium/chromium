// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_SERIALIZATION_FORMAT_H_
#define EXTENSIONS_COMMON_API_MESSAGING_SERIALIZATION_FORMAT_H_

namespace extensions {

enum class SerializationFormat {
  kStructuredCloned,
  kJson,
  kLast = kJson,
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_SERIALIZATION_FORMAT_H_
