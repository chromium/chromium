// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PASS_REF_H_
#define PPAPI_CPP_PASS_REF_H_

/// @file
/// This file defines an annotation for constructors and other functions that
/// take ownership of a pointer.
namespace pp {

/// An annotation for constructors and other functions that take ownership of
/// a pointer. For example, a resource constructor that takes ownership of a
/// provided <code>PP_Resource</code> ref count would take this enumeration to
/// differentiate from the more typical use case of taking its own reference.
enum PassRef { PASS_REF };

}  // namespace pp

#endif  // PPAPI_CPP_PASS_REF_H_
