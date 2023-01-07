// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_ALIAS_H_
#define EXTENSIONS_COMMON_ALIAS_H_

namespace extensions {

// Information about an alias.
// Main usage: describing aliases for extension features (extension permissions,
// APIs), which is useful for ensuring backward-compatibility of extension
// features when they get renamed. Old feature name can be defined as an alias
// for the new feature name - this would ensure that the extensions using the
// old feature name don't break.
struct Alias {
  // This struct is meant to contain pointers to character string constants,
  // so the lifetime of the pointer parameters should exceed that of the Alias
  // instance.
  constexpr Alias(const char* name, const char* real_name)
      : name(name), real_name(real_name) {}

  // The alias name.
  const char* name;

  // The real name behind the alias.
  const char* real_name;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_ALIAS_H_
