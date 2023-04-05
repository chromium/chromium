// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_VERSION_INFO_H_
#define COMPONENTS_ZUCCHINI_VERSION_INFO_H_

// This file serves as a stable location for main Zucchini version constants,
// whose names and types should also be stable. These allow external tools to
// determine Zucchini version at compile time by inclusion or parsing.

namespace zucchini {

// A change in major version indicates breaking changes such that a patch
// definitely cannot be applied by a zucchini binary whose major version doesn't
// match. See README.md for logs.
enum : uint16_t { kMajorVersion = 2 };

// A change in minor version indicates possibly breaking changes at the element
// level, such that it may not be possible to apply a patch whose minor version
// doesn't match this version. To determine if a given patch may be applied with
// this version, VerifyPatch() should be called. See README.md for logs.
enum : uint16_t { kMinorVersion = 0 };

// A empty or error value for major or minor version numbers.
enum : uint16_t { kInvalidVersion = 0xffff };

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_VERSION_INFO_H_
