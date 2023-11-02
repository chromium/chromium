// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_MACROS_H
#define CRAZY_LINKER_MACROS_H

// Use this inside a class declaration to disallow copy construction and
// assignment.
#define CRAZY_DISALLOW_COPY_OPERATIONS(Class) \
  Class(const Class&) = delete;               \
  Class& operator=(const Class&) = delete;

// Use this inside a class declaration to disallow move construction and
// assignment.
#define CRAZY_DISALLOW_MOVE_OPERATIONS(Class) \
  Class(Class&&) = delete;                    \
  Class& operator=(Class&&) = delete;

// Use this inside a class declaration to disallow both copy and move
// construction and assignments.
#define CRAZY_DISALLOW_COPY_AND_MOVE_OPERATIONS(Class) \
  CRAZY_DISALLOW_COPY_OPERATIONS(Class)                \
  CRAZY_DISALLOW_MOVE_OPERATIONS(Class)

#endif  // CRAZY_LINKER_MACROS_H
